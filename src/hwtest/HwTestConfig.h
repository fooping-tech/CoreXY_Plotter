#pragma once

#include <stdint.h>

// ============================================================================
// Controller PCB hardware test firmware settings
// ============================================================================
// このファイルの値はhwtest env (`pio run -e m5stack-core2-hwtest`) だけで使う。
// 通常ファームウェアのPlotterConfig.hとは独立させ、初回通電を安全側に倒す。

// TMC_INITで電流未指定時に使うRMS電流 [mA]。
// 決め方: 新規PCBの初回通電なので通常値(TMC_NORMAL_RMS_CURRENT_MA)より低くし、
// モータが回ることを確認してから引数で上げる。
constexpr uint16_t HWTEST_TMC_DEFAULT_RMS_CURRENT_MA = 400;

// TMC_INITで許可する最大RMS電流 [mA]。通常profileを超えないようにする。
constexpr uint16_t HWTEST_TMC_MAX_RMS_CURRENT_MA = 1100;

// hwtestで使うmicrostep。通常profileと揃える。
constexpr uint16_t HWTEST_TMC_MICROSTEPS = 16;

// TMC2209のIOIN.VERSION期待値。
constexpr uint8_t HWTEST_TMC2209_VERSION = 0x21;

// MOTOR <A|B> <steps>の既定値と上限 [steps]。
// CoreXY単独モータ移動はXY斜め方向に動くため、作業範囲の半分程度に抑える。
// 80 steps/mmで1600 stepsはベルト20mm、XY各10mm相当。
constexpr int32_t HWTEST_MOTOR_DEFAULT_STEPS = 400;
constexpr int32_t HWTEST_MOTOR_MAX_STEPS = 1600;

// MOTORの既定/最大STEP周波数 [steps/s]。
constexpr uint32_t HWTEST_MOTOR_DEFAULT_SPEED_STEPS_S = 800;
constexpr uint32_t HWTEST_MOTOR_MAX_SPEED_STEPS_S = 4000;

// MOTOR完了待ちのタイムアウト余裕 [ms]。
constexpr uint32_t HWTEST_MOTOR_TIMEOUT_MARGIN_MS = 2000;

// MOTOR中にDRV_STATUS(open load等)を1回だけ読むための最小移動時間 [ms]。
// これより短い移動では読まない。モーション中の高頻度UART読みは禁止。
constexpr uint32_t HWTEST_MOTOR_MIN_SAMPLE_DURATION_MS = 300;

// STEP/DIRを静的レベルに設定してからIOINを読むまでの待ち [ms]。
constexpr uint32_t HWTEST_PIN_SETTLE_MS = 3;

// LIMIT_WATCHの最大時間 [s] とポーリング周期 [ms]。
constexpr uint32_t HWTEST_LIMIT_WATCH_MAX_S = 60;
constexpr uint32_t HWTEST_LIMIT_WATCH_DEFAULT_S = 10;
constexpr uint32_t HWTEST_LIMIT_POLL_MS = 2;

// LIMIT_WATCH中に個別ログを出すedge数の上限。チャタリング時のログ洪水を防ぐ。
constexpr uint16_t HWTEST_LIMIT_EDGE_LOG_MAX = 16;

// SELFTESTのlimit安定性サンプル数と間隔。
// 浮き(未接続/pull-up欠落)やノイズで値が揺れたらFAILにする。
constexpr uint16_t HWTEST_LIMIT_STABILITY_SAMPLES = 200;
constexpr uint32_t HWTEST_LIMIT_STABILITY_INTERVAL_MS = 1;

// SERVO_SWEEPの往復回数と1ステップ時間 [ms]。
constexpr uint8_t HWTEST_SERVO_SWEEP_CYCLES = 3;
constexpr uint8_t HWTEST_SERVO_SWEEP_STEP_DEG = 2;
constexpr uint32_t HWTEST_SERVO_SWEEP_STEP_MS = 20;

// LED_TESTの各色/各pixel表示時間 [ms] と輝度。
constexpr uint32_t HWTEST_LED_COLOR_MS = 700;
constexpr uint32_t HWTEST_LED_PIXEL_MS = 250;
constexpr uint8_t HWTEST_LED_TEST_LEVEL = 255;
constexpr uint8_t HWTEST_LED_BRIGHTNESS = 24;

// LCD再描画周期 [ms]。
constexpr uint32_t HWTEST_UI_REDRAW_MS = 200;

// Core1テストタスクがlimit状態をStatusへ反映する周期 [ms]。
constexpr uint32_t HWTEST_STATUS_POLL_MS = 20;
