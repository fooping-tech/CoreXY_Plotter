#include <Arduino.h>
#include "AppContext.h"

void logTask(void*) {
  LogMessage message;
  for (;;) {
    if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdTRUE) {
      Serial.println(message.text);
    }
  }
}
