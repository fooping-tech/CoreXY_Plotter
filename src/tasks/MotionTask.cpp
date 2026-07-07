#include <Arduino.h>
#include <string.h>
#include "AppContext.h"
#include "Diagnostics.h"
#include "GcodeInterpreter.h"
#include "MotionSyncTracker.h"
#include "PlotterConfig.h"
#include "TimedSegmentExecutor.h"
#include "XYMotionPlanner.h"

namespace {
GcodeInterpreter gcode_interpreter;
MotionSyncTracker motion_sync_tracker;
CommandMessage pending_command;
bool has_pending_command = false;

XYMotionPlanner& xyPlanner();

void setMotionActive(bool active) {
  if (machine_state.motion_active == active) return;
  machine_state.motion_active = active;
  syncJobActiveFlag();
  publishStatus();
}

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

void invalidateHomed(const char* reason) {
  machine_state.homed = false;
  machine_state.x_homed = false;
  machine_state.y_homed = false;
  logMessage("HOMED invalidated: %s", reason);
}

// alarm突入シーケンスの共通経路。backend停止→setAlarm→alarmed→homed無効化→LED。
// alarm理由とhomed無効化ログの文言が異なる呼び出し元のため引数を分けている。
void enterAlarm(const char* alarm_reason, const char* homed_reason,
                LedStatus led_status) {
  stepper_backend.stop();
  safety_manager.setAlarm(alarm_reason);
  machine_state.alarmed = true;
  invalidateHomed(homed_reason);
  postLedStatus(led_status);
}

void enterAlarm(const char* reason, LedStatus led_status) {
  enterAlarm(reason, reason, led_status);
}

void resetDriftReference(const char* reason) {
#if !SIMULATION_MODE
  motion_sync_tracker.resetReference(stepper_backend.currentASteps(),
                                     stepper_backend.currentBSteps(),
                                     machine_state);
#else
  motion_sync_tracker.resetReference(machine_state.a_steps,
                                     machine_state.b_steps, machine_state);
#endif
  logMessage("DRIFT reference reset reason=%s A=%ld B=%ld",
             reason, machine_state.a_steps, machine_state.b_steps);
}

void warnIfDriftDetected() {
#if !SIMULATION_MODE
  const MotionDrift drift = motion_sync_tracker.computeDrift(
      stepper_backend.currentASteps(), stepper_backend.currentBSteps(),
      machine_state);
  if (drift.detected()) {
    logMessage("WARN: DRIFT a=%ld b=%ld machine=(%ld,%ld) backend=(%ld,%ld)",
               drift.drift_a_steps, drift.drift_b_steps,
               drift.machine_delta_a_steps, drift.machine_delta_b_steps,
               drift.backend_delta_a_steps, drift.backend_delta_b_steps);
  }
#endif
}

bool stopForAbort(const char* context) {
  if (!isMotionAbortRequested()) return false;
  clearMotionAbort();
  setMotionActive(false);
  enterAlarm("abort requested", LedStatus::ERROR);
  if (job_controller.isActive()) {
    job_controller.markAborted("abort requested");
  }
  logMessage("%s: abort requested", context);
  return true;
}

// TimedSegmentExecutorのフックは方針関数(abort/motion active/alarm)を注入する。
TimedSegmentExecutorHooks makeTimedSegmentExecutorHooks() {
  TimedSegmentExecutorHooks hooks;
  hooks.stop_for_abort = &stopForAbort;
  hooks.set_motion_active = &setMotionActive;
  hooks.enter_alarm = static_cast<void (*)(const char*, const char*,
                                           LedStatus)>(&enterAlarm);
  return hooks;
}

TimedSegmentExecutor& timedExecutor() {
  static TimedSegmentExecutor executor(stepper_backend, safety_manager,
                                       machine_state,
                                       makeTimedSegmentExecutorHooks());
  return executor;
}

MotionSyncReference captureMotionSyncReference() {
  return timedExecutor().captureReference();
}

void stashPendingCommand(const CommandMessage& command) {
  pending_command = command;
  has_pending_command = true;
}

void clearPendingCommand() {
  has_pending_command = false;
}

bool receiveNextCommand(CommandMessage& command, TickType_t ticks_to_wait) {
  if (has_pending_command) {
    command = pending_command;
    has_pending_command = false;
    return true;
  }
  return xQueueReceive(command_queue, &command, ticks_to_wait) == pdTRUE;
}

bool receiveNextCommandMs(CommandMessage& command, uint32_t wait_ms) {
  return receiveNextCommand(command, pdMS_TO_TICKS(wait_ms));
}

bool commandQueueEmpty() {
  return command_queue == nullptr || uxQueueMessagesWaiting(command_queue) == 0;
}

void clearMotionQueues(const char* reason, bool clear_pending = true) {
  xyPlanner().clearQueues(reason, clear_pending);
}

JobPreflight currentJobPreflight() {
  JobPreflight preflight{};
  preflight.pending_empty = !has_pending_command;
  preflight.planner_empty = xyPlanner().plannerQueueEmpty();
  preflight.segment_empty = xyPlanner().segmentQueueEmpty();
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
  if (!JOB_BEGIN_AUTO_HOME || machine_state.homed) {
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
  postLedStatus(LedStatus::HOMING);
  if (!homing_controller.runHome(stepper_backend, safety_manager,
                                 machine_state)) {
    machine_state.alarmed = safety_manager.isAlarmed();
    job_controller.markFailed("auto_home_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_BEGIN rejected reason=auto_home_failed");
    return false;
  }
  logMessage("JOB_BEGIN AUTO_HOME OK");
  postLedStatus(LedStatus::COMPLETED);
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
  machine_state.feed_mm_min = DEFAULT_FEED_MM_MIN;
  logMessage("JOB modal reset units=MM distance=ABSOLUTE feed=%.3f",
             DEFAULT_FEED_MM_MIN);
}

// XYMotionPlannerへコマンド受信・job許可・G-code変換の方針関数を注入する。
XYMotionPlannerHooks makeXYMotionPlannerHooks() {
  XYMotionPlannerHooks hooks;
  hooks.stop_for_abort = &stopForAbort;
  hooks.receive_next_command = &receiveNextCommandMs;
  hooks.stash_pending_command = &stashPendingCommand;
  hooks.reject_disallowed = &rejectDisallowedJobCommand;
  hooks.translate_gcode = &translateGcodeCommand;
  hooks.clear_pending_command = &clearPendingCommand;
  hooks.warn_if_drift_detected = &warnIfDriftDetected;
  return hooks;
}

XYMotionPlanner& xyPlanner() {
  static XYMotionPlanner planner(safety_manager, machine_state, job_controller,
                                 timedExecutor(), makeXYMotionPlannerHooks());
  return planner;
}

bool moveToJobEndPark() {
  if (!JOB_END_PARK_ENABLED) {
    logMessage("JOB_END park skipped: disabled by config");
    return true;
  }
  if (fabsf(machine_state.x_mm - JOB_END_PARK_X_MM) < 0.01f &&
      fabsf(machine_state.y_mm - JOB_END_PARK_Y_MM) < 0.01f) {
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
  return xyPlanner().handleBatch(park);
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
  postLedStatus(LedStatus::DRAWING_PEN_UP);
  logMessage("JOB_END pen up before park");

  if (!moveToJobEndPark()) {
    job_controller.markFailed("job_end_park_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_END failed reason=park_failed");
    return;
  }
  if (!motor_melody_controller.playJobEndJingle(stepper_backend, tmc_manager,
                                                safety_manager)) {
    job_controller.markFailed("job_end_jingle_failed");
    postLedStatus(LedStatus::ERROR);
    logMessage("JOB_END failed reason=jingle_failed");
    return;
  }
  job_controller.endJob(currentJobPreflight(), safety_manager, machine_state,
                        pen_controller);
  postLedStatus(LedStatus::COMPLETED);
}

void logAbTimedState(const char* detail) {
  logMessage("AB_TIMED %s queueEntries A=%u B=%u running A=%u B=%u",
             detail, stepper_backend.motorAQueueEntries(),
             stepper_backend.motorBQueueEntries(),
             stepper_backend.isMotorARunning(),
             stepper_backend.isMotorBRunning());
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
  const MotionSyncReference reference = captureMotionSyncReference();
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
      timedExecutor().handleQueueError(reference, "AB_TIMED queue");
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
    timedExecutor().handleQueueError(reference, "AB_TIMED start");
    logMessage("NACK_AB_TIMED reason=start_error");
    return;
  }
  setMotionActive(true);
  if (!timedExecutor().waitForMotionOrLimit(reference)) {
    setMotionActive(false);
    logAbTimedState("result=STOPPED");
    logMessage("NACK_AB_TIMED reason=stopped");
    return;
  }
  setMotionActive(false);
  logAbTimedState("result=OK");
  logMessage("ACK_AB_TIMED a_steps=%ld b_steps=%ld duration_us=%lu",
             command.a_steps, command.b_steps, command.duration_us);
}

using HomingRun = bool (HomingController::*)(StepperBackendFastAccel&,
                                             SafetyManager&, MachineState&);

void handleHomeCommand(const char* name, HomingRun run) {
  if (!ensureTmcReadyForMotion(name)) return;
  char reason[24] = {};
  snprintf(reason, sizeof(reason), "%s start", name);
  clearMotionQueues(reason);
  postLedStatus(LedStatus::HOMING);
  if ((homing_controller.*run)(stepper_backend, safety_manager,
                               machine_state)) {
    resetDriftReference(name);
    postLedStatus(LedStatus::COMPLETED);
  } else {
    postLedStatus(LedStatus::ERROR);
  }
  snprintf(reason, sizeof(reason), "%s end", name);
  clearMotionQueues(reason);
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
  setMotionActive(true);
  if (!timedExecutor().waitForMotionOrLimit()) {
    logMessage("ERROR: TEST_%c stopped", motor_a ? 'A' : 'B');
  }
  setMotionActive(false);
#endif
}
}

void motionTask(void*) {
  CommandMessage command;
  for (;;) {
    if (!receiveNextCommand(command, portMAX_DELAY)) continue;
    syncJobActiveFlag();
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
      case CommandType::POS: {
        Diagnostics::printPosition(captureStatus());
        break;
      }
      case CommandType::ZERO:
        machine_state.x_mm = 0;
        machine_state.y_mm = 0;
        machine_state.a_steps = 0;
        machine_state.b_steps = 0;
        resetDriftReference("ZERO");
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
          xyPlanner().handleBatch(command);
        }
        break;
      case CommandType::DWELL:
        logMessage("DWELL P=%lums", static_cast<unsigned long>(command.dwell_ms));
        vTaskDelay(pdMS_TO_TICKS(command.dwell_ms));
        break;
      case CommandType::PEN_UP:
        pen_controller.penUp();
        machine_state.pen_down = false;
        postLedStatus(LedStatus::DRAWING_PEN_UP);
        logMessage("PEN UP");
        break;
      case CommandType::PEN_DOWN:
        pen_controller.penDown();
        machine_state.pen_down = true;
        postLedStatus(LedStatus::DRAWING_PEN_DOWN);
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
        handleHomeCommand("HOME", &HomingController::runHome);
        break;
      case CommandType::HOME_X:
        handleHomeCommand("HOME_X", &HomingController::runHomeX);
        break;
      case CommandType::HOME_Y:
        handleHomeCommand("HOME_Y", &HomingController::runHomeY);
        break;
      case CommandType::HOME_STATUS:
        Diagnostics::printHomingStatus(captureStatus());
        break;
      case CommandType::LIMIT_STATUS:
        safety_manager.poll();
        Diagnostics::printLimitStatus(captureStatus());
        break;
      case CommandType::ALARM_CLEAR:
        safety_manager.poll();
        if (safety_manager.xLimitRawActive() || safety_manager.yLimitRawActive() ||
            safety_manager.xLimitActive() || safety_manager.yLimitActive()) {
          invalidateHomed("ALARM_CLEAR with limit active");
        }
        safety_manager.clearAlarm();
        machine_state.alarmed = false;
        postLedStatus(LedStatus::IDLE);
        logMessage("ALARM_CLEAR complete");
        break;
      case CommandType::ABORT:
        clearMotionAbort();
        stepper_backend.stop();
        clearMotionQueues("ABORT");
        enterAlarm("abort requested", LedStatus::ERROR);
        if (job_controller.isActive()) {
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
          resetDriftReference("JOB_BEGIN");
          postLedStatus(machine_state.pen_down ? LedStatus::DRAWING_PEN_DOWN
                                               : LedStatus::DRAWING_PEN_UP);
        } else if (safety_manager.isAlarmed()) {
          postLedStatus(LedStatus::ERROR);
        }
        break;
      case CommandType::JOB_END:
        handleJobEnd();
        break;
      case CommandType::JOB_ABORT:
        clearMotionQueues("JOB_ABORT");
        if (job_controller.abortJob("job abort requested")) {
          clearMotionAbort();
          enterAlarm("job abort requested", LedStatus::WARNING);
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
            xyPlanner().handleBatch(translated);
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
      case CommandType::LED_AUTO:
      case CommandType::LED_STATUS_SET:
      case CommandType::LED_STATUS:
      case CommandType::INVALID:
        break;
    }
    machine_state.alarmed = safety_manager.isAlarmed();
    if (machine_state.alarmed && job_controller.isActive()) {
      job_controller.markFailed(safety_manager.alarmReason());
      postLedStatus(LedStatus::ERROR);
    }
    syncJobActiveFlag();
    publishStatus();
  }
}
