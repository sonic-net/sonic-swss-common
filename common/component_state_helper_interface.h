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

#ifndef COMMON_COMPONENT_STATE_HELPER_INTERFACE_H_
#define COMMON_COMPONENT_STATE_HELPER_INTERFACE_H_

#include <ctime>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "logger.h"

namespace swss {

// A component provides a set of specific services for the system.
// A component can be composed of multiple processes.
enum class SystemComponent {
  kHost,
  kP4rt,
  kOrchagent,
  kSyncd,
  kTelemetry,
  kLinkqual,
  kPlatformMonitor,
  kInbandmgr,
  kSwssCfgmgr,
};

// C++11 requires custom hash for sets and tuples.
struct SystemComponentHash {
  template <typename T>
  size_t operator()(T t) const {
    return static_cast<std::size_t>(t);
  }
};

// Component string will have the format of <container_name>:<process_name>.
inline std::string SystemComponentToString(SystemComponent component) {
  switch (component) {
    case SystemComponent::kHost:
      return "host";
    case SystemComponent::kP4rt:
      return "p4rt:p4rt";
    case SystemComponent::kOrchagent:
      return "swss:orchagent";
    case SystemComponent::kSyncd:
      return "syncd:syncd";
    case SystemComponent::kTelemetry:
      return "telemetry:telemetry";
    case SystemComponent::kLinkqual:
      return "linkqual:linkqual";
    case SystemComponent::kPlatformMonitor:
      return "pmon";
    case SystemComponent::kInbandmgr:
      return "inbandmgr:inbandapp";
    case SystemComponent::kSwssCfgmgr:
      return "swss:cfgmgr";
    default:
      SWSS_LOG_ERROR("Invalid SystemComponent");
      return "invalid:invalid";
  }
}

// Converts a component state string into SystemComponent enum.
// Returns true if success.
inline bool StringToSystemComponent(std::string component_str,
                                    SystemComponent& component) {
  static const auto* const kStringToComponentMap =
      new std::unordered_map<std::string, SystemComponent>({
          {"host", SystemComponent::kHost},
          {"p4rt:p4rt", SystemComponent::kP4rt},
          {"swss:orchagent", SystemComponent::kOrchagent},
          {"syncd:syncd", SystemComponent::kSyncd},
          {"telemetry:telemetry", SystemComponent::kTelemetry},
          {"linkqual:linkqual", SystemComponent::kLinkqual},
          {"pmon", SystemComponent::kPlatformMonitor},
          {"inbandmgr:inbandapp", SystemComponent::kInbandmgr},
          {"swss:cfgmgr", SystemComponent::kSwssCfgmgr},
      });

  auto lookup = kStringToComponentMap->find(component_str);
  if (lookup == kStringToComponentMap->end()) {
    return false;
  }
  component = lookup->second;
  return true;
}

// Returns a list of essential components, which provide core functionalities to
// the system.
inline const std::unordered_set<SystemComponent, SystemComponentHash>&
    EssentialComponents() {
  static const auto* const kEssentialComponents =
      new std::unordered_set<SystemComponent, SystemComponentHash>(
          {SystemComponent::kHost, SystemComponent::kP4rt,
           SystemComponent::kOrchagent, SystemComponent::kSyncd,
           SystemComponent::kTelemetry, SystemComponent::kPlatformMonitor,
           SystemComponent::kInbandmgr});
  return *kEssentialComponents;
}

enum class ComponentState {
  // Service is running but not yet ready to serve.
  // If no state is ever reported for a component, it is also considered as in
  // kInitializing state.
  kInitializing,
  // Service is ready and fully functional.
  kUp,
  // Service is running but encountered a minor error. Service is not impacted.
  kMinor,
  // Service is running but encountered an uncorrectable error. Service may be
  // impacted.
  kError,
  // Service is not running and it will not be restarted.
  kInactive,
};

inline std::string ComponentStateToString(ComponentState state) {
  switch (state) {
    case ComponentState::kInitializing:
      return "INITIALIZING";
    case ComponentState::kUp:
      return "UP";
    case ComponentState::kMinor:
      return "MINOR";
    case ComponentState::kError:
      return "ERROR";
    case ComponentState::kInactive:
      return "INACTIVE";
    default:
      SWSS_LOG_ERROR("Invalid ComponentState");
      return "INVALID";
  }
}

// Converts a component state string into ComponentState enum.
// Returns true if success.
inline bool StringToComponentState(std::string state_str,
                                   ComponentState& state) {
  static const auto* const kStringToComponentStateMap =
      new std::unordered_map<std::string, ComponentState>({
          {"INITIALIZING", ComponentState::kInitializing},
          {"UP", ComponentState::kUp},
          {"MINOR", ComponentState::kMinor},
          {"ERROR", ComponentState::kError},
          {"INACTIVE", ComponentState::kInactive},
      });

  auto lookup = kStringToComponentStateMap->find(state_str);
  if (lookup == kStringToComponentStateMap->end()) {
    SWSS_LOG_ERROR("Invalid ComponentState string %s", state_str.c_str());
    return false;
  }
  state = lookup->second;
  return true;
}

// ComponentStateInfo includes the basic information of the component state.
// It containes the state enum, reason, and timestamp.
struct ComponentStateInfo {
  ComponentState state = ComponentState::kInitializing;
  std::string reason = "";
  timespec timestamp = {};

  bool operator==(const ComponentStateInfo& x) const {
    return state == x.state && reason == x.reason &&
           timestamp.tv_sec == x.timestamp.tv_sec &&
           timestamp.tv_nsec == x.timestamp.tv_nsec;
  }

  bool operator!=(const ComponentStateInfo& x) const { return !((*this) == x); }
};

// ComponentStateHelperInterface provides helper methods to report and query
// component state.
class ComponentStateHelperInterface {
 public:
  virtual ~ComponentStateHelperInterface() = default;

  // Reports a component state update.
  // Returns true if the state update was successful.
  // Returns false if the state update did not happen.
  // Implementation should log whether state update attempt was successful or
  // failed, no need for client code to log this again.
  virtual bool ReportComponentState(ComponentState state,
                                    const std::string& reason) = 0;

  // Reports kMinor or kError state with indication of a hardware related error.
  // Same as ReportComponentState but only applies to kMinor and kError state.
  virtual bool ReportHardwareError(
      const std::string& reason,
      ComponentState state = ComponentState::kMinor) = 0;

  // Returns the self component state information.
  // Returns the default value of ComponentStateInfo if the component did not
  // report any state.
  virtual ComponentStateInfo StateInfo() const = 0;
};

// SystemState represents the overall system's status.
// It is determined by the states of the essential components.
enum class SystemState {
  // When any essential component is in kInitializing state.
  kSysInitializing,
  // When all essential components are in the kUp or kMinor state.
  kSysUp,
  // When any essential component is in kError or kInactive state.
  kCritical
};

// SystemStateHelperInterface provides helper methods to query the overall
// system state.
// It also provide helper methods to query all components' state information.
class SystemStateHelperInterface {
 public:
  virtual ~SystemStateHelperInterface() = default;

  // Gets the overall system state.
  virtual SystemState GetSystemState() const = 0;

  // Returns true if the overall system state is kCritical.
  virtual bool IsSystemCritical() const = 0;

  // Returns a description of why the system state is kCritical.
  // Returns empty string if the system state is not kCritical.
  // The description should be a concatenation of each kError or kInactive
  // essential component's description, with space as separator. Each component
  // description should have the following format (without quotes):
  // "<component_id> in state <state_str> with reason: <reason>."
  virtual std::string GetSystemCriticalReason() const = 0;

  // Returns state information of all components.
  // For essential components, if they didn't report any state, the
  // kInitializing state will be returned. For non-essential components, if they
  // didn't report any state, no state information will be returned for them.
  virtual std::unordered_map<SystemComponent, ComponentStateInfo,
            SystemComponentHash>
  AllComponentStates() const = 0;
};

}  // namespace swss

#endif  // COMMON_COMPONENT_STATE_HELPER_INTERFACE_H_
