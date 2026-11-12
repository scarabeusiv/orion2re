#include "pch.h"

#include <algorithm>

#include "game/settings_blend.h"

namespace SETTINGS_BLEND {

    int Luminance(uint8_t r, uint8_t g, uint8_t b) {
        return (299 * (int)r + 587 * (int)g + 114 * (int)b) / 1000;
    }

    uint8_t Blend_Channel(uint8_t src, uint8_t dst, int alpha) {
        if (alpha <= 0) {
            return dst;
        }
        if (alpha >= 255) {
            return src;
        }
        return (uint8_t)((src * alpha + dst * (255 - alpha) + 127) / 255);
    }

    s_rgb Blend_RGB(const s_rgb& src, const s_rgb& dst, int alpha) {
        return s_rgb{Blend_Channel(src.r, dst.r, alpha),
            Blend_Channel(src.g, dst.g, alpha),
            Blend_Channel(src.b, dst.b, alpha)};
    }

    uint8_t Nearest_Palette_Index(const s_rgb& rgb,
        const uint8_t palette[256][3]) {
        int best_index = 0;
        int best_dist = -1;
        for (int i = 0; i < 256; ++i) {
            const int dr = (int)rgb.r - (int)palette[i][0];
            const int dg = (int)rgb.g - (int)palette[i][1];
            const int db = (int)rgb.b - (int)palette[i][2];
            const int dist = dr * dr + dg * dg + db * db;
            if (best_dist < 0 || dist < best_dist) {
                best_dist = dist;
                best_index = i;
            }
        }
        return (uint8_t)best_index;
    }

    std::array<uint8_t, 256> Build_Blend_Table(const s_rgb& src,
        int alpha, const uint8_t palette[256][3]) {
        std::array<uint8_t, 256> table{};
        if (alpha <= 0) {
            for (int i = 0; i < 256; ++i) {
                table[(size_t)i] = (uint8_t)i;
            }
            return table;
        }
        for (int i = 0; i < 256; ++i) {
            const s_rgb dst{palette[i][0], palette[i][1], palette[i][2]};
            table[(size_t)i] =
                Nearest_Palette_Index(Blend_RGB(src, dst, alpha), palette);
        }
        return table;
    }

} // namespace SETTINGS_BLEND
