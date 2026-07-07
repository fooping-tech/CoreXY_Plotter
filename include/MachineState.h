#pragma once

#include <stdint.h>

struct MachineState {
  float x_mm = 0.0f;
  float y_mm = 0.0f;
  int32_t a_steps = 0;
  int32_t b_steps = 0;
  float feed_mm_min = 0.0f;
  bool homed = false;
  bool x_homed = false;
  bool y_homed = false;
  bool pen_down = false;
  bool alarmed = false;
  bool tmc_ready = false;
  bool homing_active = false;
  bool motion_active = false;
  bool job_active = false;
  char homing_state[16] = "Idle";
};
