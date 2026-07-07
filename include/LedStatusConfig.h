#pragma once

#include <stdint.h>

#include "LedTypes.h"
#include "PlotterConfig.h"

// LedStatusごとの表示演出設定。LedPatternEngineのapplyStatusConfigが参照する。
// hue/saturation/intensityはPACIFICA系statusだけが上書きし、
// それ以外のstatusでは直前値(LED_PARAM設定を含む)を保持する。
struct LedStatusVisual {
  LedStatus status;
  LedPattern pattern;
  RgbColor color;
  uint8_t brightness;
  uint8_t speed;
  bool set_hue;
  uint8_t hue;
  bool set_saturation;
  uint8_t saturation;
  bool set_intensity;
  uint8_t intensity;
  uint32_t transient_ms;  // 0なら恒常表示
};

constexpr uint8_t LED_IDLE_BRIGHTNESS =
    NEOPIXEL_BRIGHTNESS_DEFAULT > NEOPIXEL_IDLE_BRIGHTNESS_MAX
        ? NEOPIXEL_IDLE_BRIGHTNESS_MAX
        : NEOPIXEL_BRIGHTNESS_DEFAULT;

constexpr LedStatusVisual LED_STATUS_VISUALS[] = {
    {LedStatus::IDLE, LedPattern::PACIFICA, RgbColor{0, 90, 120},
     LED_IDLE_BRIGHTNESS, 42, true, 176, true, 160, true, 120, 0},
    {LedStatus::HOMING, LedPattern::CHASE, RgbColor{255, 180, 0},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 140, false, 0, false, 0, false, 0, 0},
    {LedStatus::DRAWING_PEN_UP, LedPattern::CHASE, RgbColor{0, 80, 255},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 120, false, 0, false, 0, false, 0, 0},
    {LedStatus::DRAWING_PEN_DOWN, LedPattern::CHASE, RgbColor{0, 220, 80},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 150, false, 0, false, 0, false, 0, 0},
    {LedStatus::PROCESSING, LedPattern::PACIFICA, RgbColor{120, 40, 180},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 96, true, 184, false, 0, true, 170, 0},
    {LedStatus::PAUSED, LedPattern::ALERT, RgbColor{255, 190, 0},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 56, false, 0, false, 0, false, 0, 0},
    {LedStatus::COMPLETED, LedPattern::SUCCESS, RgbColor{0, 255, 80},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 150, false, 0, false, 0, false, 0,
     NEOPIXEL_TRANSIENT_STATUS_MS},
    {LedStatus::WARNING, LedPattern::ALERT, RgbColor{255, 80, 0},
     NEOPIXEL_BRIGHTNESS_DEFAULT, 90, false, 0, false, 0, false, 0,
     NEOPIXEL_TRANSIENT_STATUS_MS},
    {LedStatus::ERROR, LedPattern::ALERT, RgbColor{255, 0, 0},
     NEOPIXEL_BRIGHTNESS_MAX, 190, false, 0, false, 0, false, 0, 0},
};
