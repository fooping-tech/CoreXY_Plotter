#include "JobController.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "AppContext.h"
#include "MachineState.h"
#include "PenController.h"
#include "SafetyManager.h"
#include "TMC2209Manager.h"

namespace {
void copyText(char* destination, size_t size, const char* source) {
  if (size == 0) return;
  snprintf(destination, size, "%s", source != nullptr ? source : "none");
}
}

const char* JobController::stateName() const {
  switch (state_) {
    case JobState::IDLE:
      return "IDLE";
    case JobState::STARTING:
      return "STARTING";
    case JobState::RUNNING:
      return "RUNNING";
    case JobState::ENDING:
      return "ENDING";
    case JobState::COMPLETE:
      return "COMPLETE";
    case JobState::ABORTED:
      return "ABORTED";
    case JobState::FAILED:
      return "FAILED";
  }
  return "UNKNOWN";
}

void JobController::setState(JobState state) {
  state_ = state;
}

bool JobController::reject(const char* reason, const char* prefix) {
  copyText(last_error_, sizeof(last_error_), reason);
  copyText(result_, sizeof(result_), "rejected");
  logMessage("%s rejected reason=%s", prefix, reason);
  return false;
}

bool JobController::checkPreflightEmpty(const JobPreflight& preflight,
                                        const char* prefix) {
  if (!preflight.pending_empty) return reject("pending_command_not_empty", prefix);
  if (!preflight.planner_empty) return reject("planner_queue_not_empty", prefix);
  if (!preflight.segment_empty) return reject("segment_queue_not_empty", prefix);
  if (!preflight.command_queue_empty) {
    return reject("command_queue_not_empty", prefix);
  }
  if (!preflight.backend_idle) return reject("backend_running", prefix);
  return true;
}

bool JobController::beginJob(const JobPreflight& preflight, SafetyManager& safety,
                             MachineState& machine, PenController& pen,
                             TMC2209Manager& tmc) {
  homed_at_begin_ = false;
  if (state_ == JobState::COMPLETE) {
    setState(JobState::IDLE);
  }
  if (state_ == JobState::FAILED || state_ == JobState::ABORTED) {
    safety.poll();
    machine.alarmed = safety.isAlarmed();
    if (!machine.alarmed && machine.homed) {
      logMessage("JOB recovery state=%s -> IDLE", stateName());
      setState(JobState::IDLE);
    }
  }
  if (state_ != JobState::IDLE) {
    return reject("job_not_idle", "JOB_BEGIN");
  }

  setState(JobState::STARTING);
  copyText(result_, sizeof(result_), "starting");
  copyText(last_error_, sizeof(last_error_), "none");
  if (!checkPreflightEmpty(preflight, "JOB_BEGIN")) {
    setState(JobState::FAILED);
    return false;
  }

  safety.poll();
  machine.alarmed = safety.isAlarmed();
  if (machine.alarmed) {
    setState(JobState::FAILED);
    return reject(safety.alarmReason(), "JOB_BEGIN");
  }
  if (!machine.tmc_ready || !tmc.isReady()) {
    logMessage("JOB_BEGIN TMC_INIT auto");
    machine.tmc_ready = tmc.begin();
  }
  if (!machine.tmc_ready) {
    setState(JobState::FAILED);
    return reject("tmc_not_ready", "JOB_BEGIN");
  }
  if (!machine.homed) {
    setState(JobState::FAILED);
    return reject("not_homed", "JOB_BEGIN");
  }

  homed_at_begin_ = true;
  pen.penUp();
  machine.pen_down = false;
  ++job_sequence_;
  copyText(result_, sizeof(result_), "running");
  copyText(last_error_, sizeof(last_error_), "none");
  setState(JobState::RUNNING);
  logMessage("JOB_BEGIN OK seq=%lu homed=YES tmc=READY pen=UP limitX=%s limitY=%s",
             static_cast<unsigned long>(job_sequence_),
             safety.xLimitActive() ? "ACTIVE" : "OPEN",
             safety.yLimitActive() ? "ACTIVE" : "OPEN");
  return true;
}

bool JobController::endJob(const JobPreflight& preflight, SafetyManager& safety,
                           MachineState& machine, PenController& pen) {
  if (state_ != JobState::RUNNING) {
    return reject("no_active_job", "JOB_END");
  }
  setState(JobState::ENDING);
  copyText(result_, sizeof(result_), "ending");

  if (!checkPreflightEmpty(preflight, "JOB_END")) {
    homed_at_begin_ = false;
    setState(JobState::FAILED);
    return false;
  }

  safety.poll();
  machine.alarmed = safety.isAlarmed();
  if (!machine.alarmed) {
    pen.penUp();
    machine.pen_down = false;
  }
  if (machine.alarmed) {
    homed_at_begin_ = false;
    setState(JobState::FAILED);
    copyText(result_, sizeof(result_), "failed");
    copyText(last_error_, sizeof(last_error_), safety.alarmReason());
    logMessage("JOB_END failed reason=%s", safety.alarmReason());
    return false;
  }

  copyText(result_, sizeof(result_), "complete");
  copyText(last_error_, sizeof(last_error_), "none");
  homed_at_begin_ = false;
  setState(JobState::COMPLETE);
  logMessage("JOB_END OK seq=%lu X=%.3f Y=%.3f HOMED=%s PEN=UP ALARM=NO TMC=%s LIMIT_X=%s LIMIT_Y=%s",
             static_cast<unsigned long>(job_sequence_), machine.x_mm,
             machine.y_mm, machine.homed ? "YES" : "NO",
             machine.tmc_ready ? "READY" : "NO",
             safety.xLimitActive() ? "ACTIVE" : "OPEN",
             safety.yLimitActive() ? "ACTIVE" : "OPEN");
  setState(JobState::IDLE);
  return true;
}

bool JobController::abortJob(const char* reason) {
  if (!isActive()) {
    return reject("no_active_job", "JOB_ABORT");
  }
  markAborted(reason != nullptr ? reason : "job abort requested");
  logMessage("JOB_ABORT requested reason=%s", last_error_);
  return true;
}

void JobController::markAborted(const char* reason) {
  copyText(result_, sizeof(result_), "aborted");
  copyText(last_error_, sizeof(last_error_),
           reason != nullptr ? reason : "abort requested");
  homed_at_begin_ = false;
  setState(JobState::ABORTED);
}

void JobController::markFailed(const char* reason) {
  if (state_ == JobState::ABORTED) return;
  copyText(result_, sizeof(result_), "failed");
  copyText(last_error_, sizeof(last_error_),
           reason != nullptr ? reason : "failed");
  homed_at_begin_ = false;
  setState(JobState::FAILED);
}

void JobController::resetToIdleIfComplete() {
  if (state_ == JobState::COMPLETE) {
    setState(JobState::IDLE);
  }
}

bool JobController::allowCommand(CommandType type, bool from_gcode) const {
  if (state_ != JobState::RUNNING) {
    if (type == CommandType::JOB_ABORT) {
      return false;
    }
    return true;
  }

  switch (type) {
    case CommandType::GCODE:
    case CommandType::JOB_END:
    case CommandType::JOB_ABORT:
    case CommandType::ABORT:
    case CommandType::JOB_STATUS:
    case CommandType::POS:
      return true;
    case CommandType::XY:
    case CommandType::DWELL:
    case CommandType::PEN_UP:
    case CommandType::PEN_DOWN:
      return from_gcode;
    default:
      return false;
  }
}

void JobController::printStatus() const {
  logMessage("JOB_STATUS state=%s result=%s last_error=%s seq=%lu",
             stateName(), result_, last_error_,
             static_cast<unsigned long>(job_sequence_));
}
