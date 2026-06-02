#pragma once

#include <stdint.h>

#define BOARD_M5STACK_CORE2 1
#ifndef SIMULATION_MODE
#define SIMULATION_MODE 0
#endif

constexpr float STEPS_PER_MM = 80.0f;
constexpr float DEFAULT_FEED_MM_MIN = 600.0f;
constexpr float MAX_FEED_MM_MIN = 1200.0f;
constexpr uint32_t DEFAULT_MOTOR_SPEED_STEPS_S = 3000;
constexpr uint32_t MAX_MOTOR_SPEED_STEPS_S = 5000;
constexpr uint32_t DEFAULT_MOTOR_ACCEL_STEPS_S2 = 10000;
constexpr float X_MIN_MM = 0.0f;
constexpr float X_MAX_MM = 300.0f;
constexpr float Y_MIN_MM = 0.0f;
constexpr float Y_MAX_MM = 300.0f;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t DIR_CHANGE_DELAY_US = 200;
constexpr uint8_t PEN_UP_ANGLE_DEG = 30;
constexpr uint8_t PEN_DOWN_ANGLE_DEG = 70;
constexpr bool MOTOR_A_DIRECTION_INVERTED = false;
constexpr bool MOTOR_B_DIRECTION_INVERTED = false;

constexpr uint16_t NEOPIXEL_LED_COUNT = 8;
constexpr uint8_t NEOPIXEL_BRIGHTNESS_MAX = 64;
constexpr uint8_t NEOPIXEL_BRIGHTNESS_DEFAULT = 24;
constexpr uint32_t NEOPIXEL_FRAME_INTERVAL_MS = 33;
constexpr uint8_t NEOPIXEL_INITIAL_PATTERN = 0;  // OFF

constexpr uint16_t TMC_NORMAL_MICROSTEPS = 16;
constexpr uint16_t TMC_NORMAL_RMS_CURRENT_MA = 700;
constexpr bool TMC_NORMAL_SPREADCYCLE = true;
constexpr float TMC_R_SENSE_OHM = 0.11f;
constexpr float TMC_HOLD_MULTIPLIER = 0.5f;
constexpr bool TMC_CURRENT_VSENSE = false;
constexpr uint8_t TMC_IHOLDDELAY = 1;
constexpr uint8_t TMC_TPOWERDOWN = 20;
constexpr uint8_t TMC_TOFF = 5;
constexpr uint8_t TMC_HSTRT = 5;
constexpr uint8_t TMC_HEND = 0;
constexpr uint8_t TMC_TBL = 2;
constexpr uint8_t TMC_SGTHRS_DEFAULT = 80;
constexpr uint32_t TMC_TCOOLTHRS_DEFAULT = 0xFFFFF;
constexpr bool MOTOR_MELODY_ENABLED = true;
constexpr uint16_t MOTOR_MELODY_MICROSTEPS = 2;
constexpr uint16_t MOTOR_MELODY_RMS_CURRENT_MA = 1200;
constexpr bool MOTOR_MELODY_SPREADCYCLE = true;
constexpr uint16_t MOTOR_MELODY_NOTE_GAP_MS = 25;
