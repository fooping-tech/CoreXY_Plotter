#pragma once

#include <stdint.h>
#include "CommandTypes.h"

struct MachineState;
class PenController;
class SafetyManager;
class TMC2209Manager;

enum class JobState {
  IDLE,
  STARTING,
  RUNNING,
  ENDING,
  COMPLETE,
  ABORTED,
  FAILED,
};

struct JobPreflight {
  bool pending_empty = true;
  bool planner_empty = true;
  bool segment_empty = true;
  bool command_queue_empty = true;
  bool backend_idle = true;
};

class JobController {
 public:
  JobState state() const { return state_; }
  const char* stateName() const;
  const char* result() const { return result_; }
  const char* lastError() const { return last_error_; }
  bool isRunning() const { return state_ == JobState::RUNNING; }
  bool isActive() const {
    return state_ == JobState::STARTING || state_ == JobState::RUNNING ||
           state_ == JobState::ENDING;
  }
  bool hasHomedJobMotionGrant() const {
    return homed_at_begin_ &&
           (state_ == JobState::RUNNING || state_ == JobState::ENDING);
  }

  bool beginJob(const JobPreflight& preflight, SafetyManager& safety,
                MachineState& machine, PenController& pen,
                TMC2209Manager& tmc);
  bool endJob(const JobPreflight& preflight, SafetyManager& safety,
              MachineState& machine, PenController& pen);
  bool abortJob(const char* reason);
  void markAborted(const char* reason);
  void markFailed(const char* reason);
  void resetToIdleIfComplete();
  bool allowCommand(CommandType type, bool from_gcode) const;
  void printStatus() const;

 private:
  void setState(JobState state);
  bool reject(const char* reason, const char* prefix);
  bool checkPreflightEmpty(const JobPreflight& preflight, const char* prefix);

  JobState state_ = JobState::IDLE;
  char result_[16] = "none";
  char last_error_[64] = "none";
  uint32_t job_sequence_ = 0;
  bool homed_at_begin_ = false;
};
