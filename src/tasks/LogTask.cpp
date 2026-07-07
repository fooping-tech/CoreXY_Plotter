#include <Arduino.h>
#include "AppContext.h"

void logTask(void*) {
  LogMessage message;
  for (;;) {
    if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdTRUE) {
      Serial.println(message.text);
      // queue満杯でdropされたログがあれば、空きができたこの時点で報告する。
      const uint32_t dropped = takeDroppedLogCount();
      if (dropped > 0) {
        Serial.print(LOG_PREFIX_WARN "LogQueue dropped ");
        Serial.print(dropped);
        Serial.println(" messages");
      }
    }
  }
}
