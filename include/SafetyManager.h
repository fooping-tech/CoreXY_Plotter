#pragma once

class SafetyManager {
 public:
  void begin();
  bool validateMove(float target_x_mm, float target_y_mm,
                    float& feed_mm_min) const;
  bool xLimitActive() const;
  bool yLimitActive() const;
  bool isAlarmed() const;
  void setAlarm(bool alarmed);
  void poll();

 private:
  bool alarmed_ = false;
  // Placeholder: add E-stop input and pre-homing movement policy here.
};
