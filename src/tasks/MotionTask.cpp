#include <Arduino.h>
#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "Diagnostics.h"
#include "PlotterConfig.h"

namespace {
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

bool waitForMotionOrLimit() {
  while (stepper_backend.isRunning()) {
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

void handleXY(const CommandMessage& command) {
  float feed_mm_min = command.feed_mm_min;
  if (!safety_manager.validateMove(command.x_mm, command.y_mm, feed_mm_min)) {
    return;
  }
  const CoreXYDelta delta = CoreXYKinematics::xyMoveToABSteps(
      machine_state.x_mm, machine_state.y_mm, command.x_mm, command.y_mm,
      STEPS_PER_MM);
  logMessage("XY current=(%.3f,%.3f) target=(%.3f,%.3f) dx=%.3f dy=%.3f A=%ld B=%ld F=%.3f",
             machine_state.x_mm, machine_state.y_mm, command.x_mm, command.y_mm,
             delta.dx_mm, delta.dy_mm, delta.a_steps, delta.b_steps,
             feed_mm_min);
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: no motor output");
#else
  if (!stepper_backend.moveABSteps(delta.a_steps, delta.b_steps, feed_mm_min)) {
    logMessage("ERROR: backend rejected XY move");
    return;
  }
  if (!waitForMotionOrLimit()) {
    return;
  }
#endif
  machine_state.x_mm = command.x_mm;
  machine_state.y_mm = command.y_mm;
  machine_state.a_steps += delta.a_steps;
  machine_state.b_steps += delta.b_steps;
  machine_state.feed_mm_min = feed_mm_min;
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
  stepper_backend.waitUntilIdle();
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
