#pragma once

#include <stdint.h>
#include "LedTypes.h"
#include "PlotterConfig.h"

class NeoPixelController {
 public:
  void begin();
  void setAllRgb(uint8_t r, uint8_t g, uint8_t b);
  bool setPixelRgb(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
  void setBrightness(uint8_t brightness);
  void show();
  void off();
  RgbColor* pixels();
  uint16_t count() const;

 private:
  RgbColor pixels_[NEOPIXEL_LED_COUNT];
  uint8_t brightness_ = NEOPIXEL_BRIGHTNESS_DEFAULT;
};
