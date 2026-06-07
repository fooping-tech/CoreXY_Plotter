#include "GcodeInterpreter.h"

#include "PlotterConfig.h"

#include <stdio.h>

namespace {
constexpr float INCH_TO_MM = 25.4f;

void formatLog(char* log, unsigned int log_size, const char* message) {
  if (log != nullptr && log_size > 0) {
    snprintf(log, log_size, "%s", message);
  }
}
}

GcodeInterpreterResult GcodeInterpreter::interpret(const ParsedGcode& parsed,
                                                   const MachineState& machine,
                                                   CommandMessage& output,
                                                   char* log,
                                                   unsigned int log_size) {
  output = CommandMessage{};
  switch (parsed.type) {
    case ParsedGcodeType::G20:
      units_inches_ = true;
      formatLog(log, log_size, "GCODE units=INCH X/Y converted to mm; F remains mm/min");
      return GcodeInterpreterResult::MODAL_UPDATE;
    case ParsedGcodeType::G21:
      units_inches_ = false;
      formatLog(log, log_size, "GCODE units=MM");
      return GcodeInterpreterResult::MODAL_UPDATE;
    case ParsedGcodeType::G90:
      absolute_mode_ = true;
      formatLog(log, log_size, "GCODE distance=ABSOLUTE");
      return GcodeInterpreterResult::MODAL_UPDATE;
    case ParsedGcodeType::G91:
      absolute_mode_ = false;
      formatLog(log, log_size, "GCODE distance=RELATIVE");
      return GcodeInterpreterResult::MODAL_UPDATE;
    case ParsedGcodeType::G28:
      output.type = CommandType::HOME;
      snprintf(output.name, sizeof(output.name), "G28");
      return GcodeInterpreterResult::COMMAND;
    case ParsedGcodeType::M3:
      output.type = CommandType::PEN_DOWN;
      snprintf(output.name, sizeof(output.name), "M3");
      return GcodeInterpreterResult::COMMAND;
    case ParsedGcodeType::M5:
      output.type = CommandType::PEN_UP;
      snprintf(output.name, sizeof(output.name), "M5");
      return GcodeInterpreterResult::COMMAND;
    case ParsedGcodeType::M114:
      output.type = CommandType::POS;
      snprintf(output.name, sizeof(output.name), "M114");
      return GcodeInterpreterResult::COMMAND;
    case ParsedGcodeType::G0:
    case ParsedGcodeType::G1: {
      const float unit_scale = units_inches_ ? INCH_TO_MM : 1.0f;
      const float x_value_mm = parsed.has_x ? parsed.x * unit_scale : 0.0f;
      const float y_value_mm = parsed.has_y ? parsed.y * unit_scale : 0.0f;
      output.type = CommandType::XY;
      snprintf(output.name, sizeof(output.name),
               parsed.type == ParsedGcodeType::G0 ? "G0" : "G1");
      if (absolute_mode_) {
        output.x_mm = parsed.has_x ? x_value_mm : machine.x_mm;
        output.y_mm = parsed.has_y ? y_value_mm : machine.y_mm;
      } else {
        output.x_mm = machine.x_mm + (parsed.has_x ? x_value_mm : 0.0f);
        output.y_mm = machine.y_mm + (parsed.has_y ? y_value_mm : 0.0f);
      }
      output.feed_mm_min =
          parsed.has_f
              ? parsed.f_mm_min
              : (machine.feed_mm_min > 0.0f ? machine.feed_mm_min
                                            : DEFAULT_FEED_MM_MIN);
      return GcodeInterpreterResult::COMMAND;
    }
    case ParsedGcodeType::NONE:
      formatLog(log, log_size, "GCODE parse result is empty");
      return GcodeInterpreterResult::ERROR;
  }
  formatLog(log, log_size, "GCODE unsupported interpreter state");
  return GcodeInterpreterResult::ERROR;
}
