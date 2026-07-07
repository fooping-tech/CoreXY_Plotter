#pragma once

#include "LedTypes.h"
#include "MachineState.h"
#include "MotionSyncTracker.h"
#include "SegmentQueue.h"

class SafetyManager;
class StepperBackendFastAccel;

// abort停止・motion active・alarm突入はMotionTask側の方針に依存するため、
// 関数フックとして注入する(グローバル参照を増やさない)。
struct TimedSegmentExecutorHooks {
  // 停止要求があれば処理してtrueを返す。
  bool (*stop_for_abort)(const char* context) = nullptr;
  void (*set_motion_active)(bool active) = nullptr;
  void (*enter_alarm)(const char* alarm_reason, const char* homed_reason,
                      LedStatus led_status) = nullptr;
};

// SegmentQueueのtimed segmentをbackendへ投入・監視する実行エンジン。
// safety pollとMachineState位置推定を実行中に継続する。
class TimedSegmentExecutor {
 public:
  TimedSegmentExecutor(StepperBackendFastAccel& backend, SafetyManager& safety,
                       MachineState& machine,
                       const TimedSegmentExecutorHooks& hooks)
      : backend_(backend), safety_(safety), machine_(machine), hooks_(hooks) {}

  MotionSyncReference captureReference() const;

  // backend停止まで待つ。abort/alarm時はfalse。位置推定を随時更新する。
  bool waitForMotionOrLimit(const MotionSyncReference& reference);
  bool waitForMotionOrLimit();

  // queue内の全segmentを投入して完了まで待つ。
  bool executeQueue(SegmentQueue& queue);

  // 投入失敗時の位置信頼性喪失処理(backend再同期+alarm)。
  void handleQueueError(const MotionSyncReference& reference,
                        const char* context);

 private:
  bool queueSegmentWithRetry(const MotionSegment& segment, bool start,
                             const MotionSyncReference& reference);
  void updateEstimate(const MotionSyncReference& reference);

  StepperBackendFastAccel& backend_;
  SafetyManager& safety_;
  MachineState& machine_;
  TimedSegmentExecutorHooks hooks_;
};
