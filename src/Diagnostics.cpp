#include "Diagnostics.h"
#include "AppContext.h"
#include "Core2PinMap.h"
#include "CoreXYKinematics.h"
#include "PlotterConfig.h"
#include "TaskConfig.h"

void Diagnostics::printHelp() {
  logMessage("HELP CONFIG POS ENABLE DISABLE ZERO TEST_A <steps> TEST_B <steps>");
  logMessage("XY <x_mm> <y_mm> <feed_mm_min> PENUP PENDOWN SELFTEST TMC_INIT TMC_STATUS");
  logMessage("LED <r> <g> <b> LED_PIXEL <index> <r> <g> <b> LED_OFF LED_STATUS");
  logMessage("LED_PATTERN <OFF|SOLID|PACIFICA|FIRE> LED_BRIGHTNESS <value> LED_PARAM <name> <value> MELODY");
}

void Diagnostics::printConfig() {
  logMessage("BOARD=M5STACK_CORE2 SIMULATION_MODE=%u", SIMULATION_MODE);
  logMessage("MOTOR A STEP=%u DIR=%u B STEP=%u DIR=%u EN=%u low-active",
             MOTOR_A_STEP_PIN, MOTOR_A_DIR_PIN, MOTOR_B_STEP_PIN,
             MOTOR_B_DIR_PIN, MOTOR_EN_PIN);
  logMessage("TMC UART TX=%u RX=%u baud=%lu A.address=%u B.address=%u",
             TMC_UART_TX_PIN, TMC_UART_RX_PIN, TMC_UART_BAUD,
             TMC_A_UART_ADDRESS, TMC_B_UART_ADDRESS);
  logMessage("LIMIT X=%u Y=%u PEN=%u NEOPIXEL=%u count=%u brightness_max=%u",
             X_LIMIT_PIN, Y_LIMIT_PIN, PEN_SERVO_PIN, NEOPIXEL_PIN,
             NEOPIXEL_LED_COUNT, NEOPIXEL_BRIGHTNESS_MAX);
  logMessage("CORE UI=%u MOTION=%u PRIORITY ui=%u command=%u tmc=%u safety=%u motion=%u stepper=%u",
             CORE_UI, CORE_MOTION, PRIORITY_UI, PRIORITY_COMMAND, PRIORITY_TMC,
             PRIORITY_SAFETY, PRIORITY_MOTION, PRIORITY_STEPPER_FEED);
}

void Diagnostics::printPosition(const StatusMessage& status) {
  const MachineState& state = status.machine;
  logMessage("POS X=%.3f Y=%.3f A=%ld B=%ld F=%.3f EN=%s HOMED=%s PEN=%s ALARM=%s TMC=%s LIMIT_X=%s LIMIT_Y=%s",
             state.x_mm, state.y_mm, state.a_steps, state.b_steps,
             state.feed_mm_min, state.enabled ? "YES" : "NO",
             state.homed ? "YES" : "NO", state.pen_down ? "DOWN" : "UP",
             state.alarmed ? "YES" : "NO", state.tmc_ready ? "READY" : "NO",
             status.x_limit_active ? "ACTIVE" : "OPEN",
             status.y_limit_active ? "ACTIVE" : "OPEN");
}

bool Diagnostics::runSelfTest() {
  struct TestCase {
    float current_x, current_y, target_x, target_y;
    int32_t expected_a, expected_b;
  };
  const TestCase cases[] = {
      {0, 0, 10, 0, 800, 800}, {0, 0, 0, 10, 800, -800},
      {0, 0, 10, 10, 1600, 0}, {10, 0, 10, 10, 800, -800},
      {10, 10, 0, 0, -1600, 0},
  };
  for (const auto& test : cases) {
    const CoreXYDelta delta = CoreXYKinematics::xyMoveToABSteps(
        test.current_x, test.current_y, test.target_x, test.target_y,
        STEPS_PER_MM);
    if (delta.a_steps != test.expected_a || delta.b_steps != test.expected_b) {
      logMessage("SELFTEST FAIL current=(%.1f,%.1f) target=(%.1f,%.1f) got A=%ld B=%ld expected A=%ld B=%ld",
                 test.current_x, test.current_y, test.target_x, test.target_y,
                 delta.a_steps, delta.b_steps, test.expected_a,
                 test.expected_b);
      return false;
    }
  }
  logMessage("SELFTEST PASS");
  return true;
}
