#pragma once

class SafetyManager;
class StepperBackendFastAccel;
class TMC2209Manager;

class MotorMelodyController {
 public:
  bool play(StepperBackendFastAccel& backend, TMC2209Manager& tmc,
            SafetyManager& safety);

 private:
  bool shouldAbort(SafetyManager& safety) const;
};
