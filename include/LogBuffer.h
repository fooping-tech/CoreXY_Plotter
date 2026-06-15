#pragma once

#include <Arduino.h>

void appendLogBuffer(const char* text);
String latestLogBufferJson();
