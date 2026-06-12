#pragma once

#include <stdint.h>

struct RuntimeConfig {
  float steps_per_mm;
  uint32_t max_motor_speed_steps_s;
  float max_feed_mm_min;
  float default_feed_mm_min;
  float default_accel_mm_s2;
  float max_accel_mm_s2;
  float junction_deviation_mm;
  float classic_jerk_limit_mm_s;
  uint32_t lookahead_batch_collect_ms;
  uint32_t ab_timed_min_duration_us;

  float x_min_mm;
  float x_max_mm;
  float y_min_mm;
  float y_max_mm;

  bool job_begin_auto_home;
  bool job_end_park_enabled;
  float job_end_park_x_mm;
  float job_end_park_y_mm;
  float job_end_park_feed_mm_min;
  bool job_end_jingle_enabled;

  uint8_t pen_up_angle_deg;
  uint8_t pen_down_angle_deg;

  bool homing_enabled;
  int8_t homing_x_dir;
  int8_t homing_y_dir;
  float homing_seek_feed_mm_min;
  float homing_slow_feed_mm_min;
  float homing_backoff_mm;
  float homing_start_backoff_mm;
  float homing_max_travel_x_mm;
  float homing_max_travel_y_mm;
  float homing_set_x_mm;
  float homing_set_y_mm;
  uint32_t homing_limit_debounce_ms;
  uint32_t hard_limit_unexpected_alarm_ms;
  float normal_move_limit_release_mm;
  bool homing_require_homed_for_xy_move;

  uint16_t tmc_normal_microsteps;
  uint16_t tmc_normal_rms_current_ma;
  bool tmc_normal_spreadcycle;
  float tmc_hold_multiplier;
  bool tmc_current_vsense;
  uint8_t tmc_iholddelay;
  uint8_t tmc_tpowerdown;
  uint8_t tmc_toff;
  uint8_t tmc_hstrt;
  uint8_t tmc_hend;
  uint8_t tmc_tbl;
  uint8_t tmc_sgthrs;
  uint32_t tmc_tcoolthrs;

  bool motor_melody_enabled;
  uint16_t motor_melody_microsteps;
  uint16_t motor_melody_rms_current_ma;
  bool motor_melody_spreadcycle;
  uint16_t motor_melody_note_gap_ms;
};

enum class RuntimeConfigSetResult {
  OK,
  UNKNOWN_KEY,
  INVALID_VALUE,
  READ_ONLY,
};

extern RuntimeConfig runtime_config;

void resetRuntimeConfig();
RuntimeConfigSetResult setRuntimeConfigValue(const char* key, const char* value);
bool runtimeConfigNeedsTmcReconfigure(const char* key);
void printRuntimeConfig();
