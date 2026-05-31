#include "LedPatternEngine.h"
#include <Arduino.h>
#include <FastLED.h>
#include "AppContext.h"
#include "NeoPixelController.h"

void LedPatternEngine::begin(NeoPixelController& controller) {
  controller_ = &controller;
  config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
  controller_->setBrightness(config_.brightness);
  controller_->off();
}

void LedPatternEngine::tick(uint32_t now_ms) {
  if (controller_ == nullptr) return;
  const bool animated =
      pattern_ == LedPattern::PACIFICA || pattern_ == LedPattern::FIRE;
  if (!dirty_ && !animated) return;
  if (now_ms - last_frame_ms_ < NEOPIXEL_FRAME_INTERVAL_MS) return;
  render(now_ms);
  controller_->show();
  last_frame_ms_ = now_ms;
  dirty_ = false;
}

void LedPatternEngine::applyCommand(const LedCommand& command) {
  if (controller_ == nullptr) return;
  switch (command.type) {
    case LedCommandType::SET_ALL:
      solid_ = {command.r, command.g, command.b};
      pattern_ = LedPattern::MANUAL;
      break;
    case LedCommandType::SET_PIXEL:
      pattern_ = LedPattern::SOLID;
      if (!controller_->setPixelRgb(command.index, command.r, command.g,
                                    command.b)) {
        logMessage("LED ERROR: index %u out of range", command.index);
      }
      dirty_ = true;
      return;
    case LedCommandType::OFF:
      pattern_ = LedPattern::OFF;
      break;
    case LedCommandType::SET_PATTERN:
      pattern_ = command.pattern;
      break;
    case LedCommandType::SET_BRIGHTNESS:
      config_.brightness = command.value > NEOPIXEL_BRIGHTNESS_MAX
                               ? NEOPIXEL_BRIGHTNESS_MAX
                               : command.value;
      controller_->setBrightness(config_.brightness);
      break;
    case LedCommandType::SET_PARAMETER:
      switch (command.parameter) {
        case LedParameter::BRIGHTNESS:
          config_.brightness = command.value > NEOPIXEL_BRIGHTNESS_MAX
                                   ? NEOPIXEL_BRIGHTNESS_MAX
                                   : command.value;
          controller_->setBrightness(config_.brightness);
          break;
        case LedParameter::HUE: config_.hue = command.value; break;
        case LedParameter::SATURATION: config_.saturation = command.value; break;
        case LedParameter::SPEED: config_.speed = command.value; break;
        case LedParameter::INTENSITY: config_.intensity = command.value; break;
        case LedParameter::COOLING: config_.cooling = command.value; break;
        case LedParameter::SPARKING: config_.sparking = command.value; break;
      }
      break;
    case LedCommandType::STATUS:
      printStatus();
      return;
  }
  dirty_ = true;
}

void LedPatternEngine::render(uint32_t now_ms) {
  RgbColor* leds = controller_->pixels();
  const uint16_t count = controller_->count();
  switch (pattern_) {
    case LedPattern::OFF:
      controller_->setAllRgb(0, 0, 0);
      break;
    case LedPattern::SOLID:
      controller_->setAllRgb(solid_.r, solid_.g, solid_.b);
      break;
    case LedPattern::MANUAL:
      break;
    case LedPattern::PACIFICA:
      renderPacifica(now_ms, leds, count);
      break;
    case LedPattern::FIRE:
      renderFire(leds, count);
      break;
  }
}

void LedPatternEngine::renderPacifica(uint32_t now_ms, RgbColor* leds,
                                      uint16_t count) {
  const CRGBPalette16 palette(CRGB(0, 5, 16), CRGB(0, 24, 80),
                              CRGB(0, 90, 120), CRGB(8, 160, 180));
  const uint16_t phase = static_cast<uint16_t>(now_ms * (config_.speed + 16U) / 32U);
  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t wave_a = sin8(static_cast<uint8_t>(phase / 8U + i * 29U));
    const uint8_t wave_b = sin8(static_cast<uint8_t>(phase / 13U + i * 47U));
    const uint8_t index = scale8(qadd8(wave_a, scale8(wave_b, 120)), 180);
    CRGB color = ColorFromPalette(palette, index, config_.intensity, LINEARBLEND);
    color += CHSV(config_.hue, config_.saturation, scale8(wave_b, 24));
    leds[i] = {color.r, color.g, color.b};
  }
}

void LedPatternEngine::renderFire(RgbColor* leds, uint16_t count) {
  for (uint16_t i = 0; i < count; ++i) {
    heat_[i] = qsub8(heat_[i], random8(0, ((config_.cooling * 10U) / count) + 2));
  }
  for (int i = count - 1; i >= 2; --i) {
    heat_[i] = (heat_[i - 1] + heat_[i - 2] + heat_[i - 2]) / 3;
  }
  if (random8() < config_.sparking) {
    const uint8_t y = random8(count > 7 ? 7 : count);
    heat_[y] = qadd8(heat_[y], random8(160, 255));
  }
  for (uint16_t i = 0; i < count; ++i) {
    const CRGB color = HeatColor(scale8(heat_[i], config_.intensity));
    leds[i] = {color.r, color.g, color.b};
  }
}

const char* LedPatternEngine::patternName(LedPattern pattern) {
  switch (pattern) {
    case LedPattern::OFF: return "OFF";
    case LedPattern::SOLID: return "SOLID";
    case LedPattern::MANUAL: return "MANUAL";
    case LedPattern::PACIFICA: return "PACIFICA";
    case LedPattern::FIRE: return "FIRE";
  }
  return "UNKNOWN";
}

void LedPatternEngine::printStatus() const {
  logMessage("LED_STATUS pattern=%s count=%u brightness=%u hue=%u saturation=%u speed=%u intensity=%u cooling=%u sparking=%u",
             patternName(pattern_), NEOPIXEL_LED_COUNT, config_.brightness,
             config_.hue, config_.saturation, config_.speed, config_.intensity,
             config_.cooling, config_.sparking);
}
