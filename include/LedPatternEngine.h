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
  void printStatus() const;

 private:
  void render(uint32_t now_ms);
  void renderPacifica(uint32_t now_ms, RgbColor* leds, uint16_t count);
  void renderFire(RgbColor* leds, uint16_t count);
  static const char* patternName(LedPattern pattern);
  static const char* parameterName(LedParameter parameter);

  NeoPixelController* controller_ = nullptr;
  LedAnimationConfig config_;
  LedPattern pattern_ = static_cast<LedPattern>(NEOPIXEL_INITIAL_PATTERN);
  RgbColor solid_;
  uint8_t heat_[NEOPIXEL_LED_COUNT] = {};
  uint32_t last_frame_ms_ = 0;
  bool dirty_ = true;
};
