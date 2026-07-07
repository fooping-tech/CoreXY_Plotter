#include "GcodeCommandTranslator.h"

#include "AppContext.h"
#include "MachineState.h"
#include "PlotterConfig.h"

GcodeInterpreterResult GcodeCommandTranslator::translate(
    const CommandMessage& command, const MachineState& reference,
    CommandMessage& translated) {
  char log[128] = {};
  const GcodeInterpreterResult result = interpreter_.interpret(
      command.gcode, reference, translated, log, sizeof(log));
  if (result == GcodeInterpreterResult::MODAL_UPDATE) {
    logMessage("%s", log);
  } else if (result == GcodeInterpreterResult::ERROR) {
    logMessage("ERROR: %s", log);
  } else if (translated.type == CommandType::XY) {
    logMessage("GCODE %s -> XY X=%.3f Y=%.3f F=%.3f mode=%s units=%s",
               command.name, translated.x_mm, translated.y_mm,
               translated.feed_mm_min,
               interpreter_.absoluteMode() ? "ABS" : "REL",
               interpreter_.unitsInches() ? "INCH" : "MM");
  } else if (translated.type == CommandType::DWELL) {
    logMessage("GCODE %s -> DWELL P=%lums", command.name,
               static_cast<unsigned long>(translated.dwell_ms));
  } else {
    logMessage("GCODE %s -> command %s", command.name, translated.name);
  }
  return result;
}

void GcodeCommandTranslator::resetModalStateForJob(MachineState& machine) {
  interpreter_.resetModalState();
  machine.feed_mm_min = DEFAULT_FEED_MM_MIN;
  logMessage("JOB modal reset units=MM distance=ABSOLUTE feed=%.3f",
             DEFAULT_FEED_MM_MIN);
}
