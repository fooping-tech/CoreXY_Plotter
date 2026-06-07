#pragma once

#include "ParsedGcode.h"

class GcodeParser {
 public:
  static bool parse(const char* line, ParsedGcode& parsed, char* error,
                    unsigned int error_size);
};
