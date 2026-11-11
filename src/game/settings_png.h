#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SETTINGS_PNG {

    // An RGBA image decoded from a PNG: 4 bytes per pixel, row-major,
    // top row first. Alpha is the PNG's own alpha (0 = transparent,
    // 255 = opaque); the blit blends it over the page (real alpha
    // blend), so a sprite's glow halo shows the background through.
    struct s_rgba_image {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;
        bool empty() const {
            return width <= 0 || height <= 0 || pixels.empty();
        }
        // RGBA bytes of the pixel at (x, y); nullptr when outside.
        const uint8_t* Pixel(int x, int y) const {
            if (x < 0 || y < 0 || x >= width || y >= height) {
                return nullptr;
            }
            return pixels.data() +
                ((size_t)y * (size_t)width + (size_t)x) * 4;
        }
    };

    // Decode a PNG from memory (stb_image, forced to 4 channels).
    // An empty image on failure: not a PNG, truncated, or an
    // unsupported format. Never throws.
    s_rgba_image Decode_Png(const uint8_t* data, size_t size);

    // Load a PNG through the game VFS (base.zip / data dir), e.g.
    // "sprites/hires/button-normal.png". An empty image when the
    // file is missing or does not decode: the caller falls back to
    // its vector rendering, so the screen works with a stale
    // base.zip that predates a sprite.
    s_rgba_image Load_Png(const char* vfs_path);

} // namespace SETTINGS_PNG
