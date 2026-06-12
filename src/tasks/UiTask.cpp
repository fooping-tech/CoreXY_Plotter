#include <Arduino.h>
#include <M5Unified.h>
#include "AppContext.h"
#include "Core2PinMap.h"
#include "PlotterConfig.h"
#include <math.h>
#include <string.h>

namespace {
enum class UiPage : uint8_t {
  STATUS,
  CONTROL,
  DETAIL,
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr uint16_t COLOR_BG = 0x0841;
constexpr uint16_t COLOR_PANEL = 0x10A2;
constexpr uint16_t COLOR_PANEL_2 = 0x18E3;
constexpr uint16_t COLOR_TEXT = TFT_WHITE;
constexpr uint16_t COLOR_MUTED = 0x9CF3;
constexpr uint16_t COLOR_ACCENT = 0x05FF;
constexpr uint16_t COLOR_GREEN = 0x37E6;
constexpr uint16_t COLOR_YELLOW = 0xFEA0;
constexpr uint16_t COLOR_RED = 0xF946;
constexpr uint16_t COLOR_BLUE = 0x3D7F;
constexpr uint16_t COLOR_DISABLED = 0x4A69;

constexpr float UI_JOG_STEP_MM = 1.0f;
constexpr float UI_JOG_FEED_MM_MIN = 900.0f;

UiPage current_page = UiPage::STATUS;
bool force_redraw = true;
uint32_t last_clock_redraw_ms = 0;
char ui_notice[48] = "Ready";
uint32_t ui_notice_until_ms = 0;
M5Canvas ui_canvas(&M5.Display);
LovyanGFX* ui_gfx = &M5.Display;
float ui_jog_base_x_mm = 0.0f;
float ui_jog_base_y_mm = 0.0f;
bool ui_have_jog_base = false;

LovyanGFX& gfx() {
  return *ui_gfx;
}

const char* pageName(UiPage page) {
  switch (page) {
    case UiPage::STATUS:
      return "Status";
    case UiPage::CONTROL:
      return "Control";
    case UiPage::DETAIL:
      return "Detail";
  }
  return "";
}

uint16_t safetyColor(const MachineState& state) {
  if (state.alarmed) return COLOR_RED;
  if (state.homing_active) return COLOR_YELLOW;
  if (!state.homed) return COLOR_BLUE;
  return COLOR_GREEN;
}

const char* safetyText(const MachineState& state) {
  if (state.alarmed) return "ALARM";
  if (state.homing_active) return "HOMING";
  if (!state.homed) return "NEED HOME";
  return "READY";
}

bool contains(const Rect& rect, int16_t x, int16_t y) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y &&
         y < rect.y + rect.h;
}

void setNotice(const char* text) {
  snprintf(ui_notice, sizeof(ui_notice), "%s", text);
  ui_notice_until_ms = millis() + 1800;
  force_redraw = true;
}

void drawTextInRect(const char* text, const Rect& rect, uint16_t color,
                    uint8_t size = 1) {
  gfx().setTextSize(size);
  gfx().setTextColor(color, COLOR_PANEL);
  gfx().setTextDatum(middle_center);
  gfx().drawString(text, rect.x + rect.w / 2, rect.y + rect.h / 2);
  gfx().setTextDatum(top_left);
}

void drawRoundPanel(const Rect& rect, uint16_t color = COLOR_PANEL) {
  gfx().fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, color);
}

void drawButton(const Rect& rect, const char* label, uint16_t color,
                bool enabled = true, uint8_t size = 1) {
  const uint16_t fill = enabled ? color : COLOR_DISABLED;
  const uint16_t text = enabled ? COLOR_TEXT : COLOR_MUTED;
  gfx().fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, fill);
  gfx().drawRoundRect(rect.x, rect.y, rect.w, rect.h, 7,
                           enabled ? TFT_WHITE : COLOR_PANEL_2);
  drawTextInRect(label, rect, text, size);
}

void drawHeader(const StatusMessage& status) {
  const MachineState& state = status.machine;
  gfx().fillRect(0, 0, 320, 34, COLOR_BG);
  gfx().setTextDatum(top_left);
  gfx().setTextSize(2);
  gfx().setTextColor(COLOR_TEXT, COLOR_BG);
  gfx().drawString("CoreXY", 10, 8);

  gfx().setTextSize(1);
  gfx().setTextColor(COLOR_MUTED, COLOR_BG);
  gfx().drawString(pageName(current_page), 82, 14);

  const Rect pill{218, 6, 92, 22};
  gfx().fillRoundRect(pill.x, pill.y, pill.w, pill.h, 11,
                           safetyColor(state));
  gfx().setTextColor(COLOR_BG, safetyColor(state));
  gfx().setTextDatum(middle_center);
  gfx().drawString(safetyText(state), pill.x + pill.w / 2,
                        pill.y + pill.h / 2);
  gfx().setTextDatum(top_left);
}

void drawFooter() {
  const int16_t y = 214;
  gfx().fillRect(0, y, 320, 26, COLOR_BG);
  const Rect tabs[] = {{12, y + 3, 86, 20}, {117, y + 3, 86, 20},
                       {222, y + 3, 86, 20}};
  const UiPage pages[] = {UiPage::STATUS, UiPage::CONTROL, UiPage::DETAIL};
  const char* names[] = {"STAT", "CTRL", "INFO"};
  for (uint8_t i = 0; i < 3; ++i) {
    const bool active = current_page == pages[i];
    gfx().fillRoundRect(tabs[i].x, tabs[i].y, tabs[i].w, tabs[i].h, 10,
                             active ? COLOR_ACCENT : COLOR_PANEL_2);
    gfx().setTextColor(active ? COLOR_BG : COLOR_MUTED,
                            active ? COLOR_ACCENT : COLOR_PANEL_2);
    gfx().setTextDatum(middle_center);
    gfx().drawString(names[i], tabs[i].x + tabs[i].w / 2,
                          tabs[i].y + tabs[i].h / 2);
  }
  gfx().setTextDatum(top_left);
}

void drawNotice() {
  if (millis() > ui_notice_until_ms) return;
  const Rect rect{72, 188, 176, 18};
  gfx().fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, COLOR_PANEL_2);
  gfx().setTextColor(COLOR_TEXT, COLOR_PANEL_2);
  gfx().setTextDatum(middle_center);
  gfx().drawString(ui_notice, rect.x + rect.w / 2, rect.y + rect.h / 2);
  gfx().setTextDatum(top_left);
}

void drawMetric(const Rect& rect, const char* label, const char* value,
                uint16_t value_color = COLOR_TEXT) {
  drawRoundPanel(rect);
  if (rect.h < 40) {
    gfx().setTextSize(1);
    gfx().setTextColor(COLOR_MUTED, COLOR_PANEL);
    gfx().drawString(label, rect.x + 10, rect.y + 9);
    gfx().setTextColor(value_color, COLOR_PANEL);
    gfx().setTextDatum(middle_right);
    gfx().drawString(value, rect.x + rect.w - 10, rect.y + rect.h / 2);
    gfx().setTextDatum(top_left);
    return;
  }
  gfx().setTextSize(1);
  gfx().setTextColor(COLOR_MUTED, COLOR_PANEL);
  gfx().drawString(label, rect.x + 10, rect.y + 8);
  gfx().setTextSize(2);
  gfx().setTextColor(value_color, COLOR_PANEL);
  gfx().drawString(value, rect.x + 10, rect.y + 26);
}

void drawStatusPage(const StatusMessage& status) {
  const MachineState& state = status.machine;
  char value[32] = {};
  const Rect hero{10, 42, 300, 64};
  gfx().fillRoundRect(hero.x, hero.y, hero.w, hero.h, 10,
                           safetyColor(state));
  gfx().setTextColor(COLOR_BG, safetyColor(state));
  gfx().setTextDatum(top_left);
  gfx().setTextSize(1);
  gfx().drawString(SIMULATION_MODE ? "SIMULATION MODE" : "MACHINE STATE",
                        hero.x + 14, hero.y + 12);
  gfx().setTextSize(3);
  gfx().drawString(safetyText(state), hero.x + 14, hero.y + 30);

  snprintf(value, sizeof(value), "X %.1f", state.x_mm);
  drawMetric({10, 116, 94, 58}, "Position", value, COLOR_ACCENT);
  snprintf(value, sizeof(value), "Y %.1f", state.y_mm);
  drawMetric({113, 116, 94, 58}, "Position", value, COLOR_ACCENT);
  drawMetric({216, 116, 94, 58}, "Pen", state.pen_down ? "DOWN" : "UP",
             state.pen_down ? COLOR_YELLOW : COLOR_GREEN);

  drawMetric({10, 182, 145, 26}, "Home",
             state.homed ? "OK" : state.homing_active ? "RUN" : "WAIT",
             state.homed ? COLOR_GREEN : COLOR_YELLOW);
  drawMetric({165, 182, 145, 26}, "TMC",
             state.tmc_ready ? "READY" : "OFF",
             state.tmc_ready ? COLOR_GREEN : COLOR_MUTED);
}

bool canManualMove(const MachineState& state) {
  return state.homed && !state.alarmed && !state.homing_active;
}

void drawControlPage(const StatusMessage& status) {
  const MachineState& state = status.machine;
  const bool enabled = canManualMove(state);
  drawButton({10, 42, 145, 36}, "HOME", COLOR_BLUE, !state.homing_active, 2);
  drawButton({165, 42, 145, 36}, "CLEAR ALARM", COLOR_RED, state.alarmed, 1);

  drawButton({119, 86, 82, 34}, "UP", COLOR_PANEL_2, enabled, 1);
  drawButton({119, 162, 82, 34}, "DOWN", COLOR_PANEL_2, enabled, 1);
  drawButton({32, 124, 82, 34}, "LEFT", COLOR_PANEL_2, enabled, 1);
  drawButton({206, 124, 82, 34}, "RIGHT", COLOR_PANEL_2, enabled, 1);
  drawButton({119, 124, 82, 34}, "STOP", COLOR_RED, true, 1);

  drawButton({10, 178, 94, 30}, "PEN UP", COLOR_GREEN, enabled, 1);
  drawButton({216, 178, 94, 30}, "PEN DOWN", COLOR_YELLOW, enabled, 1);

  if (!enabled) {
    gfx().setTextColor(COLOR_MUTED, COLOR_BG);
    gfx().setTextDatum(middle_center);
    gfx().drawString(state.homed ? "Manual locked" : "Home required",
                          160, 105);
    gfx().setTextDatum(top_left);
  }
}

void drawDetailPage(const StatusMessage& status) {
  const MachineState& state = status.machine;
  char value[40] = {};
  drawRoundPanel({10, 42, 300, 36});
  gfx().setTextColor(COLOR_MUTED, COLOR_PANEL);
  gfx().setTextSize(1);
  gfx().drawString("Homing", 22, 51);
  gfx().setTextColor(COLOR_TEXT, COLOR_PANEL);
  snprintf(value, sizeof(value), "%s X%s Y%s", state.homing_state,
           state.x_homed ? "+" : "-", state.y_homed ? "+" : "-");
  gfx().drawString(value, 112, 51);

  snprintf(value, sizeof(value), "%.0f mm/min", state.feed_mm_min);
  drawMetric({10, 88, 145, 46}, "Feed", value, COLOR_ACCENT);
  snprintf(value, sizeof(value), "A %ld", static_cast<long>(state.a_steps));
  drawMetric({165, 88, 145, 46}, "Motor", value, COLOR_TEXT);
  snprintf(value, sizeof(value), "B %ld", static_cast<long>(state.b_steps));
  drawMetric({165, 142, 145, 46}, "Motor", value, COLOR_TEXT);

  drawMetric({10, 142, 145, 46}, "Limit",
             status.x_limit_active || status.y_limit_active ? "ACTIVE" : "OPEN",
             status.x_limit_active || status.y_limit_active ? COLOR_RED
                                                            : COLOR_GREEN);
  gfx().setTextColor(COLOR_MUTED, COLOR_BG);
  gfx().setTextDatum(middle_center);
  snprintf(value, sizeof(value), "X:%s raw:%s  Y:%s raw:%s",
           status.x_limit_active ? "ON" : "OFF",
           status.x_limit_raw_active ? "ON" : "OFF",
           status.y_limit_active ? "ON" : "OFF",
           status.y_limit_raw_active ? "ON" : "OFF");
  gfx().drawString(value, 160, 202);
  gfx().setTextDatum(top_left);
}

void drawUi(const StatusMessage& status) {
  gfx().startWrite();
  gfx().fillScreen(COLOR_BG);
  drawHeader(status);
  switch (current_page) {
    case UiPage::STATUS:
      drawStatusPage(status);
      break;
    case UiPage::CONTROL:
      drawControlPage(status);
      break;
    case UiPage::DETAIL:
      drawDetailPage(status);
      break;
  }
  drawNotice();
  drawFooter();
  gfx().endWrite();
  if (ui_gfx == &ui_canvas) {
    ui_canvas.pushSprite(&M5.Display, 0, 0);
  }
}

void nextPage(int8_t delta) {
  int8_t page = static_cast<int8_t>(current_page) + delta;
  if (page < 0) page = 2;
  if (page > 2) page = 0;
  current_page = static_cast<UiPage>(page);
  force_redraw = true;
}

bool queueCommand(const CommandMessage& command) {
  if (xQueueSend(command_queue, &command, pdMS_TO_TICKS(10)) == pdTRUE) {
    return true;
  }
  setNotice("Queue full");
  return false;
}

void queueSimpleCommand(CommandType type, const char* name) {
  CommandMessage command{};
  command.type = type;
  snprintf(command.name, sizeof(command.name), "%s", name);
  if (queueCommand(command)) setNotice(name);
}

float clampFloat(float value, float lower, float upper) {
  if (value < lower) return lower;
  if (value > upper) return upper;
  return value;
}

void queueJog(const StatusMessage& status, float dx_mm, float dy_mm) {
  const MachineState& state = status.machine;
  if (!canManualMove(state)) {
    setNotice(state.homed ? "Manual locked" : "Home first");
    return;
  }
  if (!ui_have_jog_base ||
      fabsf(state.x_mm - ui_jog_base_x_mm) > UI_JOG_STEP_MM * 2.0f ||
      fabsf(state.y_mm - ui_jog_base_y_mm) > UI_JOG_STEP_MM * 2.0f) {
    ui_jog_base_x_mm = state.x_mm;
    ui_jog_base_y_mm = state.y_mm;
    ui_have_jog_base = true;
  }
  CommandMessage command{};
  command.type = CommandType::XY;
  snprintf(command.name, sizeof(command.name), "UI_JOG");
  command.x_mm = clampFloat(ui_jog_base_x_mm + dx_mm,
                            runtime_config.x_min_mm,
                            runtime_config.x_max_mm);
  command.y_mm = clampFloat(ui_jog_base_y_mm + dy_mm,
                            runtime_config.y_min_mm,
                            runtime_config.y_max_mm);
  command.feed_mm_min = UI_JOG_FEED_MM_MIN;
  if (fabsf(command.x_mm - ui_jog_base_x_mm) < 0.001f &&
      fabsf(command.y_mm - ui_jog_base_y_mm) < 0.001f) {
    setNotice("Soft limit");
    return;
  }
  if (queueCommand(command)) {
    ui_jog_base_x_mm = command.x_mm;
    ui_jog_base_y_mm = command.y_mm;
    setNotice("Jog queued");
  }
}

void handleControlTouch(const StatusMessage& status, int16_t x, int16_t y) {
  const MachineState& state = status.machine;
  if (contains({10, 42, 145, 36}, x, y)) {
    if (state.homing_active) {
      setNotice("Homing now");
    } else {
      queueSimpleCommand(CommandType::HOME, "HOME");
    }
  } else if (contains({165, 42, 145, 36}, x, y)) {
    if (state.alarmed) {
      queueSimpleCommand(CommandType::ALARM_CLEAR, "ALARM_CLEAR");
    } else {
      setNotice("No alarm");
    }
  } else if (contains({119, 86, 82, 34}, x, y)) {
    queueJog(status, 0.0f, UI_JOG_STEP_MM);
  } else if (contains({119, 162, 82, 34}, x, y)) {
    queueJog(status, 0.0f, -UI_JOG_STEP_MM);
  } else if (contains({32, 124, 82, 34}, x, y)) {
    queueJog(status, -UI_JOG_STEP_MM, 0.0f);
  } else if (contains({206, 124, 82, 34}, x, y)) {
    queueJog(status, UI_JOG_STEP_MM, 0.0f);
  } else if (contains({119, 124, 82, 34}, x, y)) {
    queueSimpleCommand(CommandType::ABORT, "ABORT");
  } else if (contains({10, 178, 94, 30}, x, y)) {
    if (canManualMove(state)) {
      queueSimpleCommand(CommandType::PEN_UP, "PENUP");
    } else {
      setNotice(state.homed ? "Manual locked" : "Home first");
    }
  } else if (contains({216, 178, 94, 30}, x, y)) {
    if (canManualMove(state)) {
      queueSimpleCommand(CommandType::PEN_DOWN, "PENDOWN");
    } else {
      setNotice(state.homed ? "Manual locked" : "Home first");
    }
  }
}

void handleTouch(const StatusMessage& status) {
  if (M5.BtnA.wasClicked()) nextPage(-1);
  if (M5.BtnC.wasClicked()) nextPage(1);

  if (M5.Touch.getCount() == 0) return;
  const auto touch = M5.Touch.getDetail();
  if (touch.wasFlicked() && abs(touch.distanceX()) > 55 &&
      abs(touch.distanceX()) > abs(touch.distanceY())) {
    nextPage(touch.distanceX() < 0 ? 1 : -1);
    return;
  }
  if (!touch.wasClicked()) return;

  const int16_t x = touch.x;
  const int16_t y = touch.y;
  if (y >= 214) {
    if (x < 107) current_page = UiPage::STATUS;
    else if (x < 214) current_page = UiPage::CONTROL;
    else current_page = UiPage::DETAIL;
    force_redraw = true;
    return;
  }
  if (x < 24 && y > 38 && y < 210) {
    nextPage(-1);
    return;
  }
  if (x > 296 && y > 38 && y < 210) {
    nextPage(1);
    return;
  }
  if (current_page == UiPage::CONTROL) {
    handleControlTouch(status, x, y);
  }
}
}

void uiTask(void*) {
  bool lcd_ready = false;
  if (M5_UI_ENABLED) {
    auto m5_config = M5.config();
    m5_config.internal_spk = false;
    m5_config.internal_mic = false;
    M5.begin(m5_config);
    lcd_ready = M5.Display.begin();
    M5.Display.setRotation(1);
    M5.Display.setTextFont(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_left);
    if (lcd_ready) {
      ui_canvas.setColorDepth(16);
      if (ui_canvas.createSprite(M5.Display.width(), M5.Display.height())) {
        ui_canvas.setTextFont(1);
        ui_canvas.setTextSize(1);
        ui_canvas.setTextDatum(top_left);
        ui_gfx = &ui_canvas;
        logMessage("UI canvas enabled %dx%d",
                   static_cast<int>(M5.Display.width()),
                   static_cast<int>(M5.Display.height()));
      } else {
        logMessage("WARN: UI canvas allocation failed; direct LCD drawing");
      }
    }
  } else {
    logMessage("ERROR: M5 UI disabled by pin configuration");
  }
  neopixel_controller.begin();
  led_pattern_engine.begin(neopixel_controller);

  StatusMessage status;
  StatusMessage displayed_status;
  bool have_status = false;
  bool have_displayed_status = false;
  for (;;) {
    if (M5_UI_ENABLED) M5.update();
    LedCommand led_command;
    while (xQueueReceive(led_command_queue, &led_command, 0) == pdTRUE) {
      led_pattern_engine.applyCommand(led_command);
    }
    led_pattern_engine.tick(millis());

    if (xQueueReceive(status_queue, &status, pdMS_TO_TICKS(20)) == pdTRUE) {
      have_status = true;
    }

    if (lcd_ready && have_status) {
      handleTouch(status);
      const bool changed =
          !have_displayed_status ||
          memcmp(&status, &displayed_status, sizeof(status)) != 0;
      const bool notice_expired =
          ui_notice_until_ms != 0 && millis() > ui_notice_until_ms;
      const bool clock_tick = millis() - last_clock_redraw_ms > 1000;
      if (changed || force_redraw || notice_expired || clock_tick) {
        drawUi(status);
        displayed_status = status;
        have_displayed_status = true;
        force_redraw = false;
        if (notice_expired) ui_notice_until_ms = 0;
        last_clock_redraw_ms = millis();
      }
    }
  }
}
