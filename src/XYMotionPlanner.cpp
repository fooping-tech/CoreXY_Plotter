#include "XYMotionPlanner.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "JobController.h"
#include "PlotterConfig.h"
#include "SafetyManager.h"
#include "TimedSegmentExecutor.h"

namespace {
void ackXY(float target_x_mm, float target_y_mm, int32_t a_steps,
           int32_t b_steps, float feed_mm_min) {
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f", target_x_mm,
             target_y_mm, static_cast<long>(a_steps),
             static_cast<long>(b_steps), feed_mm_min);
}

void nackXY(float target_x_mm, float target_y_mm, const char* reason) {
  logMessage("NACK_XY target=(%.3f,%.3f) reason=%s", target_x_mm, target_y_mm,
             reason);
}
}  // namespace

void XYMotionPlanner::clearQueues(const char* reason, bool clear_pending) {
  planner_queue_.clear();
  segment_queue_.clear();
  if (clear_pending) {
    hooks_.clear_pending_command();
  }
  if (reason != nullptr) {
    logMessage("MOTION_QUEUES cleared reason=%s", reason);
  }
}

bool XYMotionPlanner::buildBlock(const CommandMessage& command,
                                 float start_x_mm, float start_y_mm,
                                 int32_t start_a_steps, int32_t start_b_steps,
                                 MotionBlock& block) {
  if (hooks_.stop_for_abort("XY rejected before planning")) {
    nackXY(command.x_mm, command.y_mm, "abort");
    return false;
  }
  safety_.poll();
  float feed_mm_min = command.feed_mm_min;
  if (!safety_.validateMove(command.x_mm, command.y_mm, feed_mm_min)) {
    nackXY(command.x_mm, command.y_mm, "rejected");
    return false;
  }
  const float dx_mm = command.x_mm - start_x_mm;
  const float dy_mm = command.y_mm - start_y_mm;
  const CoreXYPositionSteps target_steps =
      CoreXYKinematics::xyPositionToABSteps(command.x_mm, command.y_mm,
                                            STEPS_PER_MM);
  block = MotionBlock{};
  block.start_x_mm = start_x_mm;
  block.start_y_mm = start_y_mm;
  block.target_x_mm = command.x_mm;
  block.target_y_mm = command.y_mm;
  block.dx_mm = dx_mm;
  block.dy_mm = dy_mm;
  block.length_mm = sqrtf(dx_mm * dx_mm + dy_mm * dy_mm);
  block.nominal_speed_mm_min = feed_mm_min;
  block.entry_speed_mm_min = 0.0f;
  block.exit_speed_mm_min = 0.0f;
  block.a_steps = target_steps.a_steps - start_a_steps;
  block.b_steps = target_steps.b_steps - start_b_steps;
  block.target_a_steps = target_steps.a_steps;
  block.target_b_steps = target_steps.b_steps;
  block.pen_down = machine_.pen_down;
  return true;
}

bool XYMotionPlanner::isNoOpBlock(const MotionBlock& block) {
  return block.a_steps == 0 && block.b_steps == 0;
}

void XYMotionPlanner::acknowledgeNoOp(const MotionBlock& block,
                                      bool update_machine_state) {
  if (update_machine_state) {
    machine_.x_mm = block.target_x_mm;
    machine_.y_mm = block.target_y_mm;
    machine_.a_steps = block.target_a_steps;
    machine_.b_steps = block.target_b_steps;
    machine_.feed_mm_min = block.nominal_speed_mm_min;
  }
  logMessage("XY no-op current=(%.3f,%.3f) target=(%.3f,%.3f) F=%.3f",
             block.start_x_mm, block.start_y_mm, block.target_x_mm,
             block.target_y_mm, block.nominal_speed_mm_min);
  ackXY(block.target_x_mm, block.target_y_mm, block.a_steps, block.b_steps,
        block.nominal_speed_mm_min);
}

bool XYMotionPlanner::planQueuedBlocks() {
  if (!junction_planner_.plan(planner_queue_)) {
    logMessage("ERROR: junction planner rejected XY batch");
    return false;
  }
  for (size_t index = 0; index < planner_queue_.count(); ++index) {
    MotionBlock* block = planner_queue_.at(index);
    if (block == nullptr || !trapezoid_planner_.plan(*block)) {
      logMessage("ERROR: trapezoid planner rejected XY batch index=%u",
                 static_cast<unsigned>(index));
      return false;
    }
  }
  return true;
}

bool XYMotionPlanner::executePlannedBlock(MotionBlock& block, size_t index,
                                          size_t count) {
  logMessage("XY batch=%u/%u current=(%.3f,%.3f) target=(%.3f,%.3f) dx=%.3f dy=%.3f A=%ld B=%ld F=%.3f entry=%.3f exit=%.3f",
             static_cast<unsigned>(index + 1), static_cast<unsigned>(count),
             block.start_x_mm, block.start_y_mm, block.target_x_mm,
             block.target_y_mm, block.dx_mm, block.dy_mm, block.a_steps,
             block.b_steps,
             block.nominal_speed_mm_min, block.entry_speed_mm_min,
             block.exit_speed_mm_min);
  logMessage("TRAPEZOID profile=%s length=%.3f accel=%.3f peak=%.3f accel_d=%.3f cruise_d=%.3f decel_d=%.3f t=%.3f",
             block.triangular_profile ? "TRIANGULAR" : "TRAPEZOID",
             block.length_mm, block.acceleration_mm_s2, block.peak_speed_mm_s,
             block.acceleration_distance_mm, block.cruise_distance_mm,
             block.deceleration_distance_mm,
             block.acceleration_time_s + block.cruise_time_s +
                 block.deceleration_time_s);
  if (!segment_generator_.generate(block, segment_queue_)) {
    logMessage("ERROR: segment generator rejected XY move");
    nackXY(block.target_x_mm, block.target_y_mm, "segment");
    return false;
  }
  logMessage("SEGMENTS count=%u duration=%.3f dda=YES",
             static_cast<unsigned>(segment_queue_.count()),
             block.acceleration_time_s + block.cruise_time_s +
                 block.deceleration_time_s);
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: no motor output");
  ackXY(block.target_x_mm, block.target_y_mm, block.a_steps, block.b_steps,
        block.nominal_speed_mm_min);
#else
  safety_.poll();
  const bool allow_x_limit_release =
      safety_.xLimitActive() &&
      block.dx_mm * static_cast<float>(HOMING_X_DIR) < 0.0f;
  const bool allow_y_limit_release =
      safety_.yLimitActive() &&
      block.dy_mm * static_cast<float>(HOMING_Y_DIR) < 0.0f;
  if (allow_x_limit_release || allow_y_limit_release) {
    logMessage("LIMIT_RELEASE_ALLOW X=%s Y=%s max=%.3f",
               allow_x_limit_release ? "YES" : "NO",
               allow_y_limit_release ? "YES" : "NO",
               NORMAL_MOVE_LIMIT_RELEASE_MM);
  }
  safety_.beginNormalMoveLimitReleaseAllowance(
      allow_x_limit_release, allow_y_limit_release, block.start_x_mm,
      block.start_y_mm);
  const bool executed = executor_.executeQueue(segment_queue_);
  safety_.clearNormalMoveLimitReleaseAllowance();
  if (!executed) {
    const char* alarm_reason = safety_.alarmReason();
    if (safety_.isAlarmed()) {
      const bool abort_alarm =
          strstr(alarm_reason, "abort requested") != nullptr;
      logMessage("ERROR: timed XY move stopped: alarm reason=%s",
                 alarm_reason);
      nackXY(block.target_x_mm, block.target_y_mm,
             abort_alarm ? "abort" : "alarm");
    } else {
      logMessage("ERROR: backend rejected timed XY move");
      nackXY(block.target_x_mm, block.target_y_mm, "backend");
    }
    return false;
  }
  ackXY(block.target_x_mm, block.target_y_mm, block.a_steps, block.b_steps,
        block.nominal_speed_mm_min);
#endif
  machine_.x_mm = block.target_x_mm;
  machine_.y_mm = block.target_y_mm;
  machine_.a_steps = block.target_a_steps;
  machine_.b_steps = block.target_b_steps;
  machine_.feed_mm_min = block.nominal_speed_mm_min;
  return true;
}

bool XYMotionPlanner::handleBatch(const CommandMessage& first_command) {
  planner_queue_.clear();
  float planned_x_mm = machine_.x_mm;
  float planned_y_mm = machine_.y_mm;
  int32_t planned_a_steps = machine_.a_steps;
  int32_t planned_b_steps = machine_.b_steps;
  float planned_feed_mm_min = machine_.feed_mm_min;

  MotionBlock block{};
  if (!buildBlock(first_command, planned_x_mm, planned_y_mm, planned_a_steps,
                  planned_b_steps, block)) {
    clearQueues("XY build first failed");
    return false;
  }
  if (isNoOpBlock(block)) {
    acknowledgeNoOp(block, true);
  } else {
    if (!planner_queue_.enqueue(block)) {
      nackXY(first_command.x_mm, first_command.y_mm, "planner_queue_full");
      return false;
    }
  }
  planned_x_mm = first_command.x_mm;
  planned_y_mm = first_command.y_mm;
  planned_a_steps = block.target_a_steps;
  planned_b_steps = block.target_b_steps;
  planned_feed_mm_min = first_command.feed_mm_min;

  CommandMessage next_command;
  while (!planner_queue_.isFull() &&
         hooks_.receive_next_command(next_command,
                                     LOOKAHEAD_BATCH_COLLECT_MS)) {
    if (hooks_.reject_disallowed(next_command)) {
      continue;
    }
    if (next_command.type == CommandType::GCODE) {
      MachineState planned_state = machine_;
      planned_state.x_mm = planned_x_mm;
      planned_state.y_mm = planned_y_mm;
      planned_state.a_steps = planned_a_steps;
      planned_state.b_steps = planned_b_steps;
      planned_state.feed_mm_min = planned_feed_mm_min;
      CommandMessage translated{};
      const GcodeInterpreterResult result =
          hooks_.translate_gcode(next_command, planned_state, translated);
      if (result == GcodeInterpreterResult::ERROR) {
        clearQueues("GCODE translate failed");
        return false;
      }
      if (result == GcodeInterpreterResult::MODAL_UPDATE) {
        continue;
      }
      if (translated.type != CommandType::XY) {
        hooks_.stash_pending_command(translated);
        break;
      }
      next_command = translated;
    }
    if (hooks_.reject_disallowed(next_command)) {
      continue;
    }
    if (next_command.type != CommandType::XY) {
      hooks_.stash_pending_command(next_command);
      break;
    }
    MotionBlock next_block{};
    if (!buildBlock(next_command, planned_x_mm, planned_y_mm, planned_a_steps,
                    planned_b_steps, next_block)) {
      clearQueues("XY build batch failed");
      return false;
    }
    if (isNoOpBlock(next_block)) {
      acknowledgeNoOp(next_block, planner_queue_.isEmpty());
    } else {
      if (!planner_queue_.enqueue(next_block)) {
        nackXY(next_command.x_mm, next_command.y_mm, "planner_queue_full");
        return false;
      }
    }
    planned_x_mm = next_command.x_mm;
    planned_y_mm = next_command.y_mm;
    planned_a_steps = next_block.target_a_steps;
    planned_b_steps = next_block.target_b_steps;
    planned_feed_mm_min = next_command.feed_mm_min;
  }

  if (planner_queue_.isEmpty()) {
    return true;
  }

  if (!planQueuedBlocks()) {
    const MotionBlock* failed = planner_queue_.peekNext();
    nackXY(failed != nullptr ? failed->target_x_mm : first_command.x_mm,
           failed != nullptr ? failed->target_y_mm : first_command.y_mm,
           "planner");
    clearQueues("XY planning failed");
    return false;
  }

  logMessage("LOOKAHEAD blocks=%u junction_deviation=%.3f classic_jerk=%.3f",
             static_cast<unsigned>(planner_queue_.count()),
             JUNCTION_DEVIATION_MM, CLASSIC_JERK_LIMIT_MM_S);

  postLedStatus(machine_.pen_down ? LedStatus::DRAWING_PEN_DOWN
                                  : LedStatus::DRAWING_PEN_UP);
  const size_t planned_count = planner_queue_.count();
  for (size_t index = 0; index < planned_count; ++index) {
    MotionBlock* planned_block = planner_queue_.at(index);
    if (planned_block == nullptr ||
        !executePlannedBlock(*planned_block, index, planned_count)) {
      postLedStatus(safety_.isAlarmed() ? LedStatus::ERROR
                                        : LedStatus::WARNING);
      clearQueues("XY execution failed");
      return false;
    }
  }
  machine_.x_mm = planned_x_mm;
  machine_.y_mm = planned_y_mm;
  machine_.a_steps = planned_a_steps;
  machine_.b_steps = planned_b_steps;
  machine_.feed_mm_min = planned_feed_mm_min;
  hooks_.warn_if_drift_detected();
  clearQueues("XY complete", false);
  if (!job_.isRunning()) {
    postLedStatus(LedStatus::IDLE);
  }
  return true;
}
