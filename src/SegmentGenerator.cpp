#include "SegmentGenerator.h"

#include <Arduino.h>
#include <math.h>
#include "PlotterConfig.h"
#include "SegmentQueue.h"

namespace {
constexpr float DEFAULT_SEGMENT_TIME_S = 0.020f;
constexpr float MIN_SEGMENT_TIME_S = 0.001f;
constexpr float MIN_MOVE_TIME_S = 0.000001f;
constexpr float STEP_ROUNDING_TOLERANCE_STEPS = 0.5f;

bool segmentWithinMotorSpeedLimit(const MotionSegment& segment) {
  if (segment.duration_us == 0) return false;
  const float allowed_steps =
      static_cast<float>(MAX_MOTOR_SPEED_STEPS_S) *
          static_cast<float>(segment.duration_us) / 1000000.0f +
      STEP_ROUNDING_TOLERANCE_STEPS;
  return fabsf(static_cast<float>(segment.a_steps)) <= allowed_steps &&
         fabsf(static_cast<float>(segment.b_steps)) <= allowed_steps;
}
}

bool SegmentGenerator::generate(const MotionBlock& block,
                                MotionSegment& segment) const {
  if (!block.trapezoid_planned) return false;
  segment.a_steps = block.a_steps;
  segment.b_steps = block.b_steps;
  segment.duration_us = static_cast<uint32_t>(
      lroundf((block.acceleration_time_s + block.cruise_time_s +
               block.deceleration_time_s) *
              1000000.0f));
  return segmentWithinMotorSpeedLimit(segment);
}

bool SegmentGenerator::generate(const MotionBlock& block,
                                SegmentQueue& queue) const {
  queue.clear();
  if (!block.trapezoid_planned || block.length_mm <= 0.0f) return false;

  const float total_time_s =
      block.acceleration_time_s + block.cruise_time_s +
      block.deceleration_time_s;
  if (total_time_s < MIN_MOVE_TIME_S) return false;

  size_t segment_count =
      static_cast<size_t>(ceilf(total_time_s / DEFAULT_SEGMENT_TIME_S));
  if (segment_count == 0) segment_count = 1;
  if (segment_count > SegmentQueue::CAPACITY) {
    segment_count = SegmentQueue::CAPACITY;
  }

  const float segment_time_s =
      max(MIN_SEGMENT_TIME_S, total_time_s / static_cast<float>(segment_count));
  int32_t previous_a_steps = 0;
  int32_t previous_b_steps = 0;
  float previous_time_s = 0.0f;

  for (size_t index = 1; index <= segment_count; ++index) {
    const float time_s =
        index == segment_count
            ? total_time_s
            : min(total_time_s, segment_time_s * static_cast<float>(index));
    const float distance_mm = distanceAtTime(block, time_s);
    const float fraction =
        block.length_mm > 0.0f ? constrain(distance_mm / block.length_mm, 0.0f, 1.0f)
                               : 1.0f;

    const int32_t cumulative_a_steps = lroundf(block.a_steps * fraction);
    const int32_t cumulative_b_steps = lroundf(block.b_steps * fraction);
    MotionSegment segment{};
    segment.a_steps = cumulative_a_steps - previous_a_steps;
    segment.b_steps = cumulative_b_steps - previous_b_steps;
    segment.duration_us =
        static_cast<uint32_t>(max(1.0f, (time_s - previous_time_s) * 1000000.0f));

    if (!segmentWithinMotorSpeedLimit(segment)) return false;
    if (!queue.enqueue(segment)) return false;

    previous_a_steps = cumulative_a_steps;
    previous_b_steps = cumulative_b_steps;
    previous_time_s = time_s;
  }

  return queue.count() > 0 && previous_a_steps == block.a_steps &&
         previous_b_steps == block.b_steps;
}

float SegmentGenerator::distanceAtTime(const MotionBlock& block,
                                       float time_s) const {
  time_s = constrain(time_s, 0.0f,
                     block.acceleration_time_s + block.cruise_time_s +
                         block.deceleration_time_s);
  if (time_s <= block.acceleration_time_s) {
    return block.entry_speed_mm_s * time_s +
           0.5f * block.acceleration_mm_s2 * time_s * time_s;
  }

  const float cruise_end_time_s =
      block.acceleration_time_s + block.cruise_time_s;
  if (time_s <= cruise_end_time_s) {
    return block.acceleration_distance_mm +
           block.peak_speed_mm_s * (time_s - block.acceleration_time_s);
  }

  const float deceleration_time_s = time_s - cruise_end_time_s;
  return block.acceleration_distance_mm + block.cruise_distance_mm +
         block.peak_speed_mm_s * deceleration_time_s -
         0.5f * block.acceleration_mm_s2 * deceleration_time_s *
             deceleration_time_s;
}
