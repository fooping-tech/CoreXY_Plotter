#pragma once

#include <stdint.h>
#include "LedTypes.h"
#include "PlotterConfig.h"

class NeoPixelController;

class LedPatternEngine {
 public:
  void begin(NeoPixelController& controller);
  void tick(uint32_t now_ms);
  void applyCommand(const LedCommand& command);
  void setStatus(LedStatus status);
  void setProgress(uint8_t percent);
  void printStatus() const;

 private:
  void render(uint32_t now_ms);
  void renderBreath(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderChase(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderProgress(RgbColor* leds, uint16_t count);
  void renderAlert(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderSuccess(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderPacifica(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderFire(RgbColor* leds, uint16_t count);
  void applyStatusConfig(LedStatus status);
  static const char* patternName(LedPattern pattern);
  static const char* parameterName(LedParameter parameter);
  static const char* statusName(LedStatus status);
  static RgbColor scaleColor(RgbColor color, uint8_t scale);

  NeoPixelController* controller_ = nullptr;
  LedAnimationConfig config_;
  LedPattern pattern_ = static_cast<LedPattern>(NEOPIXEL_INITIAL_PATTERN);
  LedStatus status_ = LedStatus::IDLE;
  RgbColor solid_;
  RgbColor active_color_;
  uint8_t heat_[NEOPIXEL_LED_COUNT] = {};
  uint8_t progress_percent_ = 0;
  uint32_t last_frame_ms_ = 0;
  uint32_t status_started_ms_ = 0;
  bool dirty_ = true;
  bool auto_status_enabled_ = true;
};
