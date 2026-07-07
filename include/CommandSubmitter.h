#pragma once

#include "CommandMessage.h"

enum class CommandSubmitMode {
  SERIAL_COMPAT,
  HTTP_NOWAIT,
};

enum class CommandSubmitCode {
  ACCEPTED,
  INVALID,
  QUEUE_FULL,
  LED_QUEUE_FULL,
};

struct CommandSubmitResult {
  CommandSubmitCode code = CommandSubmitCode::INVALID;
  char command[16] = {};
  char reason[96] = {};
  uint16_t queue_depth = 0;
};

const char* commandSubmitCodeName(CommandSubmitCode code);
bool commandSubmitAccepted(CommandSubmitCode code);
CommandSubmitResult submitParsedCommand(CommandMessage command,
                                        CommandSubmitMode mode);
CommandSubmitResult submitCommandLine(const char* line,
                                      CommandSubmitMode mode,
                                      bool mark_gcode_source = false);
