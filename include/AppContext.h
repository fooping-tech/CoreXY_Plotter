#pragma once

#include <Arduino.h>
#include "CommandMessage.h"
#include "MachineState.h"
#include "HomingController.h"
#include "JobController.h"
#include "LedPatternEngine.h"
#include "NeoPixelController.h"
#include "MotorMelodyController.h"
#include "PenController.h"
#include "SafetyManager.h"
#include "StepperBackendFastAccel.h"
#include "TMC2209Manager.h"

extern QueueHandle_t command_queue;
extern QueueHandle_t status_queue;
extern QueueHandle_t log_queue;
extern QueueHandle_t led_command_queue;
extern MachineState machine_state;
extern SafetyManager safety_manager;
extern StepperBackendFastAccel stepper_backend;
extern TMC2209Manager tmc_manager;
extern PenController pen_controller;
extern NeoPixelController neopixel_controller;
extern LedPatternEngine led_pattern_engine;
extern MotorMelodyController motor_melody_controller;
extern HomingController homing_controller;
extern JobController job_controller;

void requestMotionAbort();
bool isMotionAbortRequested();
void clearMotionAbort();
void logMessage(const char* format, ...);
StatusMessage captureStatus();
void publishStatus();
void postLedStatus(LedStatus status);
void syncJobActiveFlag();
void uiTask(void*);
void commandTask(void*);
void motionTask(void*);
void stepperFeedTask(void*);
void tmcTask(void*);
void safetyTask(void*);
void logTask(void*);
