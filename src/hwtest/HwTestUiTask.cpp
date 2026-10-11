// Core 0: LCD status, Core2 touch buttons, NEOPIXEL test and AXP192 power
// readout. Never touches STEP/DIR, TMC UART or the servo.

#include <Arduino.h>
#include <M5Unified.h>
#include <string.h>
#include "Core2PinMap.h"
#include "NeoPixelController.h"
#include "PlotterConfig.h"
#include "hwtest/HwTestConfig.h"
#include "hwtest/HwTestShared.h"

extern NeoPixelController neopixel_controller;

namespace {
constexpr uint16_t COLOR_BG = 0x0841;
constexpr uint16_t COLOR_TEXT = TFT_WHITE;
constexpr uint16_t COLOR_MUTED = 0x9CF3;
constexpr uint16_t COLOR_GREEN = 0x37E6;
constexpr uint16_t COLOR_YELLOW = 0xFEA0;
constexpr uint16_t COLOR_RED = 0xF946;

struct LedRequestState {
  volatile HwLedRequest request = HwLedRequest::NONE;
  volatile uint8_t r = 0;
  volatile uint8_t g = 0;
  volatile uint8_t b = 0;
};

LedRequestState led_request;
volatile bool power_report_requested = false;

M5Canvas canvas(&M5.Display);
LovyanGFX* gfx = &M5.Display;

// LED_TEST sequence: solid R, G, B, W, then one white pixel walking the chain.
constexpr uint8_t LED_SOLID_STEPS = 4;
bool led_test_active = false;
uint16_t led_test_step = 0;
uint32_t led_test_step_ms = 0;
char led_state_text[24] = "off";

void showSolid(uint8_t r, uint8_t g, uint8_t b) {
  neopixel_controller.setAllRgb(r, g, b);
  neopixel_controller.show();
}

void applyLedTestStep() {
  constexpr uint8_t L = HWTEST_LED_TEST_LEVEL;
  static const char* solid_names[LED_SOLID_STEPS] = {"RED", "GREEN", "BLUE",
                                                     "WHITE"};
  if (led_test_step < LED_SOLID_STEPS) {
    const uint8_t rgb[LED_SOLID_STEPS][3] = {
        {L, 0, 0}, {0, L, 0}, {0, 0, L}, {L, L, L}};
    showSolid(rgb[led_test_step][0], rgb[led_test_step][1],
              rgb[led_test_step][2]);
    logMessage("LED_TEST all %s", solid_names[led_test_step]);
    snprintf(led_state_text, sizeof(led_state_text), "test %s",
             solid_names[led_test_step]);
    return;
  }
  const uint16_t pixel = led_test_step - LED_SOLID_STEPS;
  neopixel_controller.setAllRgb(0, 0, 0);
  neopixel_controller.setPixelRgb(pixel, L, L, L);
  neopixel_controller.show();
  if (pixel == 0) {
    logMessage("LED_TEST walking pixel 0..%u (count each LED)",
               NEOPIXEL_LED_COUNT - 1);
  }
  snprintf(led_state_text, sizeof(led_state_text), "test px%u", pixel);
}

void tickLed() {
  const HwLedRequest request = led_request.request;
  if (request != HwLedRequest::NONE) {
    led_request.request = HwLedRequest::NONE;
    led_test_active = false;
    switch (request) {
      case HwLedRequest::TEST:
        led_test_active = true;
        led_test_step = 0;
        led_test_step_ms = millis();
        logMessage("LED_TEST start count=%u pin=GPIO%u", NEOPIXEL_LED_COUNT,
                   NEOPIXEL_PIN);
        applyLedTestStep();
        break;
      case HwLedRequest::OFF:
        neopixel_controller.off();
        strcpy(led_state_text, "off");
        logMessage("LED_OFF DONE");
        break;
      case HwLedRequest::RGB:
        showSolid(led_request.r, led_request.g, led_request.b);
        snprintf(led_state_text, sizeof(led_state_text), "%u,%u,%u",
                 led_request.r, led_request.g, led_request.b);
        logMessage("LED %u %u %u DONE", led_request.r, led_request.g,
                   led_request.b);
        break;
      case HwLedRequest::NONE:
        break;
    }
  }
  if (!led_test_active) return;
  const uint32_t step_ms =
      led_test_step < LED_SOLID_STEPS ? HWTEST_LED_COLOR_MS : HWTEST_LED_PIXEL_MS;
  if (millis() - led_test_step_ms < step_ms) return;
  led_test_step_ms = millis();
  ++led_test_step;
  if (led_test_step >= LED_SOLID_STEPS + NEOPIXEL_LED_COUNT) {
    led_test_active = false;
    neopixel_controller.off();
    strcpy(led_state_text, "off");
    logMessage("LED_TEST DONE (confirm colors and LED count visually)");
    return;
  }
  applyLedTestStep();
}

void reportPower() {
  const int16_t vbus_mv = M5.Power.getVBUSVoltage();
  const int16_t bat_mv = M5.Power.getBatteryVoltage();
  const int32_t bat_pct = M5.Power.getBatteryLevel();
  const auto charging = M5.Power.isCharging();
  logMessage("POWER vbus=%dmV battery=%dmV level=%ld%% charging=%s",
             vbus_mv, bat_mv, static_cast<long>(bat_pct),
             charging == m5::Power_Class::is_charging      ? "YES"
             : charging == m5::Power_Class::is_discharging ? "NO"
                                                           : "UNKNOWN");
  logMessage("POWER DONE (TMC VM is not measured by Core2; check with a meter)");
}

const char* tmcText(uint8_t connection, uint8_t version, char* buffer,
                    size_t size) {
  if (connection == 0xFF) return "not tested";
  if (connection != 0) {
    snprintf(buffer, size, "NO REPLY (%u)", connection);
  } else {
    snprintf(buffer, size, "OK ver=0x%02X", version);
  }
  return buffer;
}

uint16_t tmcColor(uint8_t connection, uint8_t version) {
  if (connection == 0xFF) return COLOR_MUTED;
  return connection == 0 && version == HWTEST_TMC2209_VERSION ? COLOR_GREEN
                                                               : COLOR_RED;
}

void drawRow(int16_t y, const char* label, const char* value, uint16_t color) {
  gfx->setTextColor(COLOR_MUTED, COLOR_BG);
  gfx->drawString(label, 8, y);
  gfx->setTextColor(color, COLOR_BG);
  gfx->drawString(value, 96, y);
}

void draw(const HwStatus& s) {
  gfx->fillScreen(COLOR_BG);
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->drawString("PCB HW TEST", 8, 6);

  const char* st = "-";
  uint16_t st_color = COLOR_MUTED;
  switch (s.selftest) {
    case HwSelftestResult::RUNNING: st = "RUN"; st_color = COLOR_YELLOW; break;
    case HwSelftestResult::PASS: st = "PASS"; st_color = COLOR_GREEN; break;
    case HwSelftestResult::FAIL: st = "FAIL"; st_color = COLOR_RED; break;
    case HwSelftestResult::NONE: break;
  }
  gfx->setTextColor(st_color, COLOR_BG);
  gfx->drawString(st, 250, 6);

  gfx->setTextSize(1);
  char buf[48];
  int16_t y = 34;
  constexpr int16_t ROW = 16;
  snprintf(buf, sizeof(buf), "X %s (%s)  Y %s (%s)",
           s.x_limit_active ? "ON " : "off", s.x_limit_raw_high ? "H" : "L",
           s.y_limit_active ? "ON " : "off", s.y_limit_raw_high ? "H" : "L");
  drawRow(y, "LIMIT", buf,
          s.x_limit_active || s.y_limit_active ? COLOR_YELLOW : COLOR_GREEN);
  y += ROW;
  drawRow(y, "TMC A (0)", tmcText(s.tmc_a_connection, s.tmc_a_version, buf, sizeof(buf)),
          tmcColor(s.tmc_a_connection, s.tmc_a_version));
  y += ROW;
  drawRow(y, "TMC B (1)", tmcText(s.tmc_b_connection, s.tmc_b_version, buf, sizeof(buf)),
          tmcColor(s.tmc_b_connection, s.tmc_b_version));
  y += ROW;
  if (s.tmc_configured) {
    snprintf(buf, sizeof(buf), "configured %umA", s.tmc_current_ma);
  } else {
    strcpy(buf, "not configured");
  }
  drawRow(y, "TMC profile", buf, s.tmc_configured ? COLOR_GREEN : COLOR_MUTED);
  y += ROW;
  snprintf(buf, sizeof(buf), "A=%ld B=%ld %s", static_cast<long>(s.motor_a_steps),
           static_cast<long>(s.motor_b_steps), s.motor_running ? "RUN" : "");
  drawRow(y, "MOTOR", buf, s.motor_running ? COLOR_YELLOW : COLOR_TEXT);
  y += ROW;
  drawRow(y, "STEP/DIR", s.pins_released_to_backend ? "backend" : "probe",
          COLOR_TEXT);
  y += ROW;
  snprintf(buf, sizeof(buf), "%d deg", s.servo_angle_deg);
  drawRow(y, "SERVO", buf, COLOR_TEXT);
  y += ROW;
  drawRow(y, "LED", led_state_text, COLOR_TEXT);
  y += ROW;
  snprintf(buf, sizeof(buf), "%dmV", M5.Power.getVBUSVoltage());
  drawRow(y, "VBUS", buf, COLOR_TEXT);
  y += ROW;
  drawRow(y, "EVENT", s.last_event, COLOR_TEXT);

  // Core2 touch buttons A/B/C sit just below the screen.
  gfx->setTextColor(COLOR_MUTED, COLOR_BG);
  gfx->drawString("SELFTEST", 22, 226);
  gfx->drawString("PEN UP/DN", 128, 226);
  gfx->drawString("LED TEST", 238, 226);

  if (gfx != &M5.Display) canvas.pushSprite(0, 0);
}

void handleButtons(const HwStatus& s) {
  if (M5.BtnA.wasClicked()) {
    HwCommand command;
    command.type = HwCommandType::SELFTEST;
    if (!enqueueHwCommand(command)) logMessage("ERROR: command queue full");
  }
  if (M5.BtnB.wasClicked()) {
    HwCommand command;
    command.type = s.servo_angle_deg == PEN_DOWN_ANGLE_DEG
                       ? HwCommandType::PENUP
                       : HwCommandType::PENDOWN;
    if (!enqueueHwCommand(command)) logMessage("ERROR: command queue full");
  }
  if (M5.BtnC.wasClicked()) requestHwLed(HwLedRequest::TEST);
}
}  // namespace

void requestHwLed(HwLedRequest request, uint8_t r, uint8_t g, uint8_t b) {
  led_request.r = r;
  led_request.g = g;
  led_request.b = b;
  led_request.request = request;
}

void requestHwPowerReport() { power_report_requested = true; }

void hwUiTask(void*) {
  auto config = M5.config();
  config.internal_spk = false;
  config.internal_mic = false;
  M5.begin(config);
  M5.Display.setRotation(1);
  M5.Display.setTextFont(1);
  canvas.setColorDepth(16);
  if (canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    canvas.setTextFont(1);
    gfx = &canvas;
  }
  neopixel_controller.begin();
  neopixel_controller.setBrightness(HWTEST_LED_BRIGHTNESS);
  neopixel_controller.off();

  HwStatus status;
  uint32_t last_draw_ms = 0;
  for (;;) {
    M5.update();
    xQueuePeek(hw_status_queue, &status, 0);
    handleButtons(status);
    tickLed();
    if (power_report_requested) {
      power_report_requested = false;
      reportPower();
    }
    if (millis() - last_draw_ms >= HWTEST_UI_REDRAW_MS) {
      last_draw_ms = millis();
      draw(status);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
