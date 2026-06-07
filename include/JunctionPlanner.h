#pragma once

#include "PlannerQueue.h"

class JunctionPlanner {
 public:
  bool plan(PlannerQueue& queue) const;
};
