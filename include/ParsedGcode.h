#pragma once

enum class ParsedGcodeType {
  NONE,
  G0,
  G1,
  G20,
  G21,
  G28,
  G90,
  G91,
  M3,
  M5,
  M114,
};

struct ParsedGcode {
  ParsedGcodeType type = ParsedGcodeType::NONE;
  bool has_x = false;
  bool has_y = false;
  bool has_f = false;
  float x = 0.0f;
  float y = 0.0f;
  float f_mm_min = 0.0f;
};
