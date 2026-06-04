#include <Arduino.h>
#include "AppContext.h"
#include "CommandDispatcher.h"

namespace {
bool isLedCommand(CommandType type) {
  return type == CommandType::LED || type == CommandType::LED_PIXEL ||
         type == CommandType::LED_OFF || type == CommandType::LED_PATTERN ||
         type == CommandType::LED_BRIGHTNESS || type == CommandType::LED_PARAM ||
         type == CommandType::LED_STATUS;
}
}

void commandTask(void*) {
  char line[128] = {};
  size_t length = 0;
  for (;;) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\r' || c == '\n') {
        line[length] = '\0';
        if (length > 0) {
          CommandMessage command = CommandDispatcher::parse(line);
          if (command.type == CommandType::INVALID) {
            logMessage("ERROR: %s", command.error);
          } else if (isLedCommand(command.type) &&
                     xQueueSend(led_command_queue, &command.led, 0) != pdTRUE) {
            logMessage("ERROR: LedCommandQueue full");
          } else if (!isLedCommand(command.type) &&
                     xQueueSend(command_queue, &command, pdMS_TO_TICKS(50)) !=
                     pdTRUE) {
            logMessage("ERROR: CommandQueue full");
          } else {
            logMessage("ACK QUEUED %s", command.name);
          }
        }
        length = 0;
      } else if (length + 1 < sizeof(line)) {
        line[length++] = c;
      } else {
        length = 0;
        logMessage("ERROR: input line too long");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
