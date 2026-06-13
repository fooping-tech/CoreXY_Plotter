#include <Arduino.h>
#include "AppContext.h"

void safetyTask(void*) {
  bool last_alarmed = false;
  for (;;) {
    safety_manager.poll();
    machine_state.alarmed = safety_manager.isAlarmed();
    if (machine_state.alarmed != last_alarmed && led_command_queue != nullptr) {
      LedCommand command{};
      command.type = LedCommandType::SET_STATUS;
      command.status =
          machine_state.alarmed ? LedStatus::ERROR : LedStatus::IDLE;
      (void)xQueueSend(led_command_queue, &command, 0);
      last_alarmed = machine_state.alarmed;
    }
    machine_state.job_active = job_controller.isActive() ||
                               job_controller.isRunning();
    if (machine_state.motion_active && !stepper_backend.isRunning()) {
      machine_state.motion_active = false;
    }
    publishStatus();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
