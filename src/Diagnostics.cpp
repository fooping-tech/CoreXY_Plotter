#include "Diagnostics.h"
#include "AppContext.h"
#include "Core2PinMap.h"
#include "CoreXYKinematics.h"
#include "PlannerQueue.h"
#include "PlotterConfig.h"
#include "TaskConfig.h"

void Diagnostics::printHelp() {
  logMessage("HELP CONFIG POS ZERO TEST_A <steps> TEST_B <steps> AB_TIMED <a_steps> <b_steps> <duration_us>");
  logMessage("XY <x_mm> <y_mm> [feed_mm_min] HOME HOME_X HOME_Y HOME_STATUS LIMIT_STATUS ALARM_CLEAR ABORT");
  logMessage("JOB_BEGIN JOB_END JOB_ABORT JOB_STATUS");
  logMessage("PENUP PENDOWN SELFTEST TMC_INIT TMC_STATUS");
  logMessage("G0/G1 X Y F G4 P<ms> G20 G21 G28 G90 G91 M3 M5 M114");
  logMessage("LED <r> <g> <b> LED_PIXEL <index> <r> <g> <b> LED_OFF LED_STATUS");
  logMessage("LED_PATTERN <OFF|SOLID|PACIFICA|FIRE> LED_BRIGHTNESS <value> LED_PARAM <name> <value> MELODY");
}

void Diagnostics::printConfig() {
  logMessage("BOARD=M5STACK_CORE2 SIMULATION_MODE=%u", SIMULATION_MODE);
  logMessage("MOTOR A STEP=%u DIR=%u B STEP=%u DIR=%u EN=HARDWIRED_GND active",
             MOTOR_A_STEP_PIN, MOTOR_A_DIR_PIN, MOTOR_B_STEP_PIN,
             MOTOR_B_DIR_PIN);
  logMessage("TMC UART TX=%u RX=%u baud=%lu A.address=%u B.address=%u",
             TMC_UART_TX_PIN, TMC_UART_RX_PIN, TMC_UART_BAUD,
             TMC_A_UART_ADDRESS, TMC_B_UART_ADDRESS);
  logMessage("LIMIT X=%u Y=%u PEN=%u NEOPIXEL=%u count=%u brightness_max=%u",
             X_LIMIT_PIN, Y_LIMIT_PIN, PEN_SERVO_PIN, NEOPIXEL_PIN,
             NEOPIXEL_LED_COUNT, NEOPIXEL_BRIGHTNESS_MAX);
  logMessage("MOTION steps_per_mm=%.3f default_feed=%.3f max_feed=%.3f accel=%.3f soft_limit X=[%.3f,%.3f] Y=[%.3f,%.3f]",
             STEPS_PER_MM, DEFAULT_FEED_MM_MIN, MAX_FEED_MM_MIN,
             DEFAULT_ACCEL_MM_S2, X_MIN_MM, X_MAX_MM, Y_MIN_MM, Y_MAX_MM);
  logMessage("LOOKAHEAD junction_deviation=%.3f classic_jerk=%.3f batch_collect_ms=%lu planner_capacity=%u",
             JUNCTION_DEVIATION_MM, CLASSIC_JERK_LIMIT_MM_S,
             LOOKAHEAD_BATCH_COLLECT_MS,
             static_cast<unsigned>(PlannerQueue::CAPACITY));
  logMessage("JOB state=%s result=%s last_error=%s",
             job_controller.stateName(), job_controller.result(),
             job_controller.lastError());
  logMessage("JOB_BEGIN auto_home=%u", JOB_BEGIN_AUTO_HOME);
  logMessage("JOB_END park_enabled=%u park=(%.3f,%.3f) park_feed=%.3f jingle_enabled=%u",
             JOB_END_PARK_ENABLED, JOB_END_PARK_X_MM, JOB_END_PARK_Y_MM,
             JOB_END_PARK_FEED_MM_MIN, JOB_END_JINGLE_ENABLED);
  logMessage("HOMING enabled=%u require_homed_xy=%u x_dir=%d y_dir=%d seek=%.3f slow=%.3f backoff=%.3f start_backoff=%.3f maxX=%.3f maxY=%.3f debounce=%lums hard_limit_ms=%lums release=%.3f active=%s",
             HOMING_ENABLED, HOMING_REQUIRE_HOMED_FOR_XY_MOVE, HOMING_X_DIR,
             HOMING_Y_DIR, HOMING_SEEK_FEED_MM_MIN, HOMING_SLOW_FEED_MM_MIN,
             HOMING_BACKOFF_MM, HOMING_START_BACKOFF_MM, HOMING_MAX_TRAVEL_X_MM,
             HOMING_MAX_TRAVEL_Y_MM, HOMING_LIMIT_DEBOUNCE_MS,
             HARD_LIMIT_UNEXPECTED_ALARM_MS, NORMAL_MOVE_LIMIT_RELEASE_MM,
             LIMIT_ACTIVE_LOW ? "LOW" : "HIGH");
  logMessage("M5_UI=%s LCD_SPI_MOSI=%u MOTOR_EN=HARDWIRED_GND",
             M5_UI_ENABLED ? "ENABLED" : "DISABLED", CORE2_LCD_SPI_MOSI_PIN);
  logMessage("CORE UI=%u MOTION=%u PRIORITY ui=%u command=%u tmc=%u safety=%u motion=%u stepper=%u",
             CORE_UI, CORE_MOTION, PRIORITY_UI, PRIORITY_COMMAND, PRIORITY_TMC,
             PRIORITY_SAFETY, PRIORITY_MOTION, PRIORITY_STEPPER_FEED);
}

void Diagnostics::printPosition(const StatusMessage& status) {
  const MachineState& state = status.machine;
  logMessage("POS X=%.3f Y=%.3f A=%ld B=%ld F=%.3f EN=HARDWIRED_ACTIVE HOMED=%s X_HOMED=%s Y_HOMED=%s HOMING=%s PEN=%s ALARM=%s TMC=%s LIMIT_X=%s LIMIT_Y=%s LIMIT_X_RAW=%s LIMIT_Y_RAW=%s",
             state.x_mm, state.y_mm, state.a_steps, state.b_steps,
             state.feed_mm_min, state.homed ? "YES" : "NO",
             state.x_homed ? "YES" : "NO", state.y_homed ? "YES" : "NO",
             state.homing_state,
             state.pen_down ? "DOWN" : "UP",
             state.alarmed ? "YES" : "NO", state.tmc_ready ? "READY" : "NO",
             status.x_limit_active ? "ACTIVE" : "OPEN",
             status.y_limit_active ? "ACTIVE" : "OPEN",
             status.x_limit_raw_active ? "ACTIVE" : "OPEN",
             status.y_limit_raw_active ? "ACTIVE" : "OPEN");
}

void Diagnostics::printLimitStatus(const StatusMessage& status) {
  logMessage("LIMIT_STATUS X_RAW=%s X_DEBOUNCED=%s Y_RAW=%s Y_DEBOUNCED=%s debounce=%lums active=%s",
             status.x_limit_raw_active ? "ON" : "OFF",
             status.x_limit_active ? "ON" : "OFF",
             status.y_limit_raw_active ? "ON" : "OFF",
             status.y_limit_active ? "ON" : "OFF",
             HOMING_LIMIT_DEBOUNCE_MS, LIMIT_ACTIVE_LOW ? "LOW" : "HIGH");
}

void Diagnostics::printHomingStatus(const StatusMessage& status) {
  const MachineState& state = status.machine;
  logMessage("HOME_STATUS state=%s active=%s homed=%s x_homed=%s y_homed=%s alarm=%s limitX=%s limitY=%s reason=%s",
             state.homing_state, state.homing_active ? "YES" : "NO",
             state.homed ? "YES" : "NO", state.x_homed ? "YES" : "NO",
             state.y_homed ? "YES" : "NO", state.alarmed ? "YES" : "NO",
             status.x_limit_active ? "ON" : "OFF",
             status.y_limit_active ? "ON" : "OFF",
             homing_controller.lastReason());
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
