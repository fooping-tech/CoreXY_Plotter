#pragma once

#include "StepperBackend.h"
#include "SegmentGenerator.h"

class FastAccelStepper;

class StepperBackendFastAccel : public StepperBackend {
 public:
  bool begin() override;
  bool isReady() const override;
  bool moveASteps(int32_t steps) override;
  bool moveBSteps(int32_t steps) override;
  bool moveABSteps(int32_t a_steps, int32_t b_steps,
                   float feed_mm_min) override;
  TimedSegmentResult queueTimedSegment(const MotionSegment& segment,
                                       bool start);
  bool startTimedSegments();
  bool beginDiagnosticTone() override;
  DiagnosticPulseResult queueDiagnosticPulse(uint32_t frequency_hz) override;
  void endDiagnosticTone() override;
  bool beginDiagnosticChord();
  DiagnosticPulseResult queueDiagnosticPulseA(uint32_t frequency_hz);
  DiagnosticPulseResult queueDiagnosticPulseB(uint32_t frequency_hz);
  void endDiagnosticChord();
  void stop() override;
  bool isRunning() const override;
  void waitUntilIdle() override;
  int32_t currentASteps() const;
  int32_t currentBSteps() const;
  bool isMotorARunning() const;
  bool isMotorBRunning() const;
  uint16_t motorAQueueEntries() const;
  uint16_t motorBQueueEntries() const;

 private:
  void configureSpeed(float feed_mm_min);
  TimedSegmentResult mapMoveTimedResult(int8_t result) const;
  uint16_t estimateMoveTimedEntries(int32_t steps,
                                    uint32_t duration_ticks) const;
  bool hasTimedSegmentCapacity(const MotionSegment& segment,
                               uint32_t duration_ticks) const;
  FastAccelStepper* motor_a_ = nullptr;
  FastAccelStepper* motor_b_ = nullptr;
  bool ready_ = false;
  bool diagnostic_direction_positive_ = true;
  bool diagnostic_direction_positive_b_ = true;
};
