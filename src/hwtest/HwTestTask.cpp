// Core 1: hardware test executor.
// Owns limit inputs, STEP/DIR static probe, TMC2209Manager, stepper backend
// and PenController. Never touches the LCD.

#include <Arduino.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "Core2PinMap.h"
#include "PenController.h"
#include "PlotterConfig.h"
#include "StepperBackendFastAccel.h"
#include "TMC2209Manager.h"
#include "hwtest/HwTestConfig.h"
#include "hwtest/HwTestShared.h"

extern StepperBackendFastAccel stepper_backend;
extern TMC2209Manager tmc_manager;
extern PenController pen_controller;

namespace {
// ---------------------------------------------------------------------------
// TMC2209 register bit positions (datasheet IOIN 0x06, GSTAT 0x01,
// DRV_STATUS 0x6F).
// ---------------------------------------------------------------------------
constexpr uint8_t IOIN_ENN = 0;
constexpr uint8_t IOIN_MS1 = 2;
constexpr uint8_t IOIN_MS2 = 3;
constexpr uint8_t IOIN_DIAG = 4;
constexpr uint8_t IOIN_PDN_UART = 6;
constexpr uint8_t IOIN_STEP = 7;
constexpr uint8_t IOIN_SPREAD_EN = 8;
constexpr uint8_t IOIN_DIR = 9;
constexpr uint8_t GSTAT_RESET = 0;
constexpr uint8_t GSTAT_DRV_ERR = 1;
constexpr uint8_t GSTAT_UV_CP = 2;
constexpr uint8_t DRV_OTPW = 0;
constexpr uint8_t DRV_OT = 1;
constexpr uint8_t DRV_S2GA = 2;
constexpr uint8_t DRV_S2GB = 3;
constexpr uint8_t DRV_S2VSA = 4;
constexpr uint8_t DRV_S2VSB = 5;
constexpr uint8_t DRV_OLA = 6;
constexpr uint8_t DRV_OLB = 7;
constexpr uint8_t DRV_STST = 31;

inline uint8_t bitOf(uint32_t value, uint8_t bit) { return (value >> bit) & 1U; }

// ---------------------------------------------------------------------------
// STEP/DIR static level probe.
// Diagnostic only: drives DC levels for multimeter/IOIN readback. It never
// generates step trains; once a MOTOR command runs, the pins are handed to
// FastAccelStepper (via StepperBackendFastAccel) until reboot.
// ---------------------------------------------------------------------------
constexpr uint8_t PROBE_PIN_COUNT = 4;
constexpr uint8_t PROBE_PINS[PROBE_PIN_COUNT] = {
    MOTOR_A_STEP_PIN, MOTOR_A_DIR_PIN, MOTOR_B_STEP_PIN, MOTOR_B_DIR_PIN};
constexpr const char* PROBE_PIN_NAMES[PROBE_PIN_COUNT] = {"A_STEP", "A_DIR",
                                                          "B_STEP", "B_DIR"};

class StepDirProbe {
 public:
  void begin() {
    for (uint8_t pin : PROBE_PINS) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
    for (bool& level : levels_) level = false;
  }
  bool available() const { return !released_; }
  void set(uint8_t index, bool high) {
    if (released_ || index >= PROBE_PIN_COUNT) return;
    digitalWrite(PROBE_PINS[index], high ? HIGH : LOW);
    levels_[index] = high;
  }
  void allLow() {
    for (uint8_t i = 0; i < PROBE_PIN_COUNT; ++i) set(i, false);
  }
  void releaseToBackend() {
    allLow();
    released_ = true;
  }

 private:
  bool released_ = false;
  bool levels_[PROBE_PIN_COUNT] = {};
};

StepDirProbe probe;
HwStatus status;
bool backend_started = false;
uint32_t last_status_poll_ms = 0;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
void setEvent(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(status.last_event, sizeof(status.last_event), format, args);
  va_end(args);
}

bool limitRawHigh(uint8_t pin) { return digitalRead(pin) == HIGH; }
bool limitActiveFromRaw(bool raw_high) {
  return LIMIT_ACTIVE_LOW ? !raw_high : raw_high;
}

void refreshLimits() {
  status.x_limit_raw_high = limitRawHigh(X_LIMIT_PIN);
  status.y_limit_raw_high = limitRawHigh(Y_LIMIT_PIN);
  status.x_limit_active = limitActiveFromRaw(status.x_limit_raw_high);
  status.y_limit_active = limitActiveFromRaw(status.y_limit_raw_high);
}

void publishStatus() {
  refreshLimits();
  status.pins_released_to_backend = !probe.available();
  if (backend_started) {
    status.motor_running = stepper_backend.isRunning();
    status.motor_a_steps = stepper_backend.currentASteps();
    status.motor_b_steps = stepper_backend.currentBSteps();
  }
  xQueueOverwrite(hw_status_queue, &status);
}

const char* axisName(TMC2209Axis axis) {
  return axis == TMC2209Axis::A ? "A" : "B";
}

void rememberTmc(TMC2209Axis axis, const TMC2209Diagnostics& d) {
  if (axis == TMC2209Axis::A) {
    status.tmc_a_connection = d.connection;
    status.tmc_a_version = d.version;
  } else {
    status.tmc_b_connection = d.connection;
    status.tmc_b_version = d.version;
  }
}

// Reads one driver, logs decoded registers and returns the number of FAILs.
uint8_t checkTmc(TMC2209Axis axis, bool include_drv_status) {
  const char* name = axisName(axis);
  TMC2209Diagnostics d;
  tmc_manager.beginUart();
  tmc_manager.readDiagnostics(axis, d);
  rememberTmc(axis, d);
  if (d.connection != 0) {
    logMessage("FAIL TMC_%s addr=%u no valid UART reply (connection=%u). "
               "Check VM power, PDN_UART wiring/1k resistor, GPIO%u/%u, MS1/MS2 address",
               name, d.address, d.connection, TMC_UART_TX_PIN, TMC_UART_RX_PIN);
    return 1;
  }
  uint8_t fails = 0;
  const uint8_t ms1 = bitOf(d.ioin, IOIN_MS1);
  const uint8_t ms2 = bitOf(d.ioin, IOIN_MS2);
  logMessage("TMC_%s addr=%u ver=0x%02X IOIN=0x%08lX ENN=%u MS1=%u MS2=%u DIAG=%u PDN_UART=%u STEP=%u DIR=%u SPREAD=%u IFCNT=%u GCONF=0x%03lX",
             name, d.address, d.version, static_cast<unsigned long>(d.ioin),
             bitOf(d.ioin, IOIN_ENN), ms1, ms2, bitOf(d.ioin, IOIN_DIAG),
             bitOf(d.ioin, IOIN_PDN_UART), bitOf(d.ioin, IOIN_STEP),
             bitOf(d.ioin, IOIN_DIR), bitOf(d.ioin, IOIN_SPREAD_EN), d.ifcnt,
             static_cast<unsigned long>(d.gconf));
  if (d.version != HWTEST_TMC2209_VERSION) {
    logMessage("FAIL TMC_%s version 0x%02X != 0x%02X", name, d.version,
               HWTEST_TMC2209_VERSION);
    ++fails;
  }
  const uint8_t pin_address = ms1 | (ms2 << 1);
  if (pin_address != d.address) {
    logMessage("FAIL TMC_%s MS1/MS2 address=%u expected %u", name, pin_address,
               d.address);
    ++fails;
  }
  if (bitOf(d.ioin, IOIN_ENN) != 0) {
    logMessage("FAIL TMC_%s ENN is HIGH (expected EN tied to GND)", name);
    ++fails;
  }

  logMessage("TMC_%s GSTAT=0x%X reset=%u drv_err=%u uv_cp=%u", name, d.gstat,
             bitOf(d.gstat, GSTAT_RESET), bitOf(d.gstat, GSTAT_DRV_ERR),
             bitOf(d.gstat, GSTAT_UV_CP));
  if (bitOf(d.gstat, GSTAT_DRV_ERR)) {
    logMessage("FAIL TMC_%s drv_err latched (overtemp/short shutdown)", name);
    ++fails;
  }
  if (bitOf(d.gstat, GSTAT_UV_CP)) {
    logMessage("FAIL TMC_%s uv_cp (charge pump undervoltage: VM too low?)", name);
    ++fails;
  }

  if (include_drv_status) {
    const uint32_t s = d.drv_status;
    logMessage("TMC_%s DRV_STATUS=0x%08lX ot=%u otpw=%u s2ga=%u s2gb=%u s2vsa=%u s2vsb=%u ola=%u olb=%u cs=%lu stst=%u sg=%u",
               name, static_cast<unsigned long>(s), bitOf(s, DRV_OT),
               bitOf(s, DRV_OTPW), bitOf(s, DRV_S2GA), bitOf(s, DRV_S2GB),
               bitOf(s, DRV_S2VSA), bitOf(s, DRV_S2VSB), bitOf(s, DRV_OLA),
               bitOf(s, DRV_OLB), static_cast<unsigned long>((s >> 16) & 0x1F),
               bitOf(s, DRV_STST), d.sg_result);
    if (bitOf(s, DRV_OT)) {
      logMessage("FAIL TMC_%s overtemperature", name);
      ++fails;
    }
    if (bitOf(s, DRV_S2GA) || bitOf(s, DRV_S2GB) || bitOf(s, DRV_S2VSA) ||
        bitOf(s, DRV_S2VSB)) {
      logMessage("FAIL TMC_%s short detected on motor output", name);
      ++fails;
    }
    if (bitOf(s, DRV_OTPW)) logMessage("WARN TMC_%s overtemperature prewarning", name);
  }
  return fails;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------
void reportLimit() {
  refreshLimits();
  logMessage("LIMIT X(GPIO%u) raw=%s active=%s  Y(GPIO%u) raw=%s active=%s",
             X_LIMIT_PIN, status.x_limit_raw_high ? "HIGH" : "LOW",
             status.x_limit_active ? "ON" : "OFF", Y_LIMIT_PIN,
             status.y_limit_raw_high ? "HIGH" : "LOW",
             status.y_limit_active ? "ON" : "OFF");
}

// Samples both limit inputs repeatedly. Returns the number of FAILs.
uint8_t checkLimitStability() {
  const bool x0 = limitRawHigh(X_LIMIT_PIN);
  const bool y0 = limitRawHigh(Y_LIMIT_PIN);
  uint16_t x_changes = 0, y_changes = 0;
  bool x_prev = x0, y_prev = y0;
  for (uint16_t i = 0; i < HWTEST_LIMIT_STABILITY_SAMPLES; ++i) {
    const bool x = limitRawHigh(X_LIMIT_PIN);
    const bool y = limitRawHigh(Y_LIMIT_PIN);
    if (x != x_prev) ++x_changes;
    if (y != y_prev) ++y_changes;
    x_prev = x;
    y_prev = y;
    vTaskDelay(pdMS_TO_TICKS(HWTEST_LIMIT_STABILITY_INTERVAL_MS));
  }
  reportLimit();
  uint8_t fails = 0;
  const struct {
    const char* name;
    uint16_t changes;
    bool active;
  } results[] = {{"X", x_changes, status.x_limit_active},
                 {"Y", y_changes, status.y_limit_active}};
  for (const auto& r : results) {
    if (r.changes > 0) {
      logMessage("FAIL LIMIT_%s unstable: %u changes in %u samples (floating input or noise; check pull-up)",
                 r.name, r.changes, HWTEST_LIMIT_STABILITY_SAMPLES);
      ++fails;
    } else if (r.active) {
      logMessage("FAIL LIMIT_%s active while idle (switch pressed, wrong polarity or missing pull-up)",
                 r.name);
      ++fails;
    } else {
      logMessage("PASS LIMIT_%s stable and released", r.name);
    }
  }
  return fails;
}

void limitWatch(uint32_t seconds) {
  if (seconds == 0) seconds = HWTEST_LIMIT_WATCH_DEFAULT_S;
  if (seconds > HWTEST_LIMIT_WATCH_MAX_S) seconds = HWTEST_LIMIT_WATCH_MAX_S;
  hw_stop_requested = false;
  logMessage("LIMIT_WATCH start %lus. Press/release X and Y switches. STOP aborts.",
             static_cast<unsigned long>(seconds));
  reportLimit();
  bool x_prev = limitRawHigh(X_LIMIT_PIN);
  bool y_prev = limitRawHigh(Y_LIMIT_PIN);
  uint16_t x_edges = 0, y_edges = 0;
  uint32_t x_last_edge_ms = 0, y_last_edge_ms = 0;
  uint16_t x_bounces = 0, y_bounces = 0;  // edges within 20 ms of previous
  constexpr uint32_t BOUNCE_WINDOW_MS = 20;
  const uint32_t start_ms = millis();
  while (millis() - start_ms < seconds * 1000UL && !hw_stop_requested) {
    const uint32_t now = millis();
    const bool x = limitRawHigh(X_LIMIT_PIN);
    const bool y = limitRawHigh(Y_LIMIT_PIN);
    if (x != x_prev) {
      if (x_edges > 0 && now - x_last_edge_ms < BOUNCE_WINDOW_MS) ++x_bounces;
      if (++x_edges <= HWTEST_LIMIT_EDGE_LOG_MAX) {
        logMessage("LIMIT_EDGE X %s t=%lums", limitActiveFromRaw(x) ? "ON" : "OFF",
                   static_cast<unsigned long>(now - start_ms));
      }
      x_last_edge_ms = now;
      x_prev = x;
    }
    if (y != y_prev) {
      if (y_edges > 0 && now - y_last_edge_ms < BOUNCE_WINDOW_MS) ++y_bounces;
      if (++y_edges <= HWTEST_LIMIT_EDGE_LOG_MAX) {
        logMessage("LIMIT_EDGE Y %s t=%lums", limitActiveFromRaw(y) ? "ON" : "OFF",
                   static_cast<unsigned long>(now - start_ms));
      }
      y_last_edge_ms = now;
      y_prev = y;
    }
    if (now - last_status_poll_ms >= HWTEST_STATUS_POLL_MS) {
      last_status_poll_ms = now;
      publishStatus();
    }
    vTaskDelay(pdMS_TO_TICKS(HWTEST_LIMIT_POLL_MS));
  }
  const bool aborted = hw_stop_requested;
  hw_stop_requested = false;
  logMessage("LIMIT_WATCH DONE%s X edges=%u bounce=%u  Y edges=%u bounce=%u",
             aborted ? " (stopped)" : "", x_edges, x_bounces, y_edges,
             y_bounces);
  setEvent("LIMIT_WATCH X%u Y%u", x_edges, y_edges);
}

// Drives each STEP/DIR pin high alone and verifies via both TMC2209 IOIN
// registers that exactly the matching driver input follows. Detects open
// traces, A/B swaps, STEP/DIR swaps and shorts between these nets.
// Each STEP high level is one rising edge = one microstep.
uint8_t pinCheck() {
  if (!probe.available()) {
    logMessage("SKIP PIN_CHECK: STEP/DIR pins are owned by stepper backend after MOTOR. Reboot to rerun.");
    return 0;
  }
  tmc_manager.beginUart();
  uint8_t fails = 0;
  // pattern index: -1 = all low, 0..3 = PROBE_PINS[index] high.
  for (int8_t active = -1; active < static_cast<int8_t>(PROBE_PIN_COUNT); ++active) {
    probe.allLow();
    if (active >= 0) probe.set(active, true);
    vTaskDelay(pdMS_TO_TICKS(HWTEST_PIN_SETTLE_MS));
    TMC2209Diagnostics a, b;
    const bool ok_a = tmc_manager.readDiagnostics(TMC2209Axis::A, a);
    const bool ok_b = tmc_manager.readDiagnostics(TMC2209Axis::B, b);
    rememberTmc(TMC2209Axis::A, a);
    rememberTmc(TMC2209Axis::B, b);
    const char* label = active < 0 ? "ALL_LOW" : PROBE_PIN_NAMES[active];
    if (!ok_a || !ok_b) {
      logMessage("FAIL PIN_CHECK %s: TMC UART read failed (A=%u B=%u)", label,
                 a.connection, b.connection);
      ++fails;
      break;
    }
    const uint8_t got[PROBE_PIN_COUNT] = {
        bitOf(a.ioin, IOIN_STEP), bitOf(a.ioin, IOIN_DIR),
        bitOf(b.ioin, IOIN_STEP), bitOf(b.ioin, IOIN_DIR)};
    bool match = true;
    for (uint8_t i = 0; i < PROBE_PIN_COUNT; ++i) {
      if (got[i] != (static_cast<int8_t>(i) == active ? 1 : 0)) match = false;
    }
    logMessage("%s PIN_CHECK %-7s -> A.STEP=%u A.DIR=%u B.STEP=%u B.DIR=%u",
               match ? "PASS" : "FAIL", label, got[0], got[1], got[2], got[3]);
    if (!match) ++fails;
  }
  probe.allLow();
  return fails;
}

void pinSet(uint8_t index, bool high) {
  if (!probe.available()) {
    logMessage("SKIP PIN_SET: pins owned by stepper backend. Reboot to use PIN_SET.");
    return;
  }
  probe.set(index, high);
  logMessage("PIN_SET %s(GPIO%u)=%u DONE (measure at driver pin; reboot or PIN_SET 0 to clear)",
             PROBE_PIN_NAMES[index], PROBE_PINS[index], high ? 1 : 0);
  setEvent("PIN %s=%u", PROBE_PIN_NAMES[index], high ? 1 : 0);
}

void tmcPing() {
  uint8_t fails = checkTmc(TMC2209Axis::A, false);
  fails += checkTmc(TMC2209Axis::B, false);
  logMessage("TMC_PING %s fails=%u", fails == 0 ? "PASS" : "FAIL", fails);
  setEvent("TMC_PING %s", fails == 0 ? "PASS" : "FAIL");
}

void tmcInit(const HwCommand& command) {
  uint32_t current_ma = command.has_arg0 ? command.arg0
                                         : HWTEST_TMC_DEFAULT_RMS_CURRENT_MA;
  if (current_ma == 0 || current_ma > HWTEST_TMC_MAX_RMS_CURRENT_MA) {
    logMessage("ERROR: TMC_INIT current must be 1..%u mA",
               HWTEST_TMC_MAX_RMS_CURRENT_MA);
    return;
  }
  const TMC2209Profile profile = {HWTEST_TMC_MICROSTEPS,
                                  static_cast<uint16_t>(current_ma),
                                  TMC_NORMAL_SPREADCYCLE};
  tmc_manager.beginUart();
  const bool ready = tmc_manager.applyCustomProfile(profile, "hwtest");
  status.tmc_configured = ready;
  status.tmc_current_ma = ready ? current_ma : 0;
  logMessage("TMC_INIT %s current=%lumA microsteps=1/%u",
             ready ? "DONE" : "FAIL", static_cast<unsigned long>(current_ma),
             HWTEST_TMC_MICROSTEPS);
  setEvent("TMC_INIT %s %lumA", ready ? "OK" : "FAIL",
           static_cast<unsigned long>(current_ma));
}

void tmcStatus() {
  uint8_t fails = checkTmc(TMC2209Axis::A, true);
  fails += checkTmc(TMC2209Axis::B, true);
  if (status.tmc_configured) tmc_manager.printStatus();
  logMessage("TMC_STATUS DONE fails=%u configured=%s", fails,
             status.tmc_configured ? "YES" : "NO");
}

bool ensureBackend() {
  if (backend_started) return true;
  probe.releaseToBackend();
  if (!stepper_backend.begin()) {
    logMessage("FAIL MOTOR: FastAccelStepper backend init failed");
    return false;
  }
  backend_started = true;
  logMessage("MOTOR backend started; PIN_CHECK/PIN_SET disabled until reboot");
  return true;
}

void motorTest(const HwCommand& command) {
  int32_t steps = command.has_arg0 ? command.arg0 : HWTEST_MOTOR_DEFAULT_STEPS;
  uint32_t speed = command.has_arg1 ? command.arg1
                                    : HWTEST_MOTOR_DEFAULT_SPEED_STEPS_S;
  if (steps == 0 || abs(steps) > HWTEST_MOTOR_MAX_STEPS) {
    logMessage("ERROR: MOTOR steps must be 1..%ld (sign = direction)",
               static_cast<long>(HWTEST_MOTOR_MAX_STEPS));
    return;
  }
  if (speed > HWTEST_MOTOR_MAX_SPEED_STEPS_S) {
    logMessage("ERROR: MOTOR steps_s must be <= %lu",
               static_cast<unsigned long>(HWTEST_MOTOR_MAX_SPEED_STEPS_S));
    return;
  }
  if (!ensureBackend()) return;
  if (!status.tmc_configured) {
    logMessage("WARN MOTOR: TMC_INIT not done; driver runs on standalone Vref/MS pins");
  }
  const bool is_a = command.target == 'A';
  const TMC2209Axis axis = is_a ? TMC2209Axis::A : TMC2209Axis::B;
  const int32_t start_a = stepper_backend.currentASteps();
  const int32_t start_b = stepper_backend.currentBSteps();
  const bool x_start = status.x_limit_active;
  const bool y_start = status.y_limit_active;
  const float feed_mm_min = speed * 60.0f / STEPS_PER_MM;

  hw_stop_requested = false;
  logMessage("MOTOR %c start steps=%ld speed=%lu steps/s",
             command.target, static_cast<long>(steps),
             static_cast<unsigned long>(speed));
  // Single-motor diagnostic move through the backend (bring-up path).
  stepper_backend.moveABSteps(is_a ? steps : 0, is_a ? 0 : steps, feed_mm_min);

  const uint32_t expected_ms = static_cast<uint32_t>(
      static_cast<uint64_t>(abs(steps)) * 1000ULL / speed);
  const uint32_t timeout_ms = expected_ms + HWTEST_MOTOR_TIMEOUT_MARGIN_MS;
  const bool sample_enabled = expected_ms >= HWTEST_MOTOR_MIN_SAMPLE_DURATION_MS;
  bool sampled = false;
  TMC2209Diagnostics sample;
  const char* stop_reason = nullptr;
  const uint32_t start_ms = millis();
  vTaskDelay(pdMS_TO_TICKS(5));
  while (stepper_backend.isRunning()) {
    const uint32_t elapsed = millis() - start_ms;
    refreshLimits();
    if (hw_stop_requested) {
      stop_reason = "STOP command";
    } else if ((status.x_limit_active && !x_start) ||
               (status.y_limit_active && !y_start)) {
      stop_reason = status.x_limit_active && !x_start ? "X limit hit"
                                                     : "Y limit hit";
    } else if (elapsed > timeout_ms) {
      stop_reason = "timeout";
    }
    if (stop_reason != nullptr) {
      stepper_backend.stop();
      break;
    }
    // One low-frequency UART sample mid-move for open-load/short flags.
    if (sample_enabled && !sampled && elapsed >= expected_ms / 2) {
      tmc_manager.readDiagnostics(axis, sample);
      sampled = true;
    }
    if (millis() - last_status_poll_ms >= HWTEST_STATUS_POLL_MS) {
      last_status_poll_ms = millis();
      publishStatus();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  hw_stop_requested = false;
  const int32_t moved = is_a ? stepper_backend.currentASteps() - start_a
                             : stepper_backend.currentBSteps() - start_b;
  const int32_t other = is_a ? stepper_backend.currentBSteps() - start_b
                             : stepper_backend.currentASteps() - start_a;
  publishStatus();

  if (sampled && sample.connection == 0) {
    const uint32_t s = sample.drv_status;
    logMessage("MOTOR %c mid-move DRV_STATUS=0x%08lX ola=%u olb=%u s2ga=%u s2gb=%u ot=%u otpw=%u cs=%lu",
               command.target, static_cast<unsigned long>(s),
               bitOf(s, DRV_OLA), bitOf(s, DRV_OLB), bitOf(s, DRV_S2GA),
               bitOf(s, DRV_S2GB), bitOf(s, DRV_OT), bitOf(s, DRV_OTPW),
               static_cast<unsigned long>((s >> 16) & 0x1F));
    if (bitOf(s, DRV_OLA) || bitOf(s, DRV_OLB)) {
      logMessage("WARN MOTOR %c open load on coil %s%s (motor cable/connector?)",
                 command.target, bitOf(s, DRV_OLA) ? "A" : "",
                 bitOf(s, DRV_OLB) ? "B" : "");
    }
    if (bitOf(s, DRV_S2GA) || bitOf(s, DRV_S2GB) || bitOf(s, DRV_S2VSA) ||
        bitOf(s, DRV_S2VSB)) {
      logMessage("FAIL MOTOR %c short detected on motor output", command.target);
    }
  } else if (sample_enabled) {
    logMessage("WARN MOTOR %c mid-move TMC UART sample unavailable", command.target);
  }

  if (stop_reason != nullptr) {
    logMessage("MOTOR %c STOPPED reason=%s moved=%ld/%ld", command.target,
               stop_reason, static_cast<long>(moved), static_cast<long>(steps));
    setEvent("MOTOR %c stop:%s", command.target, stop_reason);
    return;
  }
  const bool ok = moved == steps && other == 0;
  logMessage("MOTOR %c DONE %s moved=%ld expected=%ld other=%ld time=%lums (confirm rotation/direction visually)",
             command.target, ok ? "PASS" : "FAIL", static_cast<long>(moved),
             static_cast<long>(steps), static_cast<long>(other),
             static_cast<unsigned long>(millis() - start_ms));
  setEvent("MOTOR %c %ld %s", command.target, static_cast<long>(steps),
           ok ? "OK" : "NG");
}

void servoAngle(int16_t angle_deg) {
  pen_controller.setAngleDeg(angle_deg);
  status.servo_angle_deg = angle_deg;
}

void servoSweep() {
  const int16_t low = min(PEN_UP_ANGLE_DEG, PEN_DOWN_ANGLE_DEG);
  const int16_t high = max(PEN_UP_ANGLE_DEG, PEN_DOWN_ANGLE_DEG);
  hw_stop_requested = false;
  logMessage("SERVO_SWEEP %d..%d deg x%u (STOP aborts)", low, high,
             HWTEST_SERVO_SWEEP_CYCLES);
  for (uint8_t cycle = 0; cycle < HWTEST_SERVO_SWEEP_CYCLES && !hw_stop_requested;
       ++cycle) {
    for (int16_t a = low; a <= high && !hw_stop_requested;
         a += HWTEST_SERVO_SWEEP_STEP_DEG) {
      servoAngle(a);
      vTaskDelay(pdMS_TO_TICKS(HWTEST_SERVO_SWEEP_STEP_MS));
    }
    for (int16_t a = high; a >= low && !hw_stop_requested;
         a -= HWTEST_SERVO_SWEEP_STEP_DEG) {
      servoAngle(a);
      vTaskDelay(pdMS_TO_TICKS(HWTEST_SERVO_SWEEP_STEP_MS));
    }
    publishStatus();
  }
  hw_stop_requested = false;
  pen_controller.penUp();
  status.servo_angle_deg = PEN_UP_ANGLE_DEG;
  logMessage("SERVO_SWEEP DONE (confirm motion visually)");
  setEvent("SERVO_SWEEP done");
}

void selftest() {
  status.selftest = HwSelftestResult::RUNNING;
  setEvent("SELFTEST running");
  publishStatus();
  logMessage("SELFTEST start (no motor motion)");

  logMessage("SELFTEST [1/3] limit inputs");
  uint8_t fails = checkLimitStability();

  logMessage("SELFTEST [2/3] TMC2209 UART");
  fails += checkTmc(TMC2209Axis::A, true);
  fails += checkTmc(TMC2209Axis::B, true);

  logMessage("SELFTEST [3/3] STEP/DIR -> TMC IOIN loopback");
  if (status.tmc_a_connection != 0 || status.tmc_b_connection != 0) {
    logMessage("SKIP PIN_CHECK: requires both TMC2209 UART replies");
  } else {
    fails += pinCheck();
  }

  status.selftest = fails == 0 ? HwSelftestResult::PASS : HwSelftestResult::FAIL;
  status.selftest_fail_count = fails;
  logMessage("SELFTEST manual checks: POWER, LED_TEST, PENUP/PENDOWN/SERVO_SWEEP, TMC_INIT + MOTOR A/B");
  logMessage("SELFTEST RESULT %s fails=%u", fails == 0 ? "PASS" : "FAIL", fails);
  setEvent("SELFTEST %s (%u)", fails == 0 ? "PASS" : "FAIL", fails);
}

void execute(const HwCommand& command) {
  switch (command.type) {
    case HwCommandType::SELFTEST:
      selftest();
      break;
    case HwCommandType::LIMIT:
      reportLimit();
      logMessage("LIMIT DONE");
      break;
    case HwCommandType::LIMIT_WATCH:
      limitWatch(command.has_arg0 ? command.arg0 : 0);
      break;
    case HwCommandType::PIN_CHECK: {
      const uint8_t fails = pinCheck();
      if (probe.available()) {
        logMessage("PIN_CHECK %s fails=%u", fails == 0 ? "PASS" : "FAIL", fails);
        setEvent("PIN_CHECK %s", fails == 0 ? "PASS" : "FAIL");
      }
      break;
    }
    case HwCommandType::PIN_SET:
      pinSet(command.arg0, command.arg1 != 0);
      break;
    case HwCommandType::TMC_PING:
      tmcPing();
      break;
    case HwCommandType::TMC_INIT:
      tmcInit(command);
      break;
    case HwCommandType::TMC_STATUS:
      tmcStatus();
      break;
    case HwCommandType::MOTOR:
      motorTest(command);
      break;
    case HwCommandType::STOP:
      if (backend_started) stepper_backend.stop();
      hw_stop_requested = false;
      logMessage("STOP DONE");
      break;
    case HwCommandType::SERVO:
      if (command.arg0 > 180) {
        logMessage("ERROR: SERVO angle must be 0..180");
        break;
      }
      servoAngle(command.arg0);
      logMessage("SERVO %ld deg DONE", static_cast<long>(command.arg0));
      setEvent("SERVO %ld", static_cast<long>(command.arg0));
      break;
    case HwCommandType::PENUP:
      pen_controller.penUp();
      status.servo_angle_deg = PEN_UP_ANGLE_DEG;
      logMessage("PENUP DONE angle=%u", PEN_UP_ANGLE_DEG);
      setEvent("PENUP");
      break;
    case HwCommandType::PENDOWN:
      pen_controller.penDown();
      status.servo_angle_deg = PEN_DOWN_ANGLE_DEG;
      logMessage("PENDOWN DONE angle=%u", PEN_DOWN_ANGLE_DEG);
      setEvent("PENDOWN");
      break;
    case HwCommandType::SERVO_SWEEP:
      servoSweep();
      break;
  }
}
}  // namespace

void hwTestTask(void*) {
  pinMode(X_LIMIT_PIN, INPUT);
  pinMode(Y_LIMIT_PIN, INPUT);
  probe.begin();
  pen_controller.begin();
  status.servo_angle_deg = PEN_UP_ANGLE_DEG;
  publishStatus();

  HwCommand command;
  for (;;) {
    if (xQueueReceive(hw_command_queue, &command,
                      pdMS_TO_TICKS(HWTEST_STATUS_POLL_MS)) == pdTRUE) {
      execute(command);
    }
    publishStatus();
  }
}
