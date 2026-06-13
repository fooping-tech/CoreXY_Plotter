#include <Arduino.h>
#include "AppContext.h"

void safetyTask(void*) {
  for (;;) {
    safety_manager.poll();
    machine_state.alarmed = safety_manager.isAlarmed();
    machine_state.job_active = job_controller.isActive() ||
                               job_controller.isRunning();
    if (machine_state.motion_active && !stepper_backend.isRunning()) {
      machine_state.motion_active = false;
    }
    publishStatus();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
