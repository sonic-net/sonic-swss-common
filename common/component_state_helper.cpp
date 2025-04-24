// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "component_state_helper.h"

#include <ctime>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.h"
#include "redisapi.h"
#include "rediscommand.h"
#include "redisreply.h"
#include "select.h"
#include "subscriberstatetable.h"

namespace swss {
namespace {

constexpr char kComponentStateTable[] = "COMPONENT_STATE_TABLE";

// Parses DB info into ComponentStateInfo.
// Returns true if success.
bool ParseComponentStateInfo(const std::vector<FieldValueTuple>& info,
                             ComponentStateInfo& state_info, bool& essential) {
  constexpr char kStateKey[] = "state";
  constexpr char kReasonKey[] = "reason";
  constexpr char kTimestampSecondKey[] = "timestamp-seconds";
  constexpr char kTimestampNanoSecondKey[] = "timestamp-nanoseconds";
  constexpr char kEssentialKey[] = "essential";

  state_info = ComponentStateInfo();
  essential = false;
  for (const FieldValueTuple& field : info) {
    if (fvField(field) == kStateKey) {
      if (!StringToComponentState(fvValue(field), state_info.state)) {
        SWSS_LOG_ERROR("Invalid state in state info: %s",
                       fvValue(field).c_str());
        return false;
      }
    } else if (fvField(field) == kReasonKey) {
      state_info.reason = fvValue(field);
    } else if (fvField(field) == kTimestampSecondKey) {
      state_info.timestamp.tv_sec =
          static_cast<time_t>(std::stoi(fvValue(field)));
    } else if (fvField(field) == kTimestampNanoSecondKey) {
      state_info.timestamp.tv_nsec = std::stoi(fvValue(field));
    } else if (fvField(field) == kEssentialKey && fvValue(field) == "true") {
      essential = true;
    }
  }

  return true;
}

}  // namespace

std::unordered_map<SystemComponent, ComponentStateHelperInterface*,
                   SystemComponentHash>
    StateHelperManager::component_state_helper_instances_ =
        std::unordered_map<SystemComponent, ComponentStateHelperInterface*,
                           SystemComponentHash>{};

std::mutex StateHelperManager::component_state_helper_instances_mutex_;

SystemStateHelperInterface* StateHelperManager::system_state_helper_instance_ =
    nullptr;

std::mutex StateHelperManager::system_state_helper_instance_mutex_;

ComponentStateHelper::ComponentStateHelper(SystemComponent component)
    : component_(component),
      component_id_(SystemComponentToString(component)),
      db_("STATE_DB", 0) {
  lua_sha_ = loadRedisScript(&db_, loadLuaScript("component_state_helper.lua"));
}

bool ComponentStateHelper::CallLuaScript(ComponentState state,
                                         const std::string& reason,
                                         bool hw_err) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timespec ts;
  timespec_get(&ts, TIME_UTC);
  char timestamp[32];  // Timestamp string has format "yyyy-MM-dd HH:mm:ss".
  strftime(timestamp, sizeof(timestamp), "%F %T", std::localtime(&ts.tv_sec));
  uint64_t timestamp_seconds = ts.tv_sec;
  uint64_t timestamp_nanoseconds = ts.tv_nsec;
  std::string hardware_err = (hw_err) ? "true" : "false";
  std::string essential_flag =
      (EssentialComponents().count(component_) != 0) ? "true" : "false";
  RedisCommand command;
  // The LUA script defines all state update logic.
  // Refer to component_state_helper.lua for details.
  command.format("EVALSHA %s 1 %s| %s %s %s %s %llu %llu %s %s",
                 lua_sha_.c_str(), kComponentStateTable, component_id_.c_str(),
                 ComponentStateToString(state).c_str(), reason.c_str(),
                 timestamp, timestamp_seconds, timestamp_nanoseconds,
                 hardware_err.c_str(), essential_flag.c_str());
  RedisReply r(&db_, command, REDIS_REPLY_ARRAY);
  auto ctx = r.getContext();

  // The LUA script should return 3 arguments.
  // Refer to component_state_helper.lua for details.
  if (ctx->type != REDIS_REPLY_ARRAY || ctx->elements != 3) {
    SWSS_LOG_ERROR("Error in reporting state %s: %s",
                   ComponentStateToString(state).c_str(), reason.c_str());
    return false;
  }

  const char* updated = ctx->element[0]->str;
  const char* old_state = ctx->element[1]->str;
  const char* current_state = ctx->element[2]->str;
  if (strcmp(updated, "true") != 0) {
    SWSS_LOG_ERROR(
        "Component %s failed to update state from %s to %s with reason: %s.",
        component_id_.c_str(), old_state, ComponentStateToString(state).c_str(),
        reason.c_str());
    return false;
  }
  if (state != ComponentState::kInitializing && state != ComponentState::kUp) {
    SWSS_LOG_ERROR(
        "Component %s successfully updated state from %s to %s with reason: "
        "%s.",
        component_id_.c_str(), old_state, current_state, reason.c_str());
  }
  state_info_.state = state;
  state_info_.reason = reason;
  state_info_.timestamp = ts;
  return true;
}

bool ComponentStateHelper::ReportComponentState(ComponentState state,
                                                const std::string& reason) {
  return CallLuaScript(state, reason, false);
}

bool ComponentStateHelper::ReportHardwareError(const std::string& reason,
                                               ComponentState state) {
  if (state != ComponentState::kMinor && state != ComponentState::kError) {
    return false;
  }
  return CallLuaScript(state, reason, true);
}

ComponentStateInfo ComponentStateHelper::StateInfo() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return state_info_;
}

SystemStateHelper::SystemStateHelper()
    : db_("STATE_DB", 0),
      select_db_("STATE_DB", 0),
      component_state_table_(&select_db_, kComponentStateTable) {
  for (const SystemComponent essential_component : EssentialComponents()) {
    component_state_info_[essential_component] = ComponentStateInfo();
  }
  ProcessComponentStateUpdate();
  update_thread_ = std::unique_ptr<std::thread>(
      new std::thread(&SystemStateHelper::ComponentStateUpdateThread, this));
}

SystemStateHelper::~SystemStateHelper() {
  if (update_thread_ != nullptr) {
    shutdown_notification_.notify();
    update_thread_->join();
  }
}

void SystemStateHelper::ComponentStateUpdateThread() {
  Select s;
  s.addSelectable(&component_state_table_);
  s.addSelectable(&shutdown_notification_);
  while (true) {
    Selectable* sel;
    if (s.select(&sel) != Select::OBJECT) {
      SWSS_LOG_ERROR(
          "SystemStateHelper::ComponentStateUpdateThread failed to select "
          "event: %s",
          strerror(errno));
      continue;
    }
    if (sel == static_cast<Selectable*>(&component_state_table_)) {
      ProcessComponentStateUpdate();
    } else if (sel == static_cast<Selectable*>(&shutdown_notification_)) {
      return;
    } else {
      SWSS_LOG_ERROR(
          "SystemStateHelper::ComponentStateUpdateThread unknown selectable "
          "returned.");
    }
  }
}

void SystemStateHelper::ProcessComponentStateUpdate() {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::deque<KeyOpFieldsValuesTuple> db_updates;
  component_state_table_.pops(db_updates);
  for (const auto& db_update : db_updates) {
    const std::string& component_id = kfvKey(db_update);
    const std::string& op = kfvOp(db_update);
    const std::vector<FieldValueTuple>& fields_values =
        kfvFieldsValues(db_update);
    SystemComponent component;
    ComponentStateInfo state_info;
    bool essential;
    if (!ParseComponentStateInfo(fields_values, state_info, essential)) {
      continue;  // Error is logged inside ParseComponentStateInfo.
    }
    if (essential && state_info.state == ComponentState::kInactive) {
      essential_inactive_components_.insert(component_id);
    }
    if (!StringToSystemComponent(component_id, component)) {
      continue;
    }
    if (op == DEL_COMMAND) {
      // If state information is not present in the DB, we will use the
      // default value of ComponentStateInfo for essential components. Refer to
      // the comments of AllComponentStates() in
      // component_state_helper_interface.h.
      if (EssentialComponents().count(component) != 0) {
        component_state_info_[component] = ComponentStateInfo();
      } else {
        component_state_info_.erase(component);
      }
      continue;
    }
    component_state_info_[component] = state_info;
  }
  if (!essential_inactive_components_.empty()) {
    system_state_ = SystemState::kCritical;
    system_critical_reason_ = "";
    for (const auto& component : essential_inactive_components_) {
      if (!system_critical_reason_.empty()) {
        system_critical_reason_ += ", ";
      }
      system_critical_reason_ += component;
    }
    system_critical_reason_ =
        std::string("Container monitor reports INACTIVE for components: ") +
        system_critical_reason_;
    return;
  }
  system_state_ = SystemState::kSysUp;
  std::stringstream reason_stream;
  for (const SystemComponent essential_component : EssentialComponents()) {
    const ComponentStateInfo& state_info =
        component_state_info_[essential_component];
    if (state_info.state == ComponentState::kError ||
        state_info.state == ComponentState::kInactive) {
      system_state_ = SystemState::kCritical;
      reason_stream << SystemComponentToString(essential_component)
                    << " in state " << ComponentStateToString(state_info.state)
                    << " with reason: " << state_info.reason << ". ";
    } else if (state_info.state == ComponentState::kInitializing &&
               system_state_ != SystemState::kCritical) {
      system_state_ = SystemState::kSysInitializing;
    }
  }
  system_critical_reason_ = reason_stream.str();
  // Removing the last space in system critical reason.
  if (!system_critical_reason_.empty()) {
    system_critical_reason_.pop_back();
  }
}

SystemState SystemStateHelper::GetSystemState() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return system_state_;
}

bool SystemStateHelper::IsSystemCritical() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return system_state_ == SystemState::kCritical;
}

std::string SystemStateHelper::GetSystemCriticalReason() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return system_critical_reason_;
}

std::unordered_map<SystemComponent, ComponentStateInfo, SystemComponentHash>
SystemStateHelper::AllComponentStates() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return component_state_info_;
}

}  // namespace swss
