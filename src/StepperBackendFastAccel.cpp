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
  pinMode(MOTOR_EN_PIN, OUTPUT);
  digitalWrite(MOTOR_EN_PIN, HIGH);
#if !SIMULATION_MODE
  engine.init();
  motor_a_ = engine.stepperConnectToPin(MOTOR_A_STEP_PIN);
  motor_b_ = engine.stepperConnectToPin(MOTOR_B_STEP_PIN);
  if (motor_a_ == nullptr || motor_b_ == nullptr) return false;
  motor_a_->setDirectionPin(MOTOR_A_DIR_PIN, MOTOR_A_DIRECTION_INVERTED,
                            DIR_CHANGE_DELAY_US);
  motor_b_->setDirectionPin(MOTOR_B_DIR_PIN, MOTOR_B_DIRECTION_INVERTED,
                            DIR_CHANGE_DELAY_US);
  motor_a_->setEnablePin(MOTOR_EN_PIN, true);
  motor_b_->setEnablePin(MOTOR_EN_PIN, true);
  configureSpeed(DEFAULT_FEED_MM_MIN);
#endif
  ready_ = true;
  return true;
}

bool StepperBackendFastAccel::isReady() const { return ready_; }

void StepperBackendFastAccel::enable() {
#if !SIMULATION_MODE
  if (!ready_) return;
  motor_a_->enableOutputs();
  motor_b_->enableOutputs();
#else
  digitalWrite(MOTOR_EN_PIN, HIGH);
#endif
}

void StepperBackendFastAccel::disable() {
#if !SIMULATION_MODE
  if (!ready_) return;
  motor_a_->disableOutputs();
  motor_b_->disableOutputs();
#else
  digitalWrite(MOTOR_EN_PIN, HIGH);
#endif
}

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

bool StepperBackendFastAccel::setDiagnosticSpeedHz(uint32_t speed_hz) {
#if SIMULATION_MODE
  (void)speed_hz;
  return true;
#else
  if (!ready_ || motor_a_ == nullptr) return false;
  motor_a_->setSpeedInHz(constrain(speed_hz, 1U, MAX_MOTOR_SPEED_STEPS_S));
  motor_a_->setAcceleration(MOTOR_MELODY_ACCEL_STEPS_S2);
  return true;
#endif
}

bool StepperBackendFastAccel::moveDiagnosticASteps(int32_t steps) {
  return moveASteps(steps);
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
