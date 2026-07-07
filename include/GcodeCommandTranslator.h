#pragma once

#include "CommandMessage.h"
#include "GcodeInterpreter.h"

struct MachineState;

// GcodeInterpreterをmotionTaskのコマンド経路へ接続するグルー。
// 解釈結果のログ整形とジョブ開始時のmodalリセットを担当する。
class GcodeCommandTranslator {
 public:
  GcodeInterpreterResult translate(const CommandMessage& command,
                                   const MachineState& reference,
                                   CommandMessage& translated);

  // JOB_BEGIN時にmodal状態(単位/座標モード)とfeedを既定へ戻す。
  void resetModalStateForJob(MachineState& machine);

 private:
  GcodeInterpreter interpreter_;
};
