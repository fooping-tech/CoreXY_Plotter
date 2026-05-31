#pragma once

class SafetyManager;
class StepperBackendFastAccel;
class TMC2209Manager;

class MotorMelodyController {
 public:
  bool play(StepperBackendFastAccel& backend, TMC2209Manager& tmc,
            SafetyManager& safety, bool motors_enabled);

 private:
  bool shouldAbort(SafetyManager& safety) const;
};
