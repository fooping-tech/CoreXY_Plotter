#pragma once

#include <stdint.h>

struct MotionBlock {
  float start_x_mm = 0.0f;
  float start_y_mm = 0.0f;
  float target_x_mm = 0.0f;
  float target_y_mm = 0.0f;
  float dx_mm = 0.0f;
  float dy_mm = 0.0f;
  float length_mm = 0.0f;
  float nominal_speed_mm_min = 0.0f;
  float entry_speed_mm_min = 0.0f;
  float exit_speed_mm_min = 0.0f;
  float acceleration_mm_s2 = 0.0f;
  float nominal_speed_mm_s = 0.0f;
  float entry_speed_mm_s = 0.0f;
  float exit_speed_mm_s = 0.0f;
  float peak_speed_mm_s = 0.0f;
  float acceleration_distance_mm = 0.0f;
  float cruise_distance_mm = 0.0f;
  float deceleration_distance_mm = 0.0f;
  float acceleration_time_s = 0.0f;
  float cruise_time_s = 0.0f;
  float deceleration_time_s = 0.0f;
  int32_t a_steps = 0;
  int32_t b_steps = 0;
  int32_t target_a_steps = 0;
  int32_t target_b_steps = 0;
  bool pen_down = false;
  bool triangular_profile = false;
  bool trapezoid_planned = false;
};
