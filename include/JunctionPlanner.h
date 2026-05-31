#pragma once

#include "PlannerQueue.h"

// Placeholder: future implementation performs queue look-ahead and applies
// junction deviation speed constraints without depending on the backend.
class JunctionPlanner {
 public:
  void plan(PlannerQueue& queue) const;
};
