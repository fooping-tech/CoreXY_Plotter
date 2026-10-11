#pragma once

#include <stdint.h>

class HardwareSerial;
class TMC2209Stepper;

struct TMC2209Profile {
  uint16_t microsteps;
  uint16_t rms_current_ma;
  bool spread_cycle;
};

enum class TMC2209Axis : uint8_t {
  A,
  B,
};

// Raw register snapshot for hardware diagnostics. Decoding is left to the
// caller so this manager stays a thin UART owner.
struct TMC2209Diagnostics {
  uint8_t address = 0;
  uint8_t connection = 0xFF;  // test_connection(): 0=OK, 1=no reply, 2=zero
  uint8_t version = 0;        // IOIN[31:24], TMC2209 expects 0x21
  uint8_t ifcnt = 0;
  uint8_t gstat = 0;
  uint32_t gconf = 0;
  uint32_t ioin = 0;
  uint32_t drv_status = 0;
  uint16_t sg_result = 0;
};

class TMC2209Manager {
 public:
  explicit TMC2209Manager(HardwareSerial& serial);
  bool begin();
  // Opens Serial2 without writing any driver register.
  bool beginUart();
  bool configureDrivers();
  bool applyNormalProfile();
  bool applyMelodyProfile();
  bool applyCustomProfile(const TMC2209Profile& profile, const char* label);
  // Low-frequency diagnostics only. Do not call in a motion loop.
  bool readDiagnostics(TMC2209Axis axis, TMC2209Diagnostics& diagnostics);
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
