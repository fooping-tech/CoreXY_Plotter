#pragma once

#include "Messages.h"

class Diagnostics {
 public:
  static void printHelp();
  static void printConfig();
  static void printPosition(const StatusMessage& status);
  static void printLimitStatus(const StatusMessage& status);
  static void printHomingStatus(const StatusMessage& status);
  static bool runSelfTest();
};
