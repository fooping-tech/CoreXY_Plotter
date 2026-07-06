#pragma once

// Arduino非依存の小さな数値ユーティリティ。
// square()やclampの自前定義を各モジュールへ重複させない。

inline float square(float value) { return value * value; }

inline float clampFloat(float value, float lower, float upper) {
  if (value < lower) return lower;
  if (value > upper) return upper;
  return value;
}
