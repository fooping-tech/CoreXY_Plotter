#include "LogBuffer.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t LOG_BUFFER_CAPACITY = 24;
constexpr size_t LOG_BUFFER_LINE_LENGTH = 192;
char log_buffer[LOG_BUFFER_CAPACITY][LOG_BUFFER_LINE_LENGTH] = {};
uint8_t log_next = 0;
uint8_t log_count = 0;
portMUX_TYPE log_buffer_mux = portMUX_INITIALIZER_UNLOCKED;

String escapeJsonString(const char* text) {
  String escaped;
  escaped.reserve(strlen(text) + 8);
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    const char c = *cursor;
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          escaped += ' ';
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}
}

void appendLogBuffer(const char* text) {
  if (text == nullptr) return;
  portENTER_CRITICAL(&log_buffer_mux);
  snprintf(log_buffer[log_next], LOG_BUFFER_LINE_LENGTH, "%s", text);
  log_next = static_cast<uint8_t>((log_next + 1) % LOG_BUFFER_CAPACITY);
  if (log_count < LOG_BUFFER_CAPACITY) {
    ++log_count;
  }
  portEXIT_CRITICAL(&log_buffer_mux);
}

String latestLogBufferJson() {
  char snapshot[LOG_BUFFER_CAPACITY][LOG_BUFFER_LINE_LENGTH] = {};
  uint8_t count = 0;

  portENTER_CRITICAL(&log_buffer_mux);
  count = log_count;
  const uint8_t start = static_cast<uint8_t>(
      (log_next + LOG_BUFFER_CAPACITY - log_count) % LOG_BUFFER_CAPACITY);
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t index = static_cast<uint8_t>((start + i) % LOG_BUFFER_CAPACITY);
    snprintf(snapshot[i], LOG_BUFFER_LINE_LENGTH, "%s", log_buffer[index]);
  }
  portEXIT_CRITICAL(&log_buffer_mux);

  String json = "[";
  for (uint8_t i = 0; i < count; ++i) {
    if (i > 0) json += ',';
    json += '"';
    json += escapeJsonString(snapshot[i]);
    json += '"';
  }
  json += ']';
  return json;
}
