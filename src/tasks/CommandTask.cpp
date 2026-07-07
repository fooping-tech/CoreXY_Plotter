#include <Arduino.h>
#include "AppContext.h"
#include "CommandSubmitter.h"

void commandTask(void*) {
  char line[128] = {};
  size_t length = 0;
  bool discard_until_newline = false;
  for (;;) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\r' || c == '\n') {
        if (discard_until_newline) {
          discard_until_newline = false;
          length = 0;
          continue;
        }
        line[length] = '\0';
        if (length > 0) {
          (void)submitCommandLine(line, CommandSubmitMode::SERIAL_COMPAT);
        }
        length = 0;
      } else if (length + 1 < sizeof(line)) {
        line[length++] = c;
      } else {
        length = 0;
        discard_until_newline = true;
        logMessage("ERROR: input line too long");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
