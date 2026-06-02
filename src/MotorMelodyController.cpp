#include "MotorMelodyController.h"
#include <Arduino.h>
#include "AppContext.h"
#include "PlotterConfig.h"

namespace {
struct MotorMelodyNote {
  uint16_t frequency_hz;
  uint16_t duration_ms;
};

constexpr MotorMelodyNote NOTES[] = {
    {262, 120}, {330, 120}, {392, 140}, {523, 180},
    {392, 100}, {523, 220},
};
constexpr int8_t NOTE_DIRECTIONS[] = {1, -1};
}

bool MotorMelodyController::shouldAbort(SafetyManager& safety) const {
  return safety.isAlarmed() || safety.xLimitActive() || safety.yLimitActive();
}

bool MotorMelodyController::play(StepperBackendFastAccel& backend,
                                 TMC2209Manager& tmc, SafetyManager& safety,
                                 bool motors_enabled) {
#if SIMULATION_MODE
  (void)backend;
  (void)tmc;
  (void)safety;
  (void)motors_enabled;
  logMessage("ERROR: MELODY unavailable in SIMULATION_MODE");
  return false;
#else
  if (!MOTOR_MELODY_ENABLED) {
    logMessage("ERROR: MELODY disabled by config");
    return false;
  }
  if (!tmc.isReady()) {
    logMessage("ERROR: MELODY TMC UART is not ready");
    return false;
  }
  if (backend.isRunning()) {
    logMessage("ERROR: MELODY motion is running");
    return false;
  }
  if (shouldAbort(safety)) {
    logMessage("ERROR: MELODY alarm or limit active");
    return false;
  }
  if (!motors_enabled) {
    logMessage("ERROR: MELODY motors are disabled");
    return false;
  }

  if (!tmc.applyMelodyProfile()) {
    tmc.applyNormalProfile();
    logMessage("ERROR: MELODY TMC profile validation failed");
    return false;
  }
  bool completed = true;
  for (const auto& note : NOTES) {
    logMessage("MELODY note frequency=%uHz duration=%ums motor=A direction=+/-",
               note.frequency_hz, note.duration_ms);
    const int32_t half_note_steps = static_cast<int32_t>(
        (static_cast<uint32_t>(note.frequency_hz) * note.duration_ms) / 2000U);
    const int32_t steps = half_note_steps > 0 ? half_note_steps : 1;
    for (const int8_t direction : NOTE_DIRECTIONS) {
      if (!backend.setDiagnosticSpeedHz(note.frequency_hz) ||
          !backend.moveDiagnosticASteps(direction * steps)) {
        logMessage("MELODY aborted: backend rejected note");
        completed = false;
        break;
      }
      const uint32_t timeout_ms = millis() + note.duration_ms / 2U + 500U;
      while (backend.isRunning()) {
        if (shouldAbort(safety) ||
            static_cast<int32_t>(millis() - timeout_ms) >= 0) {
          backend.stop();
          logMessage("MELODY aborted: alarm, limit, or note timeout");
          completed = false;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      if (!completed) break;
    }
    if (!completed) break;
    vTaskDelay(pdMS_TO_TICKS(MOTOR_MELODY_NOTE_GAP_MS));
  }
  tmc.applyNormalProfile();
  logMessage("MELODY %s; normal TMC profile restored",
             completed ? "complete" : "stopped");
  return completed;
#endif
}
