#include "TimedSegmentExecutor.h"

#include <Arduino.h>
#include "AppContext.h"
#include "PlotterConfig.h"
#include "SafetyManager.h"
#include "StepperBackendFastAccel.h"

MotionSyncReference TimedSegmentExecutor::captureReference() const {
  return MotionSyncTracker::capture(backend_.currentASteps(),
                                    backend_.currentBSteps(), machine_);
}

void TimedSegmentExecutor::updateEstimate(const MotionSyncReference& reference) {
  MotionSyncTracker::updateEstimate(reference, backend_.currentASteps(),
                                    backend_.currentBSteps(), STEPS_PER_MM,
                                    machine_);
}

bool TimedSegmentExecutor::waitForMotionOrLimit(
    const MotionSyncReference& reference) {
  while (backend_.isRunning()) {
    if (hooks_.stop_for_abort("Motion stopped")) {
      return false;
    }
    updateEstimate(reference);
    safety_.poll();
    if (safety_.isAlarmed()) {
      backend_.stop();
      postLedStatus(LedStatus::ERROR);
      logMessage("Motion stopped: alarm reason=%s", safety_.alarmReason());
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  updateEstimate(reference);
  return true;
}

bool TimedSegmentExecutor::waitForMotionOrLimit() {
  const MotionSyncReference reference = captureReference();
  return waitForMotionOrLimit(reference);
}

void TimedSegmentExecutor::handleQueueError(
    const MotionSyncReference& reference, const char* context) {
  backend_.stop();
  updateEstimate(reference);
  hooks_.enter_alarm("timed segment queue lost position confidence",
                     "timed segment queue error", LedStatus::ERROR);
  logMessage("%s: ERROR position confidence lost; resynced from backend X=%.3f Y=%.3f A=%ld B=%ld",
             context, machine_.x_mm, machine_.y_mm, machine_.a_steps,
             machine_.b_steps);
}

bool TimedSegmentExecutor::queueSegmentWithRetry(
    const MotionSegment& segment, bool start,
    const MotionSyncReference& reference) {
  for (;;) {
    if (hooks_.stop_for_abort("Motion stopped while queueing segment")) {
      return false;
    }
    const StepperBackend::TimedSegmentResult result =
        backend_.queueTimedSegment(segment, start);
    if (result == StepperBackend::TimedSegmentResult::QUEUED) return true;
    if (result == StepperBackend::TimedSegmentResult::ERROR) {
      handleQueueError(reference, "Motion stopped while queueing segment");
      return false;
    }
    safety_.poll();
    if (safety_.isAlarmed()) {
      backend_.stop();
      postLedStatus(LedStatus::ERROR);
      logMessage("Motion stopped while queueing segment: alarm reason=%s",
                 safety_.alarmReason());
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool TimedSegmentExecutor::executeQueue(SegmentQueue& queue) {
  if (hooks_.stop_for_abort("Motion stopped before timed segment start")) {
    return false;
  }
  const MotionSyncReference reference = captureReference();
  MotionSegment segment{};
  if (!queue.dequeue(segment)) return false;
  if (!queueSegmentWithRetry(segment, false, reference)) return false;
  if (!backend_.startTimedSegments()) return false;
  hooks_.set_motion_active(true);

  while (queue.dequeue(segment)) {
    if (hooks_.stop_for_abort("Motion stopped during timed segment queueing")) {
      hooks_.set_motion_active(false);
      return false;
    }
    if (!queueSegmentWithRetry(segment, true, reference)) {
      hooks_.set_motion_active(false);
      return false;
    }
  }
  const bool completed = waitForMotionOrLimit(reference);
  hooks_.set_motion_active(false);
  return completed;
}
