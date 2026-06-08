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

// CoreXYではXY速度方向によってA/Bモータのstep周波数が変わる。
// 最悪条件は45度方向で、片側モータがXY空間のsqrt(2)倍の速度になる。
constexpr float COREXY_MAX_MOTOR_GAIN = 1.41421356237f;

// 最大feed算出時の安全率。理論上限をそのまま使わず余裕を残す。
constexpr float SPEED_SAFETY = 1.0f;//0.80f;

// 標準feedを最大feedの何割にするか。
// DEFAULT_FEEDとMAX_FEEDを分け、通常描画が常に限界速度にならないようにする。
constexpr float DEFAULT_FEED_RATIO = 1.0f;//0.80f;

// モータSTEP周波数の上限 [steps/s]。
// 大きすぎると脱調、ノイズ、ドライバ発熱が増える。
constexpr uint32_t MAX_MOTOR_SPEED_STEPS_S = 30000;

// XYコマンドで許可する最大送り速度 [mm/min]。
// CoreXY最悪条件を考慮し、片側モータstep周波数が
// MAX_MOTOR_SPEED_STEPS_Sを超えない範囲から安全率込みで導出する。
constexpr float MAX_FEED_MM_MIN =
    MAX_MOTOR_SPEED_STEPS_S * 60.0f /
    (STEPS_PER_MM * COREXY_MAX_MOTOR_GAIN) * SPEED_SAFETY;

// feed未指定時や初期化時に使う標準送り速度 [mm/min]。
// 最大feedから比率で導出し、標準速度と限界速度の意味を分ける。
constexpr float DEFAULT_FEED_MM_MIN = MAX_FEED_MM_MIN * DEFAULT_FEED_RATIO;

// FastAccelStepperの初期速度 [steps/s]。
// 主に単軸テストや初期化時の値。通常XYはfeedから速度を決める。
constexpr uint32_t DEFAULT_MOTOR_SPEED_STEPS_S = static_cast<uint32_t>(
    DEFAULT_FEED_MM_MIN * STEPS_PER_MM / 60.0f + 0.5f);

// TrapezoidPlannerで使うXY空間の加速度 [mm/s^2]。
// 決め方: 低めから上げ、脱調や振動が出ない値にする。
// ペンが紙に接触する描画では負荷が増えるため、実機で段階的に確認する。
constexpr float DEFAULT_ACCEL_MM_S2 = 10000.0f;

// モータ加速度 [steps/s^2]。
// CoreXY最悪条件を考慮し、XY空間加速度にsteps/mmとsqrt(2)を掛けて導出する。
constexpr uint32_t DEFAULT_MOTOR_ACCEL_STEPS_S2 = static_cast<uint32_t>(
    DEFAULT_ACCEL_MM_S2 * STEPS_PER_MM * COREXY_MAX_MOTOR_GAIN + 0.5f);

// 将来の加速度clamp用の上限 [mm/s^2]。
// 現状はDEFAULT_ACCEL_MM_S2と同じ値。
constexpr float MAX_ACCEL_MM_S2 = DEFAULT_ACCEL_MM_S2;

// JunctionPlannerのlook-aheadで使う許容コーナー偏差 [mm]。
// 小さいほど角で減速し、大きいほど角を速く通過する。実機で角の丸まりと脱調を確認する。
constexpr float JUNCTION_DEVIATION_MM = 0.05f;

// classic jerk相当の簡易上限 [mm/s]。0以下なら無効。
// junction deviationだけで角が速すぎる場合の安全側制限として残す。
constexpr float CLASSIC_JERK_LIMIT_MM_S = 80.0f;

// motionTaskが連続XYをPlannerQueueへまとめるために待つ最大時間 [ms]。
// 0にすると、その時点でCommandQueueに溜まっているXYだけをlook-ahead対象にする。
constexpr uint32_t LOOKAHEAD_BATCH_COLLECT_MS = 15;

static_assert(STEPS_PER_MM > 0.0f, "STEPS_PER_MM must be > 0");
static_assert(COREXY_MAX_MOTOR_GAIN >= 1.4142f,
              "COREXY_MAX_MOTOR_GAIN must account for CoreXY diagonal motor speed");
static_assert(SPEED_SAFETY > 0.0f && SPEED_SAFETY <= 1.0f,
              "SPEED_SAFETY must be in (0, 1]");
static_assert(DEFAULT_FEED_RATIO > 0.0f && DEFAULT_FEED_RATIO <= 1.0f,
              "DEFAULT_FEED_RATIO must be in (0, 1]");
static_assert(MAX_MOTOR_SPEED_STEPS_S > 0,
              "MAX_MOTOR_SPEED_STEPS_S must be > 0");
static_assert(DEFAULT_FEED_MM_MIN <= MAX_FEED_MM_MIN,
              "DEFAULT_FEED_MM_MIN must be <= MAX_FEED_MM_MIN");
static_assert(DEFAULT_MOTOR_SPEED_STEPS_S <= MAX_MOTOR_SPEED_STEPS_S,
              "DEFAULT_MOTOR_SPEED_STEPS_S must be <= MAX_MOTOR_SPEED_STEPS_S");
static_assert(
    MAX_FEED_MM_MIN * STEPS_PER_MM * COREXY_MAX_MOTOR_GAIN / 60.0f
        <= MAX_MOTOR_SPEED_STEPS_S,
    "MAX_FEED_MM_MIN exceeds MAX_MOTOR_SPEED_STEPS_S in CoreXY worst case");
static_assert(DEFAULT_ACCEL_MM_S2 > 0.0f, "DEFAULT_ACCEL_MM_S2 must be > 0");
static_assert(MAX_ACCEL_MM_S2 >= DEFAULT_ACCEL_MM_S2,
              "MAX_ACCEL_MM_S2 must be >= DEFAULT_ACCEL_MM_S2");
static_assert(DEFAULT_MOTOR_ACCEL_STEPS_S2 > 0,
              "DEFAULT_MOTOR_ACCEL_STEPS_S2 must be > 0");
static_assert(JUNCTION_DEVIATION_MM > 0.0f,
              "JUNCTION_DEVIATION_MM must be > 0");

// AB_TIMED診断コマンドで許可する最小duration [us]。
// 短すぎるtimed moveはFastAccelStepper queueや機械側の切り分けに向かないため拒否する。
constexpr uint32_t AB_TIMED_MIN_DURATION_US = 1000;

// soft limit [mm]。homing後の通常XY移動をこの範囲に制限する。
// 決め方: 実際にペン先が安全に動ける作業範囲から少し余裕を引いた値。
constexpr float X_MIN_MM = 0.0f;
constexpr float X_MAX_MM = 55.0f;
constexpr float Y_MIN_MM = 0.0f;
constexpr float Y_MAX_MM = 55.0f;

// JOB_END時の退避位置 [mm]。
// 描画物からペンを離し、次ジョブの準備状態を見やすくする。
constexpr bool JOB_END_PARK_ENABLED = true;
constexpr float JOB_END_PARK_X_MM = 5.0f;
constexpr float JOB_END_PARK_Y_MM = Y_MAX_MM - 5.0f;
constexpr float JOB_END_PARK_FEED_MM_MIN = 1200.0f;

// JOB_BEGIN時にhomedでない場合、自動でHOMEを実行するか。
// trueにするとJOB_BEGINで実機が動くため、limit switchとE-stop確認後に有効化する。
constexpr bool JOB_BEGIN_AUTO_HOME = true;

static_assert(JOB_END_PARK_X_MM >= X_MIN_MM && JOB_END_PARK_X_MM <= X_MAX_MM,
              "JOB_END_PARK_X_MM must stay inside X soft limits");
static_assert(JOB_END_PARK_Y_MM >= Y_MIN_MM && JOB_END_PARK_Y_MM <= Y_MAX_MM,
              "JOB_END_PARK_Y_MM must stay inside Y soft limits");

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
// debounce後も継続するlimit ONは衝突または配線異常として速やかに止める。
constexpr uint32_t HARD_LIMIT_UNEXPECTED_ALARM_MS = 20;

// homing完了直後の通常移動で、原点limitがONのまま離れる方向へ動き出した時に、
// limitがOFFへ戻るまで許す最大移動距離 [mm]。
// これを超えてもOFFにならない場合はswitchが戻っていない、配線が短絡している、
// または移動方向が想定と違う可能性があるためalarmにする。
constexpr float NORMAL_MOVE_LIMIT_RELEASE_MM = 8.0f;

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

// JOB_END時の終了ジングル。
// 既存曲そのものではなく、短い8-bit風のオリジナル和音パターンを鳴らす。
constexpr bool JOB_END_JINGLE_ENABLED = true;
