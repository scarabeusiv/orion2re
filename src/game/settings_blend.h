#pragma once

#include <array>
#include <cstdint>

namespace SETTINGS_BLEND {

    // An 8-bit display-space colour (the values a screenshot census
    // reads: the palette stores 6-bit values, the display shows
    // them shifted left 2).
    struct s_rgb {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
    };

    // Luminance (Rec.601 weights), 0..255.
    int Luminance(uint8_t r, uint8_t g, uint8_t b);

    // One channel of a src-over-dst blend, alpha 0..255
    // (0 = dst, 255 = src).
    uint8_t Blend_Channel(uint8_t src, uint8_t dst, int alpha);

    s_rgb Blend_RGB(const s_rgb& src, const s_rgb& dst, int alpha);

    // Nearest palette entry to rgb, squared-RGB distance; ties go
    // to the lowest index. `palette` holds the 256 entries in
    // 8-bit display space (6-bit stored values shifted left 2).
    uint8_t Nearest_Palette_Index(const s_rgb& rgb,
        const uint8_t palette[256][3]);

    // For each destination palette index, the palette index of src
    // blended over it at alpha: a real per-pixel alpha blend, done
    // in RGB space and quantized back to the palette. Build once
    // per (src, alpha) and reuse for every pixel of a fill.
    std::array<uint8_t, 256> Build_Blend_Table(const s_rgb& src,
        int alpha, const uint8_t palette[256][3]);

} // namespace SETTINGS_BLEND
