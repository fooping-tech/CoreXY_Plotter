#include <Arduino.h>
#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "Diagnostics.h"
#include "PlotterConfig.h"
#include "SegmentGenerator.h"
#include "SegmentQueue.h"
#include "TrapezoidPlanner.h"

namespace {
TrapezoidPlanner trapezoid_planner;
SegmentGenerator segment_generator;
SegmentQueue segment_queue;

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
  logMessage("%s: abort requested", context);
  return true;
}

bool waitForMotionOrLimit() {
  while (stepper_backend.isRunning()) {
    if (stopForAbort("Motion stopped")) {
      return false;
    }
    safety_manager.poll();
    if (safety_manager.isAlarmed()) {
      stepper_backend.stop();
      logMessage("Motion stopped: alarm reason=%s", safety_manager.alarmReason());
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
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

void handleXY(const CommandMessage& command) {
  if (stopForAbort("XY rejected before planning")) {
    logMessage("NACK_XY target=(%.3f,%.3f) reason=abort",
               command.x_mm, command.y_mm);
    return;
  }
  float feed_mm_min = command.feed_mm_min;
  if (!safety_manager.validateMove(command.x_mm, command.y_mm, feed_mm_min)) {
    logMessage("NACK_XY target=(%.3f,%.3f) reason=rejected",
               command.x_mm, command.y_mm);
    return;
  }
  const CoreXYDelta delta = CoreXYKinematics::xyMoveToABSteps(
      machine_state.x_mm, machine_state.y_mm, command.x_mm, command.y_mm,
      STEPS_PER_MM);
  MotionBlock block{};
  block.start_x_mm = machine_state.x_mm;
  block.start_y_mm = machine_state.y_mm;
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
  if (!trapezoid_planner.plan(block)) {
    logMessage("ERROR: trapezoid planner rejected XY move");
    logMessage("NACK_XY target=(%.3f,%.3f) reason=planner",
               command.x_mm, command.y_mm);
    return;
  }
  logMessage("XY current=(%.3f,%.3f) target=(%.3f,%.3f) dx=%.3f dy=%.3f A=%ld B=%ld F=%.3f",
             machine_state.x_mm, machine_state.y_mm, command.x_mm, command.y_mm,
             delta.dx_mm, delta.dy_mm, delta.a_steps, delta.b_steps,
             feed_mm_min);
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
               command.x_mm, command.y_mm);
    return;
  }
  logMessage("SEGMENTS count=%u duration=%.3f dda=YES",
             static_cast<unsigned>(segment_queue.count()),
             block.acceleration_time_s + block.cruise_time_s +
                 block.deceleration_time_s);
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: no motor output");
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f",
             command.x_mm, command.y_mm, delta.a_steps, delta.b_steps,
             feed_mm_min);
#else
  if (!executeTimedSegments(segment_queue)) {
    logMessage("ERROR: backend rejected timed XY move");
    logMessage("NACK_XY target=(%.3f,%.3f) reason=backend",
               command.x_mm, command.y_mm);
    return;
  }
  logMessage("ACK_XY target=(%.3f,%.3f) A=%ld B=%ld F=%.3f",
             command.x_mm, command.y_mm, delta.a_steps, delta.b_steps,
             feed_mm_min);
#endif
  machine_state.x_mm = command.x_mm;
  machine_state.y_mm = command.y_mm;
  machine_state.a_steps += delta.a_steps;
  machine_state.b_steps += delta.b_steps;
  machine_state.feed_mm_min = feed_mm_min;
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
}

void motionTask(void*) {
  CommandMessage command;
  for (;;) {
    if (xQueueReceive(command_queue, &command, portMAX_DELAY) != pdTRUE) continue;
    switch (command.type) {
      case CommandType::HELP:
        Diagnostics::printHelp();
        break;
      case CommandType::CONFIG:
        Diagnostics::printConfig();
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
        handleABTimed(command);
        break;
      case CommandType::XY:
        handleXY(command);
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
        homing_controller.runHome(stepper_backend, safety_manager,
                                  machine_state);
        break;
      case CommandType::HOME_X:
        homing_controller.runHomeX(stepper_backend, safety_manager,
                                   machine_state);
        break;
      case CommandType::HOME_Y:
        homing_controller.runHomeY(stepper_backend, safety_manager,
                                   machine_state);
        break;
      case CommandType::HOME_STATUS:
        Diagnostics::printHomingStatus(currentStatus());
        break;
      case CommandType::LIMIT_STATUS:
        safety_manager.poll();
        Diagnostics::printLimitStatus(currentStatus());
        break;
      case CommandType::ALARM_CLEAR:
        safety_manager.clearAlarm();
        machine_state.alarmed = false;
        logMessage("ALARM_CLEAR complete");
        break;
      case CommandType::ABORT:
        clearMotionAbort();
        stepper_backend.stop();
        safety_manager.setAlarm("abort requested");
        machine_state.alarmed = true;
        invalidateHomed("abort requested");
        logMessage("ABORT complete");
        break;
      case CommandType::MELODY:
        motor_melody_controller.play(stepper_backend, tmc_manager,
                                     safety_manager);
        break;
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
    publishStatus();
  }
}
