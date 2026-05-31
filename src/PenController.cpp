#include "PenController.h"
#include <Arduino.h>
#include "Core2PinMap.h"
#include "PlotterConfig.h"

namespace {
constexpr uint8_t SERVO_PWM_CHANNEL = 4;
constexpr uint16_t SERVO_PWM_HZ = 50;
constexpr uint8_t SERVO_PWM_BITS = 16;
}

void PenController::begin() {
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_HZ, SERVO_PWM_BITS);
  ledcAttachPin(PEN_SERVO_PIN, SERVO_PWM_CHANNEL);
  penUp();
}

void PenController::writeAngle(unsigned angle_deg) {
  const uint32_t pulse_us = 500 + (angle_deg * 2000U) / 180U;
  const uint32_t duty = pulse_us * 65535U / 20000U;
  ledcWrite(SERVO_PWM_CHANNEL, duty);
}

void PenController::penUp() {
  writeAngle(PEN_UP_ANGLE_DEG);
  pen_down_ = false;
}

void PenController::penDown() {
  writeAngle(PEN_DOWN_ANGLE_DEG);
  pen_down_ = true;
}

bool PenController::isPenDown() const { return pen_down_; }
