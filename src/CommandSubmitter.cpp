#include "CommandSubmitter.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "AppContext.h"
#include "CommandDispatcher.h"

namespace {
const char* queueDepthText(uint16_t depth) {
  static char buffer[8];
  snprintf(buffer, sizeof(buffer), "%u", depth);
  return buffer;
}

uint16_t commandQueueDepth() {
  return command_queue == nullptr ? 0 : uxQueueMessagesWaiting(command_queue);
}

bool isLedCommand(CommandType type) {
  return type == CommandType::LED || type == CommandType::LED_PIXEL ||
         type == CommandType::LED_OFF || type == CommandType::LED_PATTERN ||
         type == CommandType::LED_BRIGHTNESS || type == CommandType::LED_PARAM ||
         type == CommandType::LED_AUTO ||
         type == CommandType::LED_STATUS_SET ||
         type == CommandType::LED_STATUS;
}

bool isReliableCommand(CommandType type) {
  return type == CommandType::GCODE || type == CommandType::XY ||
         type == CommandType::AB_TIMED || type == CommandType::JOB_END;
}

void copyText(char* destination, size_t size, const char* source) {
  if (size == 0) return;
  snprintf(destination, size, "%s", source != nullptr ? source : "");
}

CommandSubmitResult makeResult(CommandSubmitCode code, const CommandMessage& command,
                               const char* reason) {
  CommandSubmitResult result{};
  result.code = code;
  copyText(result.command, sizeof(result.command), command.name);
  copyText(result.reason, sizeof(result.reason), reason);
  result.queue_depth = commandQueueDepth();
  return result;
}

CommandSubmitResult queueReliableSerialCommand(const CommandMessage& command) {
  bool logged_wait = false;
  for (;;) {
    if (xQueueSend(command_queue, &command, pdMS_TO_TICKS(50)) == pdTRUE) {
      logMessage("ACK QUEUED %s", command.name);
      return makeResult(CommandSubmitCode::ACCEPTED, command, "queued");
    }
    if (!logged_wait) {
      logMessage("CommandQueue full; waiting to queue %s", command.name);
      logged_wait = true;
    }
    (void)isMotionAbortRequested();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

CommandSubmitResult queueStandardCommand(const CommandMessage& command,
                                         TickType_t wait_ticks) {
  if (xQueueSend(command_queue, &command, wait_ticks) == pdTRUE) {
    logMessage("ACK QUEUED %s", command.name);
    return makeResult(CommandSubmitCode::ACCEPTED, command, "queued");
  }
  logMessage("ERROR: CommandQueue full");
  return makeResult(CommandSubmitCode::QUEUE_FULL, command, "queue_full");
}
}

const char* commandSubmitCodeName(CommandSubmitCode code) {
  switch (code) {
    case CommandSubmitCode::ACCEPTED:
      return "accepted";
    case CommandSubmitCode::INVALID:
      return "invalid";
    case CommandSubmitCode::QUEUE_FULL:
      return "queue_full";
    case CommandSubmitCode::LED_QUEUE_FULL:
      return "led_queue_full";
  }
  return "unknown";
}

bool commandSubmitAccepted(CommandSubmitCode code) {
  return code == CommandSubmitCode::ACCEPTED;
}

CommandSubmitResult submitParsedCommand(CommandMessage command,
                                        CommandSubmitMode mode) {
  if (command.type == CommandType::INVALID) {
    logMessage("ERROR: %s", command.error);
    return makeResult(CommandSubmitCode::INVALID, command, command.error);
  }

  if (command.type == CommandType::ABORT ||
      command.type == CommandType::JOB_ABORT) {
    requestMotionAbort();
    if (xQueueSend(command_queue, &command, 0) == pdTRUE) {
      logMessage("ACK QUEUED %s", command.name);
      return makeResult(CommandSubmitCode::ACCEPTED, command, "queued");
    }
    logMessage("ACK %s requested", command.name);
    return makeResult(CommandSubmitCode::ACCEPTED, command, "abort_requested");
  }

  if (isLedCommand(command.type)) {
    if (xQueueSend(led_command_queue, &command.led, 0) == pdTRUE) {
      logMessage("ACK QUEUED %s", command.name);
      return makeResult(CommandSubmitCode::ACCEPTED, command, "queued");
    }
    logMessage("ERROR: LedCommandQueue full");
    return makeResult(CommandSubmitCode::LED_QUEUE_FULL, command,
                      "led_queue_full");
  }

  if (mode == CommandSubmitMode::SERIAL_COMPAT &&
      isReliableCommand(command.type)) {
    return queueReliableSerialCommand(command);
  }

  const TickType_t wait_ticks = mode == CommandSubmitMode::SERIAL_COMPAT
                                   ? pdMS_TO_TICKS(50)
                                   : 0;
  return queueStandardCommand(command, wait_ticks);
}

CommandSubmitResult submitCommandLine(const char* line, CommandSubmitMode mode,
                                      bool mark_gcode_source) {
  CommandMessage command = CommandDispatcher::parse(line);
  if (mark_gcode_source && command.type == CommandType::GCODE) {
    command.from_gcode = true;
  }
  return submitParsedCommand(command, mode);
}
