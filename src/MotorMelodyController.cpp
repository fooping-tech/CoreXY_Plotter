#include "MotorMelodyController.h"
#include <Arduino.h>
#include "AppContext.h"
#include "PlotterConfig.h"

namespace {
struct MotorMelodyNote {
  uint16_t frequency_hz;
  uint16_t duration_ms;
};

struct MotorChordNote {
  uint16_t a_frequency_hz;
  uint16_t b_frequency_hz;
  uint16_t duration_ms;
};

constexpr MotorMelodyNote NOTES[] = {
    {523, 90},
    {659, 90},
    {784, 120},
    {1047, 180},
};

constexpr MotorChordNote JOB_END_NOTES[] = {
    {523, 659, 80},
    {659, 784, 80},
    {784, 988, 100},
    {1047, 1319, 150},
    {784, 1047, 90},
    {1047, 1568, 190},
};

uint32_t evenPulseCount(uint16_t frequency_hz, uint16_t duration_ms) {
  uint32_t count =
      (static_cast<uint32_t>(frequency_hz) * duration_ms) / 1000U;
  if ((count & 1U) != 0U) ++count;
  return count;
}
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

bool MotorMelodyController::playJobEndJingle(StepperBackendFastAccel& backend,
                                             TMC2209Manager& tmc,
                                             SafetyManager& safety) {
#if SIMULATION_MODE
  (void)backend;
  (void)tmc;
  (void)safety;
  logMessage("SIMULATION_MODE: JOB_END_JINGLE no motor output");
  return true;
#else
  if (!JOB_END_JINGLE_ENABLED) {
    logMessage("JOB_END_JINGLE skipped: disabled by config");
    return true;
  }
  if (!MOTOR_MELODY_ENABLED) {
    logMessage("ERROR: JOB_END_JINGLE melody disabled by config");
    return false;
  }
  if (!tmc.isReady()) {
    logMessage("ERROR: JOB_END_JINGLE TMC UART is not ready");
    return false;
  }
  if (backend.isRunning()) {
    logMessage("ERROR: JOB_END_JINGLE motion is running");
    return false;
  }
  if (shouldAbort(safety)) {
    logMessage("ERROR: JOB_END_JINGLE alarm or limit active");
    return false;
  }
  if (!tmc.applyMelodyProfile()) {
    tmc.applyNormalProfile();
    logMessage("ERROR: JOB_END_JINGLE TMC profile validation failed");
    return false;
  }

  bool completed = true;
  for (const auto& note : JOB_END_NOTES) {
    logMessage("JOB_END_JINGLE chord A=%uHz B=%uHz duration=%ums direction=alternating",
               note.a_frequency_hz, note.b_frequency_hz, note.duration_ms);
    if (!backend.beginDiagnosticChord()) {
      logMessage("JOB_END_JINGLE aborted: backend rejected chord");
      completed = false;
      break;
    }
    const uint32_t a_pulses =
        evenPulseCount(note.a_frequency_hz, note.duration_ms);
    const uint32_t b_pulses =
        evenPulseCount(note.b_frequency_hz, note.duration_ms);
    uint32_t queued_a = 0;
    uint32_t queued_b = 0;
    while (completed && (queued_a < a_pulses || queued_b < b_pulses)) {
      bool retry = false;
      if (queued_a < a_pulses) {
        const auto result =
            backend.queueDiagnosticPulseA(note.a_frequency_hz);
        if (result == StepperBackend::DiagnosticPulseResult::QUEUED) {
          ++queued_a;
        } else if (result == StepperBackend::DiagnosticPulseResult::RETRY) {
          retry = true;
        } else if (result == StepperBackend::DiagnosticPulseResult::ERROR) {
          logMessage("JOB_END_JINGLE aborted: backend rejected A note");
          completed = false;
          break;
        }
      }
      if (queued_b < b_pulses) {
        const auto result =
            backend.queueDiagnosticPulseB(note.b_frequency_hz);
        if (result == StepperBackend::DiagnosticPulseResult::QUEUED) {
          ++queued_b;
        } else if (result == StepperBackend::DiagnosticPulseResult::RETRY) {
          retry = true;
        } else if (result == StepperBackend::DiagnosticPulseResult::ERROR) {
          logMessage("JOB_END_JINGLE aborted: backend rejected B note");
          completed = false;
          break;
        }
      }
      if (shouldAbort(safety)) {
        backend.stop();
        logMessage("JOB_END_JINGLE aborted: alarm or limit");
        completed = false;
        break;
      }
      if (retry) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    while (completed && backend.isRunning()) {
      if (shouldAbort(safety)) {
        backend.stop();
        logMessage("JOB_END_JINGLE aborted: alarm or limit");
        completed = false;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    backend.endDiagnosticChord();
    if (!completed) break;
    vTaskDelay(pdMS_TO_TICKS(MOTOR_MELODY_NOTE_GAP_MS));
  }
  tmc.applyNormalProfile();
  logMessage("JOB_END_JINGLE %s; normal TMC profile restored",
             completed ? "complete" : "stopped");
  return completed;
#endif
}
