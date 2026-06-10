#include "HomingController.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "PlotterConfig.h"
#include "SafetyManager.h"
#include "StepperBackendFastAccel.h"

namespace {
constexpr uint32_t kMovePollMs = 1;

int32_t absoluteASteps(float x_mm, float y_mm) {
  return lroundf((x_mm + y_mm) * STEPS_PER_MM);
}

int32_t absoluteBSteps(float x_mm, float y_mm) {
  return lroundf((x_mm - y_mm) * STEPS_PER_MM);
}

float maxTravelForAxis(HomingController::Axis axis) {
  return axis == HomingController::Axis::X ? HOMING_MAX_TRAVEL_X_MM
                                           : HOMING_MAX_TRAVEL_Y_MM;
}

int8_t directionForAxis(HomingController::Axis axis) {
  return axis == HomingController::Axis::X ? HOMING_X_DIR : HOMING_Y_DIR;
}

const char* axisName(HomingController::Axis axis) {
  return axis == HomingController::Axis::X ? "X" : "Y";
}

void applyABStepDeltaToMachine(int32_t delta_a_steps, int32_t delta_b_steps,
                               MachineState& machine) {
  machine.a_steps += delta_a_steps;
  machine.b_steps += delta_b_steps;
  machine.x_mm +=
      static_cast<float>(delta_a_steps + delta_b_steps) / (2.0f * STEPS_PER_MM);
  machine.y_mm +=
      static_cast<float>(delta_a_steps - delta_b_steps) / (2.0f * STEPS_PER_MM);
}
}

bool HomingController::runHomeX(StepperBackendFastAccel& backend,
                                SafetyManager& safety,
                                MachineState& machine) {
  const bool ok = homeAxis(Axis::X, backend, safety, machine);
  machine.homed = machine.x_homed && machine.y_homed;
  if (ok) {
    setState(State::Complete, machine, "COMPLETE");
  }
  safety.setHomingActive(false);
  machine.homing_active = false;
  return ok;
}

bool HomingController::runHomeY(StepperBackendFastAccel& backend,
                                SafetyManager& safety,
                                MachineState& machine) {
  const bool ok = homeAxis(Axis::Y, backend, safety, machine);
  machine.homed = machine.x_homed && machine.y_homed;
  if (ok) {
    setState(State::Complete, machine, "COMPLETE");
  }
  safety.setHomingActive(false);
  machine.homing_active = false;
  return ok;
}

bool HomingController::runHome(StepperBackendFastAccel& backend,
                               SafetyManager& safety, MachineState& machine) {
  if (!homeAxis(Axis::X, backend, safety, machine)) {
    safety.setHomingActive(false);
    machine.homing_active = false;
    return false;
  }
  if (!homeAxis(Axis::Y, backend, safety, machine)) {
    safety.setHomingActive(false);
    machine.homing_active = false;
    return false;
  }
  machine.homed = machine.x_homed && machine.y_homed;
  setState(State::Complete, machine, "COMPLETE");
  safety.setHomingActive(false);
  machine.homing_active = false;
  logMessage("HOME complete X=%.3f Y=%.3f A=%ld B=%ld limitX=%s limitY=%s",
             machine.x_mm, machine.y_mm, machine.a_steps, machine.b_steps,
             safety.xLimitActive() ? "ON" : "OFF",
             safety.yLimitActive() ? "ON" : "OFF");
  return true;
}

HomingController::State HomingController::state() const { return state_; }

const char* HomingController::stateName() const {
  switch (state_) {
    case State::Idle:
      return "Idle";
    case State::SeekFastX:
      return "SeekFastX";
    case State::BackoffX:
      return "BackoffX";
    case State::SeekSlowX:
      return "SeekSlowX";
    case State::SetXZero:
      return "SetXZero";
    case State::SeekFastY:
      return "SeekFastY";
    case State::BackoffY:
      return "BackoffY";
    case State::SeekSlowY:
      return "SeekSlowY";
    case State::SetYZero:
      return "SetYZero";
    case State::Complete:
      return "Complete";
    case State::Alarm:
      return "Alarm";
  }
  return "Unknown";
}

const char* HomingController::lastReason() const { return last_reason_; }

bool HomingController::homeAxis(Axis axis, StepperBackendFastAccel& backend,
                                SafetyManager& safety,
                                MachineState& machine) {
  if (!safety.validateHomingStart()) {
    return false;
  }
  if (backend.isRunning()) {
    logMessage("REJECT: homing backend is running");
    return false;
  }

  machine.homed = false;
  if (axis == Axis::X) {
    machine.x_homed = false;
  } else {
    machine.y_homed = false;
  }
  machine.homing_active = true;
  safety.setHomingActive(true);
  safety.poll();
  const int8_t seek_dir = directionForAxis(axis);
  bool other_limit_allowed_active = otherLimitAnyActive(axis, safety);
  const bool target_limit_active_at_start = targetLimitAnyActive(axis, safety);
  const float backoff_limit_mm =
      target_limit_active_at_start ? HOMING_START_BACKOFF_MM : HOMING_BACKOFF_MM;

  State seek_fast = axis == Axis::X ? State::SeekFastX : State::SeekFastY;
  State backoff = axis == Axis::X ? State::BackoffX : State::BackoffY;
  State seek_slow = axis == Axis::X ? State::SeekSlowX : State::SeekSlowY;
  State set_zero = axis == Axis::X ? State::SetXZero : State::SetYZero;

  setState(seek_fast, machine, "START");
  logMessage("HOME_%s started direction=%d fast=%.3f slow=%.3f backoff=%.3f max=%.3f limitRaw=%s limitDebounced=%s",
             axisName(axis), seek_dir, HOMING_SEEK_FEED_MM_MIN,
             HOMING_SLOW_FEED_MM_MIN, backoff_limit_mm,
             maxTravelForAxis(axis),
             targetLimitRawActive(axis, safety) ? "ON" : "OFF",
             targetLimitActive(axis, safety) ? "ON" : "OFF");

  if (target_limit_active_at_start) {
    setState(backoff, machine, "LIMIT_ON_AT_START");
  }

  for (;;) {
    bool stop_condition_met = false;
    safety.poll();
    if (other_limit_allowed_active && !otherLimitAnyActive(axis, safety)) {
      other_limit_allowed_active = false;
    }
    if (safety.isAlarmed()) {
      markAlarm(safety.alarmReason(), safety, machine);
      return false;
    }
    if (isMotionAbortRequested()) {
      clearMotionAbort();
      backend.stop();
      markAlarm("abort requested", safety, machine);
      return false;
    }
    if (otherLimitUnexpected(axis, safety, machine,
                             other_limit_allowed_active)) {
      markAlarm("homing target-other limit active", safety, machine);
      return false;
    }

    switch (state_) {
      case State::SeekFastX:
      case State::SeekFastY:
        if (targetLimitAnyActive(axis, safety)) {
          setState(backoff, machine, "LIMIT_PRESSED_SEEK_FAST_TO_BACKOFF");
          break;
        }
        if (!moveUntilCondition(axis, seek_dir, maxTravelForAxis(axis),
                                HOMING_SEEK_FEED_MM_MIN,
                                MoveStopCondition::TargetAnyActive, backend,
                                safety, machine, other_limit_allowed_active,
                                stop_condition_met)) {
          return false;
        }
        if (!stop_condition_met) {
          markAlarm("homing seek fast max travel", safety, machine);
          return false;
        }
        setState(backoff, machine, "LIMIT_PRESSED_SEEK_FAST_TO_BACKOFF");
        break;

      case State::BackoffX:
      case State::BackoffY:
        if (!targetLimitAnyActive(axis, safety)) {
          setState(seek_slow, machine, "BACKOFF_LIMIT_RELEASED");
          break;
        }
        if (!moveUntilCondition(axis, -seek_dir, backoff_limit_mm,
                                HOMING_SEEK_FEED_MM_MIN,
                                MoveStopCondition::TargetAnyReleased, backend,
                                safety, machine, other_limit_allowed_active,
                                stop_condition_met)) {
          return false;
        }
        if (!stop_condition_met) {
          markAlarm("homing backoff limit still on", safety, machine);
          return false;
        }
        setState(seek_slow, machine, "BACKOFF_LIMIT_RELEASED");
        break;

      case State::SeekSlowX:
      case State::SeekSlowY:
        if (targetLimitActive(axis, safety)) {
          setState(set_zero, machine, "LIMIT_PRESSED_SEEK_SLOW_TO_SET_ZERO");
          break;
        }
        if (!moveUntilCondition(axis, seek_dir, maxTravelForAxis(axis),
                                HOMING_SLOW_FEED_MM_MIN,
                                MoveStopCondition::TargetDebouncedActive,
                                backend, safety, machine,
                                other_limit_allowed_active,
                                stop_condition_met)) {
          return false;
        }
        if (!stop_condition_met) {
          markAlarm("homing seek slow max travel", safety, machine);
          return false;
        }
        setState(set_zero, machine, "LIMIT_PRESSED_SEEK_SLOW_TO_SET_ZERO");
        break;

      case State::SetXZero:
      case State::SetYZero:
        setCompletePosition(axis, machine);
        logMessage("HOME_%s set zero X=%.3f Y=%.3f A=%ld B=%ld limitRaw=%s limitDebounced=%s",
                   axisName(axis), machine.x_mm, machine.y_mm,
                   machine.a_steps, machine.b_steps,
                   targetLimitRawActive(axis, safety) ? "ON" : "OFF",
                   targetLimitActive(axis, safety) ? "ON" : "OFF");
        return true;

      case State::Idle:
      case State::Complete:
      case State::Alarm:
        markAlarm("homing invalid state", safety, machine);
        return false;
    }
  }
}

bool HomingController::moveUntilCondition(
    Axis axis, int8_t direction, float distance_limit_mm, float feed_mm_min,
    MoveStopCondition stop_condition, StepperBackendFastAccel& backend,
    SafetyManager& safety, MachineState& machine,
    bool& other_limit_allowed_active, bool& stop_condition_met) {
  stop_condition_met = false;
  const float dx_mm = axis == Axis::X
                          ? static_cast<float>(direction) * distance_limit_mm
                          : 0.0f;
  const float dy_mm = axis == Axis::Y
                          ? static_cast<float>(direction) * distance_limit_mm
                          : 0.0f;
  const CoreXYDelta delta =
      CoreXYKinematics::xyDeltaToABSteps(dx_mm, dy_mm, STEPS_PER_MM);

#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: HOMING move axis=%s dx=%.3f dy=%.3f A=%ld B=%ld F=%.3f",
             axisName(axis), dx_mm, dy_mm, delta.a_steps, delta.b_steps,
             feed_mm_min);
  applyABStepDeltaToMachine(delta.a_steps, delta.b_steps, machine);
#else
  const int32_t start_a_steps = backend.currentASteps();
  const int32_t start_b_steps = backend.currentBSteps();
  if (!backend.moveABSteps(delta.a_steps, delta.b_steps, feed_mm_min)) {
    markAlarm("homing backend rejected move", safety, machine);
    return false;
  }
  while (backend.isRunning()) {
    safety.poll();
    if (other_limit_allowed_active && !otherLimitAnyActive(axis, safety)) {
      other_limit_allowed_active = false;
    }
    if (!other_limit_allowed_active && otherLimitAnyActive(axis, safety)) {
      backend.stop();
      backend.waitUntilIdle();
      markAlarm("homing other limit active", safety, machine);
      return false;
    }
    switch (stop_condition) {
      case MoveStopCondition::None:
        break;
      case MoveStopCondition::TargetAnyActive:
        stop_condition_met = targetLimitAnyActive(axis, safety);
        break;
      case MoveStopCondition::TargetDebouncedActive:
        stop_condition_met = targetLimitActive(axis, safety);
        break;
      case MoveStopCondition::TargetAnyReleased:
        stop_condition_met = !targetLimitAnyActive(axis, safety);
        break;
    }
    if (stop_condition_met) {
      backend.stop();
      backend.waitUntilIdle();
      break;
    }
    if (isMotionAbortRequested()) {
      backend.stop();
      backend.waitUntilIdle();
      clearMotionAbort();
      markAlarm("abort requested", safety, machine);
      return false;
    }
    if (otherLimitUnexpected(axis, safety, machine, other_limit_allowed_active)) {
      backend.stop();
      backend.waitUntilIdle();
      markAlarm("homing target-other limit active", safety, machine);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(kMovePollMs));
  }

  safety.poll();
  switch (stop_condition) {
    case MoveStopCondition::None:
      break;
    case MoveStopCondition::TargetAnyActive:
      stop_condition_met = stop_condition_met || targetLimitAnyActive(axis, safety);
      break;
    case MoveStopCondition::TargetDebouncedActive:
      stop_condition_met = stop_condition_met || targetLimitActive(axis, safety);
      break;
    case MoveStopCondition::TargetAnyReleased:
      stop_condition_met = stop_condition_met || !targetLimitAnyActive(axis, safety);
      break;
  }

  const int32_t moved_a_steps = backend.currentASteps() - start_a_steps;
  const int32_t moved_b_steps = backend.currentBSteps() - start_b_steps;
  applyABStepDeltaToMachine(moved_a_steps, moved_b_steps, machine);
#endif

  return true;
}

bool HomingController::targetLimitActive(Axis axis,
                                         const SafetyManager& safety) const {
  return axis == Axis::X ? safety.xLimitActive() : safety.yLimitActive();
}

bool HomingController::targetLimitRawActive(
    Axis axis, const SafetyManager& safety) const {
  return axis == Axis::X ? safety.xLimitRawActive() : safety.yLimitRawActive();
}

bool HomingController::targetLimitAnyActive(
    Axis axis, const SafetyManager& safety) const {
  return targetLimitRawActive(axis, safety) || targetLimitActive(axis, safety);
}

bool HomingController::otherLimitActive(Axis axis,
                                        const SafetyManager& safety) const {
  return axis == Axis::X ? safety.yLimitActive() : safety.xLimitActive();
}

bool HomingController::otherLimitRawActive(
    Axis axis, const SafetyManager& safety) const {
  return axis == Axis::X ? safety.yLimitRawActive() : safety.xLimitRawActive();
}

bool HomingController::otherLimitAnyActive(
    Axis axis, const SafetyManager& safety) const {
  return otherLimitRawActive(axis, safety) || otherLimitActive(axis, safety);
}

bool HomingController::otherLimitUnexpected(
    Axis axis, const SafetyManager& safety, const MachineState& machine,
    bool other_limit_allowed_active) const {
  if (other_limit_allowed_active) {
    return false;
  }
  if (axis == Axis::X) {
    return (safety.yLimitRawActive() || safety.yLimitActive()) &&
           !machine.y_homed;
  }
  return (safety.xLimitRawActive() || safety.xLimitActive()) &&
         !machine.x_homed;
}

void HomingController::setState(State state, MachineState& machine,
                                const char* reason) {
  state_ = state;
  setLastReason(reason);
  strncpy(machine.homing_state, stateName(), sizeof(machine.homing_state) - 1);
  machine.homing_state[sizeof(machine.homing_state) - 1] = '\0';
  logMessage("Homing state: %s reason=%s", stateName(), reason);
  publishStatus();
}

void HomingController::setCompletePosition(Axis axis, MachineState& machine) {
  if (axis == Axis::X) {
    machine.x_mm = HOMING_SET_X_MM;
    machine.x_homed = true;
  } else {
    machine.y_mm = HOMING_SET_Y_MM;
    machine.y_homed = true;
  }
  machine.homed = machine.x_homed && machine.y_homed;
  machine.a_steps = absoluteASteps(machine.x_mm, machine.y_mm);
  machine.b_steps = absoluteBSteps(machine.x_mm, machine.y_mm);
}

void HomingController::markAlarm(const char* reason, SafetyManager& safety,
                                 MachineState& machine) {
  safety.setAlarm(reason);
  machine.alarmed = true;
  setState(State::Alarm, machine, reason);
  logMessage("HOMING error state=%s reason=%s X=%.3f Y=%.3f A=%ld B=%ld limitXRaw=%s limitXDebounced=%s limitYRaw=%s limitYDebounced=%s",
             stateName(), reason, machine.x_mm, machine.y_mm, machine.a_steps,
             machine.b_steps, safety.xLimitRawActive() ? "ON" : "OFF",
             safety.xLimitActive() ? "ON" : "OFF",
             safety.yLimitRawActive() ? "ON" : "OFF",
             safety.yLimitActive() ? "ON" : "OFF");
}

void HomingController::setLastReason(const char* reason) {
  strncpy(last_reason_, reason, sizeof(last_reason_) - 1);
  last_reason_[sizeof(last_reason_) - 1] = '\0';
}
