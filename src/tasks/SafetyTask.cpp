#include <Arduino.h>
#include "AppContext.h"
#include "TaskConfig.h"

void safetyTask(void*) {
  bool last_alarmed = false;
  for (;;) {
    safety_manager.poll();
    machine_state.alarmed = safety_manager.isAlarmed();
    if (machine_state.alarmed != last_alarmed) {
      postLedStatus(machine_state.alarmed ? LedStatus::ERROR : LedStatus::IDLE);
      last_alarmed = machine_state.alarmed;
    }
    syncJobActiveFlag();
    if (machine_state.motion_active && !stepper_backend.isRunning()) {
      machine_state.motion_active = false;
    }
    publishStatus();
    vTaskDelay(pdMS_TO_TICKS(SAFETY_TASK_POLL_INTERVAL_MS));
  }
}
