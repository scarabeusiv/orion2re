#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_blend.h"

namespace {

    using namespace SETTINGS_BLEND;

    // A small palette: black, dark grey, mid grey, light grey, white,
    // then red/green/blue ramps are unnecessary for the checks below.
    void Fill_Test_Palette(uint8_t palette[256][3]) {
        for (int i = 0; i < 256; ++i) {
            palette[i][0] = 0;
            palette[i][1] = 0;
            palette[i][2] = 0;
        }
        palette[0][0] = 0;
        palette[0][1] = 0;
        palette[0][2] = 0;
        palette[1][0] = 64;
        palette[1][1] = 64;
        palette[1][2] = 64;
        palette[2][0] = 128;
        palette[2][1] = 128;
        palette[2][2] = 128;
        palette[3][0] = 192;
        palette[3][1] = 192;
        palette[3][2] = 192;
        palette[4][0] = 255;
        palette[4][1] = 255;
        palette[4][2] = 255;
    }

    TEST_CASE("settings_blend luminance") {
        CHECK(Luminance(0, 0, 0) == 0);
        CHECK(Luminance(255, 255, 255) == 255);
        CHECK(Luminance(255, 0, 0) == 76);
        CHECK(Luminance(0, 255, 0) == 149);
        CHECK(Luminance(0, 0, 255) == 29);
    }

    TEST_CASE("settings_blend channel endpoints and midpoint") {
        CHECK(Blend_Channel(200, 50, 0) == 50);
        CHECK(Blend_Channel(200, 50, 255) == 200);
        CHECK(Blend_Channel(255, 0, 128) == 128);
        CHECK(Blend_Channel(0, 255, 128) == 127);
        CHECK(Blend_Channel(100, 100, 128) == 100);
    }

    TEST_CASE("settings_blend nearest palette index") {
        uint8_t palette[256][3];
        Fill_Test_Palette(palette);
        CHECK(Nearest_Palette_Index(s_rgb{0, 0, 0}, palette) == 0);
        CHECK(Nearest_Palette_Index(s_rgb{255, 255, 255}, palette) == 4);
        CHECK(Nearest_Palette_Index(s_rgb{100, 100, 100}, palette) == 2);
        CHECK(Nearest_Palette_Index(s_rgb{160, 160, 160}, palette) == 2);
        CHECK(Nearest_Palette_Index(s_rgb{161, 161, 161}, palette) == 3);
    }

    TEST_CASE("settings_blend table endpoints") {
        uint8_t palette[256][3];
        Fill_Test_Palette(palette);
        const auto identity = Build_Blend_Table(s_rgb{200, 200, 200}, 0, palette);
        for (int i = 0; i < 256; ++i) {
            CHECK(identity[(size_t)i] == (uint8_t)i);
        }
        const auto opaque = Build_Blend_Table(s_rgb{255, 255, 255}, 255, palette);
        for (int i = 0; i < 256; ++i) {
            CHECK(opaque[(size_t)i] == 4);
        }
        // Half blend of white over black lands on the mid grey.
        const auto half = Build_Blend_Table(s_rgb{255, 255, 255}, 128, palette);
        CHECK(half[0] == 2);
        CHECK(half[4] == 4);
    }

} // namespace
