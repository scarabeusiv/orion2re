#include "pch.h"

#include "doctest/doctest.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "fakes/fake_test_support.h"
#include "game/allocate.h"
#include "game/animate.h"
#include "game/fonts.h"
#include "game/sprite_art.h"
#include "game/ui_kit.h"
#include "game/video.h"

namespace test_ui_kit_detail {
    // A well-formed native bitmap frame: 12-byte header, one frame
    // offset, width*height index bytes, zero padding to a multiple of 4.
    std::string Make_Frame(int16_t width, int16_t height, uint8_t fill) {
        const int32_t pixel_count = (int32_t)width * (int32_t)height;
        const int32_t size = (pixel_count + 16 + 3) / 4 * 4;
        std::string data((size_t)size, '\0');
        std::memcpy(&data[0], &width, sizeof(width));
        std::memcpy(&data[2], &height, sizeof(height));
        const int16_t frame_count = 1;
        std::memcpy(&data[6], &frame_count, sizeof(frame_count));
        data[11] = 1; // draw mode BITMAP
        const uint32_t offset = 16;
        std::memcpy(&data[12], &offset, sizeof(offset));
        for (int32_t i = 0; i < pixel_count; ++i) {
            data[16 + i] = (char)fill;
        }
        return data;
    }

    std::string Make_Frame_Pixels(int16_t width, int16_t height,
        const std::vector<uint8_t>& pixels) {
        std::string frame = Make_Frame(width, height, 0);
        for (size_t i = 0; i < pixels.size(); ++i) {
            frame[16 + i] = (char)pixels[i];
        }
        return frame;
    }

    struct Scoped_Video_Page {
        std::vector<uint8_t> page;
        uint8_t* previous_page;

        Scoped_Video_Page()
            : page((size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y, 0xFF),
              previous_page(video::current_video_page) {
            video::current_video_page = page.data();
        }

        ~Scoped_Video_Page() {
            video::current_video_page = previous_page;
        }

        [[nodiscard]] uint8_t At(int16_t x, int16_t y) const {
            return page[(size_t)y * (size_t)video::g_screen_res_x + (size_t)x];
        }
    };

    struct Print_Record {
        int16_t x;
        int16_t y;
        std::string text;
        bool active;
    };

    // A font with scripted widths: 10 px per character, so the tab
    // geometry tests measure the bar, not the engine font.
    class Fake_Font : public ui_kit::Font {
      public:
        mutable std::vector<Print_Record> prints;

        int16_t Width(const char* text) const override {
            return (int16_t)(10 * std::strlen(text));
        }

        int16_t Height() const override {
            return 12;
        }

        void Print(int16_t x, int16_t y, const char* text,
            bool active) const override {
            prints.push_back({x, y, text, active});
        }
    };

    class Fake_Theme : public ui_kit::Theme {
      public:
        Fake_Font font;
        ui_kit::Tab_Frames frames;
        ui_kit::Window_Style window;

        Fake_Theme() {
            window.frame_outer = 244;
            window.frame_inner = 230;
            window.title_font = &font;
            window.title_y = 8;
        }

        const ui_kit::Font& Tab_Font() const override {
            return font;
        }

        ui_kit::Tab_Frames Tab_Frames() const override {
            return frames;
        }

        ui_kit::Window_Style Window_Style() const override {
            return window;
        }

        int16_t Tab_Padding() const override {
            return 8;
        }

        int16_t Tab_Gap() const override {
            return 1;
        }

        int16_t Tab_Height() const override {
            return 24;
        }

        int16_t Tab_Label_Y_Offset() const override {
            return 8;
        }
    };
} // namespace test_ui_kit_detail

DOCTEST_TEST_SUITE("ui kit") {
    DOCTEST_TEST_CASE("tab bar lays tabs out from label widths with padding and a 1 px gap") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::Tab_Bar bar(16, 446, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Add_Tab("CCC", false);

        // Width is label width + 8 px padding per side.
        DOCTEST_REQUIRE(bar.tabs().size() == 3);
        DOCTEST_CHECK(bar.tabs()[0].width == 10 + 16);
        DOCTEST_CHECK(bar.tabs()[1].width == 20 + 16);
        DOCTEST_CHECK(bar.tabs()[2].width == 30 + 16);
        // Positions advance by width + the 1 px colony gap.
        DOCTEST_CHECK(bar.tabs()[0].x == 16);
        DOCTEST_CHECK(bar.tabs()[1].x == 16 + 26 + 1);
        DOCTEST_CHECK(bar.tabs()[2].x == 16 + 26 + 1 + 36 + 1);
        DOCTEST_CHECK(bar.End_X() == 16 + 26 + 1 + 36 + 1 + 46);
        DOCTEST_CHECK(bar.y() == 446);
        DOCTEST_CHECK(bar.height() == 24);
    }

    DOCTEST_TEST_CASE("tab bar draws the active frame for the active tab and centres the labels") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Fake_Theme theme;
        theme.frames.normal.frame = (const s_animation_header*)normal.data();
        theme.frames.active.frame = (const s_animation_header*)active.data();
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Draw();

        // First tab (active) draws the active frame, second the normal
        // frame: the left slices land at each tab's x.
        DOCTEST_CHECK(page.At(0, 0) == 11);
        DOCTEST_CHECK(page.At(1, 0) == 12);
        DOCTEST_CHECK(page.At(27, 0) == 1);
        DOCTEST_CHECK(page.At(28, 0) == 2);
        // Labels are centred in their tabs, at the theme's y offset,
        // with the active flag carried through to the font.
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
        DOCTEST_CHECK(theme.font.prints[0].text == "A");
        DOCTEST_CHECK(theme.font.prints[0].x == 0 + (26 - 10) / 2);
        DOCTEST_CHECK(theme.font.prints[0].y == 8);
        DOCTEST_CHECK(theme.font.prints[0].active);
        DOCTEST_CHECK(theme.font.prints[1].text == "BB");
        DOCTEST_CHECK(theme.font.prints[1].x == 27 + (36 - 20) / 2);
        DOCTEST_CHECK(theme.font.prints[1].y == 8);
        DOCTEST_CHECK_FALSE(theme.font.prints[1].active);
    }

    DOCTEST_TEST_CASE("tab bar resamples the active frame's gradient middle instead of tiling it") {
        using namespace test_ui_kit_detail;
        // 10x1 active frame, middle a rising gradient [13..18].
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Fake_Theme theme;
        theme.frames.active.frame = (const s_animation_header*)active.data();
        theme.frames.active.resample_middle = true;
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, theme);
        bar.Add_Tab("A", true); // 26 wide: middle resampled 6 -> 22
        bar.Draw();

        // The resampled middle starts at 13 and ends at 18, and never
        // repeats: a tiled middle would run 13..18 twice.
        DOCTEST_CHECK(page.At(2, 0) == 13);
        DOCTEST_CHECK(page.At(23, 0) == 18);
        uint8_t previous = 0;
        for (int16_t x = 2; x < 24; ++x) {
            DOCTEST_CHECK(page.At(x, 0) >= previous);
            previous = page.At(x, 0);
        }
    }

    DOCTEST_TEST_CASE("tab bar falls back to a flat fill when a frame is missing") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        theme.frames.normal.frame = nullptr;
        theme.frames.active.frame = nullptr;
        theme.frames.fallback_fill_normal = 50;
        theme.frames.fallback_fill_active = 138;
        theme.frames.fallback_border = 234;

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Draw();

        // One fill per tab in the fallback colour, one border box
        // each, and the labels still print.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 138);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 50);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);
        DOCTEST_CHECK(test::fakes::g_box_calls[1].color == 234);
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
    }

    DOCTEST_TEST_CASE("engine font delegates width and print to the engine font") {
        using namespace test_ui_kit_detail;
        const s_colors normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors active = {{138, 250, 250, 250, 250, 250, 250, 250}};
        ui_kit::Engine_Font font(2, normal, active);

        test::fakes::Reset_All_();
        test::fakes::g_string_width = 42;
        test::fakes::g_font_height = 12;
        DOCTEST_CHECK(font.Width("Game Internals") == 42);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_CHECK(font.Height() == 12);

        font.Print(10, 20, "Mods", true);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 138);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[1] == 250);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].x == 10);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].y == 20);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "Mods");

        font.Print(30, 40, "Mods", false);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 29);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[1] == 248);
    }

    DOCTEST_TEST_CASE("colony theme hands out the colony tab font and frames") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame(50, 24, 50);
        const std::string active = Make_Frame(50, 24, 138);
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme((const s_animation_header*)normal.data(),
            (const s_animation_header*)active.data(), title);

        // The tab font is engine font style 2 with the measured
        // colony label ramps: white face / dark edge at rest,
        // blue-white face / mid-blue edge when active.
        test::fakes::Reset_All_();
        theme.Tab_Font().Print(0, 0, "Name", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 29);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[1] == 248);
        theme.Tab_Font().Print(0, 0, "Name", true);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 138);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[1] == 250);

        const ui_kit::Tab_Frames frames = theme.Tab_Frames();
        DOCTEST_CHECK(frames.normal.frame == (const s_animation_header*)normal.data());
        DOCTEST_CHECK(frames.active.frame == (const s_animation_header*)active.data());
        DOCTEST_CHECK_FALSE(frames.normal.resample_middle);
        DOCTEST_CHECK(frames.active.resample_middle);
        DOCTEST_CHECK(frames.left_slice == 4);
        DOCTEST_CHECK(frames.right_slice == 0);

        DOCTEST_CHECK(theme.Tab_Padding() == 8);
        DOCTEST_CHECK(theme.Tab_Gap() == 1);
        DOCTEST_CHECK(theme.Tab_Height() == 24);
        DOCTEST_CHECK(theme.Tab_Label_Y_Offset() == 8);

        // The window style is the colony frame: light outer line,
        // dark inner line, style-4 title at y 8.
        const ui_kit::Window_Style window = theme.Window_Style();
        DOCTEST_CHECK(window.frame_outer == 244);
        DOCTEST_CHECK(window.frame_inner == 230);
        DOCTEST_CHECK(window.title_y == 8);
        DOCTEST_REQUIRE(window.title_font != nullptr);
        test::fakes::Reset_All_();
        test::fakes::g_string_width = 70;
        ui_kit::Draw_Window_Frame(theme, 0, 0, 640, 480, "Settings");
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].x == 0);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].width == 640);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 244);
        DOCTEST_CHECK(test::fakes::g_box_calls[1].x == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[1].width == 638);
        DOCTEST_CHECK(test::fakes::g_box_calls[1].color == 230);
        DOCTEST_CHECK(test::fakes::g_font_style == 4);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].x == (640 - 70) / 2);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].y == 8);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "Settings");
    }
} // DOCTEST_TEST_SUITE("ui kit")
