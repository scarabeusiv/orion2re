#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_hd_font.h"

namespace {

    using namespace SETTINGS_HD_FONT;

    // Fixed 10 px per character, so widths are exact in the tests.
    int Fixed_Measure(const std::string& text) {
        return (int)text.size() * 10;
    }

    TEST_CASE("settings_hd_font decode glyph") {
        // Ramp luminance 0..255 in 8 steps.
        const uint8_t ramp[8] = {0, 36, 73, 109, 146, 182, 219, 255};
        // 3x2 glyph: row 0 = level 0 (darkest, coverage 0), level 4
        // (146), level 7 (255); row 1 = level 3 (109), transparent,
        // transparent. Row 0 ends exactly at the width, so its 0x80
        // terminator must still be consumed: without that, row 1
        // starts on the terminator and decodes blank (style 4 'P'
        // in the real font: every row ends at its width, so every
        // other row went blank and the glyph doubled).
        const uint8_t stream[] = {0x01, 0x05, 0x08, 0x80, 0x04, 0x00, 0x00, 0x80};
        const s_hd_glyph glyph = Decode_Glyph(stream, 3, 2, ramp);
        CHECK(glyph.width == 3);
        CHECK(glyph.height == 2);
        CHECK(glyph.coverage.size() == 6);
        CHECK(glyph.coverage[0] == 0);
        CHECK(glyph.coverage[1] == 146);
        CHECK(glyph.coverage[2] == 255);
        CHECK(glyph.coverage[3] == 109);
        CHECK(glyph.coverage[4] == 0);
        CHECK(glyph.coverage[5] == 0);
    }

    TEST_CASE("settings_hd_font decode row ending past width") {
        // Ramp luminance 0..255 in 8 steps.
        const uint8_t ramp[8] = {0, 36, 73, 109, 146, 182, 219, 255};
        // 3x2 glyph: row 0 = one full-bright pixel then a skip past
        // the width; row 1 = full-bright pixel, then transparent.
        // The row-0 terminator must still be consumed, or row 1
        // decodes blank.
        const uint8_t stream[] = {0x08, 0x83, 0x80, 0x08, 0x00, 0x00, 0x80};
        const s_hd_glyph glyph = Decode_Glyph(stream, 3, 2, ramp);
        CHECK(glyph.coverage[0] == 255);
        CHECK(glyph.coverage[1] == 0);
        CHECK(glyph.coverage[2] == 0);
        CHECK(glyph.coverage[3] == 255);
        CHECK(glyph.coverage[4] == 0);
        CHECK(glyph.coverage[5] == 0);
    }

    TEST_CASE("settings_hd_font decode flat ramp") {
        // All ramp colours the same luminance: every ink pixel is
        // full coverage (no span to normalize across).
        const uint8_t ramp[8] = {128, 128, 128, 128, 128, 128, 128, 128};
        const uint8_t stream[] = {0x03, 0x80};
        const s_hd_glyph glyph = Decode_Glyph(stream, 1, 1, ramp);
        CHECK(glyph.coverage[0] == 255);
    }

    TEST_CASE("settings_hd_font decode empty glyph") {
        const uint8_t ramp[8] = {0, 36, 73, 109, 146, 182, 219, 255};
        const uint8_t stream[] = {0x80};
        const s_hd_glyph glyph = Decode_Glyph(stream, 4, 1, ramp);
        CHECK(glyph.coverage.size() == 4);
        for (uint8_t c : glyph.coverage) {
            CHECK(c == 0);
        }
        const s_hd_glyph null_glyph = Decode_Glyph(nullptr, 4, 1, ramp);
        CHECK(null_glyph.coverage.size() == 4);
    }

    TEST_CASE("settings_hd_font scale coverage") {
        const std::vector<uint8_t> one = {200};
        const std::vector<uint8_t> scaled = Scale_Coverage(one, 1, 1, 2, 2);
        CHECK(scaled.size() == 4);
        for (uint8_t c : scaled) {
            CHECK(c == 200);
        }
        // 2x1 black-to-white, upscaled 4x1: the middle samples
        // interpolate (64, 191), the ends clamp.
        const std::vector<uint8_t> ramp2 = {0, 255};
        const std::vector<uint8_t> wide = Scale_Coverage(ramp2, 2, 1, 4, 1);
        CHECK(wide.size() == 4);
        CHECK(wide[0] == 0);
        CHECK(wide[1] == 64);
        CHECK(wide[2] == 191);
        CHECK(wide[3] == 255);
        // Same size is a copy.
        const std::vector<uint8_t> same = Scale_Coverage(ramp2, 2, 1, 2, 1);
        CHECK(same == ramp2);
    }

    TEST_CASE("settings_hd_font truncate") {
        CHECK(Truncate_To_Width("abcdefgh", 50, Fixed_Measure) == "ab...");
        CHECK(Truncate_To_Width("abc", 50, Fixed_Measure) == "abc");
        CHECK(Truncate_To_Width("abcdefgh", 20, Fixed_Measure) == "");
        CHECK(Truncate_To_Width("abcdefgh", 30, Fixed_Measure) == "...");
        CHECK(Truncate_To_Width("abc def", 50, Fixed_Measure) == "ab...");
    }

    TEST_CASE("settings_hd_font truncate tail") {
        CHECK(Truncate_Tail_To_Width("abcdefgh", 50, Fixed_Measure) == "...gh");
        CHECK(Truncate_Tail_To_Width("abc", 50, Fixed_Measure) == "abc");
        CHECK(Truncate_Tail_To_Width("abcdefgh", 20, Fixed_Measure) == "");
        CHECK(Truncate_Tail_To_Width("abcdefgh", 30, Fixed_Measure) == "...");
        CHECK(Truncate_Tail_To_Width("abc def", 50, Fixed_Measure) == "...ef");
    }

    TEST_CASE("settings_hd_font wrap") {
        const std::vector<std::string> lines =
            Wrap_Text("aaa bbb ccc ddd", 70, Fixed_Measure, 5);
        CHECK(lines.size() == 2);
        CHECK(lines[0] == "aaa bbb");
        CHECK(lines[1] == "ccc ddd");
        // One line only: the rest folds into an ellipsis.
        const std::vector<std::string> one =
            Wrap_Text("aaa bbb ccc ddd", 70, Fixed_Measure, 1);
        CHECK(one.size() == 1);
        CHECK(one[0] == "aaa...");
        // A word wider than the line hard-splits.
        const std::vector<std::string> split =
            Wrap_Text("abcdefghij", 50, Fixed_Measure, 5);
        CHECK(split.size() == 2);
        CHECK(split[0] == "abcde");
        CHECK(split[1] == "fghij");
        // Forced break.
        const std::vector<std::string> forced =
            Wrap_Text("aaa\nbbb", 70, Fixed_Measure, 5);
        CHECK(forced.size() == 2);
        CHECK(forced[0] == "aaa");
        CHECK(forced[1] == "bbb");
    }

} // namespace
