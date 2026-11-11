#include "pch.h"

#include <algorithm>
#include <cmath>

#include "game/settings_hd_font.h"

namespace SETTINGS_HD_FONT {

    s_hd_glyph Decode_Glyph(const uint8_t* data, int width, int height,
        const uint8_t ramp_luminance[8]) {
        s_hd_glyph glyph;
        glyph.width = width;
        glyph.height = height;
        glyph.coverage.assign((size_t)std::max(0, width) * (size_t)std::max(0, height), 0);
        if (data == nullptr || width <= 0 || height <= 0) {
            return glyph;
        }
        // The ramp's luminance span: the darkest ramp colour is
        // the glyph's outermost antialias pixel (coverage 0), the
        // brightest is its core (coverage 255).
        int lum_min = ramp_luminance[0];
        int lum_max = ramp_luminance[0];
        for (int i = 1; i < 8; ++i) {
            lum_min = std::min(lum_min, (int)ramp_luminance[i]);
            lum_max = std::max(lum_max, (int)ramp_luminance[i]);
        }
        // Worst case stream: one byte per pixel plus one 0x80
        // terminator per row.
        const size_t max_bytes = (size_t)(width + 1) * (size_t)height + 16;
        size_t pos = 0;
        for (int y = 0; y < height; ++y) {
            // Mirror the engine decoders (fonts::Print_Character_ASM,
            // Print_Clipped_Letter_To_Bitmap): a row is consumed up
            // to its 0x80 terminator whatever x reaches. A row whose
            // ink ends exactly at the width must still consume its
            // terminator, or the next row starts on the terminator
            // and decodes blank -- every other row goes blank and
            // the glyph doubles (style 4 'P': every row ends at its
            // width). Ink past the width is dropped, never written
            // out of the row.
            int x = 0;
            while (true) {
                if (pos >= max_bytes) {
                    return glyph;
                }
                const uint8_t byte = data[pos++];
                if (byte == 0x80) {
                    break;
                }
                if (byte == 0x00) {
                    ++x;
                    continue;
                }
                if (byte < 0x80) {
                    if (x < width) {
                        const int level = std::min(7, (int)byte - 1);
                        uint8_t coverage = 255;
                        if (lum_max > lum_min) {
                            coverage = (uint8_t)((ramp_luminance[level] - lum_min) * 255 /
                                (lum_max - lum_min));
                        }
                        glyph.coverage[(size_t)y * (size_t)width + (size_t)x] = coverage;
                    }
                    ++x;
                    continue;
                }
                x += (byte & 0x7F);
            }
        }
        return glyph;
    }

    std::vector<uint8_t> Scale_Coverage(const std::vector<uint8_t>& src,
        int sw, int sh, int dw, int dh) {
        std::vector<uint8_t> dst((size_t)std::max(1, dw) * (size_t)std::max(1, dh), 0);
        if (src.empty() || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
            return dst;
        }
        if (sw == dw && sh == dh) {
            dst = src;
            return dst;
        }
        for (int y = 0; y < dh; ++y) {
            const double sy = (y + 0.5) * (double)sh / (double)dh - 0.5;
            const int y0 = (int)std::floor(sy);
            const int y1 = y0 + 1;
            const double fy = sy - (double)y0;
            const int cy0 = std::min(std::max(y0, 0), sh - 1);
            const int cy1 = std::min(std::max(y1, 0), sh - 1);
            for (int x = 0; x < dw; ++x) {
                const double sx = (x + 0.5) * (double)sw / (double)dw - 0.5;
                const int x0 = (int)std::floor(sx);
                const int x1 = x0 + 1;
                const double fx = sx - (double)x0;
                const int cx0 = std::min(std::max(x0, 0), sw - 1);
                const int cx1 = std::min(std::max(x1, 0), sw - 1);
                const double v00 = src[(size_t)cy0 * (size_t)sw + (size_t)cx0];
                const double v01 = src[(size_t)cy0 * (size_t)sw + (size_t)cx1];
                const double v10 = src[(size_t)cy1 * (size_t)sw + (size_t)cx0];
                const double v11 = src[(size_t)cy1 * (size_t)sw + (size_t)cx1];
                const double v = (v00 * (1.0 - fx) + v01 * fx) * (1.0 - fy) +
                    (v10 * (1.0 - fx) + v11 * fx) * fy;
                dst[(size_t)y * (size_t)dw + (size_t)x] =
                    (uint8_t)std::min(255.0, std::max(0.0, v + 0.5));
            }
        }
        return dst;
    }

    namespace {

        std::string RTrim(const std::string& text) {
            const size_t end = text.find_last_not_of(' ');
            return end == std::string::npos ? std::string() : text.substr(0, end + 1);
        }

        // Longest prefix of text whose width plus "..." still fits;
        // the ellipsis is always appended (the caller already knows
        // text is being cut). Returns "" when not even "..." fits.
        std::string Truncate_With_Ellipsis(const std::string& text, int max_width,
            const std::function<int(const std::string&)>& measure) {
            if (measure("...") > max_width) {
                return std::string();
            }
            int lo = 0;
            int hi = (int)text.size();
            while (lo < hi) {
                const int mid = (lo + hi + 1) / 2;
                if (measure(text.substr(0, (size_t)mid) + "...") <= max_width) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            return RTrim(text.substr(0, (size_t)lo)) + "...";
        }

        // Split one over-wide word into chunks that fit, by measure.
        std::vector<std::string> Split_Word(const std::string& word, int max_width,
            const std::function<int(const std::string&)>& measure) {
            std::vector<std::string> chunks;
            std::string current;
            for (char c : word) {
                if (!current.empty() &&
                    measure(current + c) > max_width) {
                    chunks.push_back(current);
                    current.clear();
                }
                current += c;
            }
            if (!current.empty()) {
                chunks.push_back(current);
            }
            return chunks;
        }

    } // namespace

    std::string Truncate_To_Width(const std::string& text, int max_width,
        const std::function<int(const std::string&)>& measure) {
        if (measure(text) <= max_width) {
            return text;
        }
        return Truncate_With_Ellipsis(text, max_width, measure);
    }

    std::vector<std::string> Wrap_Text(const std::string& text, int max_width,
        const std::function<int(const std::string&)>& measure, int max_lines) {
        std::vector<std::string> lines;
        if (max_lines <= 0 || max_width <= 0) {
            return lines;
        }
        // Forced breaks first, then greedy word packing per paragraph.
        size_t start = 0;
        while (start <= text.size()) {
            const size_t nl = text.find('\n', start);
            const std::string paragraph = nl == std::string::npos
                ? text.substr(start)
                : text.substr(start, nl - start);
            std::string current;
            size_t word_start = 0;
            while (word_start <= paragraph.size()) {
                const size_t sp = paragraph.find(' ', word_start);
                const std::string word = sp == std::string::npos
                    ? paragraph.substr(word_start)
                    : paragraph.substr(word_start, sp - word_start);
                if (!word.empty()) {
                    std::vector<std::string> pieces;
                    if (measure(word) > max_width) {
                        pieces = Split_Word(word, max_width, measure);
                    } else {
                        pieces.push_back(word);
                    }
                    for (const std::string& piece : pieces) {
                        const std::string candidate =
                            current.empty() ? piece : current + " " + piece;
                        if (measure(candidate) <= max_width) {
                            current = candidate;
                        } else {
                            if (!current.empty()) {
                                lines.push_back(current);
                            }
                            current = piece;
                        }
                    }
                }
                if (sp == std::string::npos) {
                    break;
                }
                word_start = sp + 1;
            }
            if (!current.empty() || paragraph.empty()) {
                lines.push_back(current);
            }
            if (nl == std::string::npos) {
                break;
            }
            start = nl + 1;
        }
        if ((int)lines.size() > max_lines) {
            lines.resize((size_t)max_lines);
            // Lines were dropped: the last line always carries the
            // ellipsis, even when it fits on its own.
            lines.back() = Truncate_With_Ellipsis(lines.back(), max_width, measure);
        }
        return lines;
    }

} // namespace SETTINGS_HD_FONT
