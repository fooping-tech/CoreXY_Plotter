#include "JunctionPlanner.h"

#include <Arduino.h>
#include <math.h>
#include "PlotterConfig.h"

namespace {
constexpr float MIN_BLOCK_LENGTH_MM = 0.0001f;
constexpr float STRAIGHT_DOT_THRESHOLD = 0.9999f;
constexpr float REVERSAL_DOT_THRESHOLD = -0.9999f;

float square(float value) {
  return value * value;
}

float reachableExitSpeed(float entry_speed_mm_s,
                         float acceleration_mm_s2,
                         float distance_mm) {
  return sqrtf(max(0.0f, square(entry_speed_mm_s) +
                            2.0f * acceleration_mm_s2 * distance_mm));
}

float reachableEntrySpeed(float exit_speed_mm_s,
                          float acceleration_mm_s2,
                          float distance_mm) {
  return sqrtf(max(0.0f, square(exit_speed_mm_s) +
                            2.0f * acceleration_mm_s2 * distance_mm));
}

float junctionSpeedLimitMmS(const MotionBlock& previous,
                            const MotionBlock& current) {
  if (previous.length_mm < MIN_BLOCK_LENGTH_MM ||
      current.length_mm < MIN_BLOCK_LENGTH_MM) {
    return 0.0f;
  }

  const float previous_unit_x = previous.dx_mm / previous.length_mm;
  const float previous_unit_y = previous.dy_mm / previous.length_mm;
  const float current_unit_x = current.dx_mm / current.length_mm;
  const float current_unit_y = current.dy_mm / current.length_mm;
  const float direction_dot =
      constrain(previous_unit_x * current_unit_x +
                    previous_unit_y * current_unit_y,
                -1.0f, 1.0f);

  if (direction_dot > STRAIGHT_DOT_THRESHOLD) {
    return min(previous.nominal_speed_mm_s, current.nominal_speed_mm_s);
  }
  if (direction_dot < REVERSAL_DOT_THRESHOLD) {
    return 0.0f;
  }

  const float junction_cos_theta = -direction_dot;
  const float sin_theta_d2 =
      sqrtf(max(0.0f, 0.5f * (1.0f - junction_cos_theta)));
  if (sin_theta_d2 <= 0.0f || sin_theta_d2 >= 1.0f) {
    return 0.0f;
  }

  const float acceleration_mm_s2 =
      constrain(DEFAULT_ACCEL_MM_S2, 0.1f, MAX_ACCEL_MM_S2);
  const float junction_speed_mm_s =
      sqrtf(max(0.0f, acceleration_mm_s2 * JUNCTION_DEVIATION_MM *
                          sin_theta_d2 / (1.0f - sin_theta_d2)));
  const float classic_jerk_limited_mm_s =
      CLASSIC_JERK_LIMIT_MM_S > 0.0f
          ? min(junction_speed_mm_s, CLASSIC_JERK_LIMIT_MM_S)
          : junction_speed_mm_s;
  return min(classic_jerk_limited_mm_s,
             min(previous.nominal_speed_mm_s, current.nominal_speed_mm_s));
}
}  // namespace

bool JunctionPlanner::plan(PlannerQueue& queue) const {
  const size_t count = queue.count();
  if (count == 0) return false;

  for (size_t index = 0; index < count; ++index) {
    MotionBlock* block = queue.at(index);
    if (block == nullptr || block->length_mm < MIN_BLOCK_LENGTH_MM) return false;
    block->nominal_speed_mm_min =
        constrain(block->nominal_speed_mm_min, 0.0f, MAX_FEED_MM_MIN);
    block->nominal_speed_mm_s = block->nominal_speed_mm_min / 60.0f;
    block->acceleration_mm_s2 =
        constrain(DEFAULT_ACCEL_MM_S2, 0.1f, MAX_ACCEL_MM_S2);
    block->entry_speed_mm_s = 0.0f;
    block->exit_speed_mm_s = 0.0f;
    block->entry_speed_mm_min = 0.0f;
    block->exit_speed_mm_min = 0.0f;
  }

  for (size_t index = 1; index < count; ++index) {
    MotionBlock* previous = queue.at(index - 1);
    MotionBlock* current = queue.at(index);
    if (previous == nullptr || current == nullptr) return false;
    const float junction_speed_mm_s =
        junctionSpeedLimitMmS(*previous, *current);
    previous->exit_speed_mm_s = junction_speed_mm_s;
    previous->exit_speed_mm_min = junction_speed_mm_s * 60.0f;
    current->entry_speed_mm_s = junction_speed_mm_s;
    current->entry_speed_mm_min = junction_speed_mm_s * 60.0f;
  }

  for (int index = static_cast<int>(count) - 2; index >= 0; --index) {
    MotionBlock* current = queue.at(static_cast<size_t>(index));
    MotionBlock* next = queue.at(static_cast<size_t>(index + 1));
    if (current == nullptr || next == nullptr) return false;
    const float junction_speed_mm_s =
        min(current->exit_speed_mm_s,
            min(next->entry_speed_mm_s,
                reachableEntrySpeed(next->exit_speed_mm_s,
                                    next->acceleration_mm_s2,
                                    next->length_mm)));
    current->exit_speed_mm_s = junction_speed_mm_s;
    current->exit_speed_mm_min = junction_speed_mm_s * 60.0f;
    next->entry_speed_mm_s = junction_speed_mm_s;
    next->entry_speed_mm_min = junction_speed_mm_s * 60.0f;
  }

  for (size_t index = 1; index < count; ++index) {
    MotionBlock* previous = queue.at(index - 1);
    MotionBlock* current = queue.at(index);
    if (previous == nullptr || current == nullptr) return false;
    const float junction_speed_mm_s =
        min(previous->exit_speed_mm_s,
            min(current->entry_speed_mm_s,
                reachableExitSpeed(previous->entry_speed_mm_s,
                                   previous->acceleration_mm_s2,
                                   previous->length_mm)));
    previous->exit_speed_mm_s = junction_speed_mm_s;
    previous->exit_speed_mm_min = junction_speed_mm_s * 60.0f;
    current->entry_speed_mm_s = junction_speed_mm_s;
    current->entry_speed_mm_min = junction_speed_mm_s * 60.0f;
  }

  return true;
}
