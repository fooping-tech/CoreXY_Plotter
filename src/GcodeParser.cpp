#include "GcodeParser.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace {
bool isLineEnd(char c) {
  return c == '\0' || c == '\r' || c == '\n' || c == ';' || c == '*';
}

void setError(char* error, unsigned int error_size, const char* message) {
  if (error != nullptr && error_size > 0) {
    snprintf(error, error_size, "%s", message);
  }
}

bool setCode(char letter, long code, ParsedGcode& parsed, char* error,
             unsigned int error_size) {
  ParsedGcodeType type = ParsedGcodeType::NONE;
  if (letter == 'G') {
    if (code == 0) type = ParsedGcodeType::G0;
    else if (code == 1) type = ParsedGcodeType::G1;
    else if (code == 4) type = ParsedGcodeType::G4;
    else if (code == 20) type = ParsedGcodeType::G20;
    else if (code == 21) type = ParsedGcodeType::G21;
    else if (code == 28) type = ParsedGcodeType::G28;
    else if (code == 90) type = ParsedGcodeType::G90;
    else if (code == 91) type = ParsedGcodeType::G91;
    else {
      snprintf(error, error_size, "unsupported G-code: G%ld", code);
      return false;
    }
  } else if (letter == 'M') {
    if (code == 3) type = ParsedGcodeType::M3;
    else if (code == 5) type = ParsedGcodeType::M5;
    else if (code == 114) type = ParsedGcodeType::M114;
    else {
      snprintf(error, error_size, "unsupported M-code: M%ld", code);
      return false;
    }
  } else {
    setError(error, error_size, "internal parser error");
    return false;
  }

  if (parsed.type != ParsedGcodeType::NONE) {
    setError(error, error_size, "one G/M code per line is supported");
    return false;
  }
  parsed.type = type;
  return true;
}
}

bool GcodeParser::parse(const char* line, ParsedGcode& parsed, char* error,
                        unsigned int error_size) {
  parsed = ParsedGcode{};
  if (line == nullptr) {
    setError(error, error_size, "null G-code line");
    return false;
  }

  bool in_paren_comment = false;
  const char* cursor = line;
  while (!isLineEnd(*cursor)) {
    char c = *cursor;
    if (c == '(') {
      in_paren_comment = true;
      ++cursor;
      continue;
    }
    if (in_paren_comment) {
      if (c == ')') in_paren_comment = false;
      ++cursor;
      continue;
    }
    if (isspace(static_cast<unsigned char>(c))) {
      ++cursor;
      continue;
    }

    const char word = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    ++cursor;
    if (word == 'G' || word == 'M') {
      char* end = nullptr;
      const long code = strtol(cursor, &end, 10);
      if (end == cursor) {
        setError(error, error_size, "G/M word requires numeric code");
        return false;
      }
      if (!setCode(word, code, parsed, error, error_size)) return false;
      cursor = end;
    } else if (word == 'X' || word == 'Y' || word == 'F' || word == 'P') {
      char* end = nullptr;
      const float value = strtof(cursor, &end);
      if (end == cursor) {
        setError(error, error_size, "axis/feed word requires numeric value");
        return false;
      }
      if (word == 'X') {
        parsed.has_x = true;
        parsed.x = value;
      } else if (word == 'Y') {
        parsed.has_y = true;
        parsed.y = value;
      } else {
        if (word == 'P') {
          parsed.has_p = true;
          parsed.p_ms = value;
        } else {
          parsed.has_f = true;
          parsed.f_mm_min = value;
        }
      }
      cursor = end;
    } else if (word == 'N') {
      char* end = nullptr;
      strtol(cursor, &end, 10);
      if (end == cursor) {
        setError(error, error_size, "line number requires numeric value");
        return false;
      }
      cursor = end;
    } else {
      snprintf(error, error_size, "unsupported G-code word: %c", word);
      return false;
    }
  }

  if (parsed.type == ParsedGcodeType::NONE) {
    setError(error, error_size, "missing G/M code");
    return false;
  }
  if ((parsed.type == ParsedGcodeType::G0 ||
       parsed.type == ParsedGcodeType::G1) &&
      !parsed.has_x && !parsed.has_y) {
    setError(error, error_size, "G0/G1 requires X and/or Y");
    return false;
  }
  if (parsed.type == ParsedGcodeType::G4 && !parsed.has_p) {
    setError(error, error_size, "G4 requires P milliseconds");
    return false;
  }
  if (parsed.has_f && parsed.f_mm_min <= 0.0f) {
    setError(error, error_size, "F must be > 0 mm/min");
    return false;
  }
  if (parsed.has_p && parsed.p_ms < 0.0f) {
    setError(error, error_size, "P must be >= 0 ms");
    return false;
  }
  return true;
}
