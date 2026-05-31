#pragma once

#include "MachineState.h"

class Diagnostics {
 public:
  static void printHelp();
  static void printConfig();
  static void printPosition(const StatusMessage& status);
  static bool runSelfTest();
};
