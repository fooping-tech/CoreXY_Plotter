#pragma once

#include <stdint.h>

constexpr uint8_t CORE_UI = 0;
constexpr uint8_t CORE_MOTION = 1;
constexpr uint8_t PRIORITY_UI = 1;
constexpr uint8_t PRIORITY_COMMAND = 2;
constexpr uint8_t PRIORITY_TMC = 3;
constexpr uint8_t PRIORITY_SAFETY = 4;
constexpr uint8_t PRIORITY_MOTION = 5;
constexpr uint8_t PRIORITY_STEPPER_FEED = 6;
constexpr uint8_t PRIORITY_LOG = 1;

constexpr uint16_t COMMAND_QUEUE_LENGTH = 16;
constexpr uint16_t STATUS_QUEUE_LENGTH = 4;
constexpr uint16_t LOG_QUEUE_LENGTH = 24;
constexpr uint16_t LED_COMMAND_QUEUE_LENGTH = 12;
constexpr uint16_t STACK_UI = 6144;
constexpr uint16_t STACK_COMMAND = 4096;
constexpr uint16_t STACK_LOG = 3072;
constexpr uint16_t STACK_MOTION = 6144;
constexpr uint16_t STACK_STEPPER_FEED = 2048;
constexpr uint16_t STACK_TMC = 2048;
constexpr uint16_t STACK_SAFETY = 2048;

// タスクのポーリング周期。
constexpr uint32_t SAFETY_TASK_POLL_INTERVAL_MS = 100;
constexpr uint32_t COMMAND_TASK_POLL_INTERVAL_MS = 10;
constexpr uint32_t COMMAND_QUEUE_SEND_TIMEOUT_MS = 50;
