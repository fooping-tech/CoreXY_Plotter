#include <Arduino.h>

void tmcTask(void*) {
  // Placeholder: future low-frequency TMC diagnostics only.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
