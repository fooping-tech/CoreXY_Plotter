#include <Arduino.h>
#include "AppContext.h"

void safetyTask(void*) {
  for (;;) {
    safety_manager.poll();
    machine_state.alarmed = safety_manager.isAlarmed();
    publishStatus();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
