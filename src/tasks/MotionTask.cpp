#include <Arduino.h>
#include "AppContext.h"
#include "CoreXYKinematics.h"
#include "Diagnostics.h"
#include "PlotterConfig.h"

namespace {
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
  if (!machine_state.enabled) {
    logMessage("REJECT: motors are disabled");
    return;
  }
  if (!stepper_backend.moveABSteps(delta.a_steps, delta.b_steps, feed_mm_min)) {
    logMessage("ERROR: backend rejected XY move");
    return;
  }
  stepper_backend.waitUntilIdle();
#endif
  machine_state.x_mm = command.x_mm;
  machine_state.y_mm = command.y_mm;
  machine_state.a_steps += delta.a_steps;
  machine_state.b_steps += delta.b_steps;
  machine_state.feed_mm_min = feed_mm_min;
}

void handleSingleMotor(bool motor_a, int32_t steps) {
#if SIMULATION_MODE
  logMessage("SIMULATION_MODE: TEST_%c steps=%ld no motor output",
             motor_a ? 'A' : 'B', steps);
#else
  if (!machine_state.enabled) {
    logMessage("REJECT: motors are disabled");
    return;
  }
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
        StatusMessage status{machine_state, safety_manager.xLimitActive(),
                             safety_manager.yLimitActive()};
        Diagnostics::printPosition(status);
        break;
      }
      case CommandType::ENABLE:
        if (!stepper_backend.isReady()) {
          logMessage("ERROR: stepper backend is not ready");
          break;
        }
        stepper_backend.enable();
        machine_state.enabled = true;
        logMessage("Motors ENABLED logical gate; driver EN is hardwired active");
        break;
      case CommandType::DISABLE:
        stepper_backend.disable();
        machine_state.enabled = false;
        logMessage("Motors DISABLED logical gate only; driver EN remains hardwired active");
        break;
      case CommandType::ZERO:
        machine_state.x_mm = 0;
        machine_state.y_mm = 0;
        machine_state.a_steps = 0;
        machine_state.b_steps = 0;
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
      case CommandType::MELODY:
        motor_melody_controller.play(stepper_backend, tmc_manager,
                                     safety_manager, machine_state.enabled);
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
