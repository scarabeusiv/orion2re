#include "pch.h"

#include <algorithm>
#include <cstdint>

#include "game/settings_layout.h"

namespace SETTINGS_LAYOUT {

    namespace {

        // A fraction of a total, in permille (parts per thousand),
        // rounded to the nearest pixel. All allocation fractions
        // are permille constants, so the layout is exact integer
        // arithmetic -- no floating point, no 640x480 assumptions.
        int16_t Scale(int32_t total, int32_t permille) {
            return (int16_t)((total * permille + 500) / 1000);
        }

        // Vertical allocation, permille of the height.
        constexpr int32_t kMargin_Y = 20;
        constexpr int32_t kTab_Bar_H = 55;
        constexpr int32_t kGap_Y = 12;
        constexpr int32_t kHeader_H = 55;
        constexpr int32_t kList_H = 598;
        constexpr int32_t kDescription_H = 140;
        constexpr int32_t kBottom_Bar_H = 64;

        // Horizontal allocation, permille of the width.
        constexpr int32_t kMargin_X = 20;
        constexpr int32_t kTab_Gap = 8;
        constexpr int32_t kScrollbar_W = 8;
        constexpr int32_t kScrollbar_Gap = 6;
        constexpr int32_t kValue_X = 790;
        constexpr int32_t kValue_Gap = 10;
        constexpr int32_t kMore_W = 108;
        constexpr int32_t kDone_W = 133;
        constexpr int32_t kBottom_Gap = 12;
        // MORE / RETURN button height, permille of the height.
        constexpr int32_t kButton_H = 52;

        // A tab's width weight is its label length plus this
        // padding (in characters), so a long label gets a
        // proportionally wider tab and no tab is mostly empty.
        constexpr int32_t kTab_Padding = 4;

        // The value control's height, as a fraction of the row
        // height (percent): the control is vertically centred in
        // its row.
        constexpr int32_t kValue_Control_H_Percent = 62;

        // The scrollbar thumb's minimum height, in pixels.
        constexpr int16_t kThumb_Min_H = 8;

    } // namespace

    s_settings_layout Compute_Layout(int32_t width, int32_t height,
        int row_count, const int tab_label_lengths[3]) {
        s_settings_layout layout;
        if (width <= 0 || height <= 0 || row_count <= 0) {
            return layout;
        }
        layout.width = (int16_t)width;
        layout.height = (int16_t)height;
        layout.row_count = row_count;

        // Vertical bands, stacked from the top margin. Each band's
        // position is the sum of the bands above it, so the bands
        // tile the height exactly (top margin + bands + bottom
        // margin = 1000 permille).
        const int16_t margin_y = Scale(height, kMargin_Y);
        const int16_t tab_bar_h = Scale(height, kTab_Bar_H);
        const int16_t gap_y = Scale(height, kGap_Y);
        const int16_t header_h = Scale(height, kHeader_H);
        const int16_t list_h = Scale(height, kList_H);
        const int16_t description_h = Scale(height, kDescription_H);
        const int16_t bottom_bar_h = Scale(height, kBottom_Bar_H);
        int16_t y = margin_y;
        const int16_t tab_bar_y = y;
        y = (int16_t)(y + tab_bar_h + gap_y);
        const int16_t header_y = y;
        y = (int16_t)(y + header_h + gap_y);
        const int16_t list_y = y;
        y = (int16_t)(y + list_h + gap_y);
        const int16_t description_y = y;
        y = (int16_t)(y + description_h + gap_y);
        const int16_t bottom_bar_y = y;
        const int16_t bottom_bar_bottom = (int16_t)(bottom_bar_y + bottom_bar_h);

        // Horizontal content: the same margin on both sides.
        const int16_t margin_x = Scale(width, kMargin_X);
        const int16_t content_x = margin_x;
        const int16_t content_right = (int16_t)(width - margin_x);
        const int16_t content_w = (int16_t)(content_right - content_x);

        layout.content = s_rect{content_x, tab_bar_y, content_w,
            (int16_t)(bottom_bar_bottom - tab_bar_y)};
        layout.tab_bar = s_rect{content_x, tab_bar_y, content_w, tab_bar_h};

        // Tabs: divide the tab bar in proportion to the label
        // lengths (plus padding), with a gap between tabs. Each
        // tab's width is its share of the available width; the
        // last tab absorbs the rounding remainder, so the tabs
        // tile the bar exactly.
        const int16_t tab_gap = Scale(width, kTab_Gap);
        const int16_t tab_available =
            (int16_t)(content_w - 2 * tab_gap);
        int32_t total_weight = 0;
        int32_t weights[3] = {};
        for (int tab = 0; tab < 3; ++tab) {
            weights[tab] = tab_label_lengths[tab] + kTab_Padding;
            total_weight += weights[tab];
        }
        int16_t tab_x = content_x;
        int32_t used = 0;
        for (int tab = 0; tab < 3; ++tab) {
            int16_t tab_w;
            if (tab + 1 < 3 && total_weight > 0) {
                tab_w = (int16_t)(tab_available * weights[tab] / total_weight);
                used += tab_w;
            } else {
                tab_w = (int16_t)(tab_available - used);
            }
            if (tab_w < 1) {
                tab_w = 1;
            }
            layout.tabs[tab] = s_rect{tab_x, tab_bar_y, tab_w, tab_bar_h};
            tab_x = (int16_t)(tab_x + tab_w + tab_gap);
        }

        // The scrollbar strip sits at the content's right edge;
        // the list rows span the content width minus the strip
        // and the gap before it.
        const int16_t scrollbar_w = Scale(width, kScrollbar_W);
        const int16_t scrollbar_gap = Scale(width, kScrollbar_Gap);
        const int16_t scrollbar_x = (int16_t)(content_right - scrollbar_w);
        const int16_t list_right = (int16_t)(scrollbar_x - scrollbar_gap);
        layout.list = s_rect{content_x, list_y,
            (int16_t)(list_right - content_x), list_h};
        layout.scrollbar = s_rect{scrollbar_x, list_y, scrollbar_w, list_h};
        layout.scrollbar_track = layout.scrollbar;

        // The VALUE zone starts at the same x in the header and
        // in every row, and runs to the list's right edge.
        const int16_t value_x = Scale(width, kValue_X);
        const int16_t value_gap = Scale(width, kValue_Gap);
        const int16_t value_w = (int16_t)(list_right - value_x);
        layout.header = s_rect{content_x, header_y, content_w, header_h};
        layout.header_setting = s_rect{content_x, header_y,
            (int16_t)(value_x - value_gap - content_x), header_h};
        layout.header_value = s_rect{value_x, header_y, value_w, header_h};

        layout.description = s_rect{content_x, description_y,
            content_w, description_h};
        layout.bottom_bar = s_rect{content_x, bottom_bar_y,
            content_w, bottom_bar_h};

        // MORE and RETURN sit at the bottom bar's right end, on
        // one line, vertically centred in the bar.
        const int16_t more_w = Scale(width, kMore_W);
        const int16_t done_w = Scale(width, kDone_W);
        const int16_t bottom_gap = Scale(width, kBottom_Gap);
        const int16_t button_h = Scale(height, kButton_H);
        const int16_t button_y =
            (int16_t)(bottom_bar_y + (bottom_bar_h - button_h) / 2);
        const int16_t done_x = (int16_t)(content_right - done_w);
        const int16_t more_x = (int16_t)(done_x - bottom_gap - more_w);
        layout.show_more = s_rect{more_x, button_y, more_w, button_h};
        layout.done = s_rect{done_x, button_y, done_w, button_h};
        return layout;
    }

    s_rect Row_Rect(const s_settings_layout& layout, int index) {
        if (index < 0) {
            index = 0;
        }
        if (index >= layout.row_count) {
            index = layout.row_count - 1;
        }
        // Tile the list height exactly: each row's edges are its
        // share of the list height, so the rows meet with no gaps
        // and the last row absorbs the rounding remainder.
        const int32_t list_h = layout.list.h;
        const int32_t y0 = layout.list.y + list_h * index / layout.row_count;
        const int32_t y1 =
            layout.list.y + list_h * (index + 1) / layout.row_count;
        return s_rect{layout.list.x, (int16_t)y0, layout.list.w,
            (int16_t)(y1 - y0)};
    }

    s_rect Row_Name_Rect(const s_settings_layout& layout, int index) {
        const s_rect row = Row_Rect(layout, index);
        // The name zone ends where the header's SETTING zone ends
        // (the gap before the VALUE zone); the row starts at the
        // list's left edge, like the header.
        return s_rect{row.x, row.y,
            (int16_t)(Rect_Right(layout.header_setting) - row.x), row.h};
    }

    s_rect Row_Value_Rect(const s_settings_layout& layout, int index) {
        const s_rect row = Row_Rect(layout, index);
        const int16_t value_h =
            (int16_t)(row.h * kValue_Control_H_Percent / 100);
        const int16_t value_y =
            (int16_t)(row.y + (row.h - value_h) / 2);
        return s_rect{layout.header_value.x, value_y,
            layout.header_value.w, value_h};
    }

    s_rect Scrollbar_Thumb_Rect(const s_settings_layout& layout,
        int total_rows, int scroll_top) {
        const s_rect track = layout.scrollbar_track;
        if (total_rows <= layout.row_count || total_rows <= 0) {
            return s_rect{track.x, track.y, 0, 0};
        }
        const int max_scroll = total_rows - layout.row_count;
        if (scroll_top < 0) {
            scroll_top = 0;
        }
        if (scroll_top > max_scroll) {
            scroll_top = max_scroll;
        }
        int16_t thumb_h =
            (int16_t)(track.h * layout.row_count / total_rows);
        if (thumb_h < kThumb_Min_H) {
            thumb_h = kThumb_Min_H;
        }
        if (thumb_h > track.h) {
            thumb_h = track.h;
        }
        const int16_t travel = (int16_t)(track.h - thumb_h);
        const int16_t thumb_y = max_scroll > 0
            ? (int16_t)(track.y + travel * scroll_top / max_scroll)
            : track.y;
        return s_rect{track.x, thumb_y, track.w, thumb_h};
    }

    int16_t Rect_Right(const s_rect& rect) {
        return (int16_t)(rect.x + rect.w);
    }

    int16_t Rect_Bottom(const s_rect& rect) {
        return (int16_t)(rect.y + rect.h);
    }

} // namespace SETTINGS_LAYOUT
