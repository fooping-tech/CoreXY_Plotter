#include "SafetyManager.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "AppContext.h"
#include "Core2PinMap.h"
#include "PlotterConfig.h"

void SafetyManager::begin() {
  pinMode(X_LIMIT_PIN, INPUT);
  pinMode(Y_LIMIT_PIN, INPUT);
  x_last_raw_ = readLimitPin(X_LIMIT_PIN);
  y_last_raw_ = readLimitPin(Y_LIMIT_PIN);
  x_debounced_ = x_last_raw_;
  y_debounced_ = y_last_raw_;
  x_last_change_ms_ = millis();
  y_last_change_ms_ = millis();
}

bool SafetyManager::validateMove(float target_x_mm, float target_y_mm,
                                 float& feed_mm_min) const {
  if (alarmed_) {
    logMessage("REJECT: machine is alarmed reason=%s", alarm_reason_);
    return false;
  }
  const bool has_homed_motion_reference =
      machine_state.homed || job_controller.hasHomedJobMotionGrant();
  if (runtime_config.homing_require_homed_for_xy_move &&
      !has_homed_motion_reference) {
    logMessage("REJECT: machine is not homed");
    return false;
  }
  const float dx_mm = target_x_mm - machine_state.x_mm;
  const float dy_mm = target_y_mm - machine_state.y_mm;
  if (x_debounced_ && dx_mm * static_cast<float>(runtime_config.homing_x_dir) > 0.0f) {
    logMessage("REJECT: X limit active and move pushes toward limit");
    return false;
  }
  if (y_debounced_ && dy_mm * static_cast<float>(runtime_config.homing_y_dir) > 0.0f) {
    logMessage("REJECT: Y limit active and move pushes toward limit");
    return false;
  }
  if (target_x_mm < runtime_config.x_min_mm ||
      target_x_mm > runtime_config.x_max_mm) {
    logMessage("REJECT: X %.3f outside [%.3f, %.3f]", target_x_mm,
               runtime_config.x_min_mm, runtime_config.x_max_mm);
    return false;
  }
  if (target_y_mm < runtime_config.y_min_mm ||
      target_y_mm > runtime_config.y_max_mm) {
    logMessage("REJECT: Y %.3f outside [%.3f, %.3f]", target_y_mm,
               runtime_config.y_min_mm, runtime_config.y_max_mm);
    return false;
  }
  if (feed_mm_min <= 0.0f) {
    logMessage("REJECT: feed must be > 0");
    return false;
  }
  if (feed_mm_min > runtime_config.max_feed_mm_min) {
    logMessage("CLAMP: feed %.3f -> %.3f", feed_mm_min,
               runtime_config.max_feed_mm_min);
    feed_mm_min = runtime_config.max_feed_mm_min;
  }
  return true;
}

bool SafetyManager::validateHomingStart() const {
  if (!runtime_config.homing_enabled) {
    logMessage("REJECT: homing disabled by config");
    return false;
  }
  if (alarmed_) {
    logMessage("REJECT: machine is alarmed reason=%s", alarm_reason_);
    return false;
  }
  return true;
}

bool SafetyManager::readLimitPin(int pin) const {
  const int level = digitalRead(pin);
  return LIMIT_ACTIVE_LOW ? level == LOW : level == HIGH;
}

bool SafetyManager::xLimitRawActive() const { return readLimitPin(X_LIMIT_PIN); }
bool SafetyManager::yLimitRawActive() const { return readLimitPin(Y_LIMIT_PIN); }
bool SafetyManager::xLimitActive() const { return x_debounced_; }
bool SafetyManager::yLimitActive() const { return y_debounced_; }
bool SafetyManager::isAlarmed() const { return alarmed_; }
void SafetyManager::setAlarm(bool alarmed) {
  alarmed_ = alarmed;
  if (!alarmed_) {
    strncpy(alarm_reason_, "none", sizeof(alarm_reason_) - 1);
    alarm_reason_[sizeof(alarm_reason_) - 1] = '\0';
  }
}

void SafetyManager::setAlarm(const char* reason) {
  alarmed_ = true;
  strncpy(alarm_reason_, reason, sizeof(alarm_reason_) - 1);
  alarm_reason_[sizeof(alarm_reason_) - 1] = '\0';
}

void SafetyManager::clearAlarm() { setAlarm(false); }
const char* SafetyManager::alarmReason() const { return alarm_reason_; }
void SafetyManager::setHomingActive(bool active) { homing_active_ = active; }
void SafetyManager::beginNormalMoveLimitReleaseAllowance(bool allow_x,
                                                         bool allow_y,
                                                         float start_x_mm,
                                                         float start_y_mm) {
  x_release_allowed_ = allow_x;
  y_release_allowed_ = allow_y;
  x_release_start_mm_ = start_x_mm;
  y_release_start_mm_ = start_y_mm;
}

void SafetyManager::clearNormalMoveLimitReleaseAllowance() {
  x_release_allowed_ = false;
  y_release_allowed_ = false;
}

bool SafetyManager::updateDebounced(bool raw_active, bool& last_raw,
                                    bool& debounced,
                                    uint32_t& last_change_ms) {
  const uint32_t now_ms = millis();
  if (raw_active != last_raw) {
    last_raw = raw_active;
    last_change_ms = now_ms;
  }
  if (now_ms - last_change_ms >= runtime_config.homing_limit_debounce_ms) {
    debounced = raw_active;
  }
  return debounced;
}

void SafetyManager::poll() {
  updateDebounced(xLimitRawActive(), x_last_raw_, x_debounced_,
                  x_last_change_ms_);
  updateDebounced(yLimitRawActive(), y_last_raw_, y_debounced_,
                  y_last_change_ms_);
  if (x_release_allowed_ && !x_debounced_ && !xLimitRawActive()) {
    x_release_allowed_ = false;
    x_unexpected_since_ms_ = 0;
  }
  if (y_release_allowed_ && !y_debounced_ && !yLimitRawActive()) {
    y_release_allowed_ = false;
    y_unexpected_since_ms_ = 0;
  }
  if (!homing_active_ && machine_state.homed) {
    const uint32_t now_ms = millis();
    if (x_release_allowed_ && x_debounced_ &&
        fabsf(machine_state.x_mm - x_release_start_mm_) >=
            runtime_config.normal_move_limit_release_mm) {
      setAlarm("X home limit did not release");
      return;
    }
    if (y_release_allowed_ && y_debounced_ &&
        fabsf(machine_state.y_mm - y_release_start_mm_) >=
            runtime_config.normal_move_limit_release_mm) {
      setAlarm("Y home limit did not release");
      return;
    }
    const bool x_unexpected =
        x_debounced_ && !x_release_allowed_ &&
        ((runtime_config.homing_x_dir < 0 &&
          machine_state.x_mm > runtime_config.homing_set_x_mm + 0.5f) ||
         (runtime_config.homing_x_dir > 0 &&
          machine_state.x_mm < runtime_config.homing_set_x_mm - 0.5f));
    const bool y_unexpected =
        y_debounced_ && !y_release_allowed_ &&
        ((runtime_config.homing_y_dir < 0 &&
          machine_state.y_mm > runtime_config.homing_set_y_mm + 0.5f) ||
         (runtime_config.homing_y_dir > 0 &&
          machine_state.y_mm < runtime_config.homing_set_y_mm - 0.5f));
    if (x_unexpected && x_unexpected_since_ms_ == 0) {
      x_unexpected_since_ms_ = now_ms;
    } else if (!x_unexpected) {
      x_unexpected_since_ms_ = 0;
    }
    if (y_unexpected && y_unexpected_since_ms_ == 0) {
      y_unexpected_since_ms_ = now_ms;
    } else if (!y_unexpected) {
      y_unexpected_since_ms_ = 0;
    }
    const bool x_unexpected_persistent =
        x_unexpected_since_ms_ != 0 &&
        now_ms - x_unexpected_since_ms_ >=
            runtime_config.hard_limit_unexpected_alarm_ms;
    const bool y_unexpected_persistent =
        y_unexpected_since_ms_ != 0 &&
        now_ms - y_unexpected_since_ms_ >=
            runtime_config.hard_limit_unexpected_alarm_ms;
    if (x_unexpected_persistent || y_unexpected_persistent) {
      char reason[96];
      snprintf(reason, sizeof(reason),
               "hard limit active away from home x=%s y=%s x_raw=%s y_raw=%s",
               x_debounced_ ? "ON" : "OFF", y_debounced_ ? "ON" : "OFF",
               xLimitRawActive() ? "ON" : "OFF",
               yLimitRawActive() ? "ON" : "OFF");
      setAlarm(reason);
    }
  } else {
    x_unexpected_since_ms_ = 0;
    y_unexpected_since_ms_ = 0;
  }
}
