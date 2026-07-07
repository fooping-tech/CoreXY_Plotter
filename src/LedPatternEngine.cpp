#include "LedPatternEngine.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#include "LedRenderer.h"
#include "LedStatusConfig.h"
#include "NeoPixelController.h"

void LedPatternEngine::respond(const char* format, ...) const {
  if (responder_ == nullptr) return;
  char text[160] = {};
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  responder_(text);
}

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
    respond("ERROR: LED controller is not ready");
    return;
  }
  switch (command.type) {
    case LedCommandType::SET_ALL:
      auto_status_enabled_ = false;
      solid_ = {command.r, command.g, command.b};
      pattern_ = LedPattern::SOLID;
      respond("OK: LED r=%u g=%u b=%u", command.r, command.g, command.b);
      break;
    case LedCommandType::SET_PIXEL:
      auto_status_enabled_ = false;
      pattern_ = LedPattern::MANUAL;
      if (!controller_->setPixelRgb(command.index, command.r, command.g,
                                    command.b)) {
        respond("ERROR: LED_PIXEL index %u out of range", command.index);
        return;
      }
      respond("OK: LED_PIXEL index=%u r=%u g=%u b=%u", command.index,
              command.r, command.g, command.b);
      dirty_ = true;
      return;
    case LedCommandType::OFF:
      auto_status_enabled_ = false;
      pattern_ = LedPattern::OFF;
      respond("OK: LED_OFF");
      break;
    case LedCommandType::SET_PATTERN:
      auto_status_enabled_ = false;
      pattern_ = command.pattern;
      respond("OK: LED_PATTERN %s", patternName(pattern_));
      break;
    case LedCommandType::SET_BRIGHTNESS:
      config_.brightness = command.value > NEOPIXEL_BRIGHTNESS_MAX
                               ? NEOPIXEL_BRIGHTNESS_MAX
                               : command.value;
      controller_->setBrightness(config_.brightness);
      respond("OK: LED_BRIGHTNESS %u", config_.brightness);
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
      respond("OK: LED_PARAM %s %u", parameterName(command.parameter),
              command.value);
      break;
    case LedCommandType::SET_AUTO:
      auto_status_enabled_ = command.value != 0;
      if (auto_status_enabled_) {
        applyStatusConfig(status_, millis());
      }
      respond("OK: LED_AUTO %u", auto_status_enabled_ ? 1 : 0);
      break;
    case LedCommandType::SET_STATUS:
      if (command.value != 0) {
        auto_status_enabled_ = true;
        applyStatusConfig(command.status, millis());
      } else {
        setStatus(command.status);
      }
      respond("OK: LED_STATUS_SET %s", statusName(command.status));
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
      LedRenderer::renderPacifica(now_ms, config_, leds, count);
      break;
    case LedPattern::FIRE:
      LedRenderer::renderFire(config_, heat_, leds, count);
      break;
    case LedPattern::BREATH:
      LedRenderer::renderBreath(now_ms, config_, active_color_, leds, count);
      break;
    case LedPattern::CHASE:
      LedRenderer::renderChase(now_ms, config_, active_color_, leds, count);
      break;
    case LedPattern::PROGRESS:
      LedRenderer::renderProgress(active_color_, progress_percent_, leds,
                                  count);
      break;
    case LedPattern::ALERT:
      LedRenderer::renderAlert(now_ms, config_, active_color_, leds, count);
      break;
    case LedPattern::SUCCESS:
      LedRenderer::renderSuccess(now_ms - status_started_ms_, config_,
                                 active_color_, leds, count);
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
  for (const LedStatusVisual& visual : LED_STATUS_VISUALS) {
    if (visual.status != status) continue;
    pattern_ = visual.pattern;
    active_color_ = visual.color;
    config_.brightness = visual.brightness;
    config_.speed = visual.speed;
    if (visual.set_hue) config_.hue = visual.hue;
    if (visual.set_saturation) config_.saturation = visual.saturation;
    if (visual.set_intensity) config_.intensity = visual.intensity;
    if (visual.transient_ms != 0) {
      transient_until_ms_ = now_ms + visual.transient_ms;
    }
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
  respond("LED_STATUS mode=%s status=%s pattern=%s count=%u brightness=%u hue=%u saturation=%u speed=%u intensity=%u cooling=%u sparking=%u progress=%u color=%u,%u,%u",
          auto_status_enabled_ ? "AUTO" : "MANUAL", statusName(status_),
          patternName(pattern_), NEOPIXEL_LED_COUNT, config_.brightness,
          config_.hue, config_.saturation, config_.speed, config_.intensity,
          config_.cooling, config_.sparking, progress_percent_,
          active_color_.r, active_color_.g, active_color_.b);
}
