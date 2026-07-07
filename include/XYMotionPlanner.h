#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CommandMessage.h"
#include "GcodeInterpreter.h"
#include "JunctionPlanner.h"
#include "MachineState.h"
#include "MotionBlock.h"
#include "PlannerQueue.h"
#include "SegmentGenerator.h"
#include "SegmentQueue.h"
#include "TrapezoidPlanner.h"

class JobController;
class SafetyManager;
class TimedSegmentExecutor;

// コマンド受信・job許可判定・G-code変換はMotionTask側の責務のため、
// 関数フックとして注入する。
struct XYMotionPlannerHooks {
  bool (*stop_for_abort)(const char* context) = nullptr;
  // lookahead収集用。wait_ticks(FreeRTOS tick)だけ待って次コマンドを取り出す。
  bool (*receive_next_command)(CommandMessage& command,
                               uint32_t wait_ticks) = nullptr;
  void (*stash_pending_command)(const CommandMessage& command) = nullptr;
  bool (*reject_disallowed)(const CommandMessage& command) = nullptr;
  GcodeInterpreterResult (*translate_gcode)(const CommandMessage& command,
                                            const MachineState& reference,
                                            CommandMessage& translated) =
      nullptr;
  void (*clear_pending_command)() = nullptr;
  void (*warn_if_drift_detected)() = nullptr;
};

// XYコマンドのblock生成→lookahead収集→junction/trapezoid計画→timed segment実行。
// planner/segment queueとplanner群を所有する。
class XYMotionPlanner {
 public:
  XYMotionPlanner(SafetyManager& safety, MachineState& machine,
                  JobController& job, TimedSegmentExecutor& executor,
                  const XYMotionPlannerHooks& hooks)
      : safety_(safety),
        machine_(machine),
        job_(job),
        executor_(executor),
        hooks_(hooks) {}

  // first_commandから始まるXYバッチを計画・実行する。
  bool handleBatch(const CommandMessage& first_command);

  // planner/segment queueをクリアする。clear_pendingならpending commandも破棄する。
  void clearQueues(const char* reason, bool clear_pending = true);

  bool plannerQueueEmpty() const { return planner_queue_.isEmpty(); }
  bool segmentQueueEmpty() const { return segment_queue_.isEmpty(); }

 private:
  bool buildBlock(const CommandMessage& command, float start_x_mm,
                  float start_y_mm, int32_t start_a_steps,
                  int32_t start_b_steps, MotionBlock& block);
  static bool isNoOpBlock(const MotionBlock& block);
  void acknowledgeNoOp(const MotionBlock& block, bool update_machine_state);
  bool planQueuedBlocks();
  bool executePlannedBlock(MotionBlock& block, size_t index, size_t count);

  TrapezoidPlanner trapezoid_planner_;
  JunctionPlanner junction_planner_;
  SegmentGenerator segment_generator_;
  SegmentQueue segment_queue_;
  PlannerQueue planner_queue_;
  SafetyManager& safety_;
  MachineState& machine_;
  JobController& job_;
  TimedSegmentExecutor& executor_;
  XYMotionPlannerHooks hooks_;
};
