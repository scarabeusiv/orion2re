#pragma once

#include <cstdint>

#include "game/settings_blend.h"
#include "game/settings_layout.h"
#include "game/settings_png.h"

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
    // background onto the panel).
    inline constexpr s_rgb kPanel_Fill{16, 20, 38};
    inline constexpr int kPanel_Alpha = 150;
    inline constexpr s_rgb kControl_Fill{24, 30, 58};
    inline constexpr int kControl_Alpha = 170;
    inline constexpr s_rgb kControl_Hover_Fill{34, 42, 78};
    inline constexpr int kControl_Hover_Alpha = 185;
    inline constexpr s_rgb kControl_Pressed_Fill{14, 18, 36};
    inline constexpr int kControl_Pressed_Alpha = 190;
    // The active tab's fill: the colony's vivid blue (24, 12,
    // 252). The background installs that exact colour in palette
    // index 253 on entry (the menu palette's black there is
    // unused by this screen), so the blend maps to it exactly
    // instead of the nearest muted entry.
    inline constexpr s_rgb kActive_Fill{24, 12, 252};
    inline constexpr int kActive_Alpha = 200;
    inline constexpr s_rgb kSelection_Fill{52, 48, 136};
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
    // The scrollbar arrow buttons: a blue face (the colony's dark
    // blue arrow field, in the vivid blue's family) with a bold
    // white triangle. Brighter on hover, darker while pressed --
    // the face is the state signal, like the dropdown chevron's
    // colour shift. The blue sits darker than the active tab's
    // vivid (24, 12, 252), so it reads as a button, not as the
    // active tab.
    inline constexpr s_rgb kArrowBtn_Fill{52, 48, 136};
    inline constexpr s_rgb kArrowBtn_Hover_Fill{72, 68, 170};
    inline constexpr s_rgb kArrowBtn_Pressed_Fill{36, 32, 96};
    inline constexpr int kArrowBtn_Alpha = 200;
    inline constexpr int kSelection_Alpha = 230;
    inline constexpr s_rgb kThumb_Fill{140, 142, 150};
    inline constexpr int kThumb_Alpha = 230;
    // The thumb's bevel edges (the colony's indicator bar): a
    // lighter top/left and a darker bottom/right, around the
    // thumb fill.
    inline constexpr s_rgb kThumb_Edge_Light{180, 182, 190};
    inline constexpr s_rgb kThumb_Edge_Dark{90, 92, 100};
    inline constexpr s_rgb kStar_On{235, 200, 90};
    inline constexpr s_rgb kStar_Off{96, 100, 110};
    // The active edit field is opaque: its fill must erase the
    // field system's own pixel-font text underneath before the HD
    // text is drawn over it.
    inline constexpr s_rgb kEdit_Fill{28, 34, 64};

    enum e_button_state {
        BUTTON_NORMAL,
        BUTTON_HOVER,
        BUTTON_PRESSED,
        BUTTON_ACTIVE,
        BUTTON_DISABLED,
    };

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

    // Panel: translucent fill + multi-pixel industrial frame (light
    // outer border, dark inner line).
    void Panel(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& fill, int alpha);

    // The industrial frame alone: light outer border of
    // Frame_Thickness, plus a 1 px dark inner line.
    void Frame(const SETTINGS_LAYOUT::s_rect& rect, int16_t thickness);

    // Metal button: translucent metal fill, industrial frame, a
    // 1 px top highlight, centred label. Pressed darkens the fill
    // and shifts the label 1 px down.
    void Button(const SETTINGS_LAYOUT::s_rect& rect, const char* label,
        e_button_state state, int cell_height);

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

    // Hi-res PNG sprites (the tie-together, Tom 2026-11-11):
    // bitmap sprites, like the original game's sprites but
    // hi-res -- not scalable vector vertices. A sprite blits
    // its RGBA pixels scaled by nearest neighbour (the bitmap
    // feel survives at any resolution) and alpha-blended over
    // the page (real transparency: the background shows
    // through a translucent face, and a glow halo shows the
    // elements behind it). Alpha is quantized to multiples of
    // 16, so the blend-table cache stays bounded (the sprites
    // are authored with quantized alphas).
    //
    // Draw_Sprite scales the whole image into the rect.
    // Draw_Sprite_Tiled tiles the image 1:1 across the rect
    // (a dithered face: the speckle stays the same grain at
    // any resolution). Draw_Sprite_9Slice draws a 9-slice
    // frame/button: the corners scale to the frame thickness,
    // the edges stretch, the centre tiles. The border's first
    // two source rows/columns (the light outer edge and the
    // dark inner line) are preserved 1:1, so the industrial
    // frame's light+dark edge never washes out when the
    // border scales down.
    void Draw_Sprite(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image);
    void Draw_Sprite_Tiled(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image);
    void Draw_Sprite_9Slice(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image, int16_t border);

    // Mouse helpers for hover / pressed visuals.
    bool Point_In_Rect(int16_t px, int16_t py, const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Hover(const SETTINGS_LAYOUT::s_rect& rect);
    bool Is_Pressed(const SETTINGS_LAYOUT::s_rect& rect);

} // namespace SETTINGS_RENDER
