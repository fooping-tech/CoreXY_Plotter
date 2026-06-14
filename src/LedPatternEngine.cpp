#include "LedPatternEngine.h"
#include <Arduino.h>
#include <FastLED.h>
#include "AppContext.h"
#include "NeoPixelController.h"

void LedPatternEngine::begin(NeoPixelController& controller) {
  controller_ = &controller;
  config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
  pattern_ = static_cast<LedPattern>(NEOPIXEL_INITIAL_PATTERN);
  controller_->setBrightness(config_.brightness);
  controller_->off();
  applyStatusConfig(LedStatus::IDLE, millis());
}

void LedPatternEngine::tick(uint32_t now_ms) {
  if (controller_ == nullptr) return;
  if (auto_status_enabled_ && transient_until_ms_ != 0 &&
      static_cast<int32_t>(now_ms - transient_until_ms_) >= 0) {
    applyStatusConfig(LedStatus::IDLE, now_ms);
  }
  const bool animated =
      pattern_ == LedPattern::PACIFICA || pattern_ == LedPattern::FIRE ||
      pattern_ == LedPattern::BREATH || pattern_ == LedPattern::CHASE ||
      pattern_ == LedPattern::ALERT || pattern_ == LedPattern::SUCCESS;
  if (!dirty_ && !animated) return;
  if (now_ms - last_frame_ms_ < NEOPIXEL_FRAME_INTERVAL_MS) return;
  render(now_ms);
  controller_->show();
  last_frame_ms_ = now_ms;
  dirty_ = false;
}

void LedPatternEngine::applyCommand(const LedCommand& command) {
  if (controller_ == nullptr) {
    logMessage("ERROR: LED controller is not ready");
    return;
  }
  switch (command.type) {
    case LedCommandType::SET_ALL:
      auto_status_enabled_ = false;
      solid_ = {command.r, command.g, command.b};
      pattern_ = LedPattern::SOLID;
      logMessage("OK: LED r=%u g=%u b=%u", command.r, command.g, command.b);
      break;
    case LedCommandType::SET_PIXEL:
      auto_status_enabled_ = false;
      pattern_ = LedPattern::MANUAL;
      if (!controller_->setPixelRgb(command.index, command.r, command.g,
                                    command.b)) {
        logMessage("ERROR: LED_PIXEL index %u out of range", command.index);
        return;
      }
      logMessage("OK: LED_PIXEL index=%u r=%u g=%u b=%u", command.index,
                 command.r, command.g, command.b);
      dirty_ = true;
      return;
    case LedCommandType::OFF:
      auto_status_enabled_ = false;
      pattern_ = LedPattern::OFF;
      logMessage("OK: LED_OFF");
      break;
    case LedCommandType::SET_PATTERN:
      auto_status_enabled_ = false;
      pattern_ = command.pattern;
      logMessage("OK: LED_PATTERN %s", patternName(pattern_));
      break;
    case LedCommandType::SET_BRIGHTNESS:
      config_.brightness = command.value > NEOPIXEL_BRIGHTNESS_MAX
                               ? NEOPIXEL_BRIGHTNESS_MAX
                               : command.value;
      controller_->setBrightness(config_.brightness);
      logMessage("OK: LED_BRIGHTNESS %u", config_.brightness);
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
      logMessage("OK: LED_PARAM %s %u", parameterName(command.parameter),
                 command.value);
      break;
    case LedCommandType::SET_AUTO:
      auto_status_enabled_ = command.value != 0;
      if (auto_status_enabled_) {
        applyStatusConfig(status_, millis());
      }
      logMessage("OK: LED_AUTO %u", auto_status_enabled_ ? 1 : 0);
      break;
    case LedCommandType::SET_STATUS:
      if (command.value != 0) {
        auto_status_enabled_ = true;
        applyStatusConfig(command.status, millis());
      } else {
        setStatus(command.status);
      }
      logMessage("OK: LED_STATUS_SET %s", statusName(command.status));
      return;
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
    case LedPattern::BREATH:
      renderBreath(now_ms, leds, count);
      break;
    case LedPattern::CHASE:
      renderChase(now_ms, leds, count);
      break;
    case LedPattern::PROGRESS:
      renderProgress(leds, count);
      break;
    case LedPattern::ALERT:
      renderAlert(now_ms, leds, count);
      break;
    case LedPattern::SUCCESS:
      renderSuccess(now_ms, leds, count);
      break;
  }
}

void LedPatternEngine::setStatus(LedStatus status) {
  if (!auto_status_enabled_) return;
  const uint32_t now_ms = millis();
  if (!shouldApplyStatus(status, now_ms)) return;
  applyStatusConfig(status, now_ms);
}

void LedPatternEngine::setProgress(uint8_t percent) {
  progress_percent_ = percent > 100 ? 100 : percent;
  dirty_ = true;
}

void LedPatternEngine::applyStatusConfig(LedStatus status, uint32_t now_ms) {
  status_ = status;
  status_started_ms_ = now_ms;
  transient_until_ms_ = 0;
  switch (status) {
    case LedStatus::IDLE:
      pattern_ = LedPattern::PACIFICA;
      active_color_ = {0, 90, 120};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT > 16
                                ? 16
                                : NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.hue = 176;
      config_.saturation = 160;
      config_.speed = 42;
      config_.intensity = 120;
      break;
    case LedStatus::HOMING:
      pattern_ = LedPattern::CHASE;
      active_color_ = {255, 180, 0};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 140;
      break;
    case LedStatus::DRAWING_PEN_UP:
      pattern_ = LedPattern::CHASE;
      active_color_ = {0, 80, 255};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 120;
      break;
    case LedStatus::DRAWING_PEN_DOWN:
      pattern_ = LedPattern::CHASE;
      active_color_ = {0, 220, 80};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 150;
      break;
    case LedStatus::PROCESSING:
      pattern_ = LedPattern::PACIFICA;
      active_color_ = {120, 40, 180};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.hue = 184;
      config_.speed = 96;
      config_.intensity = 170;
      break;
    case LedStatus::PAUSED:
      pattern_ = LedPattern::ALERT;
      active_color_ = {255, 190, 0};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 56;
      break;
    case LedStatus::COMPLETED:
      pattern_ = LedPattern::SUCCESS;
      active_color_ = {0, 255, 80};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 150;
      transient_until_ms_ = now_ms + 2500U;
      break;
    case LedStatus::WARNING:
      pattern_ = LedPattern::ALERT;
      active_color_ = {255, 80, 0};
      config_.brightness = NEOPIXEL_BRIGHTNESS_DEFAULT;
      config_.speed = 90;
      transient_until_ms_ = now_ms + 2500U;
      break;
    case LedStatus::ERROR:
      pattern_ = LedPattern::ALERT;
      active_color_ = {255, 0, 0};
      config_.brightness = NEOPIXEL_BRIGHTNESS_MAX;
      config_.speed = 190;
      break;
  }
  controller_->setBrightness(config_.brightness);
  dirty_ = true;
}

uint8_t LedPatternEngine::statusPriority(LedStatus status) {
  switch (status) {
    case LedStatus::ERROR: return 9;
    case LedStatus::WARNING: return 8;
    case LedStatus::PAUSED: return 7;
    case LedStatus::HOMING: return 6;
    case LedStatus::DRAWING_PEN_DOWN: return 5;
    case LedStatus::DRAWING_PEN_UP: return 4;
    case LedStatus::PROCESSING: return 3;
    case LedStatus::COMPLETED: return 2;
    case LedStatus::IDLE: return 1;
  }
  return 0;
}

bool LedPatternEngine::shouldApplyStatus(LedStatus status,
                                         uint32_t now_ms) const {
  if (status == status_ && transient_until_ms_ == 0) return false;
  if (transient_until_ms_ != 0 &&
      static_cast<int32_t>(now_ms - transient_until_ms_) < 0 &&
      statusPriority(status) < statusPriority(status_)) {
    return false;
  }
  return true;
}

RgbColor LedPatternEngine::scaleColor(RgbColor color, uint8_t scale) {
  return {scale8(color.r, scale), scale8(color.g, scale),
          scale8(color.b, scale)};
}

void LedPatternEngine::renderBreath(uint32_t now_ms, RgbColor* leds,
                                    uint16_t count) {
  const uint16_t phase =
      static_cast<uint16_t>(now_ms * (static_cast<uint16_t>(config_.speed) + 8U) /
                            24U);
  const uint8_t wave = sin8(static_cast<uint8_t>(phase));
  const uint8_t level = 24 + scale8(wave, 96);
  const RgbColor color = scaleColor(active_color_, level);
  for (uint16_t i = 0; i < count; ++i) leds[i] = color;
}

void LedPatternEngine::renderChase(uint32_t now_ms, RgbColor* leds,
                                   uint16_t count) {
  if (count == 0) return;
  for (uint16_t i = 0; i < count; ++i) leds[i] = {0, 0, 0};
  const uint16_t step_ms =
      360U - (static_cast<uint32_t>(config_.speed) * 280U / 255U);
  const uint16_t head = (now_ms / (step_ms == 0 ? 1 : step_ms)) % count;
  for (uint16_t tail = 0; tail < 4 && tail < count; ++tail) {
    const uint16_t index = (head + count - tail) % count;
    const uint8_t level = tail == 0 ? 255 : tail == 1 ? 120 : tail == 2 ? 56 : 24;
    leds[index] = scaleColor(active_color_, level);
  }
}

void LedPatternEngine::renderProgress(RgbColor* leds, uint16_t count) {
  const uint16_t lit =
      (static_cast<uint32_t>(count) * progress_percent_ + 99U) / 100U;
  for (uint16_t i = 0; i < count; ++i) {
    leds[i] = i < lit ? active_color_ : scaleColor(active_color_, 12);
  }
}

void LedPatternEngine::renderAlert(uint32_t now_ms, RgbColor* leds,
                                   uint16_t count) {
  const uint16_t period_ms =
      900U - (static_cast<uint32_t>(config_.speed) * 700U / 255U);
  const bool on = ((now_ms / (period_ms == 0 ? 1 : period_ms)) & 1U) == 0;
  const RgbColor color = on ? active_color_ : scaleColor(active_color_, 18);
  for (uint16_t i = 0; i < count; ++i) leds[i] = color;
}

void LedPatternEngine::renderSuccess(uint32_t now_ms, RgbColor* leds,
                                     uint16_t count) {
  if (count == 0) return;
  const uint32_t elapsed_ms = now_ms - status_started_ms_;
  const uint16_t sweep_ms = 70U + (255U - config_.speed) / 2U;
  const uint16_t head = (elapsed_ms / sweep_ms) % count;
  const uint8_t fade = elapsed_ms > 1800U ? 96 : 255;
  for (uint16_t i = 0; i < count; ++i) {
    const uint16_t distance = (i + count - head) % count;
    const uint8_t level = distance == 0 ? fade : distance == 1 ? scale8(fade, 96)
                                                              : scale8(fade, 24);
    leds[i] = scaleColor(active_color_, level);
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
    case LedPattern::BREATH: return "BREATH";
    case LedPattern::CHASE: return "CHASE";
    case LedPattern::PROGRESS: return "PROGRESS";
    case LedPattern::ALERT: return "ALERT";
    case LedPattern::SUCCESS: return "SUCCESS";
  }
  return "UNKNOWN";
}

const char* LedPatternEngine::parameterName(LedParameter parameter) {
  switch (parameter) {
    case LedParameter::BRIGHTNESS: return "BRIGHTNESS";
    case LedParameter::HUE: return "HUE";
    case LedParameter::SATURATION: return "SATURATION";
    case LedParameter::SPEED: return "SPEED";
    case LedParameter::INTENSITY: return "INTENSITY";
    case LedParameter::COOLING: return "COOLING";
    case LedParameter::SPARKING: return "SPARKING";
  }
  return "UNKNOWN";
}

const char* LedPatternEngine::statusName(LedStatus status) {
  switch (status) {
    case LedStatus::IDLE: return "IDLE";
    case LedStatus::HOMING: return "HOMING";
    case LedStatus::DRAWING_PEN_UP: return "DRAWING_PEN_UP";
    case LedStatus::DRAWING_PEN_DOWN: return "DRAWING_PEN_DOWN";
    case LedStatus::PROCESSING: return "PROCESSING";
    case LedStatus::PAUSED: return "PAUSED";
    case LedStatus::COMPLETED: return "COMPLETED";
    case LedStatus::WARNING: return "WARNING";
    case LedStatus::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

void LedPatternEngine::printStatus() const {
  logMessage("LED_STATUS mode=%s status=%s pattern=%s count=%u brightness=%u hue=%u saturation=%u speed=%u intensity=%u cooling=%u sparking=%u progress=%u color=%u,%u,%u",
             auto_status_enabled_ ? "AUTO" : "MANUAL", statusName(status_),
             patternName(pattern_), NEOPIXEL_LED_COUNT, config_.brightness,
             config_.hue, config_.saturation, config_.speed, config_.intensity,
             config_.cooling, config_.sparking, progress_percent_,
             active_color_.r, active_color_.g, active_color_.b);
}
