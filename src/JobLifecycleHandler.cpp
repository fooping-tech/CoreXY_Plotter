#include "JobLifecycleHandler.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#include "AppContext.h"
#include "CommandMessage.h"
#include "HomingController.h"
#include "MotorMelodyController.h"
#include "PenController.h"
#include "PlotterConfig.h"
#include "SafetyManager.h"
#include "StepperBackendFastAccel.h"
#include "TMC2209Manager.h"
#include "XYMotionPlanner.h"

bool JobLifecycleHandler::preflightIdle(const JobPreflight& preflight) {
  return preflight.pending_empty && preflight.planner_empty &&
         preflight.segment_empty && preflight.command_queue_empty &&
         preflight.backend_idle;
}

bool JobLifecycleHandler::prepareJobBeginAutoHome() {
  if (!JOB_BEGIN_AUTO_HOME || machine_.homed) {
    return true;
  }
  job_.recoverToIdleIfSafe(safety_, machine_);
  if (job_.state() != JobState::IDLE) {
    return true;
  }

  const JobPreflight preflight = current_preflight_();
  if (!preflightIdle(preflight)) {
    return true;
  }

  safety_.poll();
  machine_.alarmed = safety_.isAlarmed();
  if (machine_.alarmed) {
    return true;
  }

  if (!machine_.tmc_ready || !tmc_.isReady()) {
    logMessage("JOB_BEGIN TMC_INIT auto");
    machine_.tmc_ready = tmc_.begin();
  }
  if (!machine_.tmc_ready) {
    job_.markFailed("tmc_not_ready");
    logMessage("JOB_BEGIN rejected reason=tmc_not_ready");
    return false;
  }

  logMessage("JOB_BEGIN AUTO_HOME start");
  postLedStatus(LedStatus::HOMING);
  if (!homing_.runHome(backend_, safety_, machine_)) {
    machine_.alarmed = safety_.isAlarmed();
    job_.markFailed("auto_home_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_BEGIN rejected reason=auto_home_failed");
    return false;
  }
  logMessage("JOB_BEGIN AUTO_HOME OK");
  postLedStatus(LedStatus::COMPLETED);
  return true;
}

bool JobLifecycleHandler::moveToJobEndPark() {
  if (!JOB_END_PARK_ENABLED) {
    logMessage("JOB_END park skipped: disabled by config");
    return true;
  }
  if (fabsf(machine_.x_mm - JOB_END_PARK_X_MM) < 0.01f &&
      fabsf(machine_.y_mm - JOB_END_PARK_Y_MM) < 0.01f) {
    logMessage("JOB_END park skipped: already at X=%.3f Y=%.3f",
               JOB_END_PARK_X_MM, JOB_END_PARK_Y_MM);
    return true;
  }
  CommandMessage park{};
  park.type = CommandType::XY;
  park.from_gcode = true;
  snprintf(park.name, sizeof(park.name), "JOB_PARK");
  park.x_mm = JOB_END_PARK_X_MM;
  park.y_mm = JOB_END_PARK_Y_MM;
  park.feed_mm_min = JOB_END_PARK_FEED_MM_MIN;
  logMessage("JOB_END park target=(%.3f,%.3f) F=%.3f",
             park.x_mm, park.y_mm, park.feed_mm_min);
  return xy_.handleBatch(park);
}

void JobLifecycleHandler::handleJobEnd() {
  if (!job_.isRunning()) {
    job_.endJob(current_preflight_(), safety_, machine_, pen_);
    return;
  }

  if (!preflightIdle(current_preflight_())) {
    job_.markFailed("job_end_queue_not_empty");
    logMessage("JOB_END failed reason=queue_not_empty");
    return;
  }

  pen_.penUp();
  machine_.pen_down = false;
  postLedStatus(LedStatus::DRAWING_PEN_UP);
  logMessage("JOB_END pen up before park");

  if (!moveToJobEndPark()) {
    job_.markFailed("job_end_park_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_END failed reason=park_failed");
    return;
  }
  if (!melody_.playJobEndJingle(backend_, tmc_, safety_)) {
    job_.markFailed("job_end_jingle_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_END failed reason=jingle_failed");
    return;
  }
  job_.endJob(current_preflight_(), safety_, machine_, pen_);
  postLedStatus(LedStatus::COMPLETED);
}
