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

void logMessage(const char* format, ...) {
  LogMessage message{};
  va_list args;
  va_start(args, format);
  vsnprintf(message.text, sizeof(message.text), format, args);
  va_end(args);
  if (log_queue != nullptr) {
    xQueueSend(log_queue, &message, 0);
  }
}

void publishStatus() {
  if (status_queue == nullptr) {
    return;
  }
  StatusMessage status{machine_state, safety_manager.xLimitActive(),
                       safety_manager.yLimitActive(),
                       safety_manager.xLimitRawActive(),
                       safety_manager.yLimitRawActive()};
  xQueueOverwrite(status_queue, &status);
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
