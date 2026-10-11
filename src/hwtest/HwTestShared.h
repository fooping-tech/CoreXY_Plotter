#pragma once

#include <Arduino.h>
#include <stdint.h>

// Core0 -> Core1 hardware test command. Core0 parses Serial/buttons and never
// touches stepper, TMC UART, STEP/DIR or servo pins directly.
enum class HwCommandType : uint8_t {
  SELFTEST,
  LIMIT,
  LIMIT_WATCH,
  PIN_CHECK,
  PIN_SET,
  TMC_PING,
  TMC_INIT,
  TMC_STATUS,
  MOTOR,
  STOP,
  SERVO,
  PENUP,
  PENDOWN,
  SERVO_SWEEP,
};

struct HwCommand {
  HwCommandType type = HwCommandType::LIMIT;
  char target = 0;  // 'A' / 'B' or pin name initial
  int32_t arg0 = 0;
  int32_t arg1 = 0;
  bool has_arg0 = false;
  bool has_arg1 = false;
};

// Core0 local LED command. NEOPIXEL is driven from the UI core like the main
// firmware's uiTask.
enum class HwLedRequest : uint8_t {
  NONE,
  TEST,
  OFF,
  RGB,
};

enum class HwSelftestResult : uint8_t {
  NONE,
  RUNNING,
  PASS,
  FAIL,
};

// Core1 -> Core0 snapshot for LCD.
struct HwStatus {
  bool x_limit_raw_high = false;
  bool y_limit_raw_high = false;
  bool x_limit_active = false;
  bool y_limit_active = false;
  uint8_t tmc_a_connection = 0xFF;
  uint8_t tmc_b_connection = 0xFF;
  uint8_t tmc_a_version = 0;
  uint8_t tmc_b_version = 0;
  bool tmc_configured = false;
  uint16_t tmc_current_ma = 0;
  bool pins_released_to_backend = false;
  bool motor_running = false;
  int32_t motor_a_steps = 0;
  int32_t motor_b_steps = 0;
  int16_t servo_angle_deg = -1;
  HwSelftestResult selftest = HwSelftestResult::NONE;
  uint8_t selftest_fail_count = 0;
  char last_event[40] = "boot";
};

extern QueueHandle_t hw_command_queue;
extern QueueHandle_t hw_status_queue;
extern QueueHandle_t hw_log_queue;
extern volatile bool hw_stop_requested;

void logMessage(const char* format, ...);
bool enqueueHwCommand(const HwCommand& command);
void requestHwLed(HwLedRequest request, uint8_t r = 0, uint8_t g = 0,
                  uint8_t b = 0);
void requestHwPowerReport();

void hwCommandTask(void*);
void hwTestTask(void*);
void hwUiTask(void*);
void hwLogTask(void*);
