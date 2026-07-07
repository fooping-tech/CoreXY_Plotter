#include "LedRenderer.h"

#include <FastLED.h>

namespace LedRenderer {

RgbColor scaleColor(RgbColor color, uint8_t scale) {
  return {scale8(color.r, scale), scale8(color.g, scale),
          scale8(color.b, scale)};
}

void renderBreath(uint32_t now_ms, const LedAnimationConfig& config,
                  RgbColor active_color, RgbColor* leds, uint16_t count) {
  const uint16_t phase =
      static_cast<uint16_t>(now_ms * (static_cast<uint16_t>(config.speed) + 8U) /
                            24U);
  const uint8_t wave = sin8(static_cast<uint8_t>(phase));
  const uint8_t level = 24 + scale8(wave, 96);
  const RgbColor color = scaleColor(active_color, level);
  for (uint16_t i = 0; i < count; ++i) leds[i] = color;
}

void renderChase(uint32_t now_ms, const LedAnimationConfig& config,
                 RgbColor active_color, RgbColor* leds, uint16_t count) {
  if (count == 0) return;
  for (uint16_t i = 0; i < count; ++i) leds[i] = {0, 0, 0};
  const uint16_t step_ms =
      360U - (static_cast<uint32_t>(config.speed) * 280U / 255U);
  const uint16_t head = (now_ms / (step_ms == 0 ? 1 : step_ms)) % count;
  for (uint16_t tail = 0; tail < 4 && tail < count; ++tail) {
    const uint16_t index = (head + count - tail) % count;
    const uint8_t level = tail == 0 ? 255 : tail == 1 ? 120 : tail == 2 ? 56 : 24;
    leds[index] = scaleColor(active_color, level);
  }
}

void renderProgress(RgbColor active_color, uint8_t progress_percent,
                    RgbColor* leds, uint16_t count) {
  const uint16_t lit =
      (static_cast<uint32_t>(count) * progress_percent + 99U) / 100U;
  for (uint16_t i = 0; i < count; ++i) {
    leds[i] = i < lit ? active_color : scaleColor(active_color, 12);
  }
}

void renderAlert(uint32_t now_ms, const LedAnimationConfig& config,
                 RgbColor active_color, RgbColor* leds, uint16_t count) {
  const uint16_t period_ms =
      900U - (static_cast<uint32_t>(config.speed) * 700U / 255U);
  const bool on = ((now_ms / (period_ms == 0 ? 1 : period_ms)) & 1U) == 0;
  const RgbColor color = on ? active_color : scaleColor(active_color, 18);
  for (uint16_t i = 0; i < count; ++i) leds[i] = color;
}

void renderSuccess(uint32_t elapsed_ms, const LedAnimationConfig& config,
                   RgbColor active_color, RgbColor* leds, uint16_t count) {
  if (count == 0) return;
  const uint16_t sweep_ms = 70U + (255U - config.speed) / 2U;
  const uint16_t head = (elapsed_ms / sweep_ms) % count;
  const uint8_t fade = elapsed_ms > 1800U ? 96 : 255;
  for (uint16_t i = 0; i < count; ++i) {
    const uint16_t distance = (i + count - head) % count;
    const uint8_t level = distance == 0 ? fade : distance == 1 ? scale8(fade, 96)
                                                              : scale8(fade, 24);
    leds[i] = scaleColor(active_color, level);
  }
}

void renderPacifica(uint32_t now_ms, const LedAnimationConfig& config,
                    RgbColor* leds, uint16_t count) {
  const CRGBPalette16 palette(CRGB(0, 5, 16), CRGB(0, 24, 80),
                              CRGB(0, 90, 120), CRGB(8, 160, 180));
  const uint16_t phase = static_cast<uint16_t>(now_ms * (config.speed + 16U) / 32U);
  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t wave_a = sin8(static_cast<uint8_t>(phase / 8U + i * 29U));
    const uint8_t wave_b = sin8(static_cast<uint8_t>(phase / 13U + i * 47U));
    const uint8_t index = scale8(qadd8(wave_a, scale8(wave_b, 120)), 180);
    CRGB color = ColorFromPalette(palette, index, config.intensity, LINEARBLEND);
    color += CHSV(config.hue, config.saturation, scale8(wave_b, 24));
    leds[i] = {color.r, color.g, color.b};
  }
}

void renderFire(const LedAnimationConfig& config, uint8_t* heat,
                RgbColor* leds, uint16_t count) {
  for (uint16_t i = 0; i < count; ++i) {
    heat[i] = qsub8(heat[i], random8(0, ((config.cooling * 10U) / count) + 2));
  }
  for (int i = count - 1; i >= 2; --i) {
    heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
  }
  if (random8() < config.sparking) {
    const uint8_t y = random8(count > 7 ? 7 : count);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }
  for (uint16_t i = 0; i < count; ++i) {
    const CRGB color = HeatColor(scale8(heat[i], config.intensity));
    leds[i] = {color.r, color.g, color.b};
  }
}

}  // namespace LedRenderer
