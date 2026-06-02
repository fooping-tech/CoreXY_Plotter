#include "StepperBackendFastAccel.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <math.h>
#include "Core2PinMap.h"
#include "PlotterConfig.h"

namespace {
FastAccelStepperEngine engine;
}

bool StepperBackendFastAccel::begin() {
#if !SIMULATION_MODE
  engine.init();
  motor_a_ = engine.stepperConnectToPin(MOTOR_A_STEP_PIN);
  motor_b_ = engine.stepperConnectToPin(MOTOR_B_STEP_PIN);
  if (motor_a_ == nullptr || motor_b_ == nullptr) return false;
  motor_a_->setDirectionPin(MOTOR_A_DIR_PIN, MOTOR_A_DIRECTION_INVERTED,
                            DIR_CHANGE_DELAY_US);
  motor_b_->setDirectionPin(MOTOR_B_DIR_PIN, MOTOR_B_DIRECTION_INVERTED,
                            DIR_CHANGE_DELAY_US);
  configureSpeed(DEFAULT_FEED_MM_MIN);
#endif
  ready_ = true;
  return true;
}

bool StepperBackendFastAccel::isReady() const { return ready_; }

void StepperBackendFastAccel::configureSpeed(float feed_mm_min) {
#if !SIMULATION_MODE
  uint32_t speed_steps_s = lroundf(feed_mm_min * STEPS_PER_MM / 60.0f);
  speed_steps_s = constrain(speed_steps_s, 1U, MAX_MOTOR_SPEED_STEPS_S);
  motor_a_->setSpeedInHz(speed_steps_s);
  motor_b_->setSpeedInHz(speed_steps_s);
  motor_a_->setAcceleration(DEFAULT_MOTOR_ACCEL_STEPS_S2);
  motor_b_->setAcceleration(DEFAULT_MOTOR_ACCEL_STEPS_S2);
#endif
}

bool StepperBackendFastAccel::moveASteps(int32_t steps) {
#if SIMULATION_MODE
  (void)steps;
  return true;
#else
  if (motor_a_ == nullptr) return false;
  motor_a_->move(steps);
  return true;
#endif
}

bool StepperBackendFastAccel::moveBSteps(int32_t steps) {
#if SIMULATION_MODE
  (void)steps;
  return true;
#else
  if (motor_b_ == nullptr) return false;
  motor_b_->move(steps);
  return true;
#endif
}

bool StepperBackendFastAccel::moveABSteps(int32_t a_steps, int32_t b_steps,
                                          float feed_mm_min) {
  // Bring-up only:
  // Independent A/B move() calls do not guarantee strict XY interpolation.
  // Future implementation must replace this path with MotionBlock,
  // PlannerQueue, SegmentGenerator, and timed A/B segment execution.
#if SIMULATION_MODE
  (void)a_steps;
  (void)b_steps;
  (void)feed_mm_min;
  return true;
#else
  if (motor_a_ == nullptr || motor_b_ == nullptr) return false;
  configureSpeed(feed_mm_min);
  motor_a_->move(a_steps);
  motor_b_->move(b_steps);
  return true;
#endif
}

bool StepperBackendFastAccel::beginDiagnosticTone() {
#if SIMULATION_MODE
  return true;
#else
  if (!ready_ || motor_a_ == nullptr || motor_a_->isRunning()) return false;
  diagnostic_direction_positive_ = true;
  motor_a_->setDirectionPin(MOTOR_A_DIR_PIN, MOTOR_A_DIRECTION_INVERTED, 0);
  return true;
#endif
}

StepperBackend::DiagnosticPulseResult
StepperBackendFastAccel::queueDiagnosticPulse(uint32_t frequency_hz) {
#if SIMULATION_MODE
  (void)frequency_hz;
  return DiagnosticPulseResult::QUEUED;
#else
  if (!ready_ || motor_a_ == nullptr || frequency_hz == 0 ||
      frequency_hz > MAX_MOTOR_SPEED_STEPS_S) {
    return DiagnosticPulseResult::ERROR;
  }
  const uint32_t ticks = TICKS_PER_S / frequency_hz;
  if (ticks == 0 || ticks > UINT16_MAX) return DiagnosticPulseResult::ERROR;
  const stepper_command_s command = {
      .ticks = static_cast<uint16_t>(ticks),
      .steps = 1,
      .count_up = diagnostic_direction_positive_,
  };
  const AqeResultCode result = motor_a_->addQueueEntry(&command);
  if (result == AQE_OK) {
    diagnostic_direction_positive_ = !diagnostic_direction_positive_;
    return DiagnosticPulseResult::QUEUED;
  }
  return aqeRetry(result) ? DiagnosticPulseResult::RETRY
                          : DiagnosticPulseResult::ERROR;
#endif
}

void StepperBackendFastAccel::endDiagnosticTone() {
#if !SIMULATION_MODE
  if (motor_a_ != nullptr) {
    motor_a_->setDirectionPin(MOTOR_A_DIR_PIN, MOTOR_A_DIRECTION_INVERTED,
                              DIR_CHANGE_DELAY_US);
  }
#endif
}

void StepperBackendFastAccel::stop() {
#if !SIMULATION_MODE
  if (motor_a_ != nullptr) motor_a_->forceStop();
  if (motor_b_ != nullptr) motor_b_->forceStop();
#endif
}

bool StepperBackendFastAccel::isRunning() const {
#if SIMULATION_MODE
  return false;
#else
  return (motor_a_ != nullptr && motor_a_->isRunning()) ||
         (motor_b_ != nullptr && motor_b_->isRunning());
#endif
}

void StepperBackendFastAccel::waitUntilIdle() {
  while (isRunning()) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
