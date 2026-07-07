#include "MotionDiagnostics.h"

#include <Arduino.h>
#include <stdio.h>

#include "AppContext.h"
#include "PlotterConfig.h"
#include "TimedSegmentExecutor.h"

namespace {
const char* timedSegmentResultName(StepperBackend::TimedSegmentResult result) {
  switch (result) {
    case StepperBackend::TimedSegmentResult::QUEUED:
      return "QUEUED";
    case StepperBackend::TimedSegmentResult::RETRY:
      return "RETRY";
    case StepperBackend::TimedSegmentResult::ERROR:
      return "ERROR";
  }
  return "UNKNOWN";
}

void logAbTimedState(const char* detail) {
  logMessage("AB_TIMED %s queueEntries A=%u B=%u running A=%u B=%u",
             detail, stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
}
}  // namespace

void runAbTimedDiagnostic(const CommandMessage& command,
                          TimedSegmentExecutor& executor,
                          const MotionDiagnosticHooks& hooks) {
  logMessage("AB_TIMED command a_steps=%ld b_steps=%ld duration_us=%lu",
             command.a_steps, command.b_steps, command.duration_us);
  if (hooks.stop_for_abort("AB_TIMED rejected before queue")) {
    logMessage("NACK_AB_TIMED reason=abort");
    return;
  }
  if (safety_manager.isAlarmed()) {
    logMessage("NACK_AB_TIMED reason=alarm");
    return;
  }
  if (!stepper_backend.isReady()) {
    logMessage("NACK_AB_TIMED reason=backend_not_ready");
    return;
  }
  if (command.a_steps == 0 && command.b_steps == 0) {
    logMessage("NACK_AB_TIMED reason=no_steps");
    return;
  }
  if (command.duration_us < AB_TIMED_MIN_DURATION_US) {
    logMessage("NACK_AB_TIMED reason=duration_too_short min_us=%lu",
               AB_TIMED_MIN_DURATION_US);
    return;
  }
  if (command.a_steps < INT16_MIN || command.a_steps > INT16_MAX ||
      command.b_steps < INT16_MIN || command.b_steps > INT16_MAX) {
    logMessage("NACK_AB_TIMED reason=steps_out_of_range int16_required=YES");
    return;
  }

  MotionSegment segment{};
  segment.a_steps = command.a_steps;
  segment.b_steps = command.b_steps;
  segment.duration_us = command.duration_us;
  const uint32_t micros_before_queue = micros();
  const MotionSyncReference reference = executor.captureReference();
  char detail[48] = {};
  snprintf(detail, sizeof(detail), "before_queue micros=%lu",
           static_cast<unsigned long>(micros_before_queue));
  logAbTimedState(detail);
  const StepperBackend::TimedSegmentResult queue_result =
      stepper_backend.queueTimedSegment(segment, false);
  const uint32_t micros_after_queue = micros();
  snprintf(detail, sizeof(detail), "after_queue micros=%lu result=%s",
           static_cast<unsigned long>(micros_after_queue),
           timedSegmentResultName(queue_result));
  logAbTimedState(detail);
  if (queue_result != StepperBackend::TimedSegmentResult::QUEUED) {
    if (queue_result == StepperBackend::TimedSegmentResult::ERROR) {
      executor.handleQueueError(reference, "AB_TIMED queue");
    }
    logMessage("NACK_AB_TIMED reason=queue_%s",
               timedSegmentResultName(queue_result));
    return;
  }
  const bool started = stepper_backend.startTimedSegments();
  snprintf(detail, sizeof(detail), "start result=%s",
           started ? "OK" : "ERROR");
  logAbTimedState(detail);
  if (!started) {
    executor.handleQueueError(reference, "AB_TIMED start");
    logMessage("NACK_AB_TIMED reason=start_error");
    return;
  }
  hooks.set_motion_active(true);
  if (!executor.waitForMotionOrLimit(reference)) {
    hooks.set_motion_active(false);
    logAbTimedState("result=STOPPED");
    logMessage("NACK_AB_TIMED reason=stopped");
    return;
  }
  hooks.set_motion_active(false);
  logAbTimedState("result=OK");
  logMessage("ACK_AB_TIMED a_steps=%ld b_steps=%ld duration_us=%lu",
             command.a_steps, command.b_steps, command.duration_us);
}

void runSingleMotorDiagnostic(bool motor_a, int32_t steps,
                              TimedSegmentExecutor& executor,
                              const MotionDiagnosticHooks& hooks) {
  hooks.invalidate_homed("independent motor test");
#if SIMULATION_MODE
  (void)executor;
  logMessage("SIMULATION_MODE: TEST_%c steps=%ld no motor output",
             motor_a ? 'A' : 'B', steps);
#else
  const bool accepted = motor_a ? stepper_backend.moveASteps(steps)
                                : stepper_backend.moveBSteps(steps);
  if (!accepted) {
    logMessage("ERROR: backend rejected TEST_%c", motor_a ? 'A' : 'B');
    return;
  }
  hooks.set_motion_active(true);
  if (!executor.waitForMotionOrLimit()) {
    logMessage("ERROR: TEST_%c stopped", motor_a ? 'A' : 'B');
  }
  hooks.set_motion_active(false);
#endif
}