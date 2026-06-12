#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

template <typename T>
T constrain(T value, T low, T high) {
  return std::min(std::max(value, low), high);
}

template <typename T>
T min(T lhs, T rhs) {
  return std::min(lhs, rhs);
}

template <typename T>
T max(T lhs, T rhs) {
  return std::max(lhs, rhs);
}
