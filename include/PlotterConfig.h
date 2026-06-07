#pragma once

#include <stdint.h>

// ============================================================================
// Board / build mode
// ============================================================================
// 対象ボード。現状はM5Stack Core2固定。
#define BOARD_M5STACK_CORE2 1

// 1にするとモータ出力を行わず、SerialログだけでCoreXY計算を確認する。
// 実機を動かす場合は0。新しいmotion処理を試す最初の段階では1が安全。
#ifndef SIMULATION_MODE
#define SIMULATION_MODE 0
#endif

// ============================================================================
// Motion geometry / feed / acceleration
// ============================================================================
// 1mm移動するために必要なステップ数。
// 決め方: motor step数、microstep、pulley歯数、belt pitchから計算し、
// 実測で補正する。例: 80 steps/mmなら10mm移動で800step。
constexpr float STEPS_PER_MM = 80.0f;

// feed未指定時や初期化時に使う標準送り速度 [mm/min]。
// 決め方: 確実に脱調しない低めの速度から始める。
constexpr float DEFAULT_FEED_MM_MIN = 1200.0f;

// XYコマンドで許可する最大送り速度 [mm/min]。
// 決め方: MAX_MOTOR_SPEED_STEPS_S / STEPS_PER_MM * 60 以下にする。
// 5000 mm/minは、STEPS_PER_MM=80では約6667 steps/s。
constexpr float MAX_FEED_MM_MIN = 5000.0f;

// FastAccelStepperの初期速度 [steps/s]。
// 主に単軸テストや初期化時の保守的な値。通常XYはfeedから速度を決める。
constexpr uint32_t DEFAULT_MOTOR_SPEED_STEPS_S = 3000;

// モータSTEP周波数の上限 [steps/s]。
// 決め方: MAX_FEED_MM_MINをsteps/sへ換算した値より少し大きくする。
// 大きすぎると脱調、ノイズ、ドライバ発熱が増える。
constexpr uint32_t MAX_MOTOR_SPEED_STEPS_S = 7000;

// モータ加速度 [steps/s^2]。
// 決め方: 低めから上げ、脱調や振動が出ない値にする。
// ペンが紙に接触する描画では負荷が増えるため、まず保守的な値にする。
// DEFAULT_ACCEL_MM_S2 = DEFAULT_MOTOR_ACCEL_STEPS_S2 / STEPS_PER_MM。
constexpr uint32_t DEFAULT_MOTOR_ACCEL_STEPS_S2 = 3000;

// TrapezoidPlannerで使う加速度 [mm/s^2]。
// 上のsteps単位設定から自動換算するため、通常は直接変更しない。
constexpr float DEFAULT_ACCEL_MM_S2 = DEFAULT_MOTOR_ACCEL_STEPS_S2 / STEPS_PER_MM;

// 将来の加速度clamp用の上限 [mm/s^2]。
// 現状はDEFAULT_ACCEL_MM_S2と同じ値。
constexpr float MAX_ACCEL_MM_S2 = DEFAULT_MOTOR_ACCEL_STEPS_S2 / STEPS_PER_MM;

// soft limit [mm]。homing後の通常XY移動をこの範囲に制限する。
// 決め方: 実際にペン先が安全に動ける作業範囲から少し余裕を引いた値。
constexpr float X_MIN_MM = 0.0f;
constexpr float X_MAX_MM = 55.0f;
constexpr float Y_MIN_MM = 0.0f;
constexpr float Y_MAX_MM = 55.0f;

// ============================================================================
// Serial / stepper electrical timing
// ============================================================================
// USB Serialの通信速度。serial_tool側と一致させる。
constexpr uint32_t SERIAL_BAUD = 115200;

// DIRピン変更後、STEPを出すまで待つ時間 [us]。
// 決め方: ドライバのDIR setup timeより十分長くする。TMC2209では200usは保守的。
constexpr uint32_t DIR_CHANGE_DELAY_US = 200;

// ============================================================================
// Pen servo
// ============================================================================
// ペン上げ/下げのサーボ角度 [deg]。
// 決め方: 機構に合わせて、紙を擦らない上げ角と十分に接地する下げ角を実測する。
constexpr uint8_t PEN_UP_ANGLE_DEG = 60;
constexpr uint8_t PEN_DOWN_ANGLE_DEG = 80;

// ============================================================================
// Motor direction
// ============================================================================
// A/Bモータの回転方向反転。
// 決め方: direction-checkで+X/+Yが期待方向に動かなければ該当軸を反転する。
// CoreXY式自体は変えず、ここで物理配線/取り付け差を吸収する。
constexpr bool MOTOR_A_DIRECTION_INVERTED = false;
constexpr bool MOTOR_B_DIRECTION_INVERTED = false;

// ============================================================================
// Homing / limit switches
// ============================================================================
// homing機能の有効/無効。
// 実機ではtrue。初期bring-upでlimit配線が未完成ならfalseも検討する。
constexpr bool HOMING_ENABLED = true;

// 原点探索方向。-1は負方向、+1は正方向へ探す。
// 決め方: limit switchを置いた原点方向に合わせる。
constexpr int8_t HOMING_X_DIR = -1;
constexpr int8_t HOMING_Y_DIR = -1;

// fast seek速度 [mm/min]。最初にlimitへ向かう速度。
// 決め方: 確実に停止できる範囲で速くする。
constexpr float HOMING_SEEK_FEED_MM_MIN = 1200.0f;

// slow seek速度 [mm/min]。backoff後に再度limitへ当てる低速。
// 決め方: 原点再現性を優先して低くする。
constexpr float HOMING_SLOW_FEED_MM_MIN = 60.0f;

// 通常homing中にlimitへ当たった後、いったん逃げる距離 [mm]。
// 決め方: switchが確実にOFFへ戻る距離より大きくする。
constexpr float HOMING_BACKOFF_MM = 8.0f;

// homing開始時からlimitがONだった場合に逃げられる最大距離 [mm]。
// 決め方: 起動時にswitch上へ乗っている可能性を考え、通常backoffより大きくする。
constexpr float HOMING_START_BACKOFF_MM = 40.0f;

// homingで探索を許す最大移動距離 [mm]。
// 決め方: 実ストロークより大きく、異常時に無限移動しない値にする。
constexpr float HOMING_MAX_TRAVEL_X_MM = 100.0f;
constexpr float HOMING_MAX_TRAVEL_Y_MM = 100.0f;

// homing完了時にMachineStateへ設定する座標 [mm]。
// 原点switchを作業座標0にするなら0。
constexpr float HOMING_SET_X_MM = 0.0f;
constexpr float HOMING_SET_Y_MM = 0.0f;

// limit switchのdebounce時間 [ms]。
// 決め方: チャタリングが消える最小値。長すぎると検出が遅れる。
constexpr uint32_t HOMING_LIMIT_DEBOUNCE_MS = 30;

// homing中ではない通常移動で、原点から離れた位置なのにlimitがONになった時に、
// アラームへ入れるまでの継続時間 [ms]。
// 決め方: 配線ノイズや瞬間的な接触では止めず、本当にlimitが押された時だけ止める。
constexpr uint32_t HARD_LIMIT_UNEXPECTED_ALARM_MS = 500;

// trueならhoming完了前の通常XY移動を拒否する。
// 実機安全のためtrue推奨。ZEROはhoming扱いにしない。
constexpr bool HOMING_REQUIRE_HOMED_FOR_XY_MOVE = true;

// limit switchの有効極性。
// true: GPIO LOWでON。外付けpull-up + switchでGNDへ落とす配線。
// false: GPIO HIGHでON。
constexpr bool LIMIT_ACTIVE_LOW = true;

// ============================================================================
// NeoPixel status LEDs
// ============================================================================
// LED個数。実装しているNeoPixelの数に合わせる。
constexpr uint16_t NEOPIXEL_LED_COUNT = 8;

// 最大輝度。電源容量と眩しさを考えて低めに制限する。
constexpr uint8_t NEOPIXEL_BRIGHTNESS_MAX = 64;

// 起動時や通常設定で使う輝度。
constexpr uint8_t NEOPIXEL_BRIGHTNESS_DEFAULT = 24;

// LEDアニメーション更新周期 [ms]。小さいほど滑らかだがCPU負荷が増える。
constexpr uint32_t NEOPIXEL_FRAME_INTERVAL_MS = 33;

// 起動時のLED pattern。0はOFF。
constexpr uint8_t NEOPIXEL_INITIAL_PATTERN = 0;  // OFF

// ============================================================================
// TMC2209 normal profile
// ============================================================================
// 通常動作時のmicrostep。
// 決め方: 分解能、最大STEP周波数、静粛性のバランスで決める。
constexpr uint16_t TMC_NORMAL_MICROSTEPS = 16;

// 通常動作時のモータ電流 [mA RMS]。
// 決め方: 脱調しない最小値から始め、モータ/ドライバ温度を見て調整する。
constexpr uint16_t TMC_NORMAL_RMS_CURRENT_MA = 1100;

// trueならspreadCycle、falseならstealthChop寄りの設定。
// plotterの確実な駆動と高めの速度ではspreadCycle推奨。
constexpr bool TMC_NORMAL_SPREADCYCLE = true;

// TMCモジュール上のsense resistor値 [ohm]。
// 決め方: 使用モジュールの回路図/実装値に合わせる。
constexpr float TMC_R_SENSE_OHM = 0.11f;

// hold current比率。run currentに対する保持電流の割合。
// 小さいほど発熱は減るが、停止中に位置保持力が落ちる。
constexpr float TMC_HOLD_MULTIPLIER = 0.5f;

// TMCの電流senseレンジ設定。通常はモジュールと電流値に合わせる。
// falseのまま問題なければ変更しない。
constexpr bool TMC_CURRENT_VSENSE = false;

// TMC IHOLDDELAY。run電流からhold電流へ落とす遅延。
constexpr uint8_t TMC_IHOLDDELAY = 1;

// TMC TPOWERDOWN。停止後にpower downへ移るまでの時間設定。
constexpr uint8_t TMC_TPOWERDOWN = 20;

// spreadCycle chopper設定。基本はTMC2209の推奨値から始める。
// 異音、発熱、脱調がある場合だけ調整する。
constexpr uint8_t TMC_TOFF = 5;
constexpr uint8_t TMC_HSTRT = 5;
constexpr uint8_t TMC_HEND = 0;
constexpr uint8_t TMC_TBL = 2;

// StallGuardしきい値。現状は診断用placeholder寄り。
// 値が小さい/大きいと検出感度が変わるため、実機で調整する。
constexpr uint8_t TMC_SGTHRS_DEFAULT = 80;

// StallGuard/CoolStepの有効速度域しきい値。現状は広めに設定。
constexpr uint32_t TMC_TCOOLTHRS_DEFAULT = 0xFFFFF;

// ============================================================================
// Diagnostic motor melody
// ============================================================================
// 診断メロディ機能の有効/無効。
// TMC UARTとAモータ出力を確認するための機能。
constexpr bool MOTOR_MELODY_ENABLED = true;

// メロディ再生時だけ使うmicrostep。
// 音程を出しやすくするため通常動作とは別設定にする。
constexpr uint16_t MOTOR_MELODY_MICROSTEPS = 2;

// メロディ再生時の電流 [mA RMS]。
// 短時間動作だが、発熱と音量を見て調整する。
constexpr uint16_t MOTOR_MELODY_RMS_CURRENT_MA = 1200;

// メロディ再生時のchop mode。
constexpr bool MOTOR_MELODY_SPREADCYCLE = true;

// 音符間の隙間 [ms]。
constexpr uint16_t MOTOR_MELODY_NOTE_GAP_MS = 25;
