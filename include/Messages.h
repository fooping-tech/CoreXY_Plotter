#pragma once

#include "MachineState.h"

// タスク間キューで運ぶメッセージ型。
// コマンドはCommandMessage.h、LEDはLedTypes.hにあり、本ファイルと並列の関係。

// Core 1 → Core 0 (StatusQueue)。LCD/UI表示用の状態スナップショット。
struct StatusMessage {
  MachineState machine;
  bool x_limit_active = false;
  bool y_limit_active = false;
  bool x_limit_raw_active = false;
  bool y_limit_raw_active = false;

  StatusMessage() = default;
  StatusMessage(const MachineState& machine_state, bool x_limit, bool y_limit,
                bool x_limit_raw, bool y_limit_raw)
      : machine(machine_state),
        x_limit_active(x_limit),
        y_limit_active(y_limit),
        x_limit_raw_active(x_limit_raw),
        y_limit_raw_active(y_limit_raw) {}
};

// 各タスク → logTask (LogQueue)。Serialへ1行として出力される。
struct LogMessage {
  char text[192];
};

// ============================================================================
// Serial応答プロトコルのプレフィクス
// ============================================================================
// これらの文字列はtools/serial_tool/serial_send.pyとtools/webui/server.pyの
// expect/停止判定パターンに直結する。変更する場合はホスト側と同時に更新すること。
// 文字列リテラル結合で使えるようマクロで定義する。
// 各ファイルのlogMessage書式文字列には歴史的に直書きが残るが、意味は同一。
#define LOG_PREFIX_OK "OK: "
#define LOG_PREFIX_ERROR "ERROR: "
#define LOG_PREFIX_WARN "WARN: "
#define LOG_PREFIX_REJECT "REJECT: "
#define LOG_PREFIX_ACK_QUEUED "ACK QUEUED"
#define LOG_PREFIX_ACK_XY "ACK_XY"
#define LOG_PREFIX_NACK_XY "NACK_XY"
#define LOG_PREFIX_ACK_AB_TIMED "ACK_AB_TIMED"
#define LOG_PREFIX_NACK_AB_TIMED "NACK_AB_TIMED"
