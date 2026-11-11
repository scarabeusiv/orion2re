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
    // Panel fills. Panels are translucent: the background
    // shows through them (real alpha blend, never a repaint of the
    // background onto the panel). Interiors are CLEAN flat fills:
    // no dither, no speckle inside an element (Tom, 2026-11-11).
    // The tie-together's dithered/speckled faces were the wrong
    // reading of "tie together" and are gone with the PNG sprites.
    // The elements themselves are hand-drawn bitmaps now
    // (settings_bitmaps); their colours live in the bitmaps'
    // tone tables, not here.
    inline constexpr s_rgb kPanel_Fill{16, 20, 38};
    inline constexpr int kPanel_Alpha = 150;
    // The vivid blue (24, 12, 252), COLSUM embedded palette
    // index 56: the popup's selection bar, and the tab bloom's
    // vivid tone. The background installs that exact colour in
    // palette index 253 on entry (the menu palette's black
    // there is unused by this screen), so the blend maps to it
    // exactly instead of the nearest muted entry.
    inline constexpr s_rgb kActive_Fill{24, 12, 252};
    // The dropdown chevron: the bright value-text white while
    // the dropdown is closed, the menu palette's saturated blue
    // (52, 48, 136) while its popup is open -- the colour shift is
    // the state signal, like an industrial knob that was flicked.
    // Black was tried for the closed state and dropped: it has too
    // little contrast against the dark value-box face. The blue sits
    // in the vivid blue's family but darker, so it reads as a knob
    // state, not as the active tab (whose vivid blue is (24, 12, 252)).
    inline constexpr s_rgb kChevron_Closed{225, 228, 235};
    inline constexpr s_rgb kChevron_Open{52, 48, 136};
    // The active edit field is opaque: its fill must erase the
    // field system's own pixel-font text underneath before the HD
    // text is drawn over it. It is the sunken box's face while a
    // row is being edited.
    inline constexpr s_rgb kEdit_Fill{28, 34, 64};
    // MORE and RETURN: the colony's RETURN button / the main
    // menu's buttons -- a grey bevelled metal button, label in
    // black. The button itself is a hand-drawn bitmap now; only
    // its label colour stays here.
    inline constexpr s_rgb kGreyButton_Text{0, 0, 0};
    // Band dividers: the colony's thin double-line separators
    // (measured: light lines (112, 112, 112) over dark (60, 60, 60)).
    inline constexpr s_rgb kDivider_Light{112, 112, 112};
    inline constexpr s_rgb kDivider_Dark{60, 60, 60};

    // Multi-pixel frame thickness for a screen of the given width:
    // 2 px at 640, 4 at 1280, 6 at 1920 and above.
    int16_t Frame_Thickness(int16_t screen_width);

    // A corner radius given in 640-space, scaled to the screen
    // width: base 2 is 2 px at 640 and 4 at 1280. At least 1, so a
    // rounded element keeps a cut corner at any resolution.
    int16_t Scaled_Radius(int16_t screen_width, int16_t base_radius);

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

    // The nearest palette index for a colour: the opaque
    // fills and the bitmap blits map through it.
    uint8_t Palette_Index(const s_rgb& color);

    // Blit a row-major buffer of palette indices at (x, y):
    // -1 is transparent (the pixel underneath is left alone),
    // anything else is written. The blit is clipped to the
    // screen. The bitmap module builds its buffers in index
    // space and hands them here in one call, so the page lock
    // is held once per element.
    void Blit_Indices(int16_t x, int16_t y, int16_t w, int16_t h,
        const int16_t* indices);

    // The horizontal span [x0, x1) of a rounded rect at row y: the
    // rect's own span on the straight rows, narrowed by the corner
    // arc on the corner rows. False when y is outside the rect or
    // the span is empty. `corners` selects which corners are
    // rounded: bit 0 top-left, 1 top-right, 2 bottom-right,
    // 3 bottom-left (all four by default); a square corner spans
    // the rect's full edge. The bitmap module uses it to punch
    // rounded corners into stretched tab bitmaps.
    bool Rounded_Row_Span(const SETTINGS_LAYOUT::s_rect& rect,
        int16_t radius, uint8_t corners, int16_t y, int16_t& x0,
        int16_t& x1);

    // Mouse helpers for hover / pressed visuals.
    bool Point_In_Rect(int16_t px, int16_t py, const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Hover(const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Pressed(const SETTINGS_LAYOUT::s_rect& rect);

} // namespace SETTINGS_RENDER
