#include <Arduino.h>
#include <string.h>
#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "Diagnostics.h"
#include "GcodeInterpreter.h"
#include "JunctionPlanner.h"
#include "PlotterConfig.h"
#include "PlannerQueue.h"
#include "SegmentGenerator.h"
#include "SegmentQueue.h"
#include "TrapezoidPlanner.h"

namespace {
TrapezoidPlanner trapezoid_planner;
JunctionPlanner junction_planner;
GcodeInterpreter gcode_interpreter;
SegmentGenerator segment_generator;
SegmentQueue segment_queue;
PlannerQueue planner_queue;
CommandMessage pending_command;
bool has_pending_command = false;

constexpr float MIN_XY_MOVE_LENGTH_MM = 0.0001f;

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

StatusMessage currentStatus() {
  return StatusMessage{machine_state, safety_manager.xLimitActive(),
                       safety_manager.yLimitActive(),
                       safety_manager.xLimitRawActive(),
                       safety_manager.yLimitRawActive()};
}

void invalidateHomed(const char* reason) {
  machine_state.homed = false;
  machine_state.x_homed = false;
  machine_state.y_homed = false;
  logMessage("HOMED invalidated: %s", reason);
}

bool stopForAbort(const char* context) {
  if (!isMotionAbortRequested()) return false;
  clearMotionAbort();
  stepper_backend.stop();
  safety_manager.setAlarm("abort requested");
  machine_state.alarmed = true;
  invalidateHomed("abort requested");
  if (job_controller.isActive() || job_controller.isRunning()) {
    job_controller.markAborted("abort requested");
  }
  logMessage("%s: abort requested", context);
  return true;
}

void updateMachinePositionEstimateFromBackend(int32_t start_backend_a_steps,
                                              int32_t start_backend_b_steps,
                                              float start_x_mm,
                                              float start_y_mm) {
  const int32_t delta_a_steps =
      stepper_backend.currentASteps() - start_backend_a_steps;
  const int32_t delta_b_steps =
      stepper_backend.currentBSteps() - start_backend_b_steps;
  const float delta_a_mm =
      static_cast<float>(delta_a_steps) / runtime_config.steps_per_mm;
  const float delta_b_mm =
      static_cast<float>(delta_b_steps) / runtime_config.steps_per_mm;
  machine_state.x_mm = start_x_mm + (delta_a_mm + delta_b_mm) * 0.5f;
  machine_state.y_mm = start_y_mm + (delta_a_mm - delta_b_mm) * 0.5f;
}

bool waitForMotionOrLimit() {
  const int32_t start_backend_a_steps = stepper_backend.currentASteps();
  const int32_t start_backend_b_steps = stepper_backend.currentBSteps();
  const float start_x_mm = machine_state.x_mm;
  const float start_y_mm = machine_state.y_mm;
  while (stepper_backend.isRunning()) {
    if (stopForAbort("Motion stopped")) {
      return false;
    }
    updateMachinePositionEstimateFromBackend(start_backend_a_steps,
                                             start_backend_b_steps, start_x_mm,
                                             start_y_mm);
    safety_manager.poll();
    if (safety_manager.isAlarmed()) {
      stepper_backend.stop();
      logMessage("Motion stopped: alarm reason=%s", safety_manager.alarmReason());
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  updateMachinePositionEstimateFromBackend(start_backend_a_steps,
                                           start_backend_b_steps, start_x_mm,
                                           start_y_mm);
  return true;
}

bool queueTimedSegmentWithRetry(const MotionSegment& segment, bool start) {
  for (;;) {
    if (stopForAbort("Motion stopped while queueing segment")) {
      return false;
    }
    const StepperBackend::TimedSegmentResult result =
        stepper_backend.queueTimedSegment(segment, start);
    if (result == StepperBackend::TimedSegmentResult::QUEUED) return true;
    if (result == StepperBackend::TimedSegmentResult::ERROR) return false;
    safety_manager.poll();
    if (safety_manager.isAlarmed()) {
      stepper_backend.stop();
      logMessage("Motion stopped while queueing segment: alarm reason=%s",
                 safety_manager.alarmReason());
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool executeTimedSegments(SegmentQueue& queue) {
  if (stopForAbort("Motion stopped before timed segment start")) {
    return false;
  }
  MotionSegment segment{};
  if (!queue.dequeue(segment)) return false;
  if (!queueTimedSegmentWithRetry(segment, false)) return false;
  if (!stepper_backend.startTimedSegments()) return false;

  while (queue.dequeue(segment)) {
    if (stopForAbort("Motion stopped during timed segment queueing")) {
      return false;
    }
    if (!queueTimedSegmentWithRetry(segment, true)) return false;
  }
  return waitForMotionOrLimit();
}

void stashPendingCommand(const CommandMessage& command) {
  pending_command = command;
  has_pending_command = true;
}

bool receiveNextCommand(CommandMessage& command, TickType_t ticks_to_wait) {
  if (has_pending_command) {
    command = pending_command;
    has_pending_command = false;
    return true;
  }
  return xQueueReceive(command_queue, &command, ticks_to_wait) == pdTRUE;
}

bool commandQueueEmpty() {
  return command_queue == nullptr || uxQueueMessagesWaiting(command_queue) == 0;
}

void clearMotionQueues(const char* reason, bool clear_pending = true) {
  planner_queue.clear();
  segment_queue.clear();
  if (clear_pending) {
    has_pending_command = false;
  }
  if (reason != nullptr) {
    logMessage("MOTION_QUEUES cleared reason=%s", reason);
  }
}

JobPreflight currentJobPreflight() {
  JobPreflight preflight{};
  preflight.pending_empty = !has_pending_command;
  preflight.planner_empty = planner_queue.isEmpty();
  preflight.segment_empty = segment_queue.isEmpty();
  preflight.command_queue_empty = commandQueueEmpty();
  preflight.backend_idle = !stepper_backend.isRunning();
  return preflight;
}

bool jobPreflightIdle(const JobPreflight& preflight) {
  return preflight.pending_empty && preflight.planner_empty &&
         preflight.segment_empty && preflight.command_queue_empty &&
         preflight.backend_idle;
}

bool prepareJobBeginAutoHome() {
  if (!runtime_config.job_begin_auto_home || machine_state.homed) {
    return true;
  }
  job_controller.recoverToIdleIfSafe(safety_manager, machine_state);
  if (job_controller.state() != JobState::IDLE) {
    return true;
  }

  const JobPreflight preflight = currentJobPreflight();
  if (!jobPreflightIdle(preflight)) {
    return true;
  }

  safety_manager.poll();
  machine_state.alarmed = safety_manager.isAlarmed();
  if (machine_state.alarmed) {
    return true;
  }

  if (!machine_state.tmc_ready || !tmc_manager.isReady()) {
    logMessage("JOB_BEGIN TMC_INIT auto");
    machine_state.tmc_ready = tmc_manager.begin();
  }
  if (!machine_state.tmc_ready) {
    job_controller.markFailed("tmc_not_ready");
    logMessage("JOB_BEGIN rejected reason=tmc_not_ready");
    return false;
  }

  logMessage("JOB_BEGIN AUTO_HOME start");
  if (!homing_controller.runHome(stepper_backend, safety_manager,
                                 machine_state)) {
    machine_state.alarmed = safety_manager.isAlarmed();
    job_controller.markFailed("auto_home_failed");
    logMessage("JOB_BEGIN rejected reason=auto_home_failed");
    return false;
  }
  logMessage("JOB_BEGIN AUTO_HOME OK");
  return true;
}

bool rejectDisallowedJobCommand(const CommandMessage& command) {
  if (job_controller.allowCommand(command.type, command.from_gcode)) {
    return false;
  }
  if (command.type == CommandType::JOB_ABORT) {
    clearMotionAbort();
    clearMotionQueues("JOB_ABORT no active job");
    logMessage("JOB_ABORT rejected reason=no_active_job");
  } else {
    logMessage("REJECT: command %s not allowed while job_state=%s source=%s",
               command.name, job_controller.stateName(),
               command.from_gcode ? "GCODE" : "SERIAL");
  }
  return true;
}

bool ensureTmcReadyForMotion(const char* context) {
  if (machine_state.tmc_ready && tmc_manager.isReady()) {
    return true;
  }
  logMessage("%s TMC_INIT auto", context);
  machine_state.tmc_ready = tmc_manager.begin();
  if (!machine_state.tmc_ready) {
    logMessage("REJECT: %s reason=tmc_not_ready", context);
    return false;
  }
  return true;
}

GcodeInterpreterResult translateGcodeCommand(const CommandMessage& command,
                                             const MachineState& reference,
                                             CommandMessage& translated) {
  char log[128] = {};
  const GcodeInterpreterResult result = gcode_interpreter.interpret(
      command.gcode, reference, translated, log, sizeof(log));
  if (result == GcodeInterpreterResult::MODAL_UPDATE) {
    logMessage("%s", log);
  } else if (result == GcodeInterpreterResult::ERROR) {
    logMessage("ERROR: %s", log);
  } else if (translated.type == CommandType::XY) {
    logMessage("GCODE %s -> XY X=%.3f Y=%.3f F=%.3f mode=%s units=%s",
               command.name, translated.x_mm, translated.y_mm,
               translated.feed_mm_min,
               gcode_interpreter.absoluteMode() ? "ABS" : "REL",
               gcode_interpreter.unitsInches() ? "INCH" : "MM");
  } else if (translated.type == CommandType::DWELL) {
    logMessage("GCODE %s -> DWELL P=%lums", command.name,
               static_cast<unsigned long>(translated.dwell_ms));
  } else {
    logMessage("GCODE %s -> command %s", command.name, translated.name);
  }
  return result;
}

void resetGcodeModalForJob() {
  gcode_interpreter.resetModalState();
  machine_state.feed_mm_min = runtime_config.default_feed_mm_min;
  logMessage("JOB modal reset units=MM distance=ABSOLUTE feed=%.3f",
             runtime_config.default_feed_mm_min);
}

bool buildXYBlock(const CommandMessage& command, float start_x_mm,
                  float start_y_mm, MotionBlock& block) {
  if (stopForAbort("XY rejected before planning")) {
    logMessage("NACK_XY target=(%.3f,%.3f) reason=abort",
               command.x_mm, command.y_mm);
    return false;
  }
  safety_manager.poll();
  float feed_mm_min = command.feed_mm_min;
  if (!safety_manager.validateMove(command.x_mm, command.y_mm, feed_mm_min)) {
    logMessage("NACK_XY target=(%.3f,%.3f) reason=rejected",
               command.x_mm, command.y_mm);
    return false;
  }
  const CoreXYDelta delta = CoreXYKinematics::xyMoveToABSteps(
      start_x_mm, start_y_mm, command.x_mm, command.y_mm,
      runtime_config.steps_per_mm);
  block = MotionBlock{};
  block.start_x_mm = start_x_mm;
  block.start_y_mm = start_y_mm;
  block.target_x_mm = command.x_mm;
  block.target_y_mm = command.y_mm;
  block.dx_mm = delta.dx_mm;
  block.dy_mm = delta.dy_mm;
  block.length_mm = sqrtf(delta.dx_mm * delta.dx_mm + delta.dy_mm * delta.dy_mm);
  block.nominal_speed_mm_min = feed_mm_min;
  block.entry_speed_mm_min = 0.0f;
  block.exit_speed_mm_min = 0.0f;
  block.a_steps = delta.a_steps;
  block.b_steps = delta.b_steps;
  block.pen_down = machine_state.pen_down;
  return true;
}

bool isNoOpXYBlock(const MotionBlock& block) {
  return block.length_mm < MIN_XY_MOVE_LENGTH_MM && block.a_steps == 0 &&
         block.b_steps == 0;
}

void acknowledgeNoOpXY(const MotionBlock& block, bool update_machine_state) {
  if (update_machine_state) {
    machine_state.x_mm = block.target_x_mm;
    machine_state.y_mm = block.target_y_mm;
    machine_state.feed_mm_min = block.nominal_speed_mm_min;
  }
  logMessage("XY no-op current=(%.3f,%.3f) target=(%.3f,%.3f) F=%.3f",
             block.start_x_mm, block.start_y_mm, block.target_x_mm,
             block.target_y_mm, block.nominal_speed_mm_min);
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f",
             block.target_x_mm, block.target_y_mm, block.a_steps,
             block.b_steps, block.nominal_speed_mm_min);
}

bool planQueuedBlocks() {
  if (!junction_planner.plan(planner_queue)) {
    logMessage("ERROR: junction planner rejected XY batch");
    return false;
  }
  for (size_t index = 0; index < planner_queue.count(); ++index) {
    MotionBlock* block = planner_queue.at(index);
    if (block == nullptr || !trapezoid_planner.plan(*block)) {
      logMessage("ERROR: trapezoid planner rejected XY batch index=%u",
                 static_cast<unsigned>(index));
      return false;
    }
  }
  return true;
}

bool executePlannedBlock(MotionBlock& block, size_t index, size_t count) {
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
  if (!segment_generator.generate(block, segment_queue)) {
    logMessage("ERROR: segment generator rejected XY move");
    logMessage("NACK_XY target=(%.3f,%.3f) reason=segment",
               block.target_x_mm, block.target_y_mm);
    return false;
  }
  logMessage("SEGMENTS count=%u duration=%.3f dda=YES",
             static_cast<unsigned>(segment_queue.count()),
             block.acceleration_time_s + block.cruise_time_s +
                 block.deceleration_time_s);
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: no motor output");
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f",
             block.target_x_mm, block.target_y_mm, block.a_steps,
             block.b_steps, block.nominal_speed_mm_min);
#else
  safety_manager.poll();
  const bool allow_x_limit_release =
      safety_manager.xLimitActive() &&
      block.dx_mm * static_cast<float>(runtime_config.homing_x_dir) < 0.0f;
  const bool allow_y_limit_release =
      safety_manager.yLimitActive() &&
      block.dy_mm * static_cast<float>(runtime_config.homing_y_dir) < 0.0f;
  if (allow_x_limit_release || allow_y_limit_release) {
    logMessage("LIMIT_RELEASE_ALLOW X=%s Y=%s max=%.3f",
               allow_x_limit_release ? "YES" : "NO",
               allow_y_limit_release ? "YES" : "NO",
               runtime_config.normal_move_limit_release_mm);
  }
  safety_manager.beginNormalMoveLimitReleaseAllowance(
      allow_x_limit_release, allow_y_limit_release, block.start_x_mm,
      block.start_y_mm);
  const bool executed = executeTimedSegments(segment_queue);
  safety_manager.clearNormalMoveLimitReleaseAllowance();
  if (!executed) {
    const char* alarm_reason = safety_manager.alarmReason();
    if (safety_manager.isAlarmed()) {
      const bool abort_alarm =
          strstr(alarm_reason, "abort requested") != nullptr;
      logMessage("ERROR: timed XY move stopped: alarm reason=%s",
                 alarm_reason);
      logMessage("NACK_XY target=(%.3f,%.3f) reason=%s",
                 block.target_x_mm, block.target_y_mm,
                 abort_alarm ? "abort" : "alarm");
    } else {
      logMessage("ERROR: backend rejected timed XY move");
      logMessage("NACK_XY target=(%.3f,%.3f) reason=backend",
                 block.target_x_mm, block.target_y_mm);
    }
    return false;
  }
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f",
             block.target_x_mm, block.target_y_mm, block.a_steps,
             block.b_steps, block.nominal_speed_mm_min);
#endif
  machine_state.x_mm = block.target_x_mm;
  machine_state.y_mm = block.target_y_mm;
  machine_state.a_steps += block.a_steps;
  machine_state.b_steps += block.b_steps;
  machine_state.feed_mm_min = block.nominal_speed_mm_min;
  return true;
}

bool handleXYBatch(const CommandMessage& first_command) {
  planner_queue.clear();
  float planned_x_mm = machine_state.x_mm;
  float planned_y_mm = machine_state.y_mm;
  float planned_feed_mm_min = machine_state.feed_mm_min;

  MotionBlock block{};
  if (!buildXYBlock(first_command, planned_x_mm, planned_y_mm, block)) {
    clearMotionQueues("XY build first failed");
    return false;
  }
  if (isNoOpXYBlock(block)) {
    acknowledgeNoOpXY(block, true);
  } else {
    if (!planner_queue.enqueue(block)) {
      logMessage("NACK_XY target=(%.3f,%.3f) reason=planner_queue_full",
                 first_command.x_mm, first_command.y_mm);
      return false;
    }
  }
  planned_x_mm = first_command.x_mm;
  planned_y_mm = first_command.y_mm;
  planned_feed_mm_min = first_command.feed_mm_min;

  CommandMessage next_command;
  while (!planner_queue.isFull() &&
         receiveNextCommand(next_command,
                            pdMS_TO_TICKS(
                                runtime_config.lookahead_batch_collect_ms))) {
    if (rejectDisallowedJobCommand(next_command)) {
      continue;
    }
    if (next_command.type == CommandType::GCODE) {
      MachineState planned_state = machine_state;
      planned_state.x_mm = planned_x_mm;
      planned_state.y_mm = planned_y_mm;
      planned_state.feed_mm_min = planned_feed_mm_min;
      CommandMessage translated{};
      const GcodeInterpreterResult result =
          translateGcodeCommand(next_command, planned_state, translated);
      if (result == GcodeInterpreterResult::ERROR) {
        clearMotionQueues("GCODE translate failed");
        return false;
      }
      if (result == GcodeInterpreterResult::MODAL_UPDATE) {
        continue;
      }
      if (translated.type != CommandType::XY) {
        stashPendingCommand(translated);
        break;
      }
      next_command = translated;
    }
    if (rejectDisallowedJobCommand(next_command)) {
      continue;
    }
    if (next_command.type != CommandType::XY) {
      stashPendingCommand(next_command);
      break;
    }
    MotionBlock next_block{};
    if (!buildXYBlock(next_command, planned_x_mm, planned_y_mm, next_block)) {
      clearMotionQueues("XY build batch failed");
      return false;
    }
    if (isNoOpXYBlock(next_block)) {
      acknowledgeNoOpXY(next_block, planner_queue.isEmpty());
    } else {
      if (!planner_queue.enqueue(next_block)) {
        logMessage("NACK_XY target=(%.3f,%.3f) reason=planner_queue_full",
                   next_command.x_mm, next_command.y_mm);
        return false;
      }
    }
    planned_x_mm = next_command.x_mm;
    planned_y_mm = next_command.y_mm;
    planned_feed_mm_min = next_command.feed_mm_min;
  }

  if (planner_queue.isEmpty()) {
    return true;
  }

  if (!planQueuedBlocks()) {
    const MotionBlock* failed = planner_queue.peekNext();
    logMessage("NACK_XY target=(%.3f,%.3f) reason=planner",
               failed != nullptr ? failed->target_x_mm : first_command.x_mm,
               failed != nullptr ? failed->target_y_mm : first_command.y_mm);
    clearMotionQueues("XY planning failed");
    return false;
  }

  logMessage("LOOKAHEAD blocks=%u junction_deviation=%.3f classic_jerk=%.3f",
             static_cast<unsigned>(planner_queue.count()),
             runtime_config.junction_deviation_mm,
             runtime_config.classic_jerk_limit_mm_s);

  const size_t planned_count = planner_queue.count();
  for (size_t index = 0; index < planned_count; ++index) {
    MotionBlock* planned_block = planner_queue.at(index);
    if (planned_block == nullptr ||
        !executePlannedBlock(*planned_block, index, planned_count)) {
      clearMotionQueues("XY execution failed");
      return false;
    }
  }
  machine_state.feed_mm_min = planned_feed_mm_min;
  clearMotionQueues("XY complete", false);
  return true;
}

bool moveToJobEndPark() {
  if (!runtime_config.job_end_park_enabled) {
    logMessage("JOB_END park skipped: disabled by config");
    return true;
  }
  if (fabsf(machine_state.x_mm - runtime_config.job_end_park_x_mm) < 0.01f &&
      fabsf(machine_state.y_mm - runtime_config.job_end_park_y_mm) < 0.01f) {
    logMessage("JOB_END park skipped: already at X=%.3f Y=%.3f",
               runtime_config.job_end_park_x_mm,
               runtime_config.job_end_park_y_mm);
    return true;
  }
  CommandMessage park{};
  park.type = CommandType::XY;
  park.from_gcode = true;
  snprintf(park.name, sizeof(park.name), "JOB_PARK");
  park.x_mm = runtime_config.job_end_park_x_mm;
  park.y_mm = runtime_config.job_end_park_y_mm;
  park.feed_mm_min = runtime_config.job_end_park_feed_mm_min;
  logMessage("JOB_END park target=(%.3f,%.3f) F=%.3f",
             park.x_mm, park.y_mm, park.feed_mm_min);
  return handleXYBatch(park);
}

void handleJobEnd() {
  if (!job_controller.isRunning()) {
    job_controller.endJob(currentJobPreflight(), safety_manager, machine_state,
                          pen_controller);
    return;
  }

  if (!jobPreflightIdle(currentJobPreflight())) {
    job_controller.markFailed("job_end_queue_not_empty");
    logMessage("JOB_END failed reason=queue_not_empty");
    return;
  }

  pen_controller.penUp();
  machine_state.pen_down = false;
  logMessage("JOB_END pen up before park");

  if (!moveToJobEndPark()) {
    job_controller.markFailed("job_end_park_failed");
    logMessage("JOB_END failed reason=park_failed");
    return;
  }
  if (!motor_melody_controller.playJobEndJingle(stepper_backend, tmc_manager,
                                                safety_manager)) {
    job_controller.markFailed("job_end_jingle_failed");
    logMessage("JOB_END failed reason=jingle_failed");
    return;
  }
  job_controller.endJob(currentJobPreflight(), safety_manager, machine_state,
                        pen_controller);
}

void handleABTimed(const CommandMessage& command) {
  logMessage("AB_TIMED command a_steps=%ld b_steps=%ld duration_us=%lu",
             command.a_steps, command.b_steps, command.duration_us);
  if (stopForAbort("AB_TIMED rejected before queue")) {
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
  if (command.duration_us < runtime_config.ab_timed_min_duration_us) {
    logMessage("NACK_AB_TIMED reason=duration_too_short min_us=%lu",
               runtime_config.ab_timed_min_duration_us);
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
  logMessage("AB_TIMED before_queue micros=%lu queueEntries A=%u B=%u running A=%u B=%u",
             micros_before_queue, stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
  const StepperBackend::TimedSegmentResult queue_result =
      stepper_backend.queueTimedSegment(segment, false);
  const uint32_t micros_after_queue = micros();
  logMessage("AB_TIMED after_queue micros=%lu result=%s queueEntries A=%u B=%u running A=%u B=%u",
             micros_after_queue, timedSegmentResultName(queue_result),
             stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
  if (queue_result != StepperBackend::TimedSegmentResult::QUEUED) {
    logMessage("NACK_AB_TIMED reason=queue_%s",
               timedSegmentResultName(queue_result));
    return;
  }
  const bool started = stepper_backend.startTimedSegments();
  logMessage("AB_TIMED start result=%s queueEntries A=%u B=%u running A=%u B=%u",
             started ? "OK" : "ERROR",
             stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
  if (!started) {
    logMessage("NACK_AB_TIMED reason=start_error");
    return;
  }
  if (!waitForMotionOrLimit()) {
    logMessage("AB_TIMED result=STOPPED queueEntries A=%u B=%u running A=%u B=%u",
               stepper_backend.motorAQueueEntries(),
               stepper_backend.motorBQueueEntries(),
               stepper_backend.isMotorARunning(),
               stepper_backend.isMotorBRunning());
    logMessage("NACK_AB_TIMED reason=stopped");
    return;
  }
  machine_state.a_steps += command.a_steps;
  machine_state.b_steps += command.b_steps;
  logMessage("AB_TIMED result=OK queueEntries A=%u B=%u running A=%u B=%u",
             stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
  logMessage("ACK_AB_TIMED a_steps=%ld b_steps=%ld duration_us=%lu",
             command.a_steps, command.b_steps, command.duration_us);
}

void handleSingleMotor(bool motor_a, int32_t steps) {
  invalidateHomed("independent motor test");
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: TEST_%c steps=%ld no motor output",
             motor_a ? 'A' : 'B', steps);
#else
  const bool accepted = motor_a ? stepper_backend.moveASteps(steps)
                                : stepper_backend.moveBSteps(steps);
  if (!accepted) {
    logMessage("ERROR: backend rejected TEST_%c", motor_a ? 'A' : 'B');
    return;
  }
  if (!waitForMotionOrLimit()) {
    logMessage("ERROR: TEST_%c stopped", motor_a ? 'A' : 'B');
  }
#endif
}

void handleConfigSet(const CommandMessage& command) {
  const RuntimeConfigSetResult result =
      setRuntimeConfigValue(command.config_key, command.config_value);
  switch (result) {
    case RuntimeConfigSetResult::OK:
      logMessage("CONFIG_SET %s=%s", command.config_key, command.config_value);
      if (runtimeConfigNeedsTmcReconfigure(command.config_key) &&
          machine_state.tmc_ready && tmc_manager.isReady()) {
        machine_state.tmc_ready = tmc_manager.applyNormalProfile();
      }
      return;
    case RuntimeConfigSetResult::UNKNOWN_KEY:
      logMessage("ERROR: CONFIG_SET unknown key=%s", command.config_key);
      return;
    case RuntimeConfigSetResult::INVALID_VALUE:
      logMessage("ERROR: CONFIG_SET invalid value key=%s value=%s",
                 command.config_key, command.config_value);
      return;
    case RuntimeConfigSetResult::READ_ONLY:
      logMessage("ERROR: CONFIG_SET read-only key=%s", command.config_key);
      return;
  }
}
}

void motionTask(void*) {
  CommandMessage command;
  for (;;) {
    if (!receiveNextCommand(command, portMAX_DELAY)) continue;
    if (rejectDisallowedJobCommand(command)) {
      publishStatus();
      continue;
    }
    switch (command.type) {
      case CommandType::HELP:
        Diagnostics::printHelp();
        break;
      case CommandType::CONFIG:
        Diagnostics::printConfig();
        break;
      case CommandType::CONFIG_GET:
        printRuntimeConfig();
        break;
      case CommandType::CONFIG_SET:
        handleConfigSet(command);
        break;
      case CommandType::CONFIG_RESET:
        resetRuntimeConfig();
        logMessage("CONFIG_RESET complete");
        if (machine_state.tmc_ready && tmc_manager.isReady()) {
          machine_state.tmc_ready = tmc_manager.applyNormalProfile();
        }
        break;
      case CommandType::POS: {
        Diagnostics::printPosition(currentStatus());
        break;
      }
      case CommandType::ZERO:
        machine_state.x_mm = 0;
        machine_state.y_mm = 0;
        machine_state.a_steps = 0;
        machine_state.b_steps = 0;
        invalidateHomed("ZERO logical origin reset");
        logMessage("ZERO logical origin reset; this is not homing");
        break;
      case CommandType::TEST_A:
        handleSingleMotor(true, command.steps);
        break;
      case CommandType::TEST_B:
        handleSingleMotor(false, command.steps);
        break;
      case CommandType::AB_TIMED:
        if (ensureTmcReadyForMotion("AB_TIMED")) {
          handleABTimed(command);
        }
        break;
      case CommandType::XY:
        if (ensureTmcReadyForMotion("XY")) {
          handleXYBatch(command);
        }
        break;
      case CommandType::DWELL:
        logMessage("DWELL P=%lums", static_cast<unsigned long>(command.dwell_ms));
        vTaskDelay(pdMS_TO_TICKS(command.dwell_ms));
        break;
      case CommandType::PEN_UP:
        pen_controller.penUp();
        machine_state.pen_down = false;
        logMessage("PEN UP");
        break;
      case CommandType::PEN_DOWN:
        pen_controller.penDown();
        machine_state.pen_down = true;
        logMessage("PEN DOWN");
        break;
      case CommandType::SELFTEST:
        Diagnostics::runSelfTest();
        break;
      case CommandType::TMC_INIT:
        machine_state.tmc_ready = tmc_manager.begin();
        break;
      case CommandType::TMC_STATUS:
        tmc_manager.printStatus();
        break;
      case CommandType::HOME:
        if (ensureTmcReadyForMotion("HOME")) {
          clearMotionQueues("HOME start");
          homing_controller.runHome(stepper_backend, safety_manager,
                                    machine_state);
          clearMotionQueues("HOME end");
        }
        break;
      case CommandType::HOME_X:
        if (ensureTmcReadyForMotion("HOME_X")) {
          clearMotionQueues("HOME_X start");
          homing_controller.runHomeX(stepper_backend, safety_manager,
                                     machine_state);
          clearMotionQueues("HOME_X end");
        }
        break;
      case CommandType::HOME_Y:
        if (ensureTmcReadyForMotion("HOME_Y")) {
          clearMotionQueues("HOME_Y start");
          homing_controller.runHomeY(stepper_backend, safety_manager,
                                     machine_state);
          clearMotionQueues("HOME_Y end");
        }
        break;
      case CommandType::HOME_STATUS:
        Diagnostics::printHomingStatus(currentStatus());
        break;
      case CommandType::LIMIT_STATUS:
        safety_manager.poll();
        Diagnostics::printLimitStatus(currentStatus());
        break;
      case CommandType::ALARM_CLEAR:
        safety_manager.poll();
        if (safety_manager.xLimitRawActive() || safety_manager.yLimitRawActive() ||
            safety_manager.xLimitActive() || safety_manager.yLimitActive()) {
          invalidateHomed("ALARM_CLEAR with limit active");
        }
        safety_manager.clearAlarm();
        machine_state.alarmed = false;
        logMessage("ALARM_CLEAR complete");
        break;
      case CommandType::ABORT:
        clearMotionAbort();
        stepper_backend.stop();
        clearMotionQueues("ABORT");
        safety_manager.setAlarm("abort requested");
        machine_state.alarmed = true;
        invalidateHomed("abort requested");
        if (job_controller.isActive() || job_controller.isRunning()) {
          job_controller.markAborted("abort requested");
        }
        logMessage("ABORT complete");
        break;
      case CommandType::JOB_BEGIN:
        if (prepareJobBeginAutoHome() &&
            job_controller.beginJob(currentJobPreflight(), safety_manager,
                                    machine_state, pen_controller,
                                    tmc_manager)) {
          resetGcodeModalForJob();
        }
        break;
      case CommandType::JOB_END:
        handleJobEnd();
        break;
      case CommandType::JOB_ABORT:
        clearMotionQueues("JOB_ABORT");
        if (job_controller.abortJob("job abort requested")) {
          clearMotionAbort();
          stepper_backend.stop();
          safety_manager.setAlarm("job abort requested");
          machine_state.alarmed = true;
          invalidateHomed("job abort requested");
          logMessage("JOB_ABORT complete");
        }
        break;
      case CommandType::JOB_STATUS:
        job_controller.printStatus();
        break;
      case CommandType::MELODY:
        motor_melody_controller.play(stepper_backend, tmc_manager,
                                     safety_manager);
        break;
      case CommandType::GCODE: {
        CommandMessage translated{};
        const GcodeInterpreterResult result =
            translateGcodeCommand(command, machine_state, translated);
        if (result == GcodeInterpreterResult::ERROR ||
            result == GcodeInterpreterResult::MODAL_UPDATE) {
          break;
        }
        if (rejectDisallowedJobCommand(translated)) {
          break;
        }
        if (translated.type == CommandType::XY) {
          if (ensureTmcReadyForMotion("GCODE_XY")) {
            handleXYBatch(translated);
          }
        } else {
          stashPendingCommand(translated);
        }
        break;
      }
      case CommandType::LED:
      case CommandType::LED_PIXEL:
      case CommandType::LED_OFF:
      case CommandType::LED_PATTERN:
      case CommandType::LED_BRIGHTNESS:
      case CommandType::LED_PARAM:
      case CommandType::LED_STATUS:
      case CommandType::INVALID:
        break;
    }
    machine_state.alarmed = safety_manager.isAlarmed();
    if (machine_state.alarmed && (job_controller.isActive() ||
                                  job_controller.isRunning())) {
      job_controller.markFailed(safety_manager.alarmReason());
    }
    publishStatus();
  }
}
