#pragma once

#include <stddef.h>
#include <stdint.h>
#include "MotionBlock.h"

class SegmentQueue;

struct MotionSegment {
  int32_t a_steps = 0;
  int32_t b_steps = 0;
  uint32_t duration_us = 0;
};

class SegmentGenerator {
 public:
  bool generate(const MotionBlock& block, MotionSegment& segment) const;
  bool generate(const MotionBlock& block, SegmentQueue& queue) const;

 private:
  float distanceAtTime(const MotionBlock& block, float time_s) const;
};
