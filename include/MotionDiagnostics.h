#pragma once

#include <stdint.h>

#include "CommandMessage.h"

class TimedSegmentExecutor;

struct MotionDiagnosticHooks {
  bool (*stop_for_abort)(const char* context) = nullptr;
  void (*set_motion_active)(bool active) = nullptr;
  void (*invalidate_homed)(const char* reason) = nullptr;
};

// AB_TIMED診断コマンドの実行経路。XY/planner/segment生成をバイパスし、
// timed segmentを直接stepper backendへ投入して切り分けに使う。
void runAbTimedDiagnostic(const CommandMessage& command,
                          TimedSegmentExecutor& executor,
                          const MotionDiagnosticHooks& hooks);

// TEST_A/TEST_B用のbring-up診断。単独モータmoveでhomedを無効化する。
void runSingleMotorDiagnostic(bool motor_a, int32_t steps,
                              TimedSegmentExecutor& executor,
                              const MotionDiagnosticHooks& hooks);
