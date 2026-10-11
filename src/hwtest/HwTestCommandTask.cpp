// Core 0: Serial line input -> HwCommand. No hardware access except logging.

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "Core2PinMap.h"
#include "PlotterConfig.h"
#include "hwtest/HwTestConfig.h"
#include "hwtest/HwTestShared.h"

namespace {
struct CommandEntry {
  const char* name;
  HwCommandType type;
};

constexpr CommandEntry CORE1_COMMANDS[] = {
    {"SELFTEST", HwCommandType::SELFTEST},
    {"LIMIT", HwCommandType::LIMIT},
    {"LIMIT_WATCH", HwCommandType::LIMIT_WATCH},
    {"PIN_CHECK", HwCommandType::PIN_CHECK},
    {"PIN_SET", HwCommandType::PIN_SET},
    {"TMC_PING", HwCommandType::TMC_PING},
    {"TMC_INIT", HwCommandType::TMC_INIT},
    {"TMC_STATUS", HwCommandType::TMC_STATUS},
    {"MOTOR", HwCommandType::MOTOR},
    {"STOP", HwCommandType::STOP},
    {"SERVO", HwCommandType::SERVO},
    {"PENUP", HwCommandType::PENUP},
    {"PENDOWN", HwCommandType::PENDOWN},
    {"SERVO_SWEEP", HwCommandType::SERVO_SWEEP},
};

// PIN_SET pin names. Index order is shared with HwTestTask.
constexpr const char* PIN_SET_NAMES[] = {"A_STEP", "A_DIR", "B_STEP", "B_DIR"};

void printHelp() {
  logMessage("HWTEST commands:");
  logMessage("  HELP | INFO | POWER");
  logMessage("  SELFTEST                      non-motion automatic checks");
  logMessage("  LIMIT | LIMIT_WATCH [sec]     limit switch inputs");
  logMessage("  PIN_CHECK                     STEP/DIR -> TMC IOIN loopback");
  logMessage("  PIN_SET <A_STEP|A_DIR|B_STEP|B_DIR> <0|1>  static level for probing");
  logMessage("  TMC_PING                      UART read only, no register write");
  logMessage("  TMC_INIT [rms_mA]             write test profile (default %u mA)",
             HWTEST_TMC_DEFAULT_RMS_CURRENT_MA);
  logMessage("  TMC_STATUS                    registers + flags");
  logMessage("  MOTOR <A|B> [steps] [steps_s] single motor move (|steps|<=%ld)",
             static_cast<long>(HWTEST_MOTOR_MAX_STEPS));
  logMessage("  STOP                          stop motors / abort watch");
  logMessage("  PENUP | PENDOWN | SERVO <deg> | SERVO_SWEEP");
  logMessage("  LED <r> <g> <b> | LED_TEST | LED_OFF");
  logMessage("HELP DONE");
}

void printInfo() {
  logMessage("INFO build=hwtest simulation=%d", SIMULATION_MODE);
  logMessage("INFO MOTOR_A STEP=%u DIR=%u  MOTOR_B STEP=%u DIR=%u  EN=GND",
             MOTOR_A_STEP_PIN, MOTOR_A_DIR_PIN, MOTOR_B_STEP_PIN,
             MOTOR_B_DIR_PIN);
  logMessage("INFO TMC_UART TX=%u RX=%u baud=%lu A.addr=%u B.addr=%u Rsense=%.3f",
             TMC_UART_TX_PIN, TMC_UART_RX_PIN,
             static_cast<unsigned long>(TMC_UART_BAUD), TMC_A_UART_ADDRESS,
             TMC_B_UART_ADDRESS, TMC_R_SENSE_OHM);
  logMessage("INFO LIMIT X=%u Y=%u active_low=%s", X_LIMIT_PIN, Y_LIMIT_PIN,
             LIMIT_ACTIVE_LOW ? "YES" : "NO");
  logMessage("INFO SERVO=%u up=%u down=%u  NEOPIXEL=%u count=%u", PEN_SERVO_PIN,
             PEN_UP_ANGLE_DEG, PEN_DOWN_ANGLE_DEG, NEOPIXEL_PIN,
             NEOPIXEL_LED_COUNT);
  logMessage("INFO DONE");
}

bool parseLong(const char* token, long& value) {
  if (token == nullptr) return false;
  char* end = nullptr;
  value = strtol(token, &end, 10);
  return end != token && *end == '\0';
}

void handleLine(char* line) {
  for (char* p = line; *p != '\0'; ++p) *p = toupper(*p);
  char* save = nullptr;
  const char* name = strtok_r(line, " \t", &save);
  if (name == nullptr) return;
  const char* t1 = strtok_r(nullptr, " \t", &save);
  const char* t2 = strtok_r(nullptr, " \t", &save);
  const char* t3 = strtok_r(nullptr, " \t", &save);

  if (strcmp(name, "HELP") == 0) return printHelp();
  if (strcmp(name, "INFO") == 0 || strcmp(name, "CONFIG") == 0) {
    return printInfo();
  }
  if (strcmp(name, "POWER") == 0) return requestHwPowerReport();
  if (strcmp(name, "LED_TEST") == 0) return requestHwLed(HwLedRequest::TEST);
  if (strcmp(name, "LED_OFF") == 0) return requestHwLed(HwLedRequest::OFF);
  if (strcmp(name, "LED") == 0) {
    long r = 0, g = 0, b = 0;
    if (!parseLong(t1, r) || !parseLong(t2, g) || !parseLong(t3, b) || r < 0 ||
        r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      logMessage("ERROR: usage LED <r 0-255> <g 0-255> <b 0-255>");
      return;
    }
    return requestHwLed(HwLedRequest::RGB, r, g, b);
  }

  HwCommand command;
  bool known = false;
  for (const auto& entry : CORE1_COMMANDS) {
    if (strcmp(name, entry.name) == 0) {
      command.type = entry.type;
      known = true;
      break;
    }
  }
  if (!known) {
    logMessage("ERROR: unknown command %s (type HELP)", name);
    return;
  }

  long value = 0;
  switch (command.type) {
    case HwCommandType::MOTOR:
      if (t1 == nullptr || (strcmp(t1, "A") != 0 && strcmp(t1, "B") != 0)) {
        logMessage("ERROR: usage MOTOR <A|B> [steps] [steps_s]");
        return;
      }
      command.target = t1[0];
      if (t2 != nullptr) {
        if (!parseLong(t2, value)) {
          logMessage("ERROR: MOTOR steps must be integer");
          return;
        }
        command.arg0 = value;
        command.has_arg0 = true;
      }
      if (t3 != nullptr) {
        if (!parseLong(t3, value) || value <= 0) {
          logMessage("ERROR: MOTOR steps_s must be positive integer");
          return;
        }
        command.arg1 = value;
        command.has_arg1 = true;
      }
      break;
    case HwCommandType::PIN_SET: {
      int pin_index = -1;
      for (int i = 0; i < 4; ++i) {
        if (t1 != nullptr && strcmp(t1, PIN_SET_NAMES[i]) == 0) pin_index = i;
      }
      if (pin_index < 0 || !parseLong(t2, value) || (value != 0 && value != 1)) {
        logMessage("ERROR: usage PIN_SET <A_STEP|A_DIR|B_STEP|B_DIR> <0|1>");
        return;
      }
      command.arg0 = pin_index;
      command.arg1 = value;
      command.has_arg0 = command.has_arg1 = true;
      break;
    }
    case HwCommandType::LIMIT_WATCH:
    case HwCommandType::TMC_INIT:
    case HwCommandType::SERVO:
      if (t1 != nullptr) {
        if (!parseLong(t1, value) || value < 0) {
          logMessage("ERROR: %s argument must be non-negative integer", name);
          return;
        }
        command.arg0 = value;
        command.has_arg0 = true;
      } else if (command.type == HwCommandType::SERVO) {
        logMessage("ERROR: usage SERVO <deg 0-180>");
        return;
      }
      break;
    default:
      break;
  }

  if (command.type == HwCommandType::STOP) hw_stop_requested = true;
  if (!enqueueHwCommand(command)) {
    logMessage("ERROR: command queue full (%s)", name);
  }
}
}  // namespace

void hwCommandTask(void*) {
  char line[96] = {};
  size_t length = 0;
  bool overflow = false;
  for (;;) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\r' || c == '\n') {
        if (overflow) {
          logMessage("ERROR: line too long");
        } else if (length > 0) {
          line[length] = '\0';
          handleLine(line);
        }
        length = 0;
        overflow = false;
      } else if (length + 1 < sizeof(line)) {
        line[length++] = c;
      } else {
        overflow = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
