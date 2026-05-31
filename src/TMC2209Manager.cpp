#include "TMC2209Manager.h"
#include <Arduino.h>
#include <TMCStepper.h>
#include <math.h>
#include "AppContext.h"
#include "Core2PinMap.h"
#include "PlotterConfig.h"

namespace {
constexpr TMC2209Profile NORMAL_PROFILE = {
    TMC_NORMAL_MICROSTEPS, TMC_NORMAL_RMS_CURRENT_MA, TMC_NORMAL_SPREADCYCLE};
constexpr TMC2209Profile MELODY_PROFILE = {
    MOTOR_MELODY_MICROSTEPS, MOTOR_MELODY_RMS_CURRENT_MA,
    MOTOR_MELODY_SPREADCYCLE};

uint8_t rmsMaToCurrentScale(float rms_ma) {
  const float vfs = TMC_CURRENT_VSENSE ? 0.180f : 0.325f;
  long current_scale = lroundf(32.0f * 1.41421f * rms_ma / 1000.0f *
                               (TMC_R_SENSE_OHM + 0.02f) / vfs - 1.0f);
  if (current_scale < 0) current_scale = 0;
  if (current_scale > 31) current_scale = 31;
  return static_cast<uint8_t>(current_scale);
}
}

TMC2209Manager::TMC2209Manager(HardwareSerial& serial)
    : serial_(serial), current_profile_(NORMAL_PROFILE) {
  static TMC2209Stepper driver_a(&serial, TMC_R_SENSE_OHM,
                                  TMC_A_UART_ADDRESS);
  static TMC2209Stepper driver_b(&serial, TMC_R_SENSE_OHM,
                                  TMC_B_UART_ADDRESS);
  driver_a_ = &driver_a;
  driver_b_ = &driver_b;
}

bool TMC2209Manager::begin() {
#if SIMULATION_MODE
  initialized_ = true;
  ready_ = true;
  applyNormalProfile();
  logMessage("TMC_INIT simulation: UART TX=%u RX=%u baud=%lu A=%u B=%u",
             TMC_UART_TX_PIN, TMC_UART_RX_PIN, TMC_UART_BAUD,
             TMC_A_UART_ADDRESS, TMC_B_UART_ADDRESS);
#else
  serial_.begin(TMC_UART_BAUD, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
  initialized_ = true;
  ready_ = true;
  configureDrivers();
  ready_ = refreshConnectionStatus();
  logMessage("TMC_INIT UART TX=%u RX=%u baud=%lu A=%u connection=%u B=%u connection=%u ready=%s",
             TMC_UART_TX_PIN, TMC_UART_RX_PIN, TMC_UART_BAUD,
             TMC_A_UART_ADDRESS, connection_a_, TMC_B_UART_ADDRESS,
             connection_b_, ready_ ? "YES" : "NO");
#endif
  return ready_;
}

bool TMC2209Manager::configureDrivers() {
  if (!initialized_) return false;
#if SIMULATION_MODE
  return applyNormalProfile();
#else
  applyDriverConfig(*driver_a_, NORMAL_PROFILE);
  applyDriverConfig(*driver_b_, NORMAL_PROFILE);
  current_profile_ = NORMAL_PROFILE;
  return true;
#endif
}

void TMC2209Manager::applyDriverConfig(TMC2209Stepper& driver,
                                       const TMC2209Profile& profile) {
  driver.begin();
  driver.GSTAT(0b111);
  driver.pdn_disable(true);
  driver.mstep_reg_select(true);
  driver.multistep_filt(true);
  driver.TPOWERDOWN(TMC_TPOWERDOWN);
  driver.toff(TMC_TOFF);
  driver.hstrt(TMC_HSTRT);
  driver.hend(TMC_HEND);
  driver.tbl(TMC_TBL);
  driver.microsteps(profile.microsteps);
  driver.intpol(true);
  driver.en_spreadCycle(profile.spread_cycle);
  driver.pwm_autoscale(true);
  driver.pwm_autograd(false);
  driver.semin(0);
  driver.SGTHRS(TMC_SGTHRS_DEFAULT);
  driver.TCOOLTHRS(TMC_TCOOLTHRS_DEFAULT);

  // Apply current last because CHOPCONF writes can overwrite vsense.
  applyDriverCurrent(driver, profile.rms_current_ma);
}

void TMC2209Manager::applyDriverCurrent(TMC2209Stepper& driver,
                                        uint16_t rms_current_ma) {
  driver.vsense(TMC_CURRENT_VSENSE);
  driver.irun(rmsMaToCurrentScale(static_cast<float>(rms_current_ma)));
  driver.ihold(rmsMaToCurrentScale(rms_current_ma * TMC_HOLD_MULTIPLIER));
  driver.iholddelay(TMC_IHOLDDELAY);
}

bool TMC2209Manager::applyProfile(const TMC2209Profile& profile,
                                  const char* label) {
  if (!initialized_) return false;
  current_profile_ = profile;
#if SIMULATION_MODE
  logMessage("TMC_PROFILE %s microsteps=1/%u current=%umA chop=%s register-write=SIMULATION",
             label, profile.microsteps, profile.rms_current_ma,
             profile.spread_cycle ? "spreadCycle" : "stealthChop");
#else
  applyDriverConfig(*driver_a_, profile);
  applyDriverConfig(*driver_b_, profile);
  const bool connected = refreshConnectionStatus();
  logMessage("TMC_PROFILE %s microsteps=1/%u current=%umA chop=%s A.connection=%u B.connection=%u ready=%s",
             label, profile.microsteps, profile.rms_current_ma,
             profile.spread_cycle ? "spreadCycle" : "stealthChop",
             connection_a_, connection_b_, connected ? "YES" : "NO");
#endif
  ready_ = refreshConnectionStatus();
  return ready_;
}

bool TMC2209Manager::applyNormalProfile() {
  return applyProfile(NORMAL_PROFILE, "normal");
}

bool TMC2209Manager::applyMelodyProfile() {
  return applyProfile(MELODY_PROFILE, "melody");
}

bool TMC2209Manager::refreshConnectionStatus() {
#if SIMULATION_MODE
  connection_a_ = 0;
  connection_b_ = 0;
#else
  connection_a_ = driver_a_->test_connection();
  connection_b_ = driver_b_->test_connection();
#endif
  return connection_a_ == 0 && connection_b_ == 0;
}

void TMC2209Manager::printDriverStatus(const char* name,
                                       TMC2209Stepper& driver) const {
  logMessage("TMC_%s address=%u connection=%u ifcnt=%u microsteps=1/%u rms=%umA irun=%u ihold=%u iholddelay=%u spreadCycle=%s toff=%u",
             name, name[0] == 'A' ? TMC_A_UART_ADDRESS : TMC_B_UART_ADDRESS,
             name[0] == 'A' ? connection_a_ : connection_b_, driver.IFCNT(),
             driver.microsteps(), driver.rms_current(), driver.irun(),
             driver.ihold(), driver.iholddelay(),
             driver.en_spreadCycle() ? "YES" : "NO", driver.toff());
}

void TMC2209Manager::printStatus() const {
  logMessage("TMC_STATUS ready=%s A.address=%u A.connection=%u B.address=%u B.connection=%u profile.microsteps=1/%u profile.current=%umA profile.chop=%s",
             ready_ ? "YES" : "NO", TMC_A_UART_ADDRESS, connection_a_,
             TMC_B_UART_ADDRESS, connection_b_, current_profile_.microsteps,
             current_profile_.rms_current_ma,
             current_profile_.spread_cycle ? "spreadCycle" : "stealthChop");
#if !SIMULATION_MODE
  printDriverStatus("A", *driver_a_);
  printDriverStatus("B", *driver_b_);
#endif
}

bool TMC2209Manager::isReady() const { return ready_; }
