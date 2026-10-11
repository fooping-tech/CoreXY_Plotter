// Controller PCB hardware test firmware entry point.
// Built only by the `m5stack-core2-hwtest` PlatformIO env.
//
// Core 0: hwCommandTask (Serial parse), hwUiTask (LCD/buttons/NEOPIXEL/AXP192),
//         hwLogTask (Serial output)
// Core 1: hwTestTask (limit inputs, STEP/DIR probe, TMC2209 UART, stepper,
//         servo)

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include "MachineState.h"
#include "NeoPixelController.h"
#include "PenController.h"
#include "PlotterConfig.h"
#include "StepperBackendFastAccel.h"
#include "TMC2209Manager.h"
#include "TaskConfig.h"
#include "hwtest/HwTestShared.h"

namespace {
constexpr uint16_t HW_COMMAND_QUEUE_LENGTH = 8;
constexpr uint32_t STACK_HW_TEST = 6144;
constexpr uint8_t PRIORITY_HW_TEST = PRIORITY_MOTION;
}

QueueHandle_t hw_command_queue = nullptr;
QueueHandle_t hw_status_queue = nullptr;
QueueHandle_t hw_log_queue = nullptr;
volatile bool hw_stop_requested = false;

// Shared module instances. Names match AppContext.h so the reused modules keep
// the same ownership model as the main firmware.
StepperBackendFastAccel stepper_backend;
TMC2209Manager tmc_manager(Serial2);
PenController pen_controller;
NeoPixelController neopixel_controller;

void logMessage(const char* format, ...) {
  LogMessage message{};
  va_list args;
  va_start(args, format);
  vsnprintf(message.text, sizeof(message.text), format, args);
  va_end(args);
  if (hw_log_queue != nullptr) {
    xQueueSend(hw_log_queue, &message, pdMS_TO_TICKS(20));
  }
}

bool enqueueHwCommand(const HwCommand& command) {
  return xQueueSend(hw_command_queue, &command, 0) == pdTRUE;
}

void hwLogTask(void*) {
  LogMessage message;
  for (;;) {
    if (xQueueReceive(hw_log_queue, &message, portMAX_DELAY) == pdTRUE) {
      Serial.println(message.text);
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  hw_command_queue = xQueueCreate(HW_COMMAND_QUEUE_LENGTH, sizeof(HwCommand));
  hw_status_queue = xQueueCreate(1, sizeof(HwStatus));
  hw_log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(LogMessage));

  xTaskCreatePinnedToCore(hwLogTask, "hwLogTask", STACK_LOG, nullptr,
                          PRIORITY_LOG, nullptr, CORE_UI);
  xTaskCreatePinnedToCore(hwUiTask, "hwUiTask", STACK_UI, nullptr, PRIORITY_UI,
                          nullptr, CORE_UI);
  xTaskCreatePinnedToCore(hwCommandTask, "hwCommandTask", STACK_COMMAND,
                          nullptr, PRIORITY_COMMAND, nullptr, CORE_UI);
  xTaskCreatePinnedToCore(hwTestTask, "hwTestTask", STACK_HW_TEST, nullptr,
                          PRIORITY_HW_TEST, nullptr, CORE_MOTION);
  logMessage("HWTEST firmware ready. Type HELP.");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
