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
    {523, 90},
    {659, 90},
    {784, 120},
    {1047, 180},
};
}

bool MotorMelodyController::shouldAbort(SafetyManager& safety) const {
  return safety.isAlarmed() || safety.xLimitActive() || safety.yLimitActive();
}

bool MotorMelodyController::play(StepperBackendFastAccel& backend,
                                 TMC2209Manager& tmc, SafetyManager& safety) {
#if SIMULATION_MODE
  (void)backend;
  (void)tmc;
  (void)safety;
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
  if (!tmc.applyMelodyProfile()) {
    tmc.applyNormalProfile();
    logMessage("ERROR: MELODY TMC profile validation failed");
    return false;
  }
  bool completed = true;
  for (const auto& note : NOTES) {
    logMessage("MELODY note frequency=%uHz duration=%ums motor=A direction=alternating",
               note.frequency_hz, note.duration_ms);
    if (!backend.beginDiagnosticTone()) {
      logMessage("MELODY aborted: backend rejected tone");
      completed = false;
      break;
    }
    const uint32_t pulse_count =
        (static_cast<uint32_t>(note.frequency_hz) * note.duration_ms) / 1000U;
    for (uint32_t pulse = 0; pulse < pulse_count;) {
      const auto result = backend.queueDiagnosticPulse(note.frequency_hz);
      if (result == StepperBackend::DiagnosticPulseResult::QUEUED) {
        ++pulse;
      } else if (result == StepperBackend::DiagnosticPulseResult::ERROR) {
        logMessage("MELODY aborted: backend rejected note");
        completed = false;
        break;
      } else {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      if (shouldAbort(safety)) {
        backend.stop();
        logMessage("MELODY aborted: alarm or limit");
        completed = false;
        break;
      }
    }
    while (completed && backend.isRunning()) {
      if (shouldAbort(safety)) {
        backend.stop();
        logMessage("MELODY aborted: alarm or limit");
        completed = false;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    backend.endDiagnosticTone();
    if (!completed) break;
    vTaskDelay(pdMS_TO_TICKS(MOTOR_MELODY_NOTE_GAP_MS));
  }
  tmc.applyNormalProfile();
  logMessage("MELODY %s; normal TMC profile restored",
             completed ? "complete" : "stopped");
  return completed;
#endif
}
