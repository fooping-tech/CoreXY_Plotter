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
  char homing_state[16] = "Idle";
};

struct StatusMessage {
  MachineState machine;
  bool x_limit_active = false;
  bool y_limit_active = false;
  bool x_limit_raw_active = false;
  bool y_limit_raw_active = false;

  StatusMessage() = default;
  StatusMessage(const MachineState& machine_state, bool x_limit, bool y_limit,
                bool x_limit_raw, bool y_limit_raw)
      : machine(machine_state),
        x_limit_active(x_limit),
        y_limit_active(y_limit),
        x_limit_raw_active(x_limit_raw),
        y_limit_raw_active(y_limit_raw) {}
};

struct LogMessage {
  char text[192];
};
