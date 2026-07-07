#pragma once

#include <stdint.h>

#include "LedTypes.h"

// 生アニメーション描画の純関数群。状態機械やI/Oへ依存せず、
// 渡されたバッファへ現在フレームを書き込むだけにする。
namespace LedRenderer {

RgbColor scaleColor(RgbColor color, uint8_t scale);

void renderBreath(uint32_t now_ms, const LedAnimationConfig& config,
                  RgbColor active_color, RgbColor* leds, uint16_t count);
void renderChase(uint32_t now_ms, const LedAnimationConfig& config,
                 RgbColor active_color, RgbColor* leds, uint16_t count);
void renderProgress(RgbColor active_color, uint8_t progress_percent,
                    RgbColor* leds, uint16_t count);
void renderAlert(uint32_t now_ms, const LedAnimationConfig& config,
                 RgbColor active_color, RgbColor* leds, uint16_t count);
void renderSuccess(uint32_t elapsed_ms, const LedAnimationConfig& config,
                   RgbColor active_color, RgbColor* leds, uint16_t count);
void renderPacifica(uint32_t now_ms, const LedAnimationConfig& config,
                    RgbColor* leds, uint16_t count);
// heatはFireの持続状態。呼び出し側がcount要素のバッファを保持する。
void renderFire(const LedAnimationConfig& config, uint8_t* heat,
                RgbColor* leds, uint16_t count);

}  // namespace LedRenderer
