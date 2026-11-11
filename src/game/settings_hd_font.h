#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SETTINGS_HD_FONT {

    // One decoded glyph: coverage 0..255 per pixel, row-major.
    // Coverage comes from the engine font's own antialiasing: the
    // glyph stream's ink levels map through the font's 8-entry
    // colour ramp, and a level's coverage is its colour's
    // luminance normalized across the ramp (darkest = 0,
    // brightest = 255). The letterforms stay the game's own --
    // this only decodes them, it never redraws them.
    struct s_hd_glyph {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> coverage;
    };

    // Decode one engine-font glyph row stream (the format
    // fonts.cpp's Print_Character_ASM consumes). Per row, bytes
    // until 0x80: 0x00 = one transparent pixel; 0x01..0x7F = one
    // ink pixel whose level is byte-1 (an index into the 8-entry
    // selected-colour ramp); 0x81..0xFF = skip (byte & 0x7F)
    // transparent pixels. `ramp_luminance[k]` is the luminance
    // (0..255) of the ramp's k-th colour. Malformed streams stop
    // at a byte bound, never past it.
    s_hd_glyph Decode_Glyph(const uint8_t* data, int width, int height,
        const uint8_t ramp_luminance[8]);

    // Bilinear resample of a coverage grid to dw x dh (both >= 1).
    // The upscale is what makes the glyph read as HD: edges land
    // between source pixels and come out as smooth partial
    // coverage instead of doubled square pixels.
    std::vector<uint8_t> Scale_Coverage(const std::vector<uint8_t>& src,
        int sw, int sh, int dw, int dh);

    // Greedy word wrap: lines of at most max_width (measured with
    // `measure`, same units), at most max_lines lines; when the
    // text does not fit, the last line is truncated with "...".
    // Words longer than max_width are hard-split. '\n' forces a
    // line break.
    std::vector<std::string> Wrap_Text(const std::string& text,
        int max_width, const std::function<int(const std::string&)>& measure,
        int max_lines);

    // Truncate to max_width, appending "..." (the dots count
    // toward the width). Returns "" when not even "..." fits.
    std::string Truncate_To_Width(const std::string& text, int max_width,
        const std::function<int(const std::string&)>& measure);

} // namespace SETTINGS_HD_FONT
