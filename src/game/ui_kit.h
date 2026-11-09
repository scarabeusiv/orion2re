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
// frames at 2x, its own Font styles); no screen code changes. The
// theme owns its assets: the screen asks for a theme by kind
// (Load_Colony_Theme) and never names a sprite id or a font style
// number. Colours come from the theme, except a title ramp the
// screen shares with its other chrome, which it passes in.
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
#include <memory>
#include <vector>

struct s_animation_header;
struct s_colors;

namespace ui_kit {
    // A font as a screen sees it: measure text, draw text. The
    // implementation owns the engine font style and the colour ramp
    // per state; callers never name a style number. Width/Height/
    // Print set the engine font style first: the engine keeps one
    // global font state, and the widths belong to the style.
    // Height is part of the contract for components that centre
    // text vertically; Tab_Bar uses the theme's label offset
    // instead, so nothing in the kit calls it yet.
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

    // One frame as the kit sees it: an opaque handle over a derived
    // sprite frame. No interface in the kit exposes the frame's
    // representation: the LBX frame type appears only in this
    // adapter's constructor and private storage, which wrap a frame
    // loaded by sprite_art::Load_Sprite_Frame. A hi-res asset set
    // is frames of the same shape at 2x, loaded the same way --
    // the kit draws whatever handle it is given, and the screens
    // never see the type at all. A null frame draws nothing; the
    // caller selects the page first, as for the sprite_art draws.
    class Frame {
      public:
        Frame() = default;
        explicit Frame(const s_animation_header* frame) : frame_(frame) {}
        [[nodiscard]] bool Is_Null() const { return frame_ == nullptr; }
        void Draw_Stretched(int16_t x, int16_t y, int16_t width,
            int16_t left_slice, int16_t right_slice) const;
        void Draw_Stretched_Resampled(int16_t x, int16_t y, int16_t width,
            int16_t left_slice, int16_t right_slice) const;

      private:
        const s_animation_header* frame_ = nullptr;
    };

    // One tab frame: the derived sprite plus how its middle is
    // treated when the tab is wider than the frame. A flat middle
    // (the rest frame) tiles invisibly; a gradient middle (the
    // selected frame's blue field) must be resampled, or the
    // gradient repeats like stripes.
    struct Tab_Frame {
        Frame frame;
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

    // Spacing: the empty space between and around the bands of a
    // colony-style window -- list rows, description box, bottom bar.
    // Measured from the colony summary screen (colsum.cpp and the
    // baked COLSUM background, 640x480 game pixels): rows are 30 px
    // cells on a 31 px pitch, so 1 px of empty background shows
    // between adjacent rows; the header plate's bottom edge sits
    // 3 px above the first row; the description box starts 4 px
    // below the last row and ends 7 px above the bottom bar.
    struct Spacing {
        int16_t row_height = 30;         // one list row's cell height
        int16_t row_gap = 1;             // empty background between rows
        int16_t header_to_first_row = 3; // header bottom -> first row top
        int16_t list_to_info = 4;        // last row bottom -> info box top
        int16_t info_to_bar = 7;         // info box bottom -> bottom bar top
        int16_t Row_Pitch() const {
            return (int16_t)(row_height + row_gap);
        }
    };

    // List background colours: panel interior, panel border, row
    // frame lines -- the colony list's dark interior and grey frame,
    // in the theme's palette.
    struct List_Style {
        uint8_t interior = 0;
        uint8_t border = 0;
        uint8_t separator = 0;
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
        // The window's band spacing and the list's background
        // colours (see Spacing / List_Style above).
        virtual ui_kit::Spacing Spacing() const = 0;
        virtual ui_kit::List_Style List_Style() const = 0;
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
        Colony_Theme(Frame tab_normal, Frame tab_active,
            const s_colors& title_colors);
        const Font& Tab_Font() const override;
        ui_kit::Tab_Frames Tab_Frames() const override;
        ui_kit::Window_Style Window_Style() const override;
        ui_kit::Spacing Spacing() const override;
        ui_kit::List_Style List_Style() const override;
        int16_t Tab_Padding() const override;
        int16_t Tab_Gap() const override;
        int16_t Tab_Height() const override;
        int16_t Tab_Label_Y_Offset() const override;

      private:
        Engine_Font tab_font_;
        Engine_Font title_font_;
        Frame tab_normal_;
        Frame tab_active_;
    };

    // Loads the colony theme's frames (the derived selector-frame
    // sprite, both states) from the screen's allocation base and
    // returns the theme. The screen never names a sprite id and
    // never sees a frame pointer: the theme owns its assets, so a
    // hi-res asset set plugs in here, not in the screens. A frame
    // that fails to load stays null, and the tab bar falls back to
    // the engine-drawn button.
    std::unique_ptr<Colony_Theme> Load_Colony_Theme(
        void* allocation_base, const s_colors& title_colors);

    // A row of tabs: layout (widths from label + padding, gap
    // between tabs) and drawing (frame + centred label). Owns the
    // geometry, so the click fields and the drawing can never drift
    // apart: the screen builds its fields from Tab_Bar::tabs().
    // A tab's label must outlive the bar: Add_Tab keeps the
    // caller's pointer, it does not copy the text.
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

    // Geometry of a colony-style list: panel rect, header rect, row
    // rects, scroll track, description-box top. Derived from the
    // theme's Spacing plus the panel's position, so the screen's
    // fields, drawing and scrolling can never drift apart. Row i
    // occupies [first_row_y + i*pitch, +row_height); the row_gap
    // pixels between rows show the panel background -- the colony's
    // empty space between cells. The panel ends at the last row's
    // bottom edge; the description box starts list_to_info below it.
    class List_Layout {
      public:
        List_Layout(int16_t panel_x, int16_t panel_y, int16_t panel_w,
            int16_t content_right, int16_t header_y, int16_t header_h,
            int rows, const Spacing& spacing);
        int16_t panel_x() const { return panel_x_; }
        int16_t panel_y() const { return panel_y_; }
        int16_t panel_w() const { return panel_w_; }
        int16_t panel_h() const { return panel_h_; }
        int16_t content_right() const { return content_right_; }
        int16_t header_y() const { return header_y_; }
        int16_t header_h() const { return header_h_; }
        int rows() const { return rows_; }
        int16_t first_row_y() const { return first_row_y_; }
        int16_t row_y(int index) const {
            return (int16_t)(first_row_y_ + index * spacing_.Row_Pitch());
        }
        int16_t row_height() const { return spacing_.row_height; }
        int16_t row_pitch() const { return spacing_.Row_Pitch(); }
        // Scroll track: first row top to last row bottom, the span
        // the rows themselves occupy.
        int16_t scroll_track_y() const { return first_row_y_; }
        int16_t scroll_track_h() const { return scroll_track_h_; }
        // Description-box top: list_to_info below the panel bottom.
        int16_t info_y() const { return info_y_; }

      private:
        int16_t panel_x_;
        int16_t panel_y_;
        int16_t panel_w_;
        int16_t panel_h_;
        int16_t content_right_;
        int16_t header_y_;
        int16_t header_h_;
        int rows_;
        int16_t first_row_y_;
        int16_t scroll_track_h_;
        int16_t info_y_;
        Spacing spacing_;
    };

    // List empty space: panel fill and border, plus one frame line
    // at the top and bottom of every row, like the colony's cells --
    // so an empty row still reads as a framed cell, and the row_gap
    // between two frame lines is empty panel background. The last
    // row's bottom line is the panel border itself. Row frame lines
    // stop at content_right (the list edge), short of the scrollbar
    // strip, like the colony list's cells stop short of its bar.
    void Draw_List_Background(const Theme& theme,
        const List_Layout& layout);

    // Full-window frame: thin light outer line, dark inner line,
    // centred title -- the frame baked into the colony screens'
    // backgrounds, engine-drawn in theme colours.
    void Draw_Window_Frame(const Theme& theme, int16_t x, int16_t y,
        int16_t w, int16_t h, const char* title);
} // namespace ui_kit
