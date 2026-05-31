#pragma once

#include "MotionBlock.h"

// Placeholder: future implementation computes acceleration, cruise and
// deceleration phases without depending on the stepper backend.
class TrapezoidPlanner {
 public:
  bool plan(MotionBlock& block) const;
};
