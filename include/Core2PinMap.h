#pragma once

#include <stdint.h>

// Core2 reserved pins:
// GPIO21/22: internal I2C, GPIO1/3: USB Serial, GPIO34/35/36/38: input-only,
// GPIO0/2: boot straps. GPIO35/36 are intentionally used only as limit inputs.
constexpr uint8_t MOTOR_A_STEP_PIN = 25;
constexpr uint8_t MOTOR_A_DIR_PIN = 26;
constexpr uint8_t MOTOR_B_STEP_PIN = 27;
constexpr uint8_t MOTOR_B_DIR_PIN = 19;
constexpr uint8_t MOTOR_EN_PIN = 23;
constexpr uint8_t TMC_UART_TX_PIN = 14;
constexpr uint8_t TMC_UART_RX_PIN = 13;
constexpr uint8_t X_LIMIT_PIN = 36;
constexpr uint8_t Y_LIMIT_PIN = 35;
constexpr uint8_t PEN_SERVO_PIN = 32;
constexpr uint8_t NEOPIXEL_PIN = 33;

// Shared PDN_UART bus: GPIO14 TX should have a 1k series resistor before the
// branch to both drivers. GPIO13 RX reads the same shared bus.
constexpr uint8_t TMC_A_UART_ADDRESS = 0;
constexpr uint8_t TMC_B_UART_ADDRESS = 1;
constexpr uint32_t TMC_UART_BAUD = 115200;
