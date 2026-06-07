#pragma once

#include <stdint.h>

#include "MachineState.h"

class SafetyManager;
class StepperBackendFastAccel;

class HomingController {
 public:
  enum class Axis : uint8_t {
    X,
    Y,
  };

  enum class State : uint8_t {
    Idle,
    SeekFastX,
    BackoffX,
    SeekSlowX,
    SetXZero,
    SeekFastY,
    BackoffY,
    SeekSlowY,
    SetYZero,
    Complete,
    Alarm,
  };

  bool runHomeX(StepperBackendFastAccel& backend, SafetyManager& safety,
                MachineState& machine);
  bool runHomeY(StepperBackendFastAccel& backend, SafetyManager& safety,
                MachineState& machine);
  bool runHome(StepperBackendFastAccel& backend, SafetyManager& safety,
               MachineState& machine);
  State state() const;
  const char* stateName() const;
  const char* lastReason() const;

 private:
  bool homeAxis(Axis axis, StepperBackendFastAccel& backend,
                SafetyManager& safety, MachineState& machine);
  bool moveIncrement(Axis axis, int8_t direction, float feed_mm_min,
                     bool stop_on_target_limit,
                     StepperBackendFastAccel& backend, SafetyManager& safety,
                     MachineState& machine,
                     bool& other_limit_allowed_active);
  bool targetLimitActive(Axis axis, const SafetyManager& safety) const;
  bool targetLimitRawActive(Axis axis, const SafetyManager& safety) const;
  bool targetLimitAnyActive(Axis axis, const SafetyManager& safety) const;
  bool otherLimitActive(Axis axis, const SafetyManager& safety) const;
  bool otherLimitUnexpected(Axis axis, const SafetyManager& safety,
                            const MachineState& machine,
                            bool other_limit_allowed_active) const;
  void setState(State state, MachineState& machine, const char* reason);
  void setCompletePosition(Axis axis, MachineState& machine);
  void markAlarm(const char* reason, SafetyManager& safety,
                 MachineState& machine);
  void setLastReason(const char* reason);

  State state_ = State::Idle;
  char last_reason_[64] = "IDLE";
};
