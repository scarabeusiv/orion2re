#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_layout.h"

namespace {

    using namespace SETTINGS_LAYOUT;

    // The content rect the dropdown window is clamped into
    // (the 640x480 screen's content: 20 permille margins).
    const s_rect kContent{13, 10, 614, 460};

    TEST_CASE("dropdown window encloses the list with a margin") {
        // Title + 3 items, 20 px rows, at (100, 50), 200 wide.
        const s_dropdown_window w = Dropdown_Window(kContent,
            100, 50, 200, 80, 20, 4);
        CHECK(w.margin == 5);
        CHECK(w.step == 20);
        CHECK(w.rows == 4);
        // The window is the list plus the margin on all sides.
        CHECK(w.window.x == 95);
        CHECK(w.window.y == 45);
        CHECK(w.window.w == 210);
        CHECK(w.window.h == 90);
        // The list itself sits inside the window.
        CHECK(w.window.x <= 100);
        CHECK(w.window.y <= 50);
        CHECK(Rect_Right(w.window) >= 300);
        CHECK(Rect_Bottom(w.window) >= 130);
    }

    TEST_CASE("dropdown window clamps inside the content rect") {
        // A list hard against the content's right and bottom edges.
        const s_dropdown_window w = Dropdown_Window(kContent,
            500, 400, 120, 60, 20, 3);
        CHECK(w.window.x >= kContent.x);
        CHECK(w.window.y >= kContent.y);
        CHECK(Rect_Right(w.window) <= Rect_Right(kContent));
        CHECK(Rect_Bottom(w.window) <= Rect_Bottom(kContent));
        // A list wider than the content pins to the content.
        const s_dropdown_window wide = Dropdown_Window(kContent,
            0, 0, 700, 500, 20, 3);
        CHECK(wide.window.x == kContent.x);
        CHECK(wide.window.y == kContent.y);
        CHECK(Rect_Right(wide.window) <= Rect_Right(kContent));
        CHECK(Rect_Bottom(wide.window) <= Rect_Bottom(kContent));
    }

    TEST_CASE("dropdown separators sit on the row boundaries") {
        const s_dropdown_window w = Dropdown_Window(kContent,
            100, 50, 200, 80, 20, 4);
        // Three separators for four rows (title + 3 items).
        CHECK(Dropdown_Separator_Y(w, 1) == 45 + 5 + 20);
        CHECK(Dropdown_Separator_Y(w, 2) == 45 + 5 + 40);
        CHECK(Dropdown_Separator_Y(w, 3) == 45 + 5 + 60);
        // Each separator is strictly inside the window, between
        // the first and last row.
        for (int k = 1; k < 4; ++k) {
            const int16_t sy = Dropdown_Separator_Y(w, k);
            CHECK(sy > w.window.y);
            CHECK(sy < Rect_Bottom(w.window));
        }
        // Out-of-range indices clamp to the first/last separator.
        CHECK(Dropdown_Separator_Y(w, 0) ==
            Dropdown_Separator_Y(w, 1));
        CHECK(Dropdown_Separator_Y(w, 99) ==
            Dropdown_Separator_Y(w, 3));
    }

    TEST_CASE("dropdown triangle sits at the button's right end") {
        const s_rect button{508, 100, 104, 26};
        const s_rect tri = Dropdown_Triangle_Rect(button);
        CHECK(tri.w == 7);
        CHECK(tri.h == 4);
        // Right edge 4 px inside the button's right edge.
        CHECK(Rect_Right(tri) == 508 + 104 - 4);
        CHECK(tri.x == 508 + 104 - 4 - 7);
        // Vertically centred on the button.
        CHECK(tri.y == 100 + (26 - 4) / 2);
        CHECK(Rect_Bottom(tri) == 100 + (26 + 4) / 2);
        // Inside the button.
        CHECK(tri.x >= button.x);
        CHECK(tri.y >= button.y);
        CHECK(Rect_Right(tri) <= Rect_Right(button));
        CHECK(Rect_Bottom(tri) <= Rect_Bottom(button));
    }

    TEST_CASE("dropdown value text clears the triangle") {
        // A 104 px button (the value-box-wide frame): the centred
        // text's right edge must clear the triangle's left edge.
        const int16_t max_w = Dropdown_Value_Text_Width(104);
        CHECK(max_w == 104 - 2 * 4 - 2 * 7);
        // The widest dropdown value, "overview" (74 px), fits.
        CHECK(max_w >= 74);
        // The text's right edge, centred, clears the triangle:
        // (104 + max_w) / 2 <= triangle left edge (104 - 4 - 7).
        CHECK((104 + max_w) / 2 <= 104 - 4 - 7);
        // Narrower buttons still leave room for a short value.
        CHECK(Dropdown_Value_Text_Width(85) >= 10);
        CHECK(Dropdown_Value_Text_Width(20) >= 1);
    }

} // namespace
