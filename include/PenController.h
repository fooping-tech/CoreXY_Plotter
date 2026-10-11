#pragma once

class PenController {
 public:
  void begin();
  void penUp();
  void penDown();
  bool isPenDown() const;
  // Diagnostic: drive the servo to an arbitrary angle without changing the
  // pen up/down state.
  void setAngleDeg(unsigned angle_deg);

 private:
  void writeAngle(unsigned angle_deg);
  bool pen_down_ = false;
};
