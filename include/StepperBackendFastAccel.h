#pragma once

#include "StepperBackend.h"

class FastAccelStepper;

class StepperBackendFastAccel : public StepperBackend {
 public:
  bool begin() override;
  bool isReady() const override;
  bool moveASteps(int32_t steps) override;
  bool moveBSteps(int32_t steps) override;
  bool moveABSteps(int32_t a_steps, int32_t b_steps,
                   float feed_mm_min) override;
  bool setDiagnosticSpeedHz(uint32_t speed_hz) override;
  bool moveDiagnosticASteps(int32_t steps) override;
  void stop() override;
  bool isRunning() const override;
  void waitUntilIdle() override;

 private:
  void configureSpeed(float feed_mm_min);
  FastAccelStepper* motor_a_ = nullptr;
  FastAccelStepper* motor_b_ = nullptr;
  bool ready_ = false;
};
