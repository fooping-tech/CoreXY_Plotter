#pragma once

#include <stdint.h>

struct RgbColor {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  RgbColor() = default;
  RgbColor(uint8_t red, uint8_t green, uint8_t blue)
      : r(red), g(green), b(blue) {}
};

enum class LedPattern : uint8_t {
  OFF,
  SOLID,
  MANUAL,
  PACIFICA,
  FIRE,
};

enum class LedParameter : uint8_t {
  BRIGHTNESS,
  HUE,
  SATURATION,
  SPEED,
  INTENSITY,
  COOLING,
  SPARKING,
};

enum class LedCommandType : uint8_t {
  SET_ALL,
  SET_PIXEL,
  OFF,
  SET_PATTERN,
  SET_BRIGHTNESS,
  SET_PARAMETER,
  STATUS,
};

struct LedAnimationConfig {
  uint8_t brightness = 24;
  uint8_t hue = 128;
  uint8_t saturation = 255;
  uint8_t speed = 128;
  uint8_t intensity = 160;
  uint8_t cooling = 55;
  uint8_t sparking = 120;
};

struct LedCommand {
  LedCommandType type = LedCommandType::OFF;
  LedPattern pattern = LedPattern::OFF;
  LedParameter parameter = LedParameter::BRIGHTNESS;
  uint16_t index = 0;
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t value = 0;
};
