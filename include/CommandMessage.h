#pragma once

#include <stdint.h>
#include "CommandTypes.h"
#include "LedTypes.h"
#include "ParsedGcode.h"

struct CommandMessage {
  CommandType type = CommandType::INVALID;
  char name[16] = {};
  bool from_gcode = false;
  float x_mm = 0.0f;
  float y_mm = 0.0f;
  float feed_mm_min = 0.0f;
  int32_t steps = 0;
  int32_t a_steps = 0;
  int32_t b_steps = 0;
  uint32_t duration_us = 0;
  uint32_t dwell_ms = 0;
  LedCommand led;
  ParsedGcode gcode;
  char error[96] = {};
};
