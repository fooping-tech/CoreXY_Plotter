#pragma once

#include <stdint.h>

struct CoreXYDelta {
  float dx_mm;
  float dy_mm;
  int32_t a_steps;
  int32_t b_steps;
};

class CoreXYKinematics {
 public:
  static CoreXYDelta xyDeltaToABSteps(float dx_mm, float dy_mm,
                                      float steps_per_mm);
  static CoreXYDelta xyMoveToABSteps(float current_x_mm, float current_y_mm,
                                     float target_x_mm, float target_y_mm,
                                     float steps_per_mm);
};
