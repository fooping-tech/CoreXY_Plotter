#pragma once

#include <stdint.h>

class HardwareSerial;
class TMC2209Stepper;

struct TMC2209Profile {
  uint16_t microsteps;
  uint16_t rms_current_ma;
  bool spread_cycle;
};

class TMC2209Manager {
 public:
  explicit TMC2209Manager(HardwareSerial& serial);
  bool begin();
  bool configureDrivers();
  bool applyNormalProfile();
  bool applyMelodyProfile();
  void printStatus() const;
  bool isReady() const;

 private:
  bool applyProfile(const TMC2209Profile& profile, const char* label);
  void applyDriverConfig(TMC2209Stepper& driver,
                         const TMC2209Profile& profile);
  void applyDriverCurrent(TMC2209Stepper& driver, uint16_t rms_current_ma);
  bool refreshConnectionStatus();
  void printDriverStatus(const char* name, TMC2209Stepper& driver) const;

  HardwareSerial& serial_;
  TMC2209Stepper* driver_a_;
  TMC2209Stepper* driver_b_;
  bool initialized_ = false;
  bool ready_ = false;
  uint8_t connection_a_ = 0xFF;
  uint8_t connection_b_ = 0xFF;
  TMC2209Profile current_profile_;
};
