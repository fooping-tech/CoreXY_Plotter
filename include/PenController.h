#pragma once

class PenController {
 public:
  void begin();
  void penUp();
  void penDown();
  bool isPenDown() const;

 private:
  void writeAngle(unsigned angle_deg);
  bool pen_down_ = false;
};
