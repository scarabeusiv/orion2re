#pragma once

#include <cstdint>

#include "game/settings_blend.h"
#include "game/settings_layout.h"

namespace SETTINGS_RENDER {

    using s_rgb = SETTINGS_BLEND::s_rgb;

    // Text colours (8-bit display space).
    // The setting-name label colour matches the game: the
    // colony's cyan, orange on the hovered row. Cyan is the
    // colsum palette's bright cyan; orange is the menu
    // palette's option-label orange (252, 136, 0), exact
    // match to the original GAME SETTINGS dialog.
    inline constexpr s_rgb kText_Label{0, 228, 228};
    inline constexpr s_rgb kText_Hover{252, 136, 0};
    inline constexpr s_rgb kText_Bright{225, 228, 235};
    inline constexpr s_rgb kText_Normal{200, 208, 218};
    inline constexpr s_rgb kText_Dim{150, 158, 168};
    inline constexpr s_rgb kText_Header{235, 238, 242};
    // Panel / control fills. Panels are translucent: the background
    // shows through them (real alpha blend, never a repaint of the
    // background onto the panel). Interiors are CLEAN flat fills:
    // no dither, no speckle inside an element (Tom, 2026-11-11).
    // The tie-together's dithered/speckled faces were the wrong
    // reading of "tie together" and are gone with the PNG sprites.
    inline constexpr s_rgb kPanel_Fill{16, 20, 38};
    inline constexpr int kPanel_Alpha = 150;
    inline constexpr s_rgb kControl_Fill{24, 30, 58};
    inline constexpr int kControl_Alpha = 170;
    // The active tab's fill: the colony's vivid blue (24, 12,
    // 252), COLSUM embedded palette index 56. The background
    // installs that exact colour in palette index 253 on entry
    // (the menu palette's black there is unused by this screen),
    // so the blend maps to it exactly instead of the nearest
    // muted entry.
    inline constexpr s_rgb kActive_Fill{24, 12, 252};
    inline constexpr int kActive_Alpha = 200;
    // The dropdown chevron: the bright value-text white while
    // the dropdown is closed, the menu palette's saturated blue
    // (52, 48, 136) while its popup is open -- the colour shift is
    // the state signal, like an industrial knob that was flicked.
    // Black was tried for the closed state and dropped: it has too
    // little contrast against the dark control fill. The blue sits
    // in the vivid blue's family but darker, so it reads as a knob
    // state, not as the active tab (whose vivid blue is (24, 12, 252)).
    inline constexpr s_rgb kChevron_Closed{225, 228, 235};
    inline constexpr s_rgb kChevron_Open{52, 48, 136};
    // The scrollbar arrows: the colony's shape -- a dark-navy
    // button face with a light-grey triangle, like the colony's
    // own up/down arrows (measured from the colony reference:
    // the arrow field is (20, 20, 56) with a (180, 180, 180)
    // triangle). NOT the tie-together's blue button faces: the
    // colony's arrows are dark navy, not saturated blue.
    // Brighter on hover, darker while pressed.
    inline constexpr s_rgb kArrow_Face{20, 20, 56};
    inline constexpr s_rgb kArrow_Face_Hover{28, 28, 96};
    inline constexpr s_rgb kArrow_Face_Pressed{8, 8, 32};
    inline constexpr s_rgb kArrow_Triangle{180, 180, 180};
    // The scrollbar thumb: the colony's indicator bar (colsum.cpp
    // Draw_Bar_Indicator): a bevelled handle, 6 px wide in the
    // colony's 13 px strip, light top/left and dark bottom/right.
    // Its body is the colony thumb blue (28, 28, 164) in the
    // colony palette -- but that blue has no close hue in the
    // menu palette (nearest 138, distance 42), so the thumb
    // stays the documented grey-metal substitution, in the
    // colony's shape.
    inline constexpr s_rgb kThumb_Fill{140, 142, 150};
    inline constexpr s_rgb kThumb_Edge_Light{180, 182, 190};
    inline constexpr s_rgb kThumb_Edge_Dark{90, 92, 100};
    inline constexpr s_rgb kStar_On{235, 200, 90};
    inline constexpr s_rgb kStar_Off{96, 100, 110};
    // The active edit field is opaque: its fill must erase the
    // field system's own pixel-font text underneath before the HD
    // text is drawn over it.
    inline constexpr s_rgb kEdit_Fill{28, 34, 64};
    // Header plates: the colony's bevelled grey plates
    // (NAME/FARMERS/WORKERS/SCIENTISTS/BUILDING), measured from
    // the colony reference. The face is the plates' clean grey;
    // the top bevel is a dark outer row and a bright inner row;
    // the bottom and sides are dark. The plates are CLEAN: the
    // colony's baked dither is not reproduced (Tom, 2026-11-11:
    // interiors stay clean, no noisy texture).
    inline constexpr s_rgb kPlate_Face{60, 60, 60};
    inline constexpr s_rgb kPlate_Top_Outer{76, 76, 76};
    inline constexpr s_rgb kPlate_Top_Inner{88, 88, 88};
    inline constexpr s_rgb kPlate_Dark{44, 44, 44};
    // Category tabs: the colony's SORT band. Inactive tabs are
    // the band's flat dark navy (COLSUM embedded palette index
    // 57, (0, 0, 36)); the active tab is the same navy at its
    // top and bottom with the vivid blue band behind its label
    // (the colony's active tab is dark navy at the edges and
    // vivid in the middle, brightest behind the label). Hover
    // lightens the navy; pressed darkens the vivid.
    inline constexpr s_rgb kTab_Inactive{0, 0, 36};
    inline constexpr s_rgb kTab_Inactive_Hover{20, 20, 56};
    inline constexpr s_rgb kTab_Inactive_Pressed{0, 0, 16};
    inline constexpr s_rgb kTab_Active_Pressed{12, 8, 136};
    // MORE and RETURN: the colony's RETURN button / the main
    // menu's buttons -- a grey bevelled metal button (measured
    // from the colony's RETURN: face (112, 112, 112), label in
    // black). Opaque: the original game's buttons are solid
    // metal, not translucent panels.
    inline constexpr s_rgb kGreyButton_Face{112, 112, 112};
    inline constexpr s_rgb kGreyButton_Hover{136, 136, 136};
    inline constexpr s_rgb kGreyButton_Pressed{88, 88, 88};
    inline constexpr s_rgb kGreyButton_Text{0, 0, 0};
    // Band dividers: the colony's thin double-line separators
    // (measured: light lines (112, 112, 112) over dark (60, 60, 60)).
    inline constexpr s_rgb kDivider_Light{112, 112, 112};
    inline constexpr s_rgb kDivider_Dark{60, 60, 60};

    // Multi-pixel frame thickness for a screen of the given width:
    // 2 px at 640, 4 at 1280, 6 at 1920 and above.
    int16_t Frame_Thickness(int16_t screen_width);

    // Clear the blend-table and scaled-glyph caches. Call on screen
    // entry, after the background palette is installed (the tables
    // are built against that palette).
    void Reset();

    // Decode the engine font (style 4) glyphs into coverage grids,
    // once. False when no font is loaded: text then falls back to
    // the engine pixel font, never a blank label.
    bool Ensure_Font();

    // Width of text at the given glyph-cell height, in pixels.
    int Text_Width(const char* text, int cell_height);

    // Glyph-cell height plus its leading.
    int Line_Height(int cell_height);

    // Draw text; y is the TOP of the glyph cell. align 0 = left
    // (x is the left edge), 1 = centred on x, 2 = right (x is the
    // right edge).
    void Draw_Text(int16_t x, int16_t y, const char* text, const s_rgb& color,
        int cell_height, int align);

    // Translucent fill: a real per-pixel alpha blend over whatever
    // is already on the page (background gradient, stars, other
    // elements). The blend table is built once per (colour, alpha)
    // against the current palette.
    void Blend_Fill(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& color,
        int alpha);

    // Opaque fill in the nearest palette colour to rgb.
    void Solid_Fill(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& color);

    // The industrial frame: the colony's bevel -- a dark outer
    // border of Frame_Thickness, plus a 1 px light inner line
    // (measured from the colony's outer frame: dark outside
    // (20..52), light inside (88..112)). The frame's own border
    // is the element's border: adjacent elements share it, so
    // the borders blend into one continuous mechanical frame.
    void Frame(const SETTINGS_LAYOUT::s_rect& rect, int16_t thickness);

    // A header plate: the colony's bevelled grey plate (SETTING /
    // VALUE), a clean grey face with a dark-then-bright top bevel
    // and dark bottom/sides. The plate is its own element, with
    // its own border -- it does not borrow the frame's.
    void Plate(const SETTINGS_LAYOUT::s_rect& rect);

    // Five-point star, centred on (cx, cy), outer radius in px.
    // Filled when `filled` (bool ON); outline only when false
    // (bool OFF). Never drawn on a dropdown or a write-input row.
    void Star(int16_t cx, int16_t cy, int16_t radius, const s_rgb& color,
        bool filled);

    // Filled triangles for the dropdown chevron and scrollbar arrows.
    void Triangle_Down(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color);
    void Triangle_Up(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color);

    // Mouse helpers for hover / pressed visuals.
    bool Point_In_Rect(int16_t px, int16_t py, const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Hover(const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Pressed(const SETTINGS_LAYOUT::s_rect& rect);

} // namespace SETTINGS_RENDER
