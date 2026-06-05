#pragma once

#include "MotionBlock.h"

class TrapezoidPlanner {
 public:
  bool plan(MotionBlock& block) const;
};
