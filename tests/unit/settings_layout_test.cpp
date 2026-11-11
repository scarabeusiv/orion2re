#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_layout.h"

namespace {

    using namespace SETTINGS_LAYOUT;

    // The screen's three category labels, in tab order.
    const int kLabel_Lengths[3] = {14, 8, 4}; // Game Internals, Gameplay, Mods

    bool Overlaps(const s_rect& a, const s_rect& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
            b.y < a.y + a.h;
    }

    bool Inside(const s_rect& inner, const s_rect& outer) {
        return inner.x >= outer.x && inner.y >= outer.y &&
            Rect_Right(inner) <= Rect_Right(outer) &&
            Rect_Bottom(inner) <= Rect_Bottom(outer);
    }

    void Check_Bands_In_Order(const s_settings_layout& layout) {
        // Top to bottom, no overlaps, no gaps in the reading order:
        // tab bar, header, list, description, bottom bar.
        CHECK(layout.tab_bar.y < layout.header.y);
        CHECK(layout.header.y < layout.list.y);
        CHECK(layout.list.y < layout.description.y);
        CHECK(layout.description.y < layout.bottom_bar.y);
        CHECK_FALSE(Overlaps(layout.tab_bar, layout.header));
        CHECK_FALSE(Overlaps(layout.header, layout.list));
        CHECK_FALSE(Overlaps(layout.list, layout.description));
        CHECK_FALSE(Overlaps(layout.description, layout.bottom_bar));
        for (int tab = 0; tab < 3; ++tab) {
            CHECK(Inside(layout.tabs[tab], layout.tab_bar));
        }
        CHECK(Inside(layout.scrollbar, layout.list) == false);
        CHECK(layout.scrollbar.y == layout.list.y);
        CHECK(layout.scrollbar.h == layout.list.h);
        CHECK(Rect_Right(layout.scrollbar) <=
            Rect_Right(layout.tab_bar));
        CHECK(Inside(layout.show_more, layout.bottom_bar));
        CHECK(Inside(layout.done, layout.bottom_bar));
        CHECK_FALSE(Overlaps(layout.show_more, layout.done));
        CHECK(Rect_Right(layout.done) == Rect_Right(layout.bottom_bar));
        CHECK(layout.show_more.x < layout.done.x);
        // The VALUE zone lines up between the header and the rows.
        for (int row = 0; row < layout.row_count; ++row) {
            const s_rect value = Row_Value_Rect(layout, row);
            CHECK(value.x == layout.header_value.x);
            CHECK(value.w == layout.header_value.w);
            CHECK(Inside(value, Row_Rect(layout, row)));
            CHECK(Inside(Row_Name_Rect(layout, row), Row_Rect(layout, row)));
            CHECK_FALSE(Overlaps(Row_Name_Rect(layout, row), value));
        }
    }

    TEST_CASE("layout at 640x480 pins the percentage allocation") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        CHECK(layout.width == 640);
        CHECK(layout.height == 480);
        CHECK(layout.row_count == 10);
        CHECK(layout.content.x == 13);
        CHECK(layout.content.w == 614);
        // Vertical bands (permille of 480, rounded).
        CHECK(layout.tab_bar.y == 10);
        CHECK(layout.tab_bar.h == 26);
        CHECK(layout.header.y == 42);
        CHECK(layout.header.h == 26);
        CHECK(layout.list.y == 74);
        CHECK(layout.list.h == 287);
        CHECK(layout.description.y == 367);
        CHECK(layout.description.h == 67);
        CHECK(layout.bottom_bar.y == 440);
        CHECK(layout.bottom_bar.h == 31);
        CHECK(Rect_Bottom(layout.bottom_bar) == 471);
        // Tabs tile the bar in proportion to their label lengths.
        CHECK(layout.tabs[0].x == 13);
        CHECK(layout.tabs[0].w == 286);
        CHECK(layout.tabs[1].x == 304);
        CHECK(layout.tabs[1].w == 190);
        CHECK(layout.tabs[2].x == 499);
        CHECK(layout.tabs[2].w == 128);
        CHECK(Rect_Right(layout.tabs[2]) == Rect_Right(layout.tab_bar));
        CHECK(layout.tabs[0].w > layout.tabs[1].w);
        CHECK(layout.tabs[1].w > layout.tabs[2].w);
        // VALUE zone at 79% of the width, in the header and rows.
        CHECK(layout.header_value.x == 506);
        CHECK(layout.header_value.w == 112);
        CHECK(layout.header_setting.w == 487);
        // Scrollbar strip at the content's right edge.
        CHECK(layout.scrollbar.x == 622);
        CHECK(layout.scrollbar.w == 5);
        CHECK(layout.list.w == 605);
        // MORE / RETURN at the bottom bar's right end.
        CHECK(layout.show_more.x == 465);
        CHECK(layout.show_more.w == 69);
        CHECK(layout.done.x == 542);
        CHECK(layout.done.w == 85);
        CHECK(layout.show_more.h == 25);
        CHECK(layout.done.h == 25);
        CHECK(layout.show_more.y == 443);
        // Rows tile the list height exactly.
        CHECK(Row_Rect(layout, 0).y == 74);
        CHECK(Row_Rect(layout, 0).h == 28);
        CHECK(Row_Rect(layout, 9).y == 332);
        CHECK(Rect_Bottom(Row_Rect(layout, 9)) == 361);
        CHECK(Row_Value_Rect(layout, 0).y == 79);
        CHECK(Row_Value_Rect(layout, 0).h == 17);
        Check_Bands_In_Order(layout);
    }

    TEST_CASE("layout scales with the screen size") {
        const s_settings_layout small =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        const s_settings_layout big =
            Compute_Layout(1280, 960, 10, kLabel_Lengths);
        // Doubling the screen doubles every rect (within 1 px of
        // rounding): the layout is percentages, not fixed pixels.
        const auto near_double = [](int16_t a, int16_t b) {
            return b >= 2 * a - 1 && b <= 2 * a + 1;
        };
        CHECK(near_double(small.tab_bar.y, big.tab_bar.y));
        CHECK(near_double(small.tab_bar.h, big.tab_bar.h));
        CHECK(near_double(small.header.y, big.header.y));
        CHECK(near_double(small.list.y, big.list.y));
        CHECK(near_double(small.list.h, big.list.h));
        CHECK(near_double(small.description.y, big.description.y));
        CHECK(near_double(small.bottom_bar.y, big.bottom_bar.y));
        CHECK(near_double(small.tabs[0].w, big.tabs[0].w));
        CHECK(near_double(small.tabs[2].w, big.tabs[2].w));
        CHECK(near_double(small.header_value.x, big.header_value.x));
        CHECK(near_double(small.scrollbar.w, big.scrollbar.w));
        CHECK(near_double(small.done.w, big.done.w));
        CHECK(near_double(Row_Rect(small, 0).h, Row_Rect(big, 0).h));
        Check_Bands_In_Order(big);

        const s_settings_layout wide =
            Compute_Layout(1920, 1440, 10, kLabel_Lengths);
        CHECK(wide.tab_bar.y == 29);
        CHECK(wide.list.h == 861);
        Check_Bands_In_Order(wide);

        const s_settings_layout mid =
            Compute_Layout(800, 600, 10, kLabel_Lengths);
        CHECK(mid.tab_bar.y == 12);
        CHECK(mid.list.h == 359);
        Check_Bands_In_Order(mid);
    }

    TEST_CASE("scrollbar thumb follows the scroll offset") {
        const s_settings_layout layout =
            Compute_Layout(640, 480, 10, kLabel_Lengths);
        // A list that fits has no thumb.
        const s_rect none = Scrollbar_Thumb_Rect(layout, 10, 0);
        CHECK(none.w == 0);
        const s_rect none2 = Scrollbar_Thumb_Rect(layout, 5, 0);
        CHECK(none2.w == 0);
        // 186 rows: the thumb is ~10/186 of the track, at the top
        // when scrolled to the first rows, at the bottom at the end.
        const s_rect top = Scrollbar_Thumb_Rect(layout, 186, 0);
        CHECK(top.w == layout.scrollbar_track.w);
        CHECK(top.h == 15);
        CHECK(top.y == layout.scrollbar_track.y);
        const s_rect bottom = Scrollbar_Thumb_Rect(layout, 186, 176);
        CHECK(bottom.h == 15);
        CHECK(Rect_Bottom(bottom) == Rect_Bottom(layout.scrollbar_track));
        const s_rect middle = Scrollbar_Thumb_Rect(layout, 186, 88);
        CHECK(middle.y > top.y);
        CHECK(middle.y < bottom.y);
        CHECK(Inside(middle, layout.scrollbar_track));
        // A very long list still gets a visible thumb.
        const s_rect tiny = Scrollbar_Thumb_Rect(layout, 1000, 500);
        CHECK(tiny.h == 8);
        CHECK(Inside(tiny, layout.scrollbar_track));
    }

    TEST_CASE("degenerate sizes lay out nothing") {
        const s_settings_layout zero =
            Compute_Layout(0, 0, 10, kLabel_Lengths);
        CHECK(zero.width == 0);
        CHECK(zero.tab_bar.w == 0);
        CHECK(zero.list.h == 0);
        const s_settings_layout no_rows =
            Compute_Layout(640, 480, 0, kLabel_Lengths);
        CHECK(no_rows.width == 0);
    }

} // namespace
