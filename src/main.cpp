#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include "AppContext.h"
#include "PlotterConfig.h"
#include "TaskConfig.h"

QueueHandle_t command_queue = nullptr;
QueueHandle_t status_queue = nullptr;
QueueHandle_t log_queue = nullptr;
QueueHandle_t led_command_queue = nullptr;
MachineState machine_state;
SafetyManager safety_manager;
StepperBackendFastAccel stepper_backend;
TMC2209Manager tmc_manager(Serial2);
PenController pen_controller;
NeoPixelController neopixel_controller;
LedPatternEngine led_pattern_engine;
MotorMelodyController motor_melody_controller;
HomingController homing_controller;
JobController job_controller;
volatile bool motion_abort_requested = false;

void requestMotionAbort() { motion_abort_requested = true; }
bool isMotionAbortRequested() { return motion_abort_requested; }
void clearMotionAbort() { motion_abort_requested = false; }

// LogQueue満杯時のdropポリシー: ここではブロックもSerial直接出力もしない
// (time-critical文脈から呼ばれるため)。dropは計数し、logTaskが後から
// "WARN: LogQueue dropped N messages" として報告する。
volatile uint32_t log_dropped_count = 0;

uint32_t takeDroppedLogCount() {
  const uint32_t count = log_dropped_count;
  log_dropped_count = 0;
  return count;
}

void logMessage(const char* format, ...) {
  LogMessage message{};
  va_list args;
  va_start(args, format);
  vsnprintf(message.text, sizeof(message.text), format, args);
  va_end(args);
  if (log_queue == nullptr || xQueueSend(log_queue, &message, 0) != pdTRUE) {
    ++log_dropped_count;
  }
}

StatusMessage captureStatus() {
  return StatusMessage{machine_state, safety_manager.xLimitActive(),
                       safety_manager.yLimitActive(),
                       safety_manager.xLimitRawActive(),
                       safety_manager.yLimitRawActive()};
}

void publishStatus() {
  if (status_queue == nullptr) {
    return;
  }
  const StatusMessage status = captureStatus();
  xQueueOverwrite(status_queue, &status);
}

void postLedStatus(LedStatus status) {
  if (led_command_queue == nullptr) return;
  LedCommand command{};
  command.type = LedCommandType::SET_STATUS;
  command.status = status;
  if (xQueueSend(led_command_queue, &command, 0) != pdTRUE) {
    logMessage("WARN: LedCommandQueue full status dropped");
  }
}

void syncJobActiveFlag() {
  // JobController::isActive()はSTARTING/RUNNING/ENDINGを含む。
  machine_state.job_active = job_controller.isActive();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(CommandMessage));
  status_queue = xQueueCreate(1, sizeof(StatusMessage));
  log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(LogMessage));
  led_command_queue = xQueueCreate(LED_COMMAND_QUEUE_LENGTH, sizeof(LedCommand));

  safety_manager.begin();
  pen_controller.begin();
  if (!stepper_backend.begin()) {
    logMessage("ERROR: FastAccelStepper backend initialization failed");
  }
  publishStatus();

  xTaskCreatePinnedToCore(uiTask, "uiTask", STACK_UI, nullptr, PRIORITY_UI, nullptr,
                          CORE_UI);
  xTaskCreatePinnedToCore(commandTask, "commandTask", STACK_COMMAND, nullptr,
                          PRIORITY_COMMAND, nullptr, CORE_UI);
  xTaskCreatePinnedToCore(logTask, "logTask", STACK_LOG, nullptr, PRIORITY_LOG,
                          nullptr, CORE_UI);
  xTaskCreatePinnedToCore(motionTask, "motionTask", STACK_MOTION, nullptr,
                          PRIORITY_MOTION, nullptr, CORE_MOTION);
  xTaskCreatePinnedToCore(stepperFeedTask, "stepperFeedTask", STACK_STEPPER_FEED, nullptr,
                          PRIORITY_STEPPER_FEED, nullptr, CORE_MOTION);
  xTaskCreatePinnedToCore(tmcTask, "tmcTask", STACK_TMC, nullptr, PRIORITY_TMC,
                          nullptr, CORE_MOTION);
  xTaskCreatePinnedToCore(safetyTask, "safetyTask", STACK_SAFETY, nullptr,
                          PRIORITY_SAFETY, nullptr, CORE_MOTION);
  logMessage("CoreXY plotter ready. Type HELP.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
