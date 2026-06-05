#include <Arduino.h>

void stepperFeedTask(void*) {
  // Phase 9 queues timed segments from motionTask while safety is polled there.
  // This task remains reserved for a future always-running SegmentQueue consumer.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
