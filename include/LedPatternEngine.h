#pragma once

#include <stdint.h>
#include "LedTypes.h"
#include "PlotterConfig.h"

class NeoPixelController;

// LEDコマンド解釈と状態機械。描画はLedRenderer、状態→演出マッピングは
// LedStatusConfig.hのテーブルへ分離している。
// グローバルI/Oへ依存せず、応答はsetResponder()のコールバックへ出す。
class LedPatternEngine {
 public:
  using Responder = void (*)(const char* text);

  void begin(NeoPixelController& controller);
  void setResponder(Responder responder) { responder_ = responder; }
  void tick(uint32_t now_ms);
  void applyCommand(const LedCommand& command);
  void setStatus(LedStatus status);
  void setProgress(uint8_t percent);
  void printStatus() const;

 private:
  void render(uint32_t now_ms);
  void applyStatusConfig(LedStatus status, uint32_t now_ms);
  bool shouldApplyStatus(LedStatus status, uint32_t now_ms) const;
  void respond(const char* format, ...) const;
  static uint8_t statusPriority(LedStatus status);
  static const char* patternName(LedPattern pattern);
  static const char* parameterName(LedParameter parameter);
  static const char* statusName(LedStatus status);

  NeoPixelController* controller_ = nullptr;
  Responder responder_ = nullptr;
  LedAnimationConfig config_;
  LedPattern pattern_ = static_cast<LedPattern>(NEOPIXEL_INITIAL_PATTERN);
  LedStatus status_ = LedStatus::IDLE;
  RgbColor solid_;
  RgbColor active_color_;
  uint8_t heat_[NEOPIXEL_LED_COUNT] = {};
  uint8_t progress_percent_ = 0;
  uint32_t last_frame_ms_ = 0;
  uint32_t status_started_ms_ = 0;
  uint32_t transient_until_ms_ = 0;
  bool dirty_ = true;
  bool auto_status_enabled_ = true;
};
