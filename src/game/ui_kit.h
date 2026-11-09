#pragma once

// Reusable colony-style UI components (ui_kit).
//
// The colony screens are the game's visual reference: a full-window
// frame, a tab bar of sort buttons, a list with header plates. The
// colony art itself is baked into 640x480 backgrounds and cannot be
// reused directly, so the components here are engine-drawn or drawn
// from the derived sprite frames (tools/extract_sprite.py), behind a
// Theme. A screen takes a const Theme& and never touches a sprite id,
// a palette index, or a font style number.
//
// The Theme is the abstraction a hi-res asset set drops into: the
// same screens, the same components, different frames and fonts.
// A hi-res theme is a second Theme implementation (its own derived
// frames at 2x, its own Font styles); no screen code changes.
//
// Fonts are part of the abstraction for the same reason. The colony
// sort labels are baked glyphs, and they measure as engine font
// style 2 (widths within 1-2 px per label, ink 8-9 rows tall), not
// the style 4 the settings screen used before -- so the tab font is
// a property of the theme, not a hard-coded style number in the
// screen. A glyph only ever uses the first two ramp entries
// (fonts::Print_Character_ASM maps glyph command 1 to colors[0],
// the edge, and command 2 to colors[1], the face; style 2 has no
// command-3 pixels at all), so a label ramp is really an
// edge/face pair, with the remaining entries filled with the face.

#include <cstdint>
#include <vector>

struct s_animation_header;
struct s_colors;

namespace ui_kit {
    // A font as a screen sees it: measure text, draw text. The
    // implementation owns the engine font style and the colour ramp
    // per state; callers never name a style number. Width/Height/
    // Print set the engine font style first: the engine keeps one
    // global font state, and the widths belong to the style.
    class Font {
      public:
        virtual ~Font() = default;
        virtual int16_t Width(const char* text) const = 0;
        virtual int16_t Height() const = 0;
        virtual void Print(int16_t x, int16_t y, const char* text,
            bool active) const = 0;
    };

    // The engine-font implementation of Font: one engine font style
    // plus a label colour ramp per state. The ramp maps glyph shade
    // to palette index, low (edge) to high (face), exactly like
    // fonts::Set_Font_Style; only entries 0 (edge) and 1 (face) are
    // ever used by the colony-sized fonts, the rest repeat the face.
    class Engine_Font : public Font {
      public:
        Engine_Font(int16_t style, const s_colors& normal_colors,
            const s_colors& active_colors);
        int16_t Width(const char* text) const override;
        int16_t Height() const override;
        void Print(int16_t x, int16_t y, const char* text,
            bool active) const override;

      private:
        int16_t style_;
        s_colors normal_colors_;
        s_colors active_colors_;
    };

    // One tab frame: the derived sprite plus how its middle is
    // treated when the tab is wider than the frame. A flat middle
    // (the rest frame) tiles invisibly; a gradient middle (the
    // selected frame's blue field) must be resampled, or the
    // gradient repeats like stripes.
    struct Tab_Frame {
        const s_animation_header* frame = nullptr;
        bool resample_middle = false;
    };

    // Both tab frames, the bevel slices, and the fallback fill the
    // screen draws when a derived frame is missing (stale base.zip):
    // a flat fill in the frame's own colours, never the derived
    // frame's look, but the tab stays usable.
    struct Tab_Frames {
        Tab_Frame normal;
        Tab_Frame active;
        int16_t left_slice = 0;
        int16_t right_slice = 0;
        uint8_t fallback_fill_normal = 0;
        uint8_t fallback_fill_active = 0;
        uint8_t fallback_border = 0;
    };

    // Window chrome: outer/inner frame colours and the title font.
    // The title is centred horizontally; title_y is its print y
    // relative to the window top.
    struct Window_Style {
        uint8_t frame_outer = 0;
        uint8_t frame_inner = 0;
        const Font* title_font = nullptr; // non-owning; null = no title
        int16_t title_y = 0;
    };

    // A theme is everything about a window's look that varies with
    // the art set: fonts, tab frames, window chrome, tab metrics.
    // Screens take a const Theme&; a hi-res asset set is a second
    // Theme implementation.
    class Theme {
      public:
        virtual ~Theme() = default;
        virtual const Font& Tab_Font() const = 0;
        // Qualified: the member function name would otherwise hide
        // the same-named struct in the return type.
        virtual ui_kit::Tab_Frames Tab_Frames() const = 0;
        virtual ui_kit::Window_Style Window_Style() const = 0;
        // Per side, label to frame edge. The colony sort buttons
        // carry 8-10 px per side (frame width minus baked label
        // width, halved).
        virtual int16_t Tab_Padding() const = 0;
        // Between adjacent tabs. The colony sort buttons sit 1 px
        // apart (next x = previous x + width + 1).
        virtual int16_t Tab_Gap() const = 0;
        virtual int16_t Tab_Height() const = 0;
        // Print y of the tab label relative to the tab top. The
        // colony baked labels ink rows 8-16 of the 24 px frame.
        virtual int16_t Tab_Label_Y_Offset() const = 0;
    };

    // The colony look: derived COLSUM sort-button frames, font
    // style 2 tab labels (the baked colony labels measure as
    // style 2), engine-drawn window chrome in the settings colours.
    // The title font keeps the screen's own title colours, passed
    // in; the tab label ramps are the theme's own, measured from the
    // baked labels (menu palette): inactive is a white face with a
    // dark-grey edge, active a blue-white face with a mid-blue edge.
    class Colony_Theme : public Theme {
      public:
        Colony_Theme(const s_animation_header* tab_normal,
            const s_animation_header* tab_active,
            const s_colors& title_colors);
        const Font& Tab_Font() const override;
        ui_kit::Tab_Frames Tab_Frames() const override;
        ui_kit::Window_Style Window_Style() const override;
        int16_t Tab_Padding() const override;
        int16_t Tab_Gap() const override;
        int16_t Tab_Height() const override;
        int16_t Tab_Label_Y_Offset() const override;

      private:
        Engine_Font tab_font_;
        Engine_Font title_font_;
        const s_animation_header* tab_normal_;
        const s_animation_header* tab_active_;
    };

    // A row of tabs: layout (widths from label + padding, gap
    // between tabs) and drawing (frame + centred label). Owns the
    // geometry, so the click fields and the drawing can never drift
    // apart: the screen builds its fields from Tab_Bar::tabs().
    class Tab_Bar {
      public:
        struct Tab {
            const char* label = nullptr;
            bool active = false;
            int16_t x = 0;
            int16_t width = 0;
        };

        Tab_Bar(int16_t x, int16_t y, const Theme& theme);
        void Add_Tab(const char* label, bool active);
        const std::vector<Tab>& tabs() const;
        int16_t y() const;
        int16_t height() const;
        int16_t End_X() const; // x past the last tab
        void Draw() const;

      private:
        int16_t x_;
        int16_t y_;
        const Theme& theme_;
        std::vector<Tab> tabs_;
    };

    // Full-window frame: thin light outer line, dark inner line,
    // centred title -- the frame baked into the colony screens'
    // backgrounds, engine-drawn in theme colours.
    void Draw_Window_Frame(const Theme& theme, int16_t x, int16_t y,
        int16_t w, int16_t h, const char* title);
} // namespace ui_kit
