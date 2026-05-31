#include "SafetyManager.h"
#include <Arduino.h>
#include "AppContext.h"
#include "Core2PinMap.h"
#include "PlotterConfig.h"

void SafetyManager::begin() {
  pinMode(X_LIMIT_PIN, INPUT);
  pinMode(Y_LIMIT_PIN, INPUT);
}

bool SafetyManager::validateMove(float target_x_mm, float target_y_mm,
                                 float& feed_mm_min) const {
  if (alarmed_) {
    logMessage("REJECT: machine is alarmed");
    return false;
  }
  if (target_x_mm < X_MIN_MM || target_x_mm > X_MAX_MM) {
    logMessage("REJECT: X %.3f outside [%.3f, %.3f]", target_x_mm, X_MIN_MM,
               X_MAX_MM);
    return false;
  }
  if (target_y_mm < Y_MIN_MM || target_y_mm > Y_MAX_MM) {
    logMessage("REJECT: Y %.3f outside [%.3f, %.3f]", target_y_mm, Y_MIN_MM,
               Y_MAX_MM);
    return false;
  }
  if (feed_mm_min <= 0.0f) {
    logMessage("REJECT: feed must be > 0");
    return false;
  }
  if (feed_mm_min > MAX_FEED_MM_MIN) {
    logMessage("CLAMP: feed %.3f -> %.3f", feed_mm_min, MAX_FEED_MM_MIN);
    feed_mm_min = MAX_FEED_MM_MIN;
  }
  return true;
}

bool SafetyManager::xLimitActive() const { return digitalRead(X_LIMIT_PIN) == LOW; }
bool SafetyManager::yLimitActive() const { return digitalRead(Y_LIMIT_PIN) == LOW; }
bool SafetyManager::isAlarmed() const { return alarmed_; }
void SafetyManager::setAlarm(bool alarmed) { alarmed_ = alarmed; }

void SafetyManager::poll() {
  // Placeholder: future hard-limit and E-stop policies set alarm here.
}
