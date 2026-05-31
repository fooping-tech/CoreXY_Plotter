#include <Arduino.h>
#include <M5Unified.h>
#include "AppContext.h"
#include "PlotterConfig.h"
#include <string.h>

namespace {
void drawStatus(const StatusMessage& status) {
  const MachineState& state = status.machine;
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.printf("CoreXY Plotter\n");
  M5.Display.printf("mode: %s\n", SIMULATION_MODE ? "SIMULATION" : "REAL");
  M5.Display.printf("pos: X %.2f Y %.2f\n", state.x_mm, state.y_mm);
  M5.Display.printf("motor: %s\n", state.enabled ? "ENABLED" : "DISABLED");
  M5.Display.printf("homing: %s\n", state.homed ? "HOMED" : "NOT HOMED");
  M5.Display.printf("pen: %s\n", state.pen_down ? "DOWN" : "UP");
  M5.Display.printf("safety: %s\n", state.alarmed ? "ALARM" : "READY");
  M5.Display.printf("limit: X %s Y %s\n",
                    status.x_limit_active ? "ON" : "OFF",
                    status.y_limit_active ? "ON" : "OFF");
  M5.Display.printf("TMC: %s\n", state.tmc_ready ? "READY" : "NOT READY");
  M5.Display.endWrite();
}
}

void uiTask(void*) {
  auto m5_config = M5.config();
  M5.begin(m5_config);
  const bool lcd_ready = M5.Display.begin();
  neopixel_controller.begin();
  led_pattern_engine.begin(neopixel_controller);

  StatusMessage status;
  StatusMessage displayed_status;
  bool have_displayed_status = false;
  uint32_t last_lcd_draw_ms = 0;
  for (;;) {
    M5.update();
    LedCommand led_command;
    while (xQueueReceive(led_command_queue, &led_command, 0) == pdTRUE) {
      led_pattern_engine.applyCommand(led_command);
    }
    led_pattern_engine.tick(millis());

    if (xQueueReceive(status_queue, &status, pdMS_TO_TICKS(20)) == pdTRUE) {
      const uint32_t now_ms = millis();
      const bool changed =
          !have_displayed_status ||
          memcmp(&status, &displayed_status, sizeof(status)) != 0;
      if (lcd_ready &&
          (changed || now_ms - last_lcd_draw_ms >= LCD_RESYNC_INTERVAL_MS)) {
        drawStatus(status);
        displayed_status = status;
        have_displayed_status = true;
        last_lcd_draw_ms = now_ms;
      }
    }
  }
}
