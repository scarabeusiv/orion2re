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
        // Per-tab frames and gap profiles for the per-tab test:
        // when set, Tab_Frames_For_Tab / Tab_Gap_Profile serve
        // them; otherwise the defaults (shared frames, no gap
        // fill).
        std::vector<ui_kit::Tab_Frames> per_tab_frames;
        std::vector<std::vector<uint8_t>> gap_profiles;
        ui_kit::Frame_Style frame;
        ui_kit::Spacing spacing;
        ui_kit::List_Style list;
        ui_kit::Button_Style buttons;
        ui_kit::Image_Button_Frames return_frames;

        Fake_Theme() {
            frame.thickness_left = 3;
            frame.thickness_right = 3;
            frame.thickness_top = 3;
            frame.thickness_bottom = 3;
            frame.inset_left = 3;
            frame.inset_right = 3;
            frame.inset_top = 3;
            frame.inset_bottom = 3;
            frame.profile = {10, 11, 12};
            frame.title_font = &font;
            frame.title_y = 8;
            frame.bottom_bar_h = 24;
            spacing.row_height = 30;
            spacing.row_gap = 1;
            spacing.header_to_first_row = 3;
            spacing.list_footing = 4;
            spacing.list_to_info = 4;
            spacing.info_to_bar = 7;
            list.interior = 230;
            list.border = 234;
            list.frame_light = 241;
            list.frame_shade = 235;
            list.divider_left = 233;
            list.divider_mid = 235;
            list.divider_right = 232;
            list.plate_face = 233;
            // The colony's button colours (see ui_kit.h).
            buttons.face = 238;
            buttons.highlight = 243;
            buttons.shadow = 232;
            buttons.primary_face = 138;
            buttons.primary_highlight = 154;
            buttons.disabled_face = 234;
            buttons.cell_interior = 230;
            buttons.cell_border = 234;
            buttons.toggle_on_base = 154;
            buttons.toggle_on_bright = 247;
            buttons.toggle_off_base = 236;
            buttons.toggle_off_bright = 246;
            buttons.label_font = &font;
            buttons.cell_font = &font;
        }

        const ui_kit::Font& Tab_Font() const override {
            return font;
        }

        const ui_kit::Font& Header_Font() const override {
            return font;
        }

        ui_kit::Tab_Frames Tab_Frames() const override {
            return frames;
        }

        ui_kit::Tab_Frames Tab_Frames_For_Tab(
            int16_t tab_index) const override {
            if (tab_index >= 0 &&
                (size_t)tab_index < per_tab_frames.size()) {
                return per_tab_frames[(size_t)tab_index];
            }
            return frames;
        }

        std::vector<uint8_t> Tab_Gap_Profile(
            int16_t gap_index) const override {
            if (gap_index >= 0 &&
                (size_t)gap_index < gap_profiles.size()) {
                return gap_profiles[(size_t)gap_index];
            }
            return {};
        }

        ui_kit::Frame_Style Frame_Style() const override {
            return frame;
        }

        ui_kit::Spacing Spacing() const override {
            return spacing;
        }

        ui_kit::List_Style List_Style() const override {
            return list;
        }

        ui_kit::Button_Style Button_Style() const override {
            return buttons;
        }

        ui_kit::Image_Button_Frames Return_Frames() const override {
            return return_frames;
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

    // A second asset set: different metrics, different frames, a
    // different font. The bar and the window frame must follow it
    // with no changes to the components -- this is the plug.
    class Alt_Theme : public ui_kit::Theme {
      public:
        Fake_Font font;
        ui_kit::Tab_Frames frames;
        ui_kit::Frame_Style frame;
        ui_kit::Spacing spacing;
        ui_kit::List_Style list;
        ui_kit::Button_Style buttons;
        ui_kit::Image_Button_Frames return_frames;

        Alt_Theme() {
            frame.thickness_left = 4;
            frame.thickness_right = 5;
            frame.thickness_top = 4;
            frame.thickness_bottom = 5;
            frame.inset_left = 5;
            frame.inset_right = 6;
            frame.inset_top = 7;
            frame.inset_bottom = 8;
            frame.profile = {20, 21, 22, 23};
            frame.title_font = &font;
            frame.title_y = 4;
            frame.bottom_bar_h = 30;
            spacing.row_height = 20;
            spacing.row_gap = 2;
            spacing.header_to_first_row = 5;
            spacing.list_footing = 6;
            spacing.list_to_info = 6;
            spacing.info_to_bar = 8;
            list.interior = 10;
            list.border = 11;
            list.frame_light = 21;
            list.frame_shade = 22;
            list.divider_left = 23;
            list.divider_mid = 24;
            list.divider_right = 25;
            list.plate_face = 26;
            // A second asset set's button colours: every value
            // differs from the colony's, so the button must follow
            // the theme, not the colony's numbers.
            buttons.face = 40;
            buttons.highlight = 41;
            buttons.shadow = 42;
            buttons.primary_face = 43;
            buttons.primary_highlight = 44;
            buttons.disabled_face = 45;
            buttons.cell_interior = 46;
            buttons.cell_border = 47;
            buttons.toggle_on_base = 48;
            buttons.toggle_on_bright = 49;
            buttons.toggle_off_base = 50;
            buttons.toggle_off_bright = 51;
            buttons.label_font = &font;
            buttons.cell_font = &font;
        }

        const ui_kit::Font& Tab_Font() const override {
            return font;
        }

        const ui_kit::Font& Header_Font() const override {
            return font;
        }

        ui_kit::Tab_Frames Tab_Frames() const override {
            return frames;
        }

        ui_kit::Frame_Style Frame_Style() const override {
            return frame;
        }

        ui_kit::Spacing Spacing() const override {
            return spacing;
        }

        ui_kit::List_Style List_Style() const override {
            return list;
        }

        ui_kit::Button_Style Button_Style() const override {
            return buttons;
        }

        ui_kit::Image_Button_Frames Return_Frames() const override {
            return return_frames;
        }

        int16_t Tab_Padding() const override {
            return 4;
        }

        int16_t Tab_Gap() const override {
            return 3;
        }

        int16_t Tab_Height() const override {
            return 30;
        }

        int16_t Tab_Label_Y_Offset() const override {
            return 10;
        }
    };

    // A minimal bitmap font for the Bitmap_Font tests: cap 3, one
    // descender row, space advance 2. A is 3 wide normal, 4 wide
    // pressed (the pressed state carries its own width); B is 2
    // wide normal, 3 wide pressed.
    const char* kBitmap_Font_Json = R"JSON({
        "name": "Test",
        "cap_height": 3,
        "descender_rows": 1,
        "space_advance": 2,
        "provenance": "test",
        "glyphs": {
            "A": {"advance": 4, "width": 3, "bitmap": {
                "normal": [[1, null, 1], [1, 1, 1], [null, 1, null]],
                "pressed": [[2, 2, 2, 2], [2, null, null, 2],
                    [2, null, null, 2], [2, 2, 2, 2]]}},
            "B": {"advance": 3, "width": 2, "bitmap": {
                "normal": [[3, 3], [3, null], [3, 3]],
                "pressed": [[4, 4, 4], [4, null, 4], [4, null, 4],
                    [4, 4, 4]]}}
        }
    })JSON";
} // namespace test_ui_kit_detail

DOCTEST_TEST_SUITE("ui kit") {
    DOCTEST_TEST_CASE("tab bar lays tabs out from label widths with padding and a 1 px gap") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::Tab_Bar bar(16, 446, 126, theme);
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

    DOCTEST_TEST_CASE("tab bar fills the strip to its right edge in proportion to label widths") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::Tab_Bar bar(16, 446, 226, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Add_Tab("CCC", false);

        // Natural widths 26/36/46 (total 108) over 208 px of tab
        // width (226 - 16 - 2 gaps): 50/69/89, ending at 226.
        DOCTEST_REQUIRE(bar.tabs().size() == 3);
        DOCTEST_CHECK(bar.tabs()[0].width == 50);
        DOCTEST_CHECK(bar.tabs()[1].width == 69);
        DOCTEST_CHECK(bar.tabs()[2].width == 89);
        DOCTEST_CHECK(bar.tabs()[0].x == 16);
        DOCTEST_CHECK(bar.tabs()[1].x == 16 + 50 + 1);
        DOCTEST_CHECK(bar.tabs()[2].x == 16 + 50 + 1 + 69 + 1);
        DOCTEST_CHECK(bar.End_X() == 226);
        DOCTEST_CHECK(bar.y() == 446);
        DOCTEST_CHECK(bar.height() == 24);
    }

    DOCTEST_TEST_CASE("tab bar draws the active frame for the active tab and centres the labels") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Fake_Theme theme;
        theme.frames.normal.frame =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.frames.active.frame =
            ui_kit::Frame((const s_animation_header*)active.data());
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;
        theme.frames.normal.middle_column = 5; // value 6, tiled

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 63, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Draw();

        // First tab (active) draws the active frame, second the normal
        // frame: the left slices land at each tab's x.
        DOCTEST_CHECK(page.At(0, 0) == 11);
        DOCTEST_CHECK(page.At(1, 0) == 12);
        DOCTEST_CHECK(page.At(27, 0) == 1);
        DOCTEST_CHECK(page.At(28, 0) == 2);
        // The normal tab's flat middle is one face column tiled
        // (three-slice), not the whole middle repeated.
        DOCTEST_CHECK(page.At(29, 0) == 6);
        DOCTEST_CHECK(page.At(60, 0) == 6);
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
        theme.frames.active.frame =
            ui_kit::Frame((const s_animation_header*)active.data());
        theme.frames.active.resample_middle = true;
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 26, theme);
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
        theme.frames.normal.frame = ui_kit::Frame();
        theme.frames.active.frame = ui_kit::Frame();
        theme.frames.fallback_fill_normal = 50;
        theme.frames.fallback_fill_active = 138;
        theme.frames.fallback_border = 234;

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 63, theme);
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

    DOCTEST_TEST_CASE("tab bar follows a second theme without component changes") {
        using namespace test_ui_kit_detail;
        // 10x1 frames: active middle a rising gradient [13..18].
        const std::string normal = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Alt_Theme theme;
        theme.frames.normal.frame =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.frames.active.frame =
            ui_kit::Frame((const s_animation_header*)active.data());
        theme.frames.active.resample_middle = true;
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 49, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        // Alt metrics: padding 4, gap 3, height 30, label offset 10.
        DOCTEST_REQUIRE(bar.tabs().size() == 2);
        DOCTEST_CHECK(bar.tabs()[0].width == 10 + 8);
        DOCTEST_CHECK(bar.tabs()[1].x == 18 + 3);
        DOCTEST_CHECK(bar.tabs()[1].width == 20 + 8);
        DOCTEST_CHECK(bar.height() == 30);
        bar.Draw();
        // The active tab's resampled gradient starts at 13 and ends
        // at 18 at the alt tab's own x; the rest tab's left slice
        // lands at its x.
        DOCTEST_CHECK(page.At(2, 0) == 13);
        DOCTEST_CHECK(page.At(15, 0) == 18);
        DOCTEST_CHECK(page.At(21, 0) == 1);
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
        DOCTEST_CHECK(theme.font.prints[0].y == 10);
        DOCTEST_CHECK(theme.font.prints[1].x == 21 + (28 - 20) / 2);
        DOCTEST_CHECK(theme.font.prints[1].y == 10);
        // The window frame follows the alt chrome, not the colony's:
        // 4 + 5 + 4 + 5 band lines, and the 5th line of a 5 px side
        // repeats the profile's last entry.
        test::fakes::Reset_All_();
        ui_kit::Window_Frame frame(0, 0, 100, 50, theme);
        frame.Draw("Alt");
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 18);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 20);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].y1 == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 23);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].y1 == 45);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].color == 23);
        DOCTEST_CHECK(test::fakes::g_fill_calls[17].x1 == 95);
        DOCTEST_CHECK(test::fakes::g_fill_calls[17].y1 == 4);
        DOCTEST_CHECK(test::fakes::g_fill_calls[17].y2 == 45);
        DOCTEST_CHECK(test::fakes::g_fill_calls[17].color == 23);
        DOCTEST_CHECK(test::fakes::g_box_calls.empty());
        // The two tab labels printed above, plus the frame title.
        DOCTEST_REQUIRE(theme.font.prints.size() == 3);
        DOCTEST_CHECK(theme.font.prints[2].x == 35);
        DOCTEST_CHECK(theme.font.prints[2].y == 4);
        DOCTEST_CHECK(theme.font.prints[2].text == "Alt");
    }

    DOCTEST_TEST_CASE("colony theme falls back to a flat fill when its frames are missing") {
        using namespace test_ui_kit_detail;
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);

        const ui_kit::Tab_Frames frames = theme.Tab_Frames();
        DOCTEST_CHECK(frames.normal.frame.Is_Null());
        DOCTEST_CHECK(frames.active.frame.Is_Null());
        // Every frame three-slices on its flat face column, both
        // states, with 20 px fixed ramps: the selected frame's
        // field is flat in the middle once the ramps are fixed
        // slices, so nothing resamples. The middle column is per
        // tab: Name 26, Population 38, Food 21.
        DOCTEST_CHECK(frames.normal.middle_column == 26);
        DOCTEST_CHECK(frames.active.middle_column == 26);
        DOCTEST_CHECK_FALSE(frames.normal.resample_middle);
        DOCTEST_CHECK_FALSE(frames.active.resample_middle);
        DOCTEST_CHECK(frames.left_slice == 20);
        DOCTEST_CHECK(frames.right_slice == 20);
        DOCTEST_CHECK(theme.Tab_Frames_For_Tab(1).normal.middle_column == 38);
        DOCTEST_CHECK(theme.Tab_Frames_For_Tab(2).normal.middle_column == 21);
        DOCTEST_CHECK(theme.Tab_Frames_For_Tab(9).normal.middle_column == 21);
        // The gap profile is the band's recessed channel, one
        // entry per tab row; row 0 follows the arch, so it differs
        // per gap.
        const std::vector<uint8_t> gap0 = theme.Tab_Gap_Profile(0);
        const std::vector<uint8_t> gap1 = theme.Tab_Gap_Profile(1);
        DOCTEST_REQUIRE(gap0.size() == 24);
        DOCTEST_REQUIRE(gap1.size() == 24);
        DOCTEST_CHECK(gap0[0] == 239);
        DOCTEST_CHECK(gap1[0] == 240);
        DOCTEST_CHECK(gap0[4] == 50);
        DOCTEST_CHECK(gap1[4] == 50);
        DOCTEST_CHECK(gap0[23] == 239);
        DOCTEST_CHECK(gap1[23] == 239);

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 63, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Draw();

        // The colony fallback fills: active menu blue, rest dark
        // navy, both bordered in the panel border colour -- and the
        // labels still print. The gap between the tabs (x 31, 1 px
        // wide -- the fake engine font measures every label as 6 px,
        // so the 63-wide bar lays out 31/31) fills first, one fill
        // per run of the profile: 239, 234, 15, 84, 50 x16, 84, 94,
        // 29, 239 -- nine fills before the two tab fills.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 11);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 31);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 31);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 239);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].y1 == 4);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].y2 == 19);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 50);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].y1 == 23);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].color == 239);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].color == 138);
        DOCTEST_CHECK(test::fakes::g_fill_calls[10].color == 50);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);
        DOCTEST_CHECK(test::fakes::g_box_calls[1].color == 234);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "A");
        DOCTEST_CHECK(test::fakes::g_print_calls[1].text_copy == "BB");
    }

    DOCTEST_TEST_CASE("tab bar draws each tab's own frames and fills the gaps with the band profile") {
        using namespace test_ui_kit_detail;
        // One-pixel-row frames, 10 wide, one distinct value per
        // column, so a drawn pixel identifies its tab's frame:
        // tab 0 rest 1..10 / active 21..30, tab 1 rest 31..40 /
        // active 41..50. Slices 2/2, middle column 5.
        const std::string tab0_normal = Make_Frame_Pixels(10, 1,
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string tab0_active = Make_Frame_Pixels(10, 1,
            {21, 22, 23, 24, 25, 26, 27, 28, 29, 30});
        const std::string tab1_normal = Make_Frame_Pixels(10, 1,
            {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        const std::string tab1_active = Make_Frame_Pixels(10, 1,
            {41, 42, 43, 44, 45, 46, 47, 48, 49, 50});
        Fake_Theme theme;
        const auto per_tab = [](const std::string& normal,
            const std::string& active) {
            ui_kit::Tab_Frames frames;
            frames.normal.frame =
                ui_kit::Frame((const s_animation_header*)normal.data());
            frames.active.frame =
                ui_kit::Frame((const s_animation_header*)active.data());
            frames.normal.middle_column = 5;
            frames.active.middle_column = 5;
            frames.left_slice = 2;
            frames.right_slice = 2;
            return frames;
        };
        theme.per_tab_frames = {per_tab(tab0_normal, tab0_active),
            per_tab(tab1_normal, tab1_active)};
        // The gap profile, one entry per tab row, in three runs:
        // rows 0-1, rows 2-4, rows 5-23.
        std::vector<uint8_t> profile(24, 202);
        profile[0] = 200;
        profile[1] = 200;
        profile[2] = 201;
        profile[3] = 201;
        profile[4] = 201;
        theme.gap_profiles = {profile};

        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 63, theme);
        bar.Add_Tab("A", true);
        bar.Add_Tab("BB", false);
        bar.Draw();

        // Tab 0 (x 0, 26 wide) draws its own active frame: left
        // slice 21, 22, the middle tiled from column 5 (26), right
        // slice 29, 30. Tab 1 (x 27, 36 wide) draws its own rest
        // frame: 31, 32, middle 36, right 39, 40 -- not tab 0's.
        DOCTEST_CHECK(page.At(0, 0) == 21);
        DOCTEST_CHECK(page.At(1, 0) == 22);
        DOCTEST_CHECK(page.At(2, 0) == 26);
        DOCTEST_CHECK(page.At(23, 0) == 26);
        DOCTEST_CHECK(page.At(24, 0) == 29);
        DOCTEST_CHECK(page.At(25, 0) == 30);
        DOCTEST_CHECK(page.At(27, 0) == 31);
        DOCTEST_CHECK(page.At(28, 0) == 32);
        DOCTEST_CHECK(page.At(29, 0) == 36);
        DOCTEST_CHECK(page.At(61, 0) == 39);
        DOCTEST_CHECK(page.At(62, 0) == 40);
        // The 1 px gap at x 26 fills in three runs, one per run of
        // the profile, before the tabs draw.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 26);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 26);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 1);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 200);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 4);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 201);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y1 == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y2 == 23);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 202);
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

    DOCTEST_TEST_CASE("bitmap font parses JSON and measures text") {
        using namespace test_ui_kit_detail;
        const s_colors normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors active = {{138, 250, 250, 250, 250, 250, 250, 250}};
        ui_kit::Engine_Font fallback(2, normal, active);
        ui_kit::Bitmap_Font font(ui_kit::Bitmap_Font::State::Normal,
            fallback);

        DOCTEST_CHECK_FALSE(font.Is_Loaded());
        DOCTEST_CHECK(font.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        DOCTEST_CHECK(font.Is_Loaded());
        // The last glyph counts its bitmap width, not its advance.
        DOCTEST_CHECK(font.Width("A") == 3);
        DOCTEST_CHECK(font.Width("AB") == 4 + 2);
        DOCTEST_CHECK(font.Width("A B") == 4 + 2 + 2);
        DOCTEST_CHECK(font.Width("") == 0);
        DOCTEST_CHECK(font.Height() == 4);
        // An unknown glyph falls back to the engine font whole.
        test::fakes::Reset_All_();
        test::fakes::g_string_width = 42;
        DOCTEST_CHECK(font.Width("AC") == 42);
        // The pressed font measures with the pressed bitmap widths.
        ui_kit::Bitmap_Font pressed(ui_kit::Bitmap_Font::State::Pressed,
            fallback);
        DOCTEST_CHECK(pressed.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        DOCTEST_CHECK(pressed.Width("A") == 4);
        DOCTEST_CHECK(pressed.Width("AB") == 4 + 3);
        DOCTEST_CHECK(pressed.Height() == 4);
    }

    DOCTEST_TEST_CASE("bitmap font rejects malformed JSON and falls back") {
        using namespace test_ui_kit_detail;
        const s_colors normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors active = {{138, 250, 250, 250, 250, 250, 250, 250}};
        ui_kit::Engine_Font fallback(2, normal, active);
        ui_kit::Bitmap_Font font(ui_kit::Bitmap_Font::State::Normal,
            fallback);
        const std::string not_json = "{not json";
        DOCTEST_CHECK_FALSE(font.Load_From_Text(not_json.data(),
            not_json.size()));
        DOCTEST_CHECK_FALSE(font.Is_Loaded());
        // Normal has 1 row, not the cap height 3.
        const std::string short_rows = R"JSON({"cap_height": 3,
            "descender_rows": 1, "space_advance": 2, "glyphs": {"A":
            {"advance": 4, "width": 3, "bitmap": {"normal": [[1]],
            "pressed": [[2], [2], [2], [2]]}}}})JSON";
        DOCTEST_CHECK_FALSE(font.Load_From_Text(short_rows.data(),
            short_rows.size()));
        DOCTEST_CHECK_FALSE(font.Is_Loaded());
        // A cell value above the palette range.
        const std::string bad_value = R"JSON({"cap_height": 3,
            "descender_rows": 1, "space_advance": 2, "glyphs": {"A":
            {"advance": 4, "width": 3, "bitmap": {"normal":
            [[300, null, 1], [1, 1, 1], [null, 1, null]],
            "pressed": [[2], [2], [2], [2]]}}}})JSON";
        DOCTEST_CHECK_FALSE(font.Load_From_Text(bad_value.data(),
            bad_value.size()));
        DOCTEST_CHECK_FALSE(font.Is_Loaded());
        // A good font followed by a malformed one unloads again.
        DOCTEST_CHECK(font.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        DOCTEST_CHECK(font.Is_Loaded());
        DOCTEST_CHECK_FALSE(font.Load_From_Text(not_json.data(),
            not_json.size()));
        DOCTEST_CHECK_FALSE(font.Is_Loaded());
        test::fakes::Reset_All_();
        test::fakes::g_string_width = 42;
        test::fakes::g_font_height = 12;
        DOCTEST_CHECK(font.Width("A") == 42);
        DOCTEST_CHECK(font.Height() == 12);
    }

    DOCTEST_TEST_CASE("bitmap font blits glyph pixels onto the video page") {
        using namespace test_ui_kit_detail;
        const s_colors normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors active = {{138, 250, 250, 250, 250, 250, 250, 250}};
        ui_kit::Engine_Font fallback(2, normal, active);
        Scoped_Video_Page page;
        ui_kit::Bitmap_Font font(ui_kit::Bitmap_Font::State::Normal,
            fallback);
        DOCTEST_REQUIRE(font.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        font.Print(10, 20, "A", false);
        // A normal: row0 [1, ., 1], row1 [1, 1, 1], row2 [., 1, .].
        DOCTEST_CHECK(page.At(10, 20) == 1);
        DOCTEST_CHECK(page.At(11, 20) == 0xFF);
        DOCTEST_CHECK(page.At(12, 20) == 1);
        DOCTEST_CHECK(page.At(10, 21) == 1);
        DOCTEST_CHECK(page.At(11, 21) == 1);
        DOCTEST_CHECK(page.At(12, 21) == 1);
        DOCTEST_CHECK(page.At(10, 22) == 0xFF);
        DOCTEST_CHECK(page.At(11, 22) == 1);
        DOCTEST_CHECK(page.At(12, 22) == 0xFF);
        DOCTEST_CHECK(page.At(10, 23) == 0xFF);
        // The pressed font blits the pressed bitmaps (wider, value 2).
        ui_kit::Bitmap_Font pressed(ui_kit::Bitmap_Font::State::Pressed,
            fallback);
        DOCTEST_REQUIRE(pressed.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        pressed.Print(20, 20, "A", false);
        DOCTEST_CHECK(page.At(20, 20) == 2);
        DOCTEST_CHECK(page.At(23, 20) == 2);
        DOCTEST_CHECK(page.At(20, 23) == 2);
        DOCTEST_CHECK(page.At(23, 23) == 2);
        DOCTEST_CHECK(page.At(21, 21) == 0xFF);
        // Two glyphs advance by the first glyph's advance.
        font.Print(30, 20, "AB", false);
        DOCTEST_CHECK(page.At(30, 20) == 1);
        DOCTEST_CHECK(page.At(34, 20) == 3);
        DOCTEST_CHECK(page.At(35, 20) == 3);
    }

    DOCTEST_TEST_CASE("bitmap font falls back for the active state and unknown glyphs") {
        using namespace test_ui_kit_detail;
        const s_colors normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors active = {{138, 250, 250, 250, 250, 250, 250, 250}};
        ui_kit::Engine_Font fallback(2, normal, active);
        ui_kit::Bitmap_Font font(ui_kit::Bitmap_Font::State::Normal,
            fallback);
        DOCTEST_REQUIRE(font.Load_From_Text(kBitmap_Font_Json,
            std::strlen(kBitmap_Font_Json)));
        Scoped_Video_Page page;
        test::fakes::Reset_All_();
        // Active (blue face) has no baked reference: the whole
        // string goes to the engine font with the active ramp.
        font.Print(10, 20, "A", true);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 138);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "A");
        DOCTEST_CHECK(page.At(10, 20) == 0xFF);
        // An unknown glyph sends the whole string to the fallback.
        font.Print(10, 20, "AC", false);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 2);
        DOCTEST_CHECK(test::fakes::g_print_calls[1].text_copy == "AC");
        DOCTEST_CHECK(page.At(10, 20) == 0xFF);
    }

    DOCTEST_TEST_CASE("colony theme hands out bitmap button fonts that fall back without the font file") {
        using namespace test_ui_kit_detail;
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);
        const ui_kit::Button_Style style = theme.Button_Style();
        DOCTEST_REQUIRE(style.label_font != nullptr);
        DOCTEST_REQUIRE(style.label_pressed_font != nullptr);
        // No base.zip is mounted in the tests, so Load fails and
        // both fonts fall back to the style-2 engine font.
        test::fakes::Reset_All_();
        test::fakes::g_string_width = 30;
        test::fakes::g_font_height = 11;
        DOCTEST_CHECK(style.label_font->Width("MORE") == 30);
        DOCTEST_CHECK(style.label_font->Height() == 11);
        style.label_font->Print(10, 20, "MORE", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "MORE");
        test::fakes::Reset_All_();
        style.label_pressed_font->Print(10, 20, "MORE", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "MORE");
    }

    DOCTEST_TEST_CASE("colony theme hands out the colony tab font and frames") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame(50, 24, 50);
        const std::string active = Make_Frame(50, 24, 138);
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(
            ui_kit::Frame((const s_animation_header*)normal.data()),
            ui_kit::Frame((const s_animation_header*)active.data()),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), title);

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
        DOCTEST_CHECK_FALSE(frames.normal.frame.Is_Null());
        DOCTEST_CHECK_FALSE(frames.active.frame.Is_Null());
        DOCTEST_CHECK_FALSE(frames.normal.resample_middle);
        DOCTEST_CHECK_FALSE(frames.active.resample_middle);
        DOCTEST_CHECK(frames.left_slice == 20);
        DOCTEST_CHECK(frames.right_slice == 20);
        DOCTEST_CHECK(frames.normal.middle_column == 26);

        DOCTEST_CHECK(theme.Tab_Padding() == 8);
        DOCTEST_CHECK(theme.Tab_Gap() == 1);
        DOCTEST_CHECK(theme.Tab_Height() == 24);
        DOCTEST_CHECK(theme.Tab_Label_Y_Offset() == 8);

        // The frame style is the colony frame: the bevelled metal
        // band, 11/12/12/10 px per side, content inset the same,
        // style-4 title at y 8, 24 px bottom bar.
        const ui_kit::Frame_Style frame = theme.Frame_Style();
        DOCTEST_CHECK(frame.thickness_left == 11);
        DOCTEST_CHECK(frame.thickness_right == 12);
        DOCTEST_CHECK(frame.thickness_top == 12);
        DOCTEST_CHECK(frame.thickness_bottom == 10);
        DOCTEST_CHECK(frame.inset_left == 11);
        DOCTEST_CHECK(frame.inset_right == 12);
        DOCTEST_CHECK(frame.inset_top == 12);
        DOCTEST_CHECK(frame.inset_bottom == 10);
        DOCTEST_REQUIRE(frame.profile.size() == 11);
        DOCTEST_CHECK(frame.profile[0] == 234);
        DOCTEST_CHECK(frame.profile[3] == 239);
        DOCTEST_CHECK(frame.profile[9] == 236);
        DOCTEST_CHECK(frame.title_y == 8);
        DOCTEST_CHECK(frame.bottom_bar_h == 24);
        DOCTEST_REQUIRE(frame.title_font != nullptr);
        test::fakes::Reset_All_();
        test::fakes::g_string_width = 70;
        ui_kit::Window_Frame window(0, 0, 640, 480, theme);
        window.Draw("Settings");
        // 12 + 10 + 11 + 12 band lines; the top band's 12th line
        // repeats the profile's last entry (its thickness exceeds
        // the profile).
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 45);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 639);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 234);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].y1 == 9);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].x1 == 9);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].x2 == 630);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].color == 236);
        DOCTEST_CHECK(test::fakes::g_fill_calls[10].y1 == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[10].color == 234);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].y1 == 11);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].x1 == 11);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].x2 == 628);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].color == 234);
        DOCTEST_CHECK(test::fakes::g_fill_calls[12].y1 == 479);
        DOCTEST_CHECK(test::fakes::g_fill_calls[12].color == 234);
        DOCTEST_CHECK(test::fakes::g_fill_calls[21].y1 == 470);
        DOCTEST_CHECK(test::fakes::g_fill_calls[21].color == 234);
        DOCTEST_CHECK(test::fakes::g_fill_calls[22].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[22].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[22].y2 == 479);
        DOCTEST_CHECK(test::fakes::g_fill_calls[33].x1 == 639);
        DOCTEST_CHECK(test::fakes::g_fill_calls[33].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[33].y2 == 479);
        DOCTEST_CHECK(test::fakes::g_font_style == 4);
        DOCTEST_REQUIRE(test::fakes::g_print_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].x == (640 - 70) / 2);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].y == 8);
        DOCTEST_CHECK(test::fakes::g_print_calls[0].text_copy == "Settings");
    }
    DOCTEST_TEST_CASE("colony spacing lays list rows out on a 31 px pitch with a 1 px gap") {
        using namespace test_ui_kit_detail;
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);

        // Measured from the colony summary screen (colsum.cpp):
        // 30 px rows, y_row_end advancing 31 px per row.
        const ui_kit::Spacing spacing = theme.Spacing();
        DOCTEST_CHECK(spacing.row_height == 30);
        DOCTEST_CHECK(spacing.row_gap == 1);
        DOCTEST_CHECK(spacing.Row_Pitch() == 31);
        DOCTEST_CHECK(spacing.header_to_first_row == 3);
        DOCTEST_CHECK(spacing.list_footing == 4);
        DOCTEST_CHECK(spacing.list_to_info == 4);
        DOCTEST_CHECK(spacing.info_to_bar == 7);

        const ui_kit::List_Style style = theme.List_Style();
        DOCTEST_CHECK(style.interior == 230);
        DOCTEST_CHECK(style.border == 234);
        DOCTEST_CHECK(style.frame_light == 241);
        DOCTEST_CHECK(style.frame_shade == 235);
        DOCTEST_CHECK(style.divider_left == 233);
        DOCTEST_CHECK(style.divider_mid == 235);
        DOCTEST_CHECK(style.divider_right == 232);
        DOCTEST_CHECK(style.plate_face == 233);

        // The settings screen's panel, positioned by its frame:
        // the content starts at x 11, so the panel is 11..627;
        // header 32..53, so the first row starts 3 px below it
        // at 57.
        ui_kit::List_Layout layout(11, 30, 617, 619, 32, 22, 10, spacing);
        DOCTEST_CHECK(layout.first_row_y() == 57);
        DOCTEST_CHECK(layout.row_y(0) == 57);
        DOCTEST_CHECK(layout.row_y(1) == 88);
        DOCTEST_CHECK(layout.row_y(9) == 336);
        DOCTEST_CHECK(layout.row_pitch() == 31);
        DOCTEST_CHECK(layout.row_height() == 30);
        // The panel ends list_footing below the last row's bottom
        // edge, 369.
        DOCTEST_CHECK(layout.panel_h() == 340);
        DOCTEST_CHECK(layout.panel_y() + layout.panel_h() - 1 == 369);
        // The scroll track spans the rows, first top to last bottom.
        DOCTEST_CHECK(layout.scroll_track_y() == 57);
        DOCTEST_CHECK(layout.scroll_track_h() == 309);
        // The description box starts 4 px below the panel.
        DOCTEST_CHECK(layout.info_y() == 374);
    }

    DOCTEST_TEST_CASE("list background frames every row and leaves the gap empty") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::List_Layout layout(11, 30, 617, 619, 32, 22, 10,
            theme.Spacing());

        test::fakes::Reset_All_();
        ui_kit::Draw_List_Background(theme, layout);

        // One panel fill, one panel border box.
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].x == 11);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].y == 30);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].width == 617);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].height == 340);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);
        // Panel fill plus five frame lines per row, the
        // three-line footing and the right-edge bevel line:
        // 1 + 10 * 5 + 3 + 1 = 55 fills.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 55);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 230);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 11);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 30);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 627);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 369);
        // Row 0 is a framed cell: light lines at 57 and 85, shade
        // lines just inside at 58 and 84, and the trailing shade
        // at 86; row 1 starts at 88, so the 1 px gap at 87 is empty
        // panel background -- no fill covers it -- and no line
        // crosses the scrollbar strip: the frame lines stop at the
        // list edge, x 619.
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 57);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 57);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x1 == 12);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x2 == 619);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y1 == 58);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].y1 == 84);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].y1 == 85);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].y1 == 86);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].y1 == 88);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].x2 == 619);
        // Last row's top light line at 336, shade at 337, shade at
        // 363, bottom light line at 364, trailing shade at 365 --
        // and the footing below it: shade at 366, shade at 367,
        // light at 368, the panel border at 369 its bottom edge.
        DOCTEST_CHECK(test::fakes::g_fill_calls[46].y1 == 336);
        DOCTEST_CHECK(test::fakes::g_fill_calls[46].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[47].y1 == 337);
        DOCTEST_CHECK(test::fakes::g_fill_calls[47].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[48].y1 == 363);
        DOCTEST_CHECK(test::fakes::g_fill_calls[48].color == 235);
        // The right-edge bevel: a frame-light vertical line one
        // pixel inside the panel's right edge, full panel height.
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x1 == 626);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x2 == 626);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y1 == 30);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y2 == 369);
        DOCTEST_CHECK(test::fakes::g_fill_calls[49].y1 == 364);
        DOCTEST_CHECK(test::fakes::g_fill_calls[49].y2 == 364);
        DOCTEST_CHECK(test::fakes::g_fill_calls[49].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].y1 == 365);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].y1 == 366);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].y1 == 367);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].y1 == 368);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].color == 241);
        for (const test::fakes::Fill_Call& fill :
            test::fakes::g_fill_calls) {
            DOCTEST_CHECK(fill.y1 != 87);
            DOCTEST_CHECK(fill.y1 != 369);
        }
    }

    DOCTEST_TEST_CASE("column layout puts the 3 px divider right after the left column") {
        using namespace test_ui_kit_detail;
        // The colony list's columns, measured from the colony
        // summary screen: NAME 11..97, FARMERS 101..232, WORKERS
        // 236..374, SCIENTISTS 378..508, BUILDING 512..598 -- the
        // dividers land exactly on the baked grooves (98..100,
        // 233..235, 375..377, 509..511).
        ui_kit::Column_Layout colony(
            {{11, 87}, {101, 132}, {236, 139}, {378, 131}, {512, 87}},
            35, 343);
        DOCTEST_CHECK(colony.divider_count() == 4);
        DOCTEST_CHECK(colony.divider_x(0) == 98);
        DOCTEST_CHECK(colony.divider_x(1) == 233);
        DOCTEST_CHECK(colony.divider_x(2) == 375);
        DOCTEST_CHECK(colony.divider_x(3) == 509);
        DOCTEST_CHECK(colony.top() == 35);
        DOCTEST_CHECK(colony.bottom() == 343);

        // The settings list's columns: the SETTING / VALUE header
        // plates, 10..363 and 368..615 -- one divider, at 364,
        // leaving the last gap pixel (367) panel interior.
        ui_kit::Column_Layout settings(
            {{10, 354}, {368, 248}}, 57, 365);
        DOCTEST_CHECK(settings.divider_count() == 1);
        DOCTEST_CHECK(settings.divider_x(0) == 364);
        DOCTEST_CHECK(settings.top() == 57);
        DOCTEST_CHECK(settings.bottom() == 365);

        // A single column has no divider.
        ui_kit::Column_Layout single({{10, 354}}, 57, 365);
        DOCTEST_CHECK(single.divider_count() == 0);
        // And no columns at all is not a divider either.
        ui_kit::Column_Layout none({}, 57, 365);
        DOCTEST_CHECK(none.divider_count() == 0);
    }

    DOCTEST_TEST_CASE("list columns draw the divider triple over the rows") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::Column_Layout columns({{10, 354}, {368, 248}}, 57, 365);

        test::fakes::Reset_All_();
        ui_kit::Draw_List_Columns(theme, columns);

        // One 3 px divider: left/mid/right in the theme's colours,
        // running the rows' height, first row top to last row bottom.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 364);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 364);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 57);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 365);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 233);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x1 == 365);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x2 == 365);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 57);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 365);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 366);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x2 == 366);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 232);

        // The colony's four dividers, in a second theme's colours:
        // the divider geometry follows the columns, the colours
        // follow the theme.
        Alt_Theme alt;
        ui_kit::Column_Layout colony(
            {{11, 87}, {101, 132}, {236, 139}, {378, 131}, {512, 87}},
            35, 343);
        test::fakes::Reset_All_();
        ui_kit::Draw_List_Columns(alt, colony);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 12);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 98);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 23);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x1 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 24);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 100);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 25);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].x1 == 511);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].x2 == 511);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].y1 == 35);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].y2 == 343);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].color == 25);
    }

    DOCTEST_TEST_CASE("list layout follows a second theme's spacing without component changes") {
        using namespace test_ui_kit_detail;
        Alt_Theme theme;
        const ui_kit::Spacing spacing = theme.Spacing();
        DOCTEST_CHECK(spacing.row_height == 20);
        DOCTEST_CHECK(spacing.row_gap == 2);
        DOCTEST_CHECK(spacing.Row_Pitch() == 22);

        // Alt metrics: header 32..53, first row 5 px below it.
        ui_kit::List_Layout layout(8, 30, 624, 619, 32, 22, 10, spacing);
        DOCTEST_CHECK(layout.first_row_y() == 59);
        DOCTEST_CHECK(layout.row_y(1) == 81);
        DOCTEST_CHECK(layout.row_y(9) == 257);
        DOCTEST_CHECK(layout.panel_h() == 253);
        DOCTEST_CHECK(layout.scroll_track_h() == 218);
        DOCTEST_CHECK(layout.info_y() == 289);

        // The background follows the alt colours, not the colony's:
        // five frame lines per row including the last row's
        // trailing shade, and the three-line footing against the
        // border -- the taller footing leaves plain interior above
        // it, so no fill covers 277 or 278.
        test::fakes::Reset_All_();
        ui_kit::Draw_List_Background(theme, layout);
        // 1 panel fill + 10 * 5 row lines + 3 footing + 1 bevel = 55.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 55);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 10);
        // The right-edge bevel: the alt theme's frame light (21),
        // one pixel inside the panel's right edge (x 630), full height.
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].color == 21);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x1 == 630);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x2 == 630);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y1 == 30);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y2 == 282);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 21);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 22);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].y1 == 276);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].color == 22);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].y1 == 279);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].color == 22);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].y1 == 280);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].color == 22);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].y1 == 281);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].color == 21);
        for (const test::fakes::Fill_Call& fill :
            test::fakes::g_fill_calls) {
            DOCTEST_CHECK(fill.y1 != 277);
            DOCTEST_CHECK(fill.y1 != 278);
            DOCTEST_CHECK(fill.y1 != 282);
        }
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 11);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].height == 253);
    }

    DOCTEST_TEST_CASE("window frame derives the content rect from the theme") {
        using namespace test_ui_kit_detail;
        // The fake frame: 3 px on every side, so the content is the
        // window shrunk by 3 on each side.
        Fake_Theme fake;
        ui_kit::Window_Frame fake_frame(0, 0, 100, 50, fake);
        DOCTEST_CHECK(fake_frame.content_x() == 3);
        DOCTEST_CHECK(fake_frame.content_y() == 3);
        DOCTEST_CHECK(fake_frame.content_w() == 94);
        DOCTEST_CHECK(fake_frame.content_h() == 44);
        DOCTEST_CHECK(fake_frame.content_right() == 97);
        DOCTEST_CHECK(fake_frame.content_bottom() == 47);
        DOCTEST_CHECK(fake_frame.bottom_bar_y(24) == 23);
        DOCTEST_CHECK(fake_frame.title_y() == 8);
        // The alt frame: uneven insets, so each side derives
        // separately; the bottom bar is 30 px tall.
        Alt_Theme alt;
        ui_kit::Window_Frame alt_frame(10, 20, 200, 100, alt);
        DOCTEST_CHECK(alt_frame.content_x() == 15);
        DOCTEST_CHECK(alt_frame.content_y() == 27);
        DOCTEST_CHECK(alt_frame.content_w() == 189);
        DOCTEST_CHECK(alt_frame.content_h() == 85);
        DOCTEST_CHECK(alt_frame.content_right() == 204);
        DOCTEST_CHECK(alt_frame.content_bottom() == 112);
        DOCTEST_CHECK(alt_frame.bottom_bar_y(30) == 82);
        DOCTEST_CHECK(alt_frame.title_y() == 24);
        // The colony frame at dialog size: the home-star name dialog
        // is 240x148 at (155, 146), so its content is 217x126 at
        // (166, 158) and its 24 px bottom bar starts at y 260.
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme colony(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);
        ui_kit::Window_Frame dialog(155, 146, 240, 148, colony);
        DOCTEST_CHECK(dialog.content_x() == 166);
        DOCTEST_CHECK(dialog.content_y() == 158);
        DOCTEST_CHECK(dialog.content_w() == 217);
        DOCTEST_CHECK(dialog.content_h() == 126);
        DOCTEST_CHECK(dialog.content_right() == 383);
        DOCTEST_CHECK(dialog.content_bottom() == 284);
        DOCTEST_CHECK(dialog.bottom_bar_y(24) == 260);
        DOCTEST_CHECK(dialog.title_y() == 154);
    }

    DOCTEST_TEST_CASE("window frame draws mitred bands and centres the title") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        ui_kit::Window_Frame frame(0, 0, 100, 50, theme);

        test::fakes::Reset_All_();
        frame.Draw("Alt");

        // 3 + 3 + 3 + 3 band lines, no boxes: the frame is fills.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 12);
        DOCTEST_CHECK(test::fakes::g_box_calls.empty());
        // Top band: each line stops one pixel further in, so the
        // corners meet on a 45-degree seam.
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x2 == 97);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 12);
        // Bottom band, same seam at the bottom corners.
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].y1 == 49);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].x2 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].x1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].y1 == 47);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].x2 == 97);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].color == 12);
        // Side bands run between the mitres, not past them.
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].x1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].y2 == 49);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].color == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].x1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].y1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].y2 == 47);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].color == 12);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].x1 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].y1 == 0);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].y2 == 49);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].color == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].x1 == 97);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].y1 == 2);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].y2 == 47);
        DOCTEST_CHECK(test::fakes::g_fill_calls[11].color == 12);
        // The title is centred in the window, at the theme's title y.
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK(theme.font.prints[0].x == 35);
        DOCTEST_CHECK(theme.font.prints[0].y == 8);
        DOCTEST_CHECK(theme.font.prints[0].text == "Alt");
    }

    DOCTEST_TEST_CASE("metal button draws the colony bevel and centres a dark label") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        test::fakes::Reset_All_();
        ui_kit::Button button(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        button.Set_Label("Show");
        button.Draw();

        // Face, then the raised bevel: light top/left, dark
        // bottom/right -- the colony scrollbar thumb's shape
        // (colsum.cpp Draw_Bar_Indicator).
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 238);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 20);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 109);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 43);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 243);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 20);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 20);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 243);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x2 == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].y1 == 43);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].x1 == 109);
        DOCTEST_CHECK(test::fakes::g_box_calls.empty());
        // The label is centred on the fake font (10 px per char,
        // 12 px tall) by the field system's rule -- half the button
        // height less half the font height rounded down -- and dark
        // on the metal face.
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK(theme.font.prints[0].text == "Show");
        DOCTEST_CHECK(theme.font.prints[0].x == 10 + (100 - 40) / 2);
        DOCTEST_CHECK(theme.font.prints[0].y == 20 + 24 / 2 - (12 - 1) / 2);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);
    }

    DOCTEST_TEST_CASE("metal button inverts its bevel while pressed and turns blue when active") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;

        // Pressed: dark top/left, light bottom/right.
        test::fakes::Reset_All_();
        ui_kit::Button held(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        held.Set_Label("Show");
        held.Set_State(ui_kit::Button_State::Pressed);
        held.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 238);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 243);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 243);
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);

        // Active (latched): the blue face with its own light edge
        // and a light label.
        test::fakes::Reset_All_();
        ui_kit::Button latched(10, 20, 100, 24,
            ui_kit::Button_Kind::Metal, theme);
        latched.Set_Label("Show");
        latched.Set_Primary(true);
        latched.Set_State(ui_kit::Button_State::Active);
        latched.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 138);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 154);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 154);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 232);
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
        DOCTEST_CHECK(theme.font.prints[1].active);

        // Disabled: the flat mid-grey face with a border, no
        // bevel, dark label -- the colony never disables a
        // button, so there is no disabled bevel to copy.
        test::fakes::Reset_All_();
        ui_kit::Button off(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        off.Set_Label("Show");
        off.Set_State(ui_kit::Button_State::Disabled);
        off.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 234);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 232);
        DOCTEST_REQUIRE(theme.font.prints.size() == 3);
        DOCTEST_CHECK_FALSE(theme.font.prints[2].active);
    }

    DOCTEST_TEST_CASE("metal button prints its label in the pressed font while pressed") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        Fake_Font pressed_font;
        theme.buttons.label_pressed_font = &pressed_font;

        // Pressed: the inverted bevel, and the label through the
        // pressed font (the orange the real RETURN lights up with
        // when held) instead of the dark label font.
        test::fakes::Reset_All_();
        ui_kit::Button held(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        held.Set_Label("MORE");
        held.Set_State(ui_kit::Button_State::Pressed);
        held.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 243);
        DOCTEST_REQUIRE(pressed_font.prints.size() == 1);
        DOCTEST_CHECK(pressed_font.prints[0].text == "MORE");
        DOCTEST_CHECK(theme.font.prints.empty());

        // At rest the same button prints through the plain label
        // font, not the pressed one.
        test::fakes::Reset_All_();
        ui_kit::Button rest(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        rest.Set_Label("MORE");
        rest.Draw();
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK(theme.font.prints[0].text == "MORE");
        DOCTEST_CHECK(pressed_font.prints.size() == 1);
    }

    DOCTEST_TEST_CASE("metal button draws the RETURN frame as three slices when frames are loaded") {
        using namespace test_ui_kit_detail;
        // A fake RETURN: 85x26, one value per column (x+1 in the
        // rest frame, 100+x in the pressed frame), so the caps and
        // the middle column are all distinguishable.
        std::vector<uint8_t> normal_pixels;
        std::vector<uint8_t> pressed_pixels;
        for (int16_t y = 0; y < 26; ++y) {
            for (int16_t x = 0; x < 85; ++x) {
                normal_pixels.push_back((uint8_t)(x + 1));
                pressed_pixels.push_back((uint8_t)(100 + x));
            }
        }
        const std::string normal =
            Make_Frame_Pixels(85, 26, normal_pixels);
        const std::string pressed =
            Make_Frame_Pixels(85, 26, pressed_pixels);
        Fake_Theme theme;
        theme.return_frames.normal =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.return_frames.pressed =
            ui_kit::Frame((const s_animation_header*)pressed.data());
        Fake_Font pressed_font;
        theme.buttons.label_pressed_font = &pressed_font;

        Scoped_Video_Page page;
        // A 69 px button: left dome (17), tiled middle (27),
        // right dome (25). The caps are the frame's own pixels;
        // the middle is column 58 of the rest frame (value 59)
        // tiled across.
        ui_kit::Button rest(5, 7, 69, 26, ui_kit::Button_Kind::Metal,
            theme);
        rest.Set_Label("MORE");
        rest.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 1);
        DOCTEST_CHECK(page.At(5 + 16, 7) == 17);
        DOCTEST_CHECK(page.At(5 + 17, 7) == 59);
        DOCTEST_CHECK(page.At(5 + 30, 7) == 59);
        DOCTEST_CHECK(page.At(5 + 43, 7) == 59);
        DOCTEST_CHECK(page.At(5 + 44, 7) == 61);
        DOCTEST_CHECK(page.At(5 + 68, 7) == 85);
        // The label is centred on the frame's height (26), through
        // the plain label font at rest.
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK(theme.font.prints[0].text == "MORE");
        DOCTEST_CHECK(theme.font.prints[0].x == 5 + (69 - 40) / 2);
        DOCTEST_CHECK(theme.font.prints[0].y == 7 + 26 / 2 - (12 - 1) / 2);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);

        // Pressed: the pressed frame, middle column 59 (value
        // 159), label through the pressed font.
        ui_kit::Button held(5, 7, 69, 26, ui_kit::Button_Kind::Metal,
            theme);
        held.Set_Label("MORE");
        held.Set_State(ui_kit::Button_State::Pressed);
        held.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 100);
        DOCTEST_CHECK(page.At(5 + 16, 7) == 116);
        DOCTEST_CHECK(page.At(5 + 17, 7) == 159);
        DOCTEST_CHECK(page.At(5 + 30, 7) == 159);
        DOCTEST_CHECK(page.At(5 + 43, 7) == 159);
        DOCTEST_CHECK(page.At(5 + 44, 7) == 160);
        DOCTEST_CHECK(page.At(5 + 68, 7) == 184);
        DOCTEST_REQUIRE(pressed_font.prints.size() == 1);
        DOCTEST_CHECK(pressed_font.prints[0].text == "MORE");
        DOCTEST_CHECK(theme.font.prints.size() == 1);

        // A missing pressed frame falls back to the rest frame.
        theme.return_frames.pressed = ui_kit::Frame();
        ui_kit::Button held2(5, 7, 69, 26, ui_kit::Button_Kind::Metal,
            theme);
        held2.Set_Label("MORE");
        held2.Set_State(ui_kit::Button_State::Pressed);
        held2.Draw();
        DOCTEST_CHECK(page.At(5 + 17, 7) == 59);
        DOCTEST_CHECK(page.At(5 + 68, 7) == 85);
    }

    DOCTEST_TEST_CASE("metal button crops the slices when narrower than the caps") {
        using namespace test_ui_kit_detail;
        std::vector<uint8_t> normal_pixels;
        for (int16_t y = 0; y < 26; ++y) {
            for (int16_t x = 0; x < 85; ++x) {
                normal_pixels.push_back((uint8_t)(x + 1));
            }
        }
        const std::string normal =
            Make_Frame_Pixels(85, 26, normal_pixels);
        Fake_Theme theme;
        theme.return_frames.normal =
            ui_kit::Frame((const s_animation_header*)normal.data());

        // 30 px wide: narrower than the two caps (17 + 25), so
        // the left dome draws whole and the right dome is cropped
        // to 13 columns, with no middle at all.
        Scoped_Video_Page page;
        ui_kit::Button narrow(5, 7, 30, 26, ui_kit::Button_Kind::Metal,
            theme);
        narrow.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 1);
        DOCTEST_CHECK(page.At(5 + 16, 7) == 17);
        DOCTEST_CHECK(page.At(5 + 17, 7) == 61);
        DOCTEST_CHECK(page.At(5 + 29, 7) == 73);
    }

    DOCTEST_TEST_CASE("metal button follows a second theme's colours without component changes") {
        using namespace test_ui_kit_detail;
        Alt_Theme theme;
        test::fakes::Reset_All_();
        ui_kit::Button button(10, 20, 100, 24, ui_kit::Button_Kind::Metal,
            theme);
        button.Set_Label("Show");
        button.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 40);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 41);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 42);
        ui_kit::Button primary(10, 20, 100, 24,
            ui_kit::Button_Kind::Metal, theme);
        primary.Set_Label("Show");
        primary.Set_Primary(true);
        primary.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].color == 43);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].color == 44);
    }

    DOCTEST_TEST_CASE("selector button draws the active frame when active or pressed") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Fake_Theme theme;
        theme.frames.normal.frame =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.frames.active.frame =
            ui_kit::Frame((const s_animation_header*)active.data());
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        // Rest draws the normal frame; active and pressed both
        // draw frame 1, the colony's selected/held frame.
        Scoped_Video_Page page;
        ui_kit::Button rest(0, 0, 26, 24, ui_kit::Button_Kind::Selector,
            theme);
        rest.Set_Label("A");
        rest.Draw();
        DOCTEST_CHECK(page.At(0, 0) == 1);
        ui_kit::Button selected(0, 0, 26, 24,
            ui_kit::Button_Kind::Selector, theme);
        selected.Set_Label("A");
        selected.Set_State(ui_kit::Button_State::Active);
        selected.Draw();
        DOCTEST_CHECK(page.At(0, 0) == 11);
        ui_kit::Button held(0, 0, 26, 24, ui_kit::Button_Kind::Selector,
            theme);
        held.Set_Label("A");
        held.Set_State(ui_kit::Button_State::Pressed);
        held.Draw();
        DOCTEST_CHECK(page.At(0, 0) == 11);
        // Disabled has no frame of its own and renders as rest.
        ui_kit::Button disabled(0, 0, 26, 24,
            ui_kit::Button_Kind::Selector, theme);
        disabled.Set_Label("A");
        disabled.Set_State(ui_kit::Button_State::Disabled);
        disabled.Draw();
        DOCTEST_CHECK(page.At(0, 0) == 1);
        // The labels print at the theme's offset, active exactly
        // when the frame is the active one.
        DOCTEST_REQUIRE(theme.font.prints.size() == 4);
        DOCTEST_CHECK(theme.font.prints[0].y == 8);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);
        DOCTEST_CHECK(theme.font.prints[1].active);
        DOCTEST_CHECK(theme.font.prints[2].active);
        DOCTEST_CHECK_FALSE(theme.font.prints[3].active);
    }

    DOCTEST_TEST_CASE("tab bar draws a held tab with the active frame") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const std::string active = Make_Frame_Pixels(10, 1, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        Fake_Theme theme;
        theme.frames.normal.frame =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.frames.active.frame =
            ui_kit::Frame((const s_animation_header*)active.data());
        theme.frames.left_slice = 2;
        theme.frames.right_slice = 2;

        Scoped_Video_Page page;
        ui_kit::Tab_Bar bar(0, 0, 63, theme);
        bar.Add_Tab("A", false, true); // held, not active
        bar.Add_Tab("BB", false);
        bar.Draw();
        DOCTEST_CHECK(page.At(0, 0) == 11);
        DOCTEST_CHECK(page.At(27, 0) == 1);
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
        DOCTEST_CHECK(theme.font.prints[0].active);
        DOCTEST_CHECK_FALSE(theme.font.prints[1].active);
    }

    DOCTEST_TEST_CASE("image button draws the pressed frame while pressed and falls back to metal") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame(10, 1, 60);
        const std::string pressed = Make_Frame(10, 1, 70);
        Fake_Theme theme;
        theme.return_frames.normal =
            ui_kit::Frame((const s_animation_header*)normal.data());
        theme.return_frames.pressed =
            ui_kit::Frame((const s_animation_header*)pressed.data());

        Scoped_Video_Page page;
        ui_kit::Button rest(5, 7, 10, 1, ui_kit::Button_Kind::Image,
            theme);
        rest.Set_Label("RETURN");
        rest.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 60);
        ui_kit::Button held(5, 7, 10, 1, ui_kit::Button_Kind::Image,
            theme);
        held.Set_Label("RETURN");
        held.Set_State(ui_kit::Button_State::Pressed);
        held.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 70);
        // A missing pressed frame falls back to the rest frame.
        theme.return_frames.pressed = ui_kit::Frame();
        ui_kit::Button held2(5, 7, 10, 1, ui_kit::Button_Kind::Image,
            theme);
        held2.Set_Label("RETURN");
        held2.Set_State(ui_kit::Button_State::Pressed);
        held2.Draw();
        DOCTEST_CHECK(page.At(5, 7) == 60);

        // No derived frames at all (stale base.zip): the
        // engine-drawn metal button with the same label.
        theme.return_frames.normal = ui_kit::Frame();
        test::fakes::Reset_All_();
        ui_kit::Button fallback(10, 20, 76, 24,
            ui_kit::Button_Kind::Image, theme);
        fallback.Set_Label("RETURN");
        fallback.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 238);
        DOCTEST_REQUIRE(theme.font.prints.size() == 1);
        DOCTEST_CHECK(theme.font.prints[0].text == "RETURN");
        DOCTEST_CHECK(theme.font.prints[0].x == 10 + (76 - 60) / 2);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);
    }

    DOCTEST_TEST_CASE("toggle button fills bright when on, grey when off, bare when disabled") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;

        // On: bright cyan-blue fill with a near-white centre.
        test::fakes::Reset_All_();
        ui_kit::Button on(10, 20, 20, 20, ui_kit::Button_Kind::Toggle,
            theme);
        on.Set_State(ui_kit::Button_State::Active);
        on.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 230);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 154);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x1 == 12);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 22);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x2 == 27);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 37);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 247);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 18);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y1 == 28);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x2 == 21);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y2 == 31);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);

        // Off: the grey ramp instead.
        test::fakes::Reset_All_();
        ui_kit::Button off(10, 20, 20, 20, ui_kit::Button_Kind::Toggle,
            theme);
        off.Set_State(ui_kit::Button_State::Rest);
        off.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 236);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 246);

        // Disabled: the bare cell, no fill -- the colony's
        // no-button state.
        test::fakes::Reset_All_();
        ui_kit::Button broken(10, 20, 20, 20,
            ui_kit::Button_Kind::Toggle, theme);
        broken.Set_State(ui_kit::Button_State::Disabled);
        broken.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 230);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);

        // The toggle follows a second theme's fills.
        test::fakes::Reset_All_();
        Alt_Theme alt;
        ui_kit::Button alt_on(10, 20, 20, 20,
            ui_kit::Button_Kind::Toggle, alt);
        alt_on.Set_State(ui_kit::Button_State::Active);
        alt_on.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 46);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 48);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 49);
    }

    DOCTEST_TEST_CASE("cell button draws the value with a chevron, blue while active") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        test::fakes::Reset_All_();
        ui_kit::Button cell(10, 20, 100, 20, ui_kit::Button_Kind::Cell,
            theme);
        cell.Set_Label("pixelart");
        cell.Draw();

        // Dark interior, border, value at x+4 centred on the
        // fake font by the field system's rule, chevron 12 px from
        // the right edge.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 230);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].color == 234);
        DOCTEST_REQUIRE(theme.font.prints.size() == 2);
        DOCTEST_CHECK(theme.font.prints[0].text == "pixelart");
        DOCTEST_CHECK(theme.font.prints[0].x == 14);
        DOCTEST_CHECK(theme.font.prints[0].y == 20 + 20 / 2 - (12 - 1) / 2);
        DOCTEST_CHECK_FALSE(theme.font.prints[0].active);
        DOCTEST_CHECK(theme.font.prints[1].text == "v");
        DOCTEST_CHECK(theme.font.prints[1].x == 10 + 100 - 12);
        DOCTEST_CHECK(theme.font.prints[1].y == 20 + 20 / 2 - (12 - 1) / 2);
        DOCTEST_CHECK_FALSE(theme.font.prints[1].active);

        // A label offset pins the value to the caller's baseline
        // (the settings row's text line): the value and the
        // chevron both move to it.
        ui_kit::Button row_cell(10, 20, 100, 20,
            ui_kit::Button_Kind::Cell, theme);
        row_cell.Set_Label("pixelart");
        row_cell.Set_Label_Y_Offset(4);
        row_cell.Draw();
        DOCTEST_REQUIRE(theme.font.prints.size() == 4);
        DOCTEST_CHECK(theme.font.prints[2].text == "pixelart");
        DOCTEST_CHECK(theme.font.prints[2].y == 24);
        DOCTEST_CHECK(theme.font.prints[3].text == "v");
        DOCTEST_CHECK(theme.font.prints[3].y == 24);

        // Active (popup open): the blue face, light labels.
        ui_kit::Button open(10, 20, 100, 20, ui_kit::Button_Kind::Cell,
            theme);
        open.Set_Label("pixelart");
        open.Set_State(ui_kit::Button_State::Active);
        open.Draw();
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 3);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 138);
        DOCTEST_REQUIRE(theme.font.prints.size() == 6);
        DOCTEST_CHECK(theme.font.prints[4].active);
        DOCTEST_CHECK(theme.font.prints[5].text == "v");
        DOCTEST_CHECK(theme.font.prints[5].active);

        // Disabled: no chevron.
        ui_kit::Button disabled(10, 20, 100, 20,
            ui_kit::Button_Kind::Cell, theme);
        disabled.Set_Label("pixelart");
        disabled.Set_State(ui_kit::Button_State::Disabled);
        disabled.Draw();
        DOCTEST_REQUIRE(theme.font.prints.size() == 7);
        DOCTEST_CHECK(theme.font.prints[6].text == "pixelart");
        DOCTEST_CHECK_FALSE(theme.font.prints[6].active);
    }

    DOCTEST_TEST_CASE("colony theme hands out the button style and return frames") {
        using namespace test_ui_kit_detail;
        const std::string normal = Make_Frame(85, 26, 60);
        const std::string pressed = Make_Frame(85, 26, 70);
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame((const s_animation_header*)normal.data()),
            ui_kit::Frame((const s_animation_header*)pressed.data()),
            title);

        // The metal bevel is the scrollbar thumb's, the blue the
        // active selector's, the toggle fills the squares' own.
        const ui_kit::Button_Style style = theme.Button_Style();
        DOCTEST_CHECK(style.face == 238);
        DOCTEST_CHECK(style.highlight == 243);
        DOCTEST_CHECK(style.shadow == 232);
        DOCTEST_CHECK(style.primary_face == 138);
        DOCTEST_CHECK(style.primary_highlight == 154);
        DOCTEST_CHECK(style.disabled_face == 234);
        DOCTEST_CHECK(style.cell_interior == 230);
        DOCTEST_CHECK(style.cell_border == 234);
        DOCTEST_CHECK(style.toggle_on_base == 154);
        DOCTEST_CHECK(style.toggle_on_bright == 247);
        DOCTEST_CHECK(style.toggle_off_base == 236);
        DOCTEST_CHECK(style.toggle_off_bright == 246);
        // The scrollbar corner dots are the colony's index 80,
        // black in the colony palette, remapped to 0.
        DOCTEST_CHECK(style.scroll_corner == 0);
        // The label fonts are engine font style 2 with 1 px
        // spacing: the baked RETURN text inks 9 rows (style 4
        // inks 12) and measures 41 px as style-2 widths with
        // 1 px spacing. Dark text on the metal face, light on
        // the blue face; the cell font is the colony cyan at
        // rest, light on the blue fill.
        DOCTEST_REQUIRE(style.label_font != nullptr);
        DOCTEST_REQUIRE(style.cell_font != nullptr);
        test::fakes::Reset_All_();
        style.label_font->Print(0, 0, "Show", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_CHECK(test::fakes::g_font_spacing == 1);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 230);
        style.label_font->Print(0, 0, "Show", true);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 247);
        // The pressed label font is the orange the real RETURN
        // lights up with when held (menu palette 221), the same
        // orange whether the print is active or not.
        DOCTEST_REQUIRE(style.label_pressed_font != nullptr);
        style.label_pressed_font->Print(0, 0, "MORE", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 2);
        DOCTEST_CHECK(test::fakes::g_font_spacing == 1);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 221);
        style.label_pressed_font->Print(0, 0, "MORE", true);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 221);
        // The cell font keeps the font's own spacing: no override.
        test::fakes::Reset_All_();
        style.cell_font->Print(0, 0, "pixelart", false);
        DOCTEST_CHECK(test::fakes::g_font_spacing == -1);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 150);
        style.cell_font->Print(0, 0, "pixelart", true);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 247);

        // The RETURN frames are the derived sprite, 85x26, rest
        // and pressed.
        const ui_kit::Image_Button_Frames frames = theme.Return_Frames();
        DOCTEST_CHECK_FALSE(frames.normal.Is_Null());
        DOCTEST_CHECK_FALSE(frames.pressed.Is_Null());
        DOCTEST_CHECK(frames.normal.Width() == 85);
        DOCTEST_CHECK(frames.normal.Height() == 26);
        DOCTEST_CHECK(frames.pressed.Width() == 85);
        DOCTEST_CHECK(frames.pressed.Height() == 26);

        // Null frames stay null, for a stale base.zip.
        ui_kit::Colony_Theme bare(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);
        DOCTEST_CHECK(bare.Return_Frames().normal.Is_Null());
        DOCTEST_CHECK(bare.Return_Frames().pressed.Is_Null());
        DOCTEST_CHECK(bare.Return_Frames().normal.Width() == 0);
        DOCTEST_CHECK(bare.Return_Frames().normal.Height() == 0);
    }

    DOCTEST_TEST_CASE("colony theme hands out the header-plate display font") {
        using namespace test_ui_kit_detail;
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), title);

        // No base.zip is mounted in the tests, so Moo2Display.json
        // does not load and the header font falls back to the
        // style-4 title font, never a blank label.
        const ui_kit::Font& header = theme.Header_Font();
        test::fakes::Reset_All_();
        header.Print(0, 0, "SETTING", false);
        DOCTEST_CHECK(test::fakes::g_font_style == 4);
        DOCTEST_CHECK(test::fakes::g_font_color.colors[0] == 234);
        header.Print(0, 0, "SETTING", true);
        DOCTEST_CHECK(test::fakes::g_font_style == 4);

        // A Moo2Display-shaped bitmap font (cap 8, no descender,
        // pressed mirrors normal) measures the plate labels at the
        // baked widths. The bitmaps here are dummy fills; only the
        // advances and row lengths matter for Width.
        const char* display_json = R"JSON({
            "name": "Moo2Display",
            "cap_height": 8,
            "descender_rows": 0,
            "space_advance": 4,
            "provenance": "test",
            "glyphs": {
                "S": {"advance": 12, "width": 11, "bitmap": {
                    "normal": [[243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243]]}},
                "E": {"advance": 11, "width": 10, "bitmap": {
                    "normal": [[243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243]]}},
                "T": {"advance": 11, "width": 10, "bitmap": {
                    "normal": [[243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243]]}},
                "I": {"advance": 6, "width": 5, "bitmap": {
                    "normal": [[243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243],
                        [243,243,243,243,243]]}},
                "N": {"advance": 14, "width": 13, "bitmap": {
                    "normal": [[243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243,243,243]]}},
                "G": {"advance": 13, "width": 11, "bitmap": {
                    "normal": [[243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243]],
                    "pressed": [[243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243],
                        [243,243,243,243,243,243,243,243,243,243,243]]}}
            }
        })JSON";
        ui_kit::Engine_Font fallback(4, title, title);
        ui_kit::Bitmap_Font display(ui_kit::Bitmap_Font::State::Normal,
            fallback);
        DOCTEST_REQUIRE(display.Load_From_Text(display_json,
            std::strlen(display_json)));
        DOCTEST_CHECK(display.Is_Loaded());
        DOCTEST_CHECK(display.Height() == 8);
        // SETTING = S(12)+E(11)+T(11)+T(11)+I(6)+N(14)+G(11) = 76;
        // the last glyph counts its bitmap width, not its advance.
        DOCTEST_CHECK(display.Width("SETTING") == 76);
        DOCTEST_CHECK(display.Width("SETTING") ==
            12 + 11 + 11 + 11 + 6 + 14 + 11);
    }

    DOCTEST_TEST_CASE("scroll bar models the colony's inverted slider") {
        using namespace test_ui_kit_detail;
        // Maximum first-visible row: 0 when everything fits,
        // otherwise total - visible (the colony's num - 10).
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Max_Scroll(10, 10) == 0);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Max_Scroll(11, 10) == 1);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Max_Scroll(20, 10) == 10);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Max_Scroll(5, 10) == 0);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Max_Scroll(100, 10) == 90);
        // The slider is inverted (COLSUM::Update_First): slider
        // at max shows the first rows, slider at 0 the last.
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Slider_From_Top(0, 20, 10) == 10);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Slider_From_Top(10, 20, 10) == 0);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Slider_From_Top(3, 20, 10) == 7);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Top_From_Slider(10, 20, 10) == 0);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Top_From_Slider(0, 20, 10) == 10);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Top_From_Slider(7, 20, 10) == 3);
        // Out-of-range sliders clamp: below 0 shows the first
        // rows, above max the last.
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Top_From_Slider(-5, 20, 10) == 10);
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Top_From_Slider(99, 20, 10) == 0);
        // Visible while the list is at least as long as the
        // window, colsum's rule (num_colonies >= 10).
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Visible(10, 10));
        DOCTEST_CHECK_FALSE(ui_kit::Scroll_Bar::Visible(9, 10));
        DOCTEST_CHECK(ui_kit::Scroll_Bar::Visible(11, 10));
    }

    DOCTEST_TEST_CASE("scroll bar thumb follows colsum's formula with no minimum height") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        // The colony's track: x 621..626, y 40..311 (271, the
        // strip between the scroll arrows).
        const ui_kit::Scroll_Bar bar(621, 6, 40, 271, theme);
        DOCTEST_CHECK(bar.track_y() == 40);
        DOCTEST_CHECK(bar.track_bottom() == 311);
        // Exactly ten colonies: the thumb spans the whole track.
        DOCTEST_CHECK(bar.Thumb_Top(0, 10, 10) == 40);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 10, 10) == 311);
        // Twenty colonies: half the list visible, the thumb half
        // the track, at the top or the bottom.
        DOCTEST_CHECK(bar.Thumb_Top(0, 20, 10) == 40);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 20, 10) == 175);
        DOCTEST_CHECK(bar.Thumb_Top(10, 20, 10) == 175);
        DOCTEST_CHECK(bar.Thumb_Bottom(10, 20, 10) == 311);
        // Twelve colonies, scrolled to the top: 271 * 10 / 12 =
        // 225 px of thumb.
        DOCTEST_CHECK(bar.Thumb_Top(0, 12, 10) == 40);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 12, 10) == 265);
        // A hundred colonies: a 27 px thumb, no floor.
        DOCTEST_CHECK(bar.Thumb_Top(0, 100, 10) == 40);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 100, 10) == 67);
        DOCTEST_CHECK(bar.Thumb_Top(90, 100, 10) == 283);
        DOCTEST_CHECK(bar.Thumb_Bottom(90, 100, 10) == 311);
        // A thousand colonies: the thumb is 2 px tall -- colsum
        // has no minimum height, and neither does the kit (the
        // settings screen's old 8 px floor was its own invention).
        DOCTEST_CHECK(bar.Thumb_Top(0, 1000, 10) == 40);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 1000, 10) == 42);
        DOCTEST_CHECK(bar.Thumb_Bottom(0, 1000, 10) -
                bar.Thumb_Top(0, 1000, 10) == 2);
    }

    DOCTEST_TEST_CASE("scroll bar draws the thumb bevel and single corner dots") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        // A distinctive corner colour, so the dots prove they
        // come from the theme and are not the thumb's face.
        theme.buttons.scroll_corner = 9;
        const ui_kit::Scroll_Bar bar(621, 6, 40, 271, theme);

        // Scrolled to the bottom (top 10 of 20): the thumb sits
        // at 175..311, so the track top at y 40 carries only the
        // two corner dots -- not the full row the settings screen
        // used to draw there.
        test::fakes::Reset_All_();
        bar.Draw(10, 20, 10);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 9);
        // Face, then the bevel: bright top/left, dark
        // bottom/right, colsum's Draw_Bar_Indicator.
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 238);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 175);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 626);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 311);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 243);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 175);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y2 == 175);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 243);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x2 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].y1 == 311);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].x1 == 626);
        // The corner dots are single pixels at the track's
        // corners, in the theme's corner colour.
        for (size_t index = 5; index < 9; ++index) {
            DOCTEST_CHECK(test::fakes::g_fill_calls[index].color == 9);
        }
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].x1 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].y1 == 40);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].x2 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[5].y2 == 40);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].x1 == 626);
        DOCTEST_CHECK(test::fakes::g_fill_calls[6].y1 == 40);
        DOCTEST_CHECK(test::fakes::g_fill_calls[7].x1 == 621);
        DOCTEST_CHECK(test::fakes::g_fill_calls[7].y1 == 311);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].x1 == 626);
        DOCTEST_CHECK(test::fakes::g_fill_calls[8].y1 == 311);
        // Nothing else touches the track top row: no full-row
        // line across the strip.
        for (const test::fakes::Fill_Call& fill :
            test::fakes::g_fill_calls) {
            if (fill.y1 == 40) {
                DOCTEST_CHECK(fill.x1 == fill.x2);
            }
        }

        // Scrolled to the top: the thumb sits at 40..175, so the
        // track bottom at y 311 carries only the two dots.
        test::fakes::Reset_All_();
        bar.Draw(0, 20, 10);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 9);
        for (const test::fakes::Fill_Call& fill :
            test::fakes::g_fill_calls) {
            if (fill.y1 == 311) {
                DOCTEST_CHECK(fill.x1 == fill.x2);
            }
        }

        // A list shorter than the window draws nothing.
        test::fakes::Reset_All_();
        bar.Draw(0, 5, 10);
        DOCTEST_CHECK(test::fakes::g_fill_calls.empty());
    }

    DOCTEST_TEST_CASE("tab bar places fixed-size sort tabs at the colony's positions") {
        using namespace test_ui_kit_detail;
        Fake_Theme theme;
        // The colony's sort strip: tabs at x 89..530, y 446,
        // fixed-size sprites 1 px apart.
        ui_kit::Tab_Bar bar(89, 446, 530, theme);
        bar.Add_Fixed_Tab("Name", 50, true);
        bar.Add_Fixed_Tab("Population", 78, false);
        bar.Add_Fixed_Tab("Food", 42, false);
        bar.Add_Fixed_Tab("Industry", 63, false);
        bar.Add_Fixed_Tab("Science", 66, false);
        bar.Add_Fixed_Tab("Producing", 86, false);
        bar.Add_Fixed_Tab("BC", 50, false);

        DOCTEST_REQUIRE(bar.tabs().size() == 7);
        const int16_t expected_x[] = {89, 140, 219, 262, 326, 393, 480};
        const int16_t expected_w[] = {50, 78, 42, 63, 66, 86, 50};
        for (size_t index = 0; index < 7; ++index) {
            DOCTEST_CHECK(bar.tabs()[index].x == expected_x[index]);
            DOCTEST_CHECK(bar.tabs()[index].width == expected_w[index]);
            DOCTEST_CHECK(bar.tabs()[index].natural_width ==
                expected_w[index]);
        }
        DOCTEST_CHECK(bar.End_X() == 530);
        // BC is the reason the widths are fixed: its label is
        // short (the fake font gives 20 px + 16 padding = 36),
        // but the sprite is 50 wide -- the proportional fill
        // would shrink it and drift every tab after Name.
        DOCTEST_CHECK(bar.tabs()[6].natural_width == 50);
    }

    DOCTEST_TEST_CASE("colony list layout matches the colony summary screen") {
        using namespace test_ui_kit_detail;
        const s_colors title = {{234, 238, 240, 242, 243, 244, 246, 247}};
        ui_kit::Colony_Theme theme(ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(),
            ui_kit::Frame(), ui_kit::Frame(), ui_kit::Frame(), title);

        // The colony's list, measured from the rendered screen:
        // header plates y 4..31, first row at 35, rows on a 31 px
        // pitch, panel 4..347, content edge at 619. The panel is
        // 11..619 (609 wide): its right edge is the bevel at
        // 617..619 and the scrollbar strip (620..627) sits outside
        // it, on black -- not the settings panel's 11..627, which
        // takes the strip inside.
        const ui_kit::List_Layout layout(11, 4, 609, 619, 4, 28, 10,
            theme.Spacing());
        DOCTEST_CHECK(layout.first_row_y() == 35);
        DOCTEST_CHECK(layout.row_y(0) == 35);
        DOCTEST_CHECK(layout.row_y(1) == 66);
        DOCTEST_CHECK(layout.row_y(9) == 314);
        DOCTEST_CHECK(layout.panel_h() == 344);
        DOCTEST_CHECK(layout.panel_y() + layout.panel_h() - 1 == 347);
        DOCTEST_CHECK(layout.scroll_track_y() == 35);
        DOCTEST_CHECK(layout.scroll_track_h() == 309);
        DOCTEST_CHECK(layout.info_y() == 352);

        // The background frames the colony rows the same way:
        // row 0 light at 35, shade at 36; last row 314..343 with
        // its trailing shade at 343; footing shade/shade/light
        // at 344/345/346.
        Fake_Theme fake;
        test::fakes::Reset_All_();
        ui_kit::Draw_List_Background(fake, layout);
        // 1 panel fill + 10 * 5 row lines + 3 footing + 1 bevel = 55.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 55);
        // The panel fill and border stop at the content edge, 619:
        // the scrollbar strip beyond it stays black.
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x2 == 619);
        // The right-edge bevel: frame-light, one pixel inside the
        // panel's right edge (x 618), full panel height.
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x1 == 618);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].x2 == 618);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y1 == 4);
        DOCTEST_CHECK(test::fakes::g_fill_calls[54].y2 == 347);
        DOCTEST_REQUIRE(test::fakes::g_box_calls.size() == 1);
        DOCTEST_CHECK(test::fakes::g_box_calls[0].width == 609);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].y1 == 35);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].y1 == 36);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[46].y1 == 314);
        DOCTEST_CHECK(test::fakes::g_fill_calls[46].color == 241);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].y1 == 343);
        DOCTEST_CHECK(test::fakes::g_fill_calls[50].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].y1 == 344);
        DOCTEST_CHECK(test::fakes::g_fill_calls[51].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].y1 == 345);
        DOCTEST_CHECK(test::fakes::g_fill_calls[52].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].y1 == 346);
        DOCTEST_CHECK(test::fakes::g_fill_calls[53].color == 241);

        // The dividers land on the baked grooves: 98..100,
        // 233..235, 375..377, 509..511, over the rows only.
        test::fakes::Reset_All_();
        const ui_kit::Column_Layout columns(
            {{11, 87}, {101, 132}, {236, 139}, {378, 131}, {512, 87}},
            layout.first_row_y(),
            (int16_t)(layout.row_y(9) + layout.row_height() - 1));
        ui_kit::Draw_List_Columns(fake, columns);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 12);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].x1 == 98);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 233);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y1 == 35);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].y2 == 343);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].x1 == 99);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].x1 == 100);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].x1 == 509);
        DOCTEST_CHECK(test::fakes::g_fill_calls[9].color == 233);
    }
} // DOCTEST_TEST_SUITE("ui kit")
