#pragma once

#include <cstdint>

namespace SETTINGS_LAYOUT {

    // A rectangle: top-left corner (x, y), size (w, h). Right and
    // bottom edges are exclusive (x + w, y + h).
    struct s_rect {
        int16_t x = 0;
        int16_t y = 0;
        int16_t w = 0;
        int16_t h = 0;
    };

    // The settings screen's geometry, allocated as fractions of the
    // screen size -- never as fixed 640x480 pixel coordinates -- so
    // the same layout scales to any game resolution. Vertical
    // allocation (fractions of the height, top to bottom):
    //
    //   top margin      0.020
    //   header row      0.055  (SETTING / VALUE column labels)
    //   gap             0.012
    //   list            0.665  (10 rows, tiled exactly)
    //   gap             0.012
    //   description     0.140  (selected row's description + status)
    //   gap             0.012
    //   bottom bar      0.064  (tabs + MORE + RETURN)
    //   bottom margin   0.020
    //
    // Horizontal allocation (fractions of the width): the content
    // spans 0.020..0.980. The bottom bar holds the three category
    // tabs on the left (dividing the bar's left width in proportion
    // to their label lengths, plus padding, with a 0.008 gap between
    // tabs) and MORE and RETURN at the right end, the SAME width
    // (0.133, sized for the longer label) with a 0.012 gap between
    // them. The list's rows span the content width minus the
    // scrollbar strip (0.008 wide, at the content's right edge, with
    // a 0.006 gap). The VALUE zone starts at 0.790 of the width, in
    // the header and in every row alike.
    //
    // The layout is pure: it takes the screen size and the tab
    // label lengths and returns rectangles. No video, font or
    // global state, so unit tests pin it at several resolutions.
    struct s_settings_layout {
        int16_t width = 0;
        int16_t height = 0;
        int row_count = 0;
        // The content rect: the outer box the screen's elements
        // live in (content margins on all four sides).
        s_rect content;
        // The tab strip: the rect spanning the three tabs, inside
        // the bottom bar.
        s_rect tab_bar;
        s_rect tabs[3];
        s_rect header;
        s_rect header_setting;
        s_rect header_value;
        s_rect list;
        s_rect scrollbar;
        s_rect scrollbar_track;
        s_rect description;
        s_rect bottom_bar;
        s_rect show_more;
        s_rect done; // RETURN
    };

    // Computes the layout for a screen of the given size.
    // `row_count` is the number of visible list rows (the screen's
    // ROWS_PER_PAGE); `tab_label_lengths` holds the character
    // length of each of the 3 category labels, in tab order.
    s_settings_layout Compute_Layout(int32_t width, int32_t height,
        int row_count, const int tab_label_lengths[3]);

    // The rect of visible row `index` (0-based, clamped into
    // range). Rows tile the list area exactly; the last row
    // absorbs the rounding remainder.
    s_rect Row_Rect(const s_settings_layout& layout, int index);

    // The setting-name zone inside a row: the row minus the VALUE
    // zone and the gap between them.
    s_rect Row_Name_Rect(const s_settings_layout& layout, int index);

    // The value-control zone inside a row: the row's VALUE zone,
    // vertically centred, 62% of the row height. Dropdowns,
    // toggles and edit fields sit in this rect (step 4 renders
    // them; step 3 only allocates it).
    s_rect Row_Value_Rect(const s_settings_layout& layout, int index);

    // The scrollbar thumb for a list of `total_rows` rows whose
    // first visible row is `scroll_top`. Empty (w == 0) when the
    // whole list fits (no scrolling), otherwise a rect inside the
    // scrollbar track, its height proportional to the visible
    // fraction and its position proportional to the scroll offset
    // (top of the track = first rows).
    s_rect Scrollbar_Thumb_Rect(const s_settings_layout& layout,
        int total_rows, int scroll_top);

    // Exclusive right / bottom edges.
    int16_t Rect_Right(const s_rect& rect);
    int16_t Rect_Bottom(const s_rect& rect);

} // namespace SETTINGS_LAYOUT
