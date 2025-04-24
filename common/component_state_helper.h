/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMMON_COMPONENT_STATE_HELPER_H_
#define COMMON_COMPONENT_STATE_HELPER_H_

#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "component_state_helper_interface.h"
#include "dbconnector.h"
#include "logger.h"
#include "selectableevent.h"
#include "subscriberstatetable.h"

namespace swss {

// The ComponentStateHelper provides helper methods to report and query
// component state. This class will write to DB and cache when component state
// is reported. However, it does not update cache values from external DB
// change. All queries will be returned from the cache instead of reading the DB
// values.
// When multiple processes associate to a single component, they should use the
// SystemStateHelper to receive the correct component state.
class ComponentStateHelper : public ComponentStateHelperInterface {
 public:
  ~ComponentStateHelper() = default;
  // ComponentStateHelper is neither copyable nor movable.
  ComponentStateHelper(const ComponentStateHelper&) = delete;
  ComponentStateHelper& operator=(const ComponentStateHelper&) = delete;
  ComponentStateHelper(ComponentStateHelper&&) = delete;
  ComponentStateHelper& operator=(ComponentStateHelper&&) = delete;

  // Override methods.
  bool ReportComponentState(ComponentState state,
                            const std::string& reason) override;
  ComponentStateInfo StateInfo() const override;
  bool ReportHardwareError(
      const std::string& reason,
      ComponentState state = ComponentState::kMinor) override;

 private:
  ComponentStateHelper(SystemComponent component);

  // Execute the LUA script to update the state.
  // Returns true if state update is successful.
  bool CallLuaScript(ComponentState state, const std::string& reason,
                     bool hw_err);

  // The component that this helper manages.
  SystemComponent component_;
  // The name of this component, will be written into COMPONENT_STATE_TABLE.
  std::string component_id_;
  // Current state info of this component.
  ComponentStateInfo state_info_;
  // Hash of the component_state_helper.lua script.
  std::string lua_sha_;
  // Redis DB connector.
  DBConnector db_;
  // Mutex that guards all component state update related fields.
  mutable std::mutex mutex_;

  // The instance manager class and the unit test class need to be a friend
  // class in order to use the private constructor.
  friend class StateHelperManager;
  friend class ComponentStateHelperTest;
};

class SystemStateHelper : public SystemStateHelperInterface {
 public:
  ~SystemStateHelper();
  // SystemStateHelper is neither copyable nor movable.
  SystemStateHelper(const SystemStateHelper&) = delete;
  SystemStateHelper& operator=(const SystemStateHelper&) = delete;
  SystemStateHelper(SystemStateHelper&&) = delete;
  SystemStateHelper& operator=(SystemStateHelper&&) = delete;

  // Override methods.
  SystemState GetSystemState() const override;
  bool IsSystemCritical() const override;
  std::string GetSystemCriticalReason() const override;
  std::unordered_map<SystemComponent, ComponentStateInfo, SystemComponentHash> AllComponentStates()
      const override;

 private:
  SystemStateHelper();

  // This function runs in a seperated thread for receiving and processing
  // component state update notifications.
  void ComponentStateUpdateThread();

  // Processes component state update. This will update self state and
  // dependent component states in the cache from the DB.
  void ProcessComponentStateUpdate();

  // System state.
  SystemState system_state_;
  // System critical reason.
  std::string system_critical_reason_;
  // The cached components that the container monitor reports INACTIVE state.
  std::unordered_set<std::string> essential_inactive_components_;
  // Component state information.
  std::unordered_map<SystemComponent, ComponentStateInfo, SystemComponentHash> component_state_info_;
  // Redis DB connector.
  DBConnector db_;
  // Redis DB connector used in the ComponentStateUpdateThread for receiving
  // notification.
  DBConnector select_db_;
  // SubscriberStateTable used in the ComponentStateUpdateThread for receiving
  // DB update.
  SubscriberStateTable component_state_table_;
  // Mutex that guards all component state update related fields.
  mutable std::mutex mutex_;
  // Thread that runs ComponentStateUpdateThread.
  std::unique_ptr<std::thread> update_thread_;
  // Notification used for shutting down the ComponentStateUpdateThread.
  SelectableEvent shutdown_notification_;

  // The instance manager class and the unit test class need to be a friend
  // class in order to use the private constructor.
  friend class StateHelperManager;
  friend class ComponentStateHelperTest;
};

// StateHelperManager manages the singleton instances for
// ComponentStateHelper and SystemStateHelper.
// API user must use this manager class to access the helpers.
class StateHelperManager {
 public:
  // Returns a singleton instance of ComponentStateHelperInterface for a
  // component.
  // If SetTestComponentSingleton is not called, this will return a
  // ComponentStateHelper instance.
  static ComponentStateHelperInterface& ComponentSingleton(
      SystemComponent component) {
    const std::lock_guard<std::mutex> lock(
        component_state_helper_instances_mutex_);
    if (component_state_helper_instances_.count(component) == 0) {
      component_state_helper_instances_[component] =
          dynamic_cast<ComponentStateHelperInterface*>(new ComponentStateHelper(component));
    }
    return *component_state_helper_instances_[component];
  }

  // Sets the ComponentStateHelperInterface singleton to a test instance for a
  // component.
  // Use for testing purpose only.
  // This function does not delete the existing instance if it exists.
  // Must be called before the first call of ComponentSingleton for
  // that component.
  static void SetTestComponentSingleton(
      SystemComponent component, ComponentStateHelperInterface* instance) {
    const std::lock_guard<std::mutex> lock(
        component_state_helper_instances_mutex_);
    assert(((void)"Attempting to overwrite a component state helper with a "
                  "test instance.",
            component_state_helper_instances_.count(component) == 0));
    component_state_helper_instances_[component] = instance;
  }

  // Returns a singleton instance of SystemStateHelperInterface.
  // If SetTestSystemSingleton is not called, this will return a
  // SystemStateHelper instance.
  static SystemStateHelperInterface& SystemSingleton() {
    const std::lock_guard<std::mutex> lock(system_state_helper_instance_mutex_);
    if (system_state_helper_instance_ == nullptr) {
      system_state_helper_instance_ = dynamic_cast<SystemStateHelperInterface*>(new SystemStateHelper());
    }
    return *system_state_helper_instance_;
  }

  // Sets the SystemStateHelperInterface singleton to a test instance.
  // Use for testing purpose only.
  // This function does not delete the existing instance if it exists.
  // Must be called before the first call of SystemSingleton.
  static void SetTestSystemSingleton(SystemStateHelperInterface* instance) {
    const std::lock_guard<std::mutex> lock(system_state_helper_instance_mutex_);
    assert(((void)"Attempting to overwrite a system state helper with a test "
                  "instance.",
            system_state_helper_instance_ == nullptr));
    system_state_helper_instance_ = instance;
  }

 private:
  StateHelperManager() = default;

  // ComponentStateHelperInterface singleton instances.
  static std::unordered_map<SystemComponent, ComponentStateHelperInterface*, SystemComponentHash>
      component_state_helper_instances_;
  // SystemStateHelperInterface singleton instance.
  static SystemStateHelperInterface* system_state_helper_instance_;
  // Mutex that guards the ComponentStateHelperInterface singletion instances.
  static std::mutex component_state_helper_instances_mutex_;
  // Mutex that guards the SystemStateHelperInterface singletion instance.
  static std::mutex system_state_helper_instance_mutex_;
};

}  // namespace swss

#endif  // COMMON_COMPONENT_STATE_HELPER_H_
