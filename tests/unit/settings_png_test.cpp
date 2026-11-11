#include "pch.h"

#include <cstdint>

#include "doctest/doctest.h"
#include "game/settings_png.h"

namespace {

    using namespace SETTINGS_PNG;

    // A 4x3 RGBA PNG, written by PIL: red, green, a
    // semi-transparent blue and a transparent pixel on the first
    // row, then two rows of known colours (the last pixel is a
    // 64-alpha grey). It pins the decode: dimensions, RGBA
    // channels, and alpha, including a fully transparent pixel.
    const uint8_t kPng[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13,
        73, 72, 68, 82, 0, 0, 0, 4, 0, 0, 0, 3,
        8, 6, 0, 0, 0, 180, 244, 174, 198, 0, 0, 0,
        49, 73, 68, 65, 84, 120, 156, 53, 195, 49, 17, 0,
        32, 12, 0, 177, 47, 221, 152, 153, 17, 129, 2, 196,
        32, 164, 242, 112, 245, 76, 228, 46, 8, 34, 130, 213,
        199, 36, 162, 165, 153, 201, 207, 61, 75, 64, 213, 170,
        218, 15, 103, 107, 15, 241, 8, 151, 109, 214, 0, 0,
        0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
    };

    void Check_Pixel(const s_rgba_image& image, int x, int y,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        const uint8_t* pixel = image.Pixel(x, y);
        REQUIRE(pixel != nullptr);
        CHECK(pixel[0] == r);
        CHECK(pixel[1] == g);
        CHECK(pixel[2] == b);
        CHECK(pixel[3] == a);
    }

    TEST_CASE("png decodes a small RGBA image") {
        const s_rgba_image image = Decode_Png(kPng, sizeof(kPng));
        CHECK_FALSE(image.empty());
        CHECK(image.width == 4);
        CHECK(image.height == 3);
        CHECK(image.pixels.size() == 4 * 3 * 4);
        Check_Pixel(image, 0, 0, 255, 0, 0, 255);
        Check_Pixel(image, 1, 0, 0, 255, 0, 255);
        Check_Pixel(image, 2, 0, 0, 0, 255, 128);
        Check_Pixel(image, 3, 0, 10, 20, 30, 0);
        Check_Pixel(image, 0, 1, 1, 2, 3, 255);
        Check_Pixel(image, 3, 1, 10, 11, 12, 255);
        Check_Pixel(image, 0, 2, 200, 100, 50, 255);
        Check_Pixel(image, 2, 2, 255, 255, 255, 255);
        Check_Pixel(image, 3, 2, 128, 128, 128, 64);
        // Outside the image: no pixel.
        CHECK(image.Pixel(-1, 0) == nullptr);
        CHECK(image.Pixel(4, 0) == nullptr);
        CHECK(image.Pixel(0, 3) == nullptr);
    }

    TEST_CASE("png decode refuses garbage and empty input") {
        // Not a PNG.
        const uint8_t garbage[] = {1, 2, 3, 4, 5, 6, 7, 8};
        CHECK(Decode_Png(garbage, sizeof(garbage)).empty());
        // Truncated PNG (the header only).
        CHECK(Decode_Png(kPng, 8).empty());
        // Empty input.
        CHECK(Decode_Png(nullptr, 0).empty());
        CHECK(Decode_Png(kPng, 0).empty());
    }

} // namespace
