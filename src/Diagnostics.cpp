#include "Diagnostics.h"
#include "AppContext.h"
#include "Core2PinMap.h"
#include "CoreXYKinematics.h"
#include "PlannerQueue.h"
#include "PlotterConfig.h"
#include "TaskConfig.h"

#include <math.h>

void Diagnostics::printHelp() {
  logMessage("HELP CONFIG POS ZERO TEST_A <steps> TEST_B <steps> AB_TIMED <a_steps> <b_steps> <duration_us>");
  logMessage("CONFIG_GET CONFIG_SET <key> <value> CONFIG_RESET");
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
             runtime_config.steps_per_mm, runtime_config.default_feed_mm_min,
             runtime_config.max_feed_mm_min, runtime_config.default_accel_mm_s2,
             runtime_config.x_min_mm, runtime_config.x_max_mm,
             runtime_config.y_min_mm, runtime_config.y_max_mm);
  logMessage("LOOKAHEAD junction_deviation=%.3f classic_jerk=%.3f batch_collect_ms=%lu planner_capacity=%u",
             runtime_config.junction_deviation_mm,
             runtime_config.classic_jerk_limit_mm_s,
             runtime_config.lookahead_batch_collect_ms,
             static_cast<unsigned>(PlannerQueue::CAPACITY));
  logMessage("JOB state=%s result=%s last_error=%s",
             job_controller.stateName(), job_controller.result(),
             job_controller.lastError());
  logMessage("JOB_BEGIN auto_home=%u", runtime_config.job_begin_auto_home);
  logMessage("JOB_END park_enabled=%u park=(%.3f,%.3f) park_feed=%.3f jingle_enabled=%u",
             runtime_config.job_end_park_enabled, runtime_config.job_end_park_x_mm,
             runtime_config.job_end_park_y_mm,
             runtime_config.job_end_park_feed_mm_min,
             runtime_config.job_end_jingle_enabled);
  logMessage("HOMING enabled=%u require_homed_xy=%u x_dir=%d y_dir=%d seek=%.3f slow=%.3f backoff=%.3f start_backoff=%.3f maxX=%.3f maxY=%.3f debounce=%lums hard_limit_ms=%lums release=%.3f active=%s",
             runtime_config.homing_enabled,
             runtime_config.homing_require_homed_for_xy_move,
             runtime_config.homing_x_dir, runtime_config.homing_y_dir,
             runtime_config.homing_seek_feed_mm_min,
             runtime_config.homing_slow_feed_mm_min,
             runtime_config.homing_backoff_mm,
             runtime_config.homing_start_backoff_mm,
             runtime_config.homing_max_travel_x_mm,
             runtime_config.homing_max_travel_y_mm,
             runtime_config.homing_limit_debounce_ms,
             runtime_config.hard_limit_unexpected_alarm_ms,
             runtime_config.normal_move_limit_release_mm,
             LIMIT_ACTIVE_LOW ? "LOW" : "HIGH");
  printRuntimeConfig();
  logMessage("M5_UI=%s LCD_SPI_MOSI=%u MOTOR_EN=HARDWIRED_GND",
             M5_UI_ENABLED ? "ENABLED" : "DISABLED", CORE2_LCD_SPI_MOSI_PIN);
  logMessage("CORE UI=%u MOTION=%u PRIORITY ui=%u command=%u tmc=%u safety=%u motion=%u stepper=%u",
             CORE_UI, CORE_MOTION, PRIORITY_UI, PRIORITY_COMMAND, PRIORITY_TMC,
             PRIORITY_SAFETY, PRIORITY_MOTION, PRIORITY_STEPPER_FEED);
}

void Diagnostics::printPosition(const StatusMessage& status) {
  const MachineState& state = status.machine;
  logMessage("POS X=%.3f Y=%.3f A=%ld B=%ld F=%.3f EN=HARDWIRED_ACTIVE HOMED=%s X_HOMED=%s Y_HOMED=%s HOMING=%s PEN=%s ALARM=%s ALARM_REASON=\"%s\" TMC=%s LIMIT_X=%s LIMIT_Y=%s LIMIT_X_RAW=%s LIMIT_Y_RAW=%s",
             state.x_mm, state.y_mm, state.a_steps, state.b_steps,
             state.feed_mm_min, state.homed ? "YES" : "NO",
             state.x_homed ? "YES" : "NO", state.y_homed ? "YES" : "NO",
             state.homing_state,
             state.pen_down ? "DOWN" : "UP",
             state.alarmed ? "YES" : "NO", safety_manager.alarmReason(),
             state.tmc_ready ? "READY" : "NO",
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
             runtime_config.homing_limit_debounce_ms,
             LIMIT_ACTIVE_LOW ? "LOW" : "HIGH");
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
    float expected_a_mm, expected_b_mm;
  };
  const TestCase cases[] = {
      {0, 0, 10, 0, 10, 10}, {0, 0, 0, 10, 10, -10},
      {0, 0, 10, 10, 20, 0}, {10, 0, 10, 10, 10, -10},
      {10, 10, 0, 0, -20, 0},
  };
  for (const auto& test : cases) {
    const int32_t expected_a =
        lroundf(test.expected_a_mm * runtime_config.steps_per_mm);
    const int32_t expected_b =
        lroundf(test.expected_b_mm * runtime_config.steps_per_mm);
    const CoreXYDelta delta = CoreXYKinematics::xyMoveToABSteps(
        test.current_x, test.current_y, test.target_x, test.target_y,
        runtime_config.steps_per_mm);
    if (delta.a_steps != expected_a || delta.b_steps != expected_b) {
      logMessage("SELFTEST FAIL current=(%.1f,%.1f) target=(%.1f,%.1f) got A=%ld B=%ld expected A=%ld B=%ld",
                 test.current_x, test.current_y, test.target_x, test.target_y,
                 delta.a_steps, delta.b_steps, expected_a, expected_b);
      return false;
    }
  }
  logMessage("SELFTEST PASS");
  return true;
}
