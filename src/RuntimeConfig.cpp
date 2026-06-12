#include "RuntimeConfig.h"

#include <Arduino.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "AppContext.h"
#include "PlotterConfig.h"

RuntimeConfig runtime_config;

namespace {
RuntimeConfig defaults() {
  return RuntimeConfig{
      STEPS_PER_MM,
      MAX_MOTOR_SPEED_STEPS_S,
      MAX_FEED_MM_MIN,
      DEFAULT_FEED_MM_MIN,
      DEFAULT_ACCEL_MM_S2,
      MAX_ACCEL_MM_S2,
      JUNCTION_DEVIATION_MM,
      CLASSIC_JERK_LIMIT_MM_S,
      LOOKAHEAD_BATCH_COLLECT_MS,
      AB_TIMED_MIN_DURATION_US,
      X_MIN_MM,
      X_MAX_MM,
      Y_MIN_MM,
      Y_MAX_MM,
      JOB_BEGIN_AUTO_HOME,
      JOB_END_PARK_ENABLED,
      JOB_END_PARK_X_MM,
      JOB_END_PARK_Y_MM,
      JOB_END_PARK_FEED_MM_MIN,
      JOB_END_JINGLE_ENABLED,
      PEN_UP_ANGLE_DEG,
      PEN_DOWN_ANGLE_DEG,
      HOMING_ENABLED,
      HOMING_X_DIR,
      HOMING_Y_DIR,
      HOMING_SEEK_FEED_MM_MIN,
      HOMING_SLOW_FEED_MM_MIN,
      HOMING_BACKOFF_MM,
      HOMING_START_BACKOFF_MM,
      HOMING_MAX_TRAVEL_X_MM,
      HOMING_MAX_TRAVEL_Y_MM,
      HOMING_SET_X_MM,
      HOMING_SET_Y_MM,
      HOMING_LIMIT_DEBOUNCE_MS,
      HARD_LIMIT_UNEXPECTED_ALARM_MS,
      NORMAL_MOVE_LIMIT_RELEASE_MM,
      HOMING_REQUIRE_HOMED_FOR_XY_MOVE,
      TMC_NORMAL_MICROSTEPS,
      TMC_NORMAL_RMS_CURRENT_MA,
      TMC_NORMAL_SPREADCYCLE,
      TMC_HOLD_MULTIPLIER,
      TMC_CURRENT_VSENSE,
      TMC_IHOLDDELAY,
      TMC_TPOWERDOWN,
      TMC_TOFF,
      TMC_HSTRT,
      TMC_HEND,
      TMC_TBL,
      TMC_SGTHRS_DEFAULT,
      TMC_TCOOLTHRS_DEFAULT,
      MOTOR_MELODY_ENABLED,
      MOTOR_MELODY_MICROSTEPS,
      MOTOR_MELODY_RMS_CURRENT_MA,
      MOTOR_MELODY_SPREADCYCLE,
      MOTOR_MELODY_NOTE_GAP_MS,
  };
}

bool equalsKey(const char* lhs, const char* rhs) {
  while (*lhs && *rhs) {
    if (toupper(static_cast<unsigned char>(*lhs)) !=
        toupper(static_cast<unsigned char>(*rhs))) {
      return false;
    }
    ++lhs;
    ++rhs;
  }
  return *lhs == '\0' && *rhs == '\0';
}

bool startsWithKey(const char* text, const char* prefix) {
  while (*prefix) {
    if (*text == '\0' ||
        toupper(static_cast<unsigned char>(*text)) !=
            toupper(static_cast<unsigned char>(*prefix))) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

bool parseFloatValue(const char* text, float& output) {
  char* end = nullptr;
  const float value = strtof(text, &end);
  if (end == text || *end != '\0' || !isfinite(value)) return false;
  output = value;
  return true;
}

bool parseLongValue(const char* text, long& output) {
  char* end = nullptr;
  const long value = strtol(text, &end, 0);
  if (end == text || *end != '\0') return false;
  output = value;
  return true;
}

bool parseBoolValue(const char* text, bool& output) {
  if (equalsKey(text, "1") || equalsKey(text, "true") ||
      equalsKey(text, "yes") || equalsKey(text, "on")) {
    output = true;
    return true;
  }
  if (equalsKey(text, "0") || equalsKey(text, "false") ||
      equalsKey(text, "no") || equalsKey(text, "off")) {
    output = false;
    return true;
  }
  return false;
}

RuntimeConfigSetResult setFloat(const char* value, float& target,
                                float min_value, float max_value) {
  float parsed = 0.0f;
  if (!parseFloatValue(value, parsed) || parsed < min_value ||
      parsed > max_value) {
    return RuntimeConfigSetResult::INVALID_VALUE;
  }
  target = parsed;
  return RuntimeConfigSetResult::OK;
}

template <typename T>
RuntimeConfigSetResult setInteger(const char* value, T& target, long min_value,
                                  long max_value) {
  long parsed = 0;
  if (!parseLongValue(value, parsed) || parsed < min_value ||
      parsed > max_value) {
    return RuntimeConfigSetResult::INVALID_VALUE;
  }
  target = static_cast<T>(parsed);
  return RuntimeConfigSetResult::OK;
}

RuntimeConfigSetResult setBool(const char* value, bool& target) {
  bool parsed = false;
  if (!parseBoolValue(value, parsed)) {
    return RuntimeConfigSetResult::INVALID_VALUE;
  }
  target = parsed;
  return RuntimeConfigSetResult::OK;
}

void recalculateDerivedFeeds() {
  runtime_config.max_feed_mm_min =
      runtime_config.max_motor_speed_steps_s * 60.0f /
      (runtime_config.steps_per_mm * COREXY_MAX_MOTOR_GAIN) * SPEED_SAFETY;
}

void yieldConfigLog() { delay(2); }

void printBoolConfig(const char* key, bool value) {
  logMessage("CONFIG_VALUE %s=%u", key, value ? 1 : 0);
  yieldConfigLog();
}

void printFloatConfig(const char* key, float value) {
  logMessage("CONFIG_VALUE %s=%.6f", key, value);
  yieldConfigLog();
}

void printUIntConfig(const char* key, unsigned long value) {
  logMessage("CONFIG_VALUE %s=%lu", key, value);
  yieldConfigLog();
}

bool isTmcKey(const char* key) {
  return startsWithKey(key, "TMC_") || startsWithKey(key, "MOTOR_MELODY_");
}
}  // namespace

void resetRuntimeConfig() { runtime_config = defaults(); }

RuntimeConfigSetResult setRuntimeConfigValue(const char* key,
                                             const char* value) {
  if (key == nullptr || value == nullptr || key[0] == '\0' ||
      value[0] == '\0') {
    return RuntimeConfigSetResult::INVALID_VALUE;
  }

  const RuntimeConfig previous = runtime_config;
  RuntimeConfigSetResult result = RuntimeConfigSetResult::UNKNOWN_KEY;
  const bool recalculate_feed =
      equalsKey(key, "STEPS_PER_MM") ||
      equalsKey(key, "MAX_MOTOR_SPEED_STEPS_S");

#define TRY_FLOAT(NAME, FIELD, MIN_VALUE, MAX_VALUE) \
  if (equalsKey(key, NAME)) result = setFloat(value, runtime_config.FIELD, MIN_VALUE, MAX_VALUE)
#define TRY_UINT(NAME, FIELD, MIN_VALUE, MAX_VALUE) \
  if (equalsKey(key, NAME)) result = setInteger(value, runtime_config.FIELD, MIN_VALUE, MAX_VALUE)
#define TRY_INT(NAME, FIELD, MIN_VALUE, MAX_VALUE) \
  if (equalsKey(key, NAME)) result = setInteger(value, runtime_config.FIELD, MIN_VALUE, MAX_VALUE)
#define TRY_BOOL(NAME, FIELD) \
  if (equalsKey(key, NAME)) result = setBool(value, runtime_config.FIELD)

  TRY_FLOAT("STEPS_PER_MM", steps_per_mm, 1.0f, 10000.0f);
  TRY_UINT("MAX_MOTOR_SPEED_STEPS_S", max_motor_speed_steps_s, 1, 200000);
  TRY_FLOAT("MAX_FEED_MM_MIN", max_feed_mm_min, 1.0f, 100000.0f);
  TRY_FLOAT("DEFAULT_FEED_MM_MIN", default_feed_mm_min, 1.0f, 100000.0f);
  TRY_FLOAT("DEFAULT_ACCEL_MM_S2", default_accel_mm_s2, 0.1f, 100000.0f);
  TRY_FLOAT("MAX_ACCEL_MM_S2", max_accel_mm_s2, 0.1f, 100000.0f);
  TRY_FLOAT("JUNCTION_DEVIATION_MM", junction_deviation_mm, 0.001f, 10.0f);
  TRY_FLOAT("CLASSIC_JERK_LIMIT_MM_S", classic_jerk_limit_mm_s, 0.0f, 10000.0f);
  TRY_UINT("LOOKAHEAD_BATCH_COLLECT_MS", lookahead_batch_collect_ms, 0, 1000);
  TRY_UINT("AB_TIMED_MIN_DURATION_US", ab_timed_min_duration_us, 1, 10000000);
  TRY_FLOAT("X_MIN_MM", x_min_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("X_MAX_MM", x_max_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("Y_MIN_MM", y_min_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("Y_MAX_MM", y_max_mm, -10000.0f, 10000.0f);
  TRY_BOOL("JOB_BEGIN_AUTO_HOME", job_begin_auto_home);
  TRY_BOOL("JOB_END_PARK_ENABLED", job_end_park_enabled);
  TRY_FLOAT("JOB_END_PARK_X_MM", job_end_park_x_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("JOB_END_PARK_Y_MM", job_end_park_y_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("JOB_END_PARK_FEED_MM_MIN", job_end_park_feed_mm_min, 1.0f, 100000.0f);
  TRY_BOOL("JOB_END_JINGLE_ENABLED", job_end_jingle_enabled);
  TRY_UINT("PEN_UP_ANGLE_DEG", pen_up_angle_deg, 0, 180);
  TRY_UINT("PEN_DOWN_ANGLE_DEG", pen_down_angle_deg, 0, 180);
  TRY_BOOL("HOMING_ENABLED", homing_enabled);
  TRY_INT("HOMING_X_DIR", homing_x_dir, -1, 1);
  TRY_INT("HOMING_Y_DIR", homing_y_dir, -1, 1);
  TRY_FLOAT("HOMING_SEEK_FEED_MM_MIN", homing_seek_feed_mm_min, 1.0f, 100000.0f);
  TRY_FLOAT("HOMING_SLOW_FEED_MM_MIN", homing_slow_feed_mm_min, 1.0f, 100000.0f);
  TRY_FLOAT("HOMING_BACKOFF_MM", homing_backoff_mm, 0.0f, 10000.0f);
  TRY_FLOAT("HOMING_START_BACKOFF_MM", homing_start_backoff_mm, 0.0f, 10000.0f);
  TRY_FLOAT("HOMING_MAX_TRAVEL_X_MM", homing_max_travel_x_mm, 0.0f, 10000.0f);
  TRY_FLOAT("HOMING_MAX_TRAVEL_Y_MM", homing_max_travel_y_mm, 0.0f, 10000.0f);
  TRY_FLOAT("HOMING_SET_X_MM", homing_set_x_mm, -10000.0f, 10000.0f);
  TRY_FLOAT("HOMING_SET_Y_MM", homing_set_y_mm, -10000.0f, 10000.0f);
  TRY_UINT("HOMING_LIMIT_DEBOUNCE_MS", homing_limit_debounce_ms, 0, 10000);
  TRY_UINT("HARD_LIMIT_UNEXPECTED_ALARM_MS", hard_limit_unexpected_alarm_ms, 0, 10000);
  TRY_FLOAT("NORMAL_MOVE_LIMIT_RELEASE_MM", normal_move_limit_release_mm, 0.0f, 10000.0f);
  TRY_BOOL("HOMING_REQUIRE_HOMED_FOR_XY_MOVE", homing_require_homed_for_xy_move);
  TRY_UINT("TMC_NORMAL_MICROSTEPS", tmc_normal_microsteps, 1, 256);
  TRY_UINT("TMC_NORMAL_RMS_CURRENT_MA", tmc_normal_rms_current_ma, 1, 3000);
  TRY_BOOL("TMC_NORMAL_SPREADCYCLE", tmc_normal_spreadcycle);
  TRY_FLOAT("TMC_HOLD_MULTIPLIER", tmc_hold_multiplier, 0.0f, 1.0f);
  TRY_BOOL("TMC_CURRENT_VSENSE", tmc_current_vsense);
  TRY_UINT("TMC_IHOLDDELAY", tmc_iholddelay, 0, 15);
  TRY_UINT("TMC_TPOWERDOWN", tmc_tpowerdown, 0, 255);
  TRY_UINT("TMC_TOFF", tmc_toff, 0, 15);
  TRY_UINT("TMC_HSTRT", tmc_hstrt, 0, 7);
  TRY_UINT("TMC_HEND", tmc_hend, 0, 15);
  TRY_UINT("TMC_TBL", tmc_tbl, 0, 3);
  TRY_UINT("TMC_SGTHRS_DEFAULT", tmc_sgthrs, 0, 255);
  TRY_UINT("TMC_TCOOLTHRS_DEFAULT", tmc_tcoolthrs, 0, 0xFFFFF);
  TRY_BOOL("MOTOR_MELODY_ENABLED", motor_melody_enabled);
  TRY_UINT("MOTOR_MELODY_MICROSTEPS", motor_melody_microsteps, 1, 256);
  TRY_UINT("MOTOR_MELODY_RMS_CURRENT_MA", motor_melody_rms_current_ma, 1, 3000);
  TRY_BOOL("MOTOR_MELODY_SPREADCYCLE", motor_melody_spreadcycle);
  TRY_UINT("MOTOR_MELODY_NOTE_GAP_MS", motor_melody_note_gap_ms, 0, 10000);

#undef TRY_FLOAT
#undef TRY_UINT
#undef TRY_INT
#undef TRY_BOOL

  if (result != RuntimeConfigSetResult::OK) {
    runtime_config = previous;
    return result;
  }
  if (recalculate_feed) {
    recalculateDerivedFeeds();
  }
  if (runtime_config.homing_x_dir == 0 || runtime_config.homing_y_dir == 0 ||
      runtime_config.x_min_mm >= runtime_config.x_max_mm ||
      runtime_config.y_min_mm >= runtime_config.y_max_mm ||
      runtime_config.default_feed_mm_min > runtime_config.max_feed_mm_min ||
      runtime_config.default_accel_mm_s2 > runtime_config.max_accel_mm_s2) {
    runtime_config = previous;
    return RuntimeConfigSetResult::INVALID_VALUE;
  }
  return RuntimeConfigSetResult::OK;
}

bool runtimeConfigNeedsTmcReconfigure(const char* key) { return isTmcKey(key); }

void printRuntimeConfig() {
  printFloatConfig("STEPS_PER_MM", runtime_config.steps_per_mm);
  printUIntConfig("MAX_MOTOR_SPEED_STEPS_S", runtime_config.max_motor_speed_steps_s);
  printFloatConfig("MAX_FEED_MM_MIN", runtime_config.max_feed_mm_min);
  printFloatConfig("DEFAULT_FEED_MM_MIN", runtime_config.default_feed_mm_min);
  printFloatConfig("DEFAULT_ACCEL_MM_S2", runtime_config.default_accel_mm_s2);
  printFloatConfig("MAX_ACCEL_MM_S2", runtime_config.max_accel_mm_s2);
  printFloatConfig("JUNCTION_DEVIATION_MM", runtime_config.junction_deviation_mm);
  printFloatConfig("CLASSIC_JERK_LIMIT_MM_S", runtime_config.classic_jerk_limit_mm_s);
  printUIntConfig("LOOKAHEAD_BATCH_COLLECT_MS", runtime_config.lookahead_batch_collect_ms);
  printUIntConfig("AB_TIMED_MIN_DURATION_US", runtime_config.ab_timed_min_duration_us);
  printFloatConfig("X_MIN_MM", runtime_config.x_min_mm);
  printFloatConfig("X_MAX_MM", runtime_config.x_max_mm);
  printFloatConfig("Y_MIN_MM", runtime_config.y_min_mm);
  printFloatConfig("Y_MAX_MM", runtime_config.y_max_mm);
  printBoolConfig("JOB_BEGIN_AUTO_HOME", runtime_config.job_begin_auto_home);
  printBoolConfig("JOB_END_PARK_ENABLED", runtime_config.job_end_park_enabled);
  printFloatConfig("JOB_END_PARK_X_MM", runtime_config.job_end_park_x_mm);
  printFloatConfig("JOB_END_PARK_Y_MM", runtime_config.job_end_park_y_mm);
  printFloatConfig("JOB_END_PARK_FEED_MM_MIN", runtime_config.job_end_park_feed_mm_min);
  printBoolConfig("JOB_END_JINGLE_ENABLED", runtime_config.job_end_jingle_enabled);
  printUIntConfig("PEN_UP_ANGLE_DEG", runtime_config.pen_up_angle_deg);
  printUIntConfig("PEN_DOWN_ANGLE_DEG", runtime_config.pen_down_angle_deg);
  printBoolConfig("HOMING_ENABLED", runtime_config.homing_enabled);
  logMessage("CONFIG_VALUE HOMING_X_DIR=%d", runtime_config.homing_x_dir);
  yieldConfigLog();
  logMessage("CONFIG_VALUE HOMING_Y_DIR=%d", runtime_config.homing_y_dir);
  yieldConfigLog();
  printFloatConfig("HOMING_SEEK_FEED_MM_MIN", runtime_config.homing_seek_feed_mm_min);
  printFloatConfig("HOMING_SLOW_FEED_MM_MIN", runtime_config.homing_slow_feed_mm_min);
  printFloatConfig("HOMING_BACKOFF_MM", runtime_config.homing_backoff_mm);
  printFloatConfig("HOMING_START_BACKOFF_MM", runtime_config.homing_start_backoff_mm);
  printFloatConfig("HOMING_MAX_TRAVEL_X_MM", runtime_config.homing_max_travel_x_mm);
  printFloatConfig("HOMING_MAX_TRAVEL_Y_MM", runtime_config.homing_max_travel_y_mm);
  printFloatConfig("HOMING_SET_X_MM", runtime_config.homing_set_x_mm);
  printFloatConfig("HOMING_SET_Y_MM", runtime_config.homing_set_y_mm);
  printUIntConfig("HOMING_LIMIT_DEBOUNCE_MS", runtime_config.homing_limit_debounce_ms);
  printUIntConfig("HARD_LIMIT_UNEXPECTED_ALARM_MS", runtime_config.hard_limit_unexpected_alarm_ms);
  printFloatConfig("NORMAL_MOVE_LIMIT_RELEASE_MM", runtime_config.normal_move_limit_release_mm);
  printBoolConfig("HOMING_REQUIRE_HOMED_FOR_XY_MOVE", runtime_config.homing_require_homed_for_xy_move);
  printUIntConfig("TMC_NORMAL_MICROSTEPS", runtime_config.tmc_normal_microsteps);
  printUIntConfig("TMC_NORMAL_RMS_CURRENT_MA", runtime_config.tmc_normal_rms_current_ma);
  printBoolConfig("TMC_NORMAL_SPREADCYCLE", runtime_config.tmc_normal_spreadcycle);
  printFloatConfig("TMC_HOLD_MULTIPLIER", runtime_config.tmc_hold_multiplier);
  printBoolConfig("TMC_CURRENT_VSENSE", runtime_config.tmc_current_vsense);
  printUIntConfig("TMC_IHOLDDELAY", runtime_config.tmc_iholddelay);
  printUIntConfig("TMC_TPOWERDOWN", runtime_config.tmc_tpowerdown);
  printUIntConfig("TMC_TOFF", runtime_config.tmc_toff);
  printUIntConfig("TMC_HSTRT", runtime_config.tmc_hstrt);
  printUIntConfig("TMC_HEND", runtime_config.tmc_hend);
  printUIntConfig("TMC_TBL", runtime_config.tmc_tbl);
  printUIntConfig("TMC_SGTHRS_DEFAULT", runtime_config.tmc_sgthrs);
  printUIntConfig("TMC_TCOOLTHRS_DEFAULT", runtime_config.tmc_tcoolthrs);
  printBoolConfig("MOTOR_MELODY_ENABLED", runtime_config.motor_melody_enabled);
  printUIntConfig("MOTOR_MELODY_MICROSTEPS", runtime_config.motor_melody_microsteps);
  printUIntConfig("MOTOR_MELODY_RMS_CURRENT_MA", runtime_config.motor_melody_rms_current_ma);
  printBoolConfig("MOTOR_MELODY_SPREADCYCLE", runtime_config.motor_melody_spreadcycle);
  printUIntConfig("MOTOR_MELODY_NOTE_GAP_MS", runtime_config.motor_melody_note_gap_ms);
}
