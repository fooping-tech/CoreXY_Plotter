#include "NeoPixelController.h"
#include <FastLED.h>
#include "Core2PinMap.h"

namespace {
CRGB fastled_pixels[NEOPIXEL_LED_COUNT];
}

void NeoPixelController::begin() {
  FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(fastled_pixels,
                                              NEOPIXEL_LED_COUNT);
  FastLED.setBrightness(brightness_);
  off();
}

void NeoPixelController::setAllRgb(uint8_t r, uint8_t g, uint8_t b) {
  for (auto& pixel : pixels_) pixel = {r, g, b};
}

bool NeoPixelController::setPixelRgb(uint16_t index, uint8_t r, uint8_t g,
                                     uint8_t b) {
  if (index >= NEOPIXEL_LED_COUNT) return false;
  pixels_[index] = {r, g, b};
  return true;
}

void NeoPixelController::setBrightness(uint8_t brightness) {
  brightness_ = brightness > NEOPIXEL_BRIGHTNESS_MAX
                    ? NEOPIXEL_BRIGHTNESS_MAX
                    : brightness;
  FastLED.setBrightness(brightness_);
}

void NeoPixelController::show() {
  for (uint16_t i = 0; i < NEOPIXEL_LED_COUNT; ++i) {
    fastled_pixels[i] = CRGB(pixels_[i].r, pixels_[i].g, pixels_[i].b);
  }
  FastLED.show();
}

void NeoPixelController::off() {
  setAllRgb(0, 0, 0);
  show();
}

RgbColor* NeoPixelController::pixels() { return pixels_; }
uint16_t NeoPixelController::count() const { return NEOPIXEL_LED_COUNT; }
