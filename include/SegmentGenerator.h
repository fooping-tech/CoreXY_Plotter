#pragma once

#include <stdint.h>
#include "MotionBlock.h"

struct MotionSegment {
  int32_t a_steps = 0;
  int32_t b_steps = 0;
  uint32_t duration_us = 0;
};

// Placeholder: future implementation generates timed A/B synchronized
// segments from planned MotionBlocks.
class SegmentGenerator {
 public:
  bool generate(const MotionBlock& block, MotionSegment& segment) const;
};
