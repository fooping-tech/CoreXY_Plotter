#include "TrapezoidPlanner.h"

#include <Arduino.h>
#include <math.h>
#include "AppContext.h"
#include "PlotterConfig.h"

namespace {
constexpr float MIN_PROFILE_DISTANCE_MM = 0.0001f;

float square(float value) {
  return value * value;
}
}

bool TrapezoidPlanner::plan(MotionBlock& block) const {
  block.trapezoid_planned = false;
  block.triangular_profile = false;
  block.acceleration_distance_mm = 0.0f;
  block.cruise_distance_mm = 0.0f;
  block.deceleration_distance_mm = 0.0f;
  block.acceleration_time_s = 0.0f;
  block.cruise_time_s = 0.0f;
  block.deceleration_time_s = 0.0f;

  if (block.length_mm < MIN_PROFILE_DISTANCE_MM) {
    return false;
  }

  block.nominal_speed_mm_min =
      constrain(block.nominal_speed_mm_min, 0.0f,
                runtime_config.max_feed_mm_min);
  block.nominal_speed_mm_s = block.nominal_speed_mm_min / 60.0f;
  block.entry_speed_mm_s =
      constrain(block.entry_speed_mm_min / 60.0f, 0.0f, block.nominal_speed_mm_s);
  block.exit_speed_mm_s =
      constrain(block.exit_speed_mm_min / 60.0f, 0.0f, block.nominal_speed_mm_s);
  block.acceleration_mm_s2 =
      constrain(runtime_config.default_accel_mm_s2, 0.1f,
                runtime_config.max_accel_mm_s2);

  if (block.nominal_speed_mm_s <= 0.0f || block.acceleration_mm_s2 <= 0.0f) {
    return false;
  }

  const float nominal_speed_squared = square(block.nominal_speed_mm_s);
  const float entry_speed_squared = square(block.entry_speed_mm_s);
  const float exit_speed_squared = square(block.exit_speed_mm_s);
  const float acceleration_mm_s2 = block.acceleration_mm_s2;

  float acceleration_distance_mm =
      (nominal_speed_squared - entry_speed_squared) / (2.0f * acceleration_mm_s2);
  float deceleration_distance_mm =
      (nominal_speed_squared - exit_speed_squared) / (2.0f * acceleration_mm_s2);
  float peak_speed_mm_s = block.nominal_speed_mm_s;

  if (acceleration_distance_mm + deceleration_distance_mm > block.length_mm) {
    const float peak_speed_squared =
        acceleration_mm_s2 * block.length_mm +
        0.5f * (entry_speed_squared + exit_speed_squared);
    peak_speed_mm_s = sqrtf(max(0.0f, peak_speed_squared));
    peak_speed_mm_s = min(peak_speed_mm_s, block.nominal_speed_mm_s);
    const float peak_speed_squared_limited = square(peak_speed_mm_s);
    acceleration_distance_mm =
        max(0.0f, (peak_speed_squared_limited - entry_speed_squared) /
                      (2.0f * acceleration_mm_s2));
    deceleration_distance_mm =
        max(0.0f, (peak_speed_squared_limited - exit_speed_squared) /
                      (2.0f * acceleration_mm_s2));
    block.cruise_distance_mm = 0.0f;
    block.triangular_profile = true;
  } else {
    block.cruise_distance_mm =
        block.length_mm - acceleration_distance_mm - deceleration_distance_mm;
  }

  block.peak_speed_mm_s = peak_speed_mm_s;
  block.acceleration_distance_mm = acceleration_distance_mm;
  block.deceleration_distance_mm = deceleration_distance_mm;
  block.acceleration_time_s =
      (peak_speed_mm_s - block.entry_speed_mm_s) / acceleration_mm_s2;
  block.deceleration_time_s =
      (peak_speed_mm_s - block.exit_speed_mm_s) / acceleration_mm_s2;
  block.cruise_time_s =
      block.cruise_distance_mm > 0.0f ? block.cruise_distance_mm / peak_speed_mm_s
                                      : 0.0f;
  block.trapezoid_planned = true;
  return true;
}
