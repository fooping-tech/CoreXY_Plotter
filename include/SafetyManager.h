#pragma once

#include <stdint.h>

class SafetyManager {
 public:
  void begin();
  bool validateMove(float target_x_mm, float target_y_mm,
                    float& feed_mm_min) const;
  bool validateHomingStart() const;
  bool xLimitRawActive() const;
  bool yLimitRawActive() const;
  bool xLimitActive() const;
  bool yLimitActive() const;
  bool isAlarmed() const;
  void setAlarm(bool alarmed);
  void setAlarm(const char* reason);
  void clearAlarm();
  const char* alarmReason() const;
  void setHomingActive(bool active);
  void beginNormalMoveLimitReleaseAllowance(bool allow_x, bool allow_y,
                                            float start_x_mm,
                                            float start_y_mm);
  void clearNormalMoveLimitReleaseAllowance();
  void poll();

 private:
  bool readLimitPin(int pin) const;
  bool updateDebounced(bool raw_active, bool& last_raw, bool& debounced,
                       uint32_t& last_change_ms);

  bool alarmed_ = false;
  bool homing_active_ = false;
  bool x_last_raw_ = false;
  bool y_last_raw_ = false;
  bool x_debounced_ = false;
  bool y_debounced_ = false;
  uint32_t x_last_change_ms_ = 0;
  uint32_t y_last_change_ms_ = 0;
  uint32_t x_unexpected_since_ms_ = 0;
  uint32_t y_unexpected_since_ms_ = 0;
  bool x_release_allowed_ = false;
  bool y_release_allowed_ = false;
  float x_release_start_mm_ = 0.0f;
  float y_release_start_mm_ = 0.0f;
  char alarm_reason_[64] = "none";
};
