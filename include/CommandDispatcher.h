#pragma once

#include "CommandMessage.h"

class CommandDispatcher {
 public:
  static CommandMessage parse(const char* line);
};
