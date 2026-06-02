#pragma once

#include <stdint.h>

class StepperBackend {
 public:
  enum class DiagnosticPulseResult {
    QUEUED,
    RETRY,
    ERROR,
  };

  virtual ~StepperBackend() = default;
  virtual bool begin() = 0;
  virtual bool isReady() const = 0;
  virtual bool moveASteps(int32_t steps) = 0;
  virtual bool moveBSteps(int32_t steps) = 0;
  virtual bool moveABSteps(int32_t a_steps, int32_t b_steps,
                           float feed_mm_min) = 0;
  virtual bool beginDiagnosticTone() = 0;
  virtual DiagnosticPulseResult queueDiagnosticPulse(uint32_t frequency_hz) = 0;
  virtual void endDiagnosticTone() = 0;
  virtual void stop() = 0;
  virtual bool isRunning() const = 0;
  virtual void waitUntilIdle() = 0;
};
