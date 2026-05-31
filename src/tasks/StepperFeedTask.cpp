#include <Arduino.h>

void stepperFeedTask(void*) {
  // Placeholder: future SegmentQueue consumer. FastAccelStepper emits pulses.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
