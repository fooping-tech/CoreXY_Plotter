#include "CommandDispatcher.h"
#include "PlotterConfig.h"
#include <stdio.h>
#include <string.h>

namespace {
bool parseByte(int value, uint8_t& output) {
  if (value < 0 || value > 255) return false;
  output = static_cast<uint8_t>(value);
  return true;
}

bool parsePattern(const char* text, LedPattern& pattern) {
  if (strcmp(text, "OFF") == 0) pattern = LedPattern::OFF;
  else if (strcmp(text, "SOLID") == 0) pattern = LedPattern::SOLID;
  else if (strcmp(text, "PACIFICA") == 0) pattern = LedPattern::PACIFICA;
  else if (strcmp(text, "FIRE") == 0) pattern = LedPattern::FIRE;
  else return false;
  return true;
}

bool parseParameter(const char* text, LedParameter& parameter) {
  if (strcmp(text, "BRIGHTNESS") == 0) parameter = LedParameter::BRIGHTNESS;
  else if (strcmp(text, "HUE") == 0) parameter = LedParameter::HUE;
  else if (strcmp(text, "SATURATION") == 0) parameter = LedParameter::SATURATION;
  else if (strcmp(text, "SPEED") == 0) parameter = LedParameter::SPEED;
  else if (strcmp(text, "INTENSITY") == 0) parameter = LedParameter::INTENSITY;
  else if (strcmp(text, "COOLING") == 0) parameter = LedParameter::COOLING;
  else if (strcmp(text, "SPARKING") == 0) parameter = LedParameter::SPARKING;
  else return false;
  return true;
}
}

CommandMessage CommandDispatcher::parse(const char* line) {
  CommandMessage command;
  char name[16] = {};
  if (sscanf(line, "%15s", name) != 1) {
    snprintf(command.error, sizeof(command.error), "empty command");
    return command;
  }

  if (strcmp(name, "HELP") == 0) command.type = CommandType::HELP;
  else if (strcmp(name, "CONFIG") == 0) command.type = CommandType::CONFIG;
  else if (strcmp(name, "POS") == 0) command.type = CommandType::POS;
  else if (strcmp(name, "ENABLE") == 0) command.type = CommandType::ENABLE;
  else if (strcmp(name, "DISABLE") == 0) command.type = CommandType::DISABLE;
  else if (strcmp(name, "ZERO") == 0) command.type = CommandType::ZERO;
  else if (strcmp(name, "PENUP") == 0) command.type = CommandType::PEN_UP;
  else if (strcmp(name, "PENDOWN") == 0) command.type = CommandType::PEN_DOWN;
  else if (strcmp(name, "SELFTEST") == 0) command.type = CommandType::SELFTEST;
  else if (strcmp(name, "TMC_INIT") == 0) command.type = CommandType::TMC_INIT;
  else if (strcmp(name, "TMC_STATUS") == 0) command.type = CommandType::TMC_STATUS;
  else if (strcmp(name, "LED_OFF") == 0) {
    command.type = CommandType::LED_OFF;
    command.led.type = LedCommandType::OFF;
  } else if (strcmp(name, "LED_STATUS") == 0) {
    command.type = CommandType::LED_STATUS;
    command.led.type = LedCommandType::STATUS;
  } else if (strcmp(name, "MELODY") == 0) {
    command.type = CommandType::MELODY;
  } else if (strcmp(name, "LED") == 0) {
    int r, g, b;
    if (sscanf(line, "%*s %d %d %d", &r, &g, &b) != 3 ||
        !parseByte(r, command.led.r) || !parseByte(g, command.led.g) ||
        !parseByte(b, command.led.b)) {
      snprintf(command.error, sizeof(command.error), "LED requires RGB values 0..255");
      return command;
    }
    command.type = CommandType::LED;
    command.led.type = LedCommandType::SET_ALL;
  } else if (strcmp(name, "LED_PIXEL") == 0) {
    int index, r, g, b;
    if (sscanf(line, "%*s %d %d %d %d", &index, &r, &g, &b) != 4 ||
        index < 0 || index >= NEOPIXEL_LED_COUNT ||
        !parseByte(r, command.led.r) || !parseByte(g, command.led.g) ||
        !parseByte(b, command.led.b)) {
      snprintf(command.error, sizeof(command.error), "LED_PIXEL requires index 0..%u and RGB 0..255", NEOPIXEL_LED_COUNT - 1);
      return command;
    }
    command.type = CommandType::LED_PIXEL;
    command.led.type = LedCommandType::SET_PIXEL;
    command.led.index = index;
  } else if (strcmp(name, "LED_PATTERN") == 0) {
    char pattern[16] = {};
    if (sscanf(line, "%*s %15s", pattern) != 1 ||
        !parsePattern(pattern, command.led.pattern)) {
      snprintf(command.error, sizeof(command.error), "LED_PATTERN requires OFF|SOLID|PACIFICA|FIRE");
      return command;
    }
    command.type = CommandType::LED_PATTERN;
    command.led.type = LedCommandType::SET_PATTERN;
  } else if (strcmp(name, "LED_BRIGHTNESS") == 0) {
    int value;
    if (sscanf(line, "%*s %d", &value) != 1 || value < 0 ||
        value > NEOPIXEL_BRIGHTNESS_MAX) {
      snprintf(command.error, sizeof(command.error), "LED_BRIGHTNESS requires 0..%u", NEOPIXEL_BRIGHTNESS_MAX);
      return command;
    }
    command.type = CommandType::LED_BRIGHTNESS;
    command.led.type = LedCommandType::SET_BRIGHTNESS;
    command.led.value = value;
  } else if (strcmp(name, "LED_PARAM") == 0) {
    char parameter[16] = {};
    int value;
    if (sscanf(line, "%*s %15s %d", parameter, &value) != 2 ||
        !parseParameter(parameter, command.led.parameter) ||
        !parseByte(value, command.led.value) ||
        (command.led.parameter == LedParameter::BRIGHTNESS &&
         value > NEOPIXEL_BRIGHTNESS_MAX)) {
      snprintf(command.error, sizeof(command.error), "LED_PARAM requires parameter and value 0..255");
      return command;
    }
    command.type = CommandType::LED_PARAM;
    command.led.type = LedCommandType::SET_PARAMETER;
  }
  else if (strcmp(name, "TEST_A") == 0 || strcmp(name, "TEST_B") == 0) {
    if (sscanf(line, "%*s %ld", &command.steps) != 1) {
      snprintf(command.error, sizeof(command.error), "%s requires <steps>", name);
      return command;
    }
    command.type = strcmp(name, "TEST_A") == 0 ? CommandType::TEST_A
                                                : CommandType::TEST_B;
  } else if (strcmp(name, "XY") == 0) {
    if (sscanf(line, "%*s %f %f %f", &command.x_mm, &command.y_mm,
               &command.feed_mm_min) != 3) {
      snprintf(command.error, sizeof(command.error),
               "XY requires <x_mm> <y_mm> <feed_mm_min>");
      return command;
    }
    command.type = CommandType::XY;
  } else {
    snprintf(command.error, sizeof(command.error), "unknown command: %s", name);
  }
  return command;
}
