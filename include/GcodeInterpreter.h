#pragma once

#include "CommandMessage.h"
#include "MachineState.h"
#include "ParsedGcode.h"

enum class GcodeInterpreterResult {
  COMMAND,
  MODAL_UPDATE,
  ERROR,
};

class GcodeInterpreter {
 public:
  GcodeInterpreterResult interpret(const ParsedGcode& parsed,
                                   const MachineState& machine,
                                   CommandMessage& output, char* log,
                                   unsigned int log_size);

  bool unitsInches() const { return units_inches_; }
  bool absoluteMode() const { return absolute_mode_; }

 private:
  bool units_inches_ = false;
  bool absolute_mode_ = true;
};
