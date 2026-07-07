#include <Arduino.h>
#include "AppContext.h"
#include "Diagnostics.h"
#include "GcodeCommandTranslator.h"
#include "JobLifecycleHandler.h"
#include "MotionDiagnostics.h"
#include "MotionSyncTracker.h"
#include "PlotterConfig.h"
#include "TimedSegmentExecutor.h"
#include "XYMotionPlanner.h"

namespace {
GcodeCommandTranslator gcode_translator;
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

void clearMotionQueues(const char* reason, bool clear_pending = true) {
  xyPlanner().clearQueues(reason, clear_pending);
}

JobPreflight currentJobPreflight() {
  JobPreflight preflight{};
  preflight.pending_empty = !has_pending_command;
  preflight.planner_empty = xyPlanner().plannerQueueEmpty();
  preflight.segment_empty = xyPlanner().segmentQueueEmpty();
  preflight.command_queue_empty =
      command_queue == nullptr || uxQueueMessagesWaiting(command_queue) == 0;
  preflight.backend_idle = !stepper_backend.isRunning();
  return preflight;
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
  return gcode_translator.translate(command, reference, translated);
}

// XYMotionPlannerへコマンド受信・job許可・G-code変換の方針関数を注入する。
XYMotionPlannerHooks makeXYMotionPlannerHooks() {
  XYMotionPlannerHooks hooks;
  hooks.stop_for_abort = &stopForAbort;
  hooks.receive_next_command = &receiveNextCommand;
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

JobLifecycleHandler& jobLifecycle() {
  static JobLifecycleHandler handler(
      job_controller, safety_manager, machine_state, tmc_manager,
      homing_controller, pen_controller, motor_melody_controller,
      stepper_backend, xyPlanner(), &currentJobPreflight);
  return handler;
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

MotionDiagnosticHooks makeMotionDiagnosticHooks() {
  MotionDiagnosticHooks hooks;
  hooks.stop_for_abort = &stopForAbort;
  hooks.set_motion_active = &setMotionActive;
  hooks.invalidate_homed = &invalidateHomed;
  return hooks;
}

void handleHelp(const CommandMessage&) { Diagnostics::printHelp(); }
void handleConfig(const CommandMessage&) { Diagnostics::printConfig(); }
void handlePos(const CommandMessage&) {
  Diagnostics::printPosition(captureStatus());
}

void handleZero(const CommandMessage&) {
  machine_state.x_mm = 0;
  machine_state.y_mm = 0;
  machine_state.a_steps = 0;
  machine_state.b_steps = 0;
  resetDriftReference("ZERO");
  invalidateHomed("ZERO logical origin reset");
  logMessage("ZERO logical origin reset; this is not homing");
}

void handleTestA(const CommandMessage& command) {
  runSingleMotorDiagnostic(true, command.steps, timedExecutor(),
                           makeMotionDiagnosticHooks());
}
void handleTestB(const CommandMessage& command) {
  runSingleMotorDiagnostic(false, command.steps, timedExecutor(),
                           makeMotionDiagnosticHooks());
}

void handleAbTimedCommand(const CommandMessage& command) {
  if (ensureTmcReadyForMotion("AB_TIMED")) {
    runAbTimedDiagnostic(command, timedExecutor(),
                         makeMotionDiagnosticHooks());
  }
}

void handleXYCommand(const CommandMessage& command) {
  if (ensureTmcReadyForMotion("XY")) {
    xyPlanner().handleBatch(command);
  }
}

void handleDwell(const CommandMessage& command) {
  logMessage("DWELL P=%lums", static_cast<unsigned long>(command.dwell_ms));
  vTaskDelay(pdMS_TO_TICKS(command.dwell_ms));
}

void handlePenUp(const CommandMessage&) {
  pen_controller.penUp();
  machine_state.pen_down = false;
  postLedStatus(LedStatus::DRAWING_PEN_UP);
  logMessage("PEN UP");
}

void handlePenDown(const CommandMessage&) {
  pen_controller.penDown();
  machine_state.pen_down = true;
  postLedStatus(LedStatus::DRAWING_PEN_DOWN);
  logMessage("PEN DOWN");
}

void handleSelfTest(const CommandMessage&) { Diagnostics::runSelfTest(); }
void handleTmcInit(const CommandMessage&) {
  machine_state.tmc_ready = tmc_manager.begin();
}
void handleTmcStatus(const CommandMessage&) { tmc_manager.printStatus(); }

void handleHome(const CommandMessage&) {
  handleHomeCommand("HOME", &HomingController::runHome);
}
void handleHomeX(const CommandMessage&) {
  handleHomeCommand("HOME_X", &HomingController::runHomeX);
}
void handleHomeY(const CommandMessage&) {
  handleHomeCommand("HOME_Y", &HomingController::runHomeY);
}

void handleHomeStatus(const CommandMessage&) {
  Diagnostics::printHomingStatus(captureStatus());
}

void handleLimitStatus(const CommandMessage&) {
  safety_manager.poll();
  Diagnostics::printLimitStatus(captureStatus());
}

void handleAlarmClear(const CommandMessage&) {
  safety_manager.poll();
  if (safety_manager.xLimitRawActive() || safety_manager.yLimitRawActive() ||
      safety_manager.xLimitActive() || safety_manager.yLimitActive()) {
    invalidateHomed("ALARM_CLEAR with limit active");
  }
  safety_manager.clearAlarm();
  machine_state.alarmed = false;
  postLedStatus(LedStatus::IDLE);
  logMessage("ALARM_CLEAR complete");
}

void handleAbort(const CommandMessage&) {
  clearMotionAbort();
  stepper_backend.stop();
  clearMotionQueues("ABORT");
  enterAlarm("abort requested", LedStatus::ERROR);
  if (job_controller.isActive()) {
    job_controller.markAborted("abort requested");
  }
  logMessage("ABORT complete");
}

void handleJobBegin(const CommandMessage&) {
  if (jobLifecycle().prepareJobBeginAutoHome() &&
      job_controller.beginJob(currentJobPreflight(), safety_manager,
                              machine_state, pen_controller, tmc_manager)) {
    gcode_translator.resetModalStateForJob(machine_state);
    resetDriftReference("JOB_BEGIN");
    postLedStatus(machine_state.pen_down ? LedStatus::DRAWING_PEN_DOWN
                                         : LedStatus::DRAWING_PEN_UP);
  } else if (safety_manager.isAlarmed()) {
    postLedStatus(LedStatus::ERROR);
  }
}

void handleJobEndCommand(const CommandMessage&) {
  jobLifecycle().handleJobEnd();
}

void handleJobAbort(const CommandMessage&) {
  clearMotionQueues("JOB_ABORT");
  if (job_controller.abortJob("job abort requested")) {
    clearMotionAbort();
    enterAlarm("job abort requested", LedStatus::WARNING);
    logMessage("JOB_ABORT complete");
  }
}

void handleJobStatus(const CommandMessage&) { job_controller.printStatus(); }

void handleMelody(const CommandMessage&) {
  motor_melody_controller.play(stepper_backend, tmc_manager, safety_manager);
}

void handleGcode(const CommandMessage& command) {
  CommandMessage translated{};
  const GcodeInterpreterResult result =
      translateGcodeCommand(command, machine_state, translated);
  if (result == GcodeInterpreterResult::ERROR ||
      result == GcodeInterpreterResult::MODAL_UPDATE) {
    return;
  }
  if (rejectDisallowedJobCommand(translated)) {
    return;
  }
  if (translated.type == CommandType::XY) {
    if (ensureTmcReadyForMotion("GCODE_XY")) {
      xyPlanner().handleBatch(translated);
    }
  } else {
    stashPendingCommand(translated);
  }
}

struct CommandHandlerEntry {
  CommandType type;
  void (*handler)(const CommandMessage& command);
};

// LED系とINVALIDはmotionTaskの対象外(テーブル未登録=no-op)。
const CommandHandlerEntry kCommandHandlers[] = {
    {CommandType::HELP, &handleHelp},
    {CommandType::CONFIG, &handleConfig},
    {CommandType::POS, &handlePos},
    {CommandType::ZERO, &handleZero},
    {CommandType::TEST_A, &handleTestA},
    {CommandType::TEST_B, &handleTestB},
    {CommandType::AB_TIMED, &handleAbTimedCommand},
    {CommandType::XY, &handleXYCommand},
    {CommandType::DWELL, &handleDwell},
    {CommandType::PEN_UP, &handlePenUp},
    {CommandType::PEN_DOWN, &handlePenDown},
    {CommandType::SELFTEST, &handleSelfTest},
    {CommandType::TMC_INIT, &handleTmcInit},
    {CommandType::TMC_STATUS, &handleTmcStatus},
    {CommandType::HOME, &handleHome},
    {CommandType::HOME_X, &handleHomeX},
    {CommandType::HOME_Y, &handleHomeY},
    {CommandType::HOME_STATUS, &handleHomeStatus},
    {CommandType::LIMIT_STATUS, &handleLimitStatus},
    {CommandType::ALARM_CLEAR, &handleAlarmClear},
    {CommandType::ABORT, &handleAbort},
    {CommandType::JOB_BEGIN, &handleJobBegin},
    {CommandType::JOB_END, &handleJobEndCommand},
    {CommandType::JOB_ABORT, &handleJobAbort},
    {CommandType::JOB_STATUS, &handleJobStatus},
    {CommandType::MELODY, &handleMelody},
    {CommandType::GCODE, &handleGcode},
};

void dispatchCommand(const CommandMessage& command) {
  for (const CommandHandlerEntry& entry : kCommandHandlers) {
    if (entry.type == command.type) {
      entry.handler(command);
      return;
    }
  }
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
    dispatchCommand(command);
    machine_state.alarmed = safety_manager.isAlarmed();
    if (machine_state.alarmed && job_controller.isActive()) {
      job_controller.markFailed(safety_manager.alarmReason());
      postLedStatus(LedStatus::ERROR);
    }
    syncJobActiveFlag();
    publishStatus();
  }
}
