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
#include <map>
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
        // spacing overrides the font's own inter-glyph spacing
        // (-1 keeps it): the baked RETURN text measures as style 2
        // with 1 px spacing, not the font's own 2 px.
        Engine_Font(int16_t style, const s_colors& normal_colors,
            const s_colors& active_colors, int16_t spacing = -1);
        int16_t Width(const char* text) const override;
        int16_t Height() const override;
        void Print(int16_t x, int16_t y, const char* text,
            bool active) const override;

      private:
        int16_t style_;
        int16_t spacing_;
        s_colors normal_colors_;
        s_colors active_colors_;
    };

    // The bitmap-font implementation of Font: glyphs drawn by
    // observation from the baked button text, bundled as JSON
    // (data/base/fonts/Moo2Button.json -- see its provenance note
    // and tools/build_moo2button_font.py). One file holds both
    // states: normal (9 rows, dark core + AA) and pressed (11 rows,
    // orange core 221 with 252 fill and 112 shadow), so the two
    // button fonts are two Bitmap_Fonts over the same file, one per
    // state. Loading is soft, like the sprite frames: a missing or
    // malformed file leaves the font unloaded and every call falls
    // back to the engine font, never a blank label. The active
    // (blue-face) state has no baked reference, so it always falls
    // back too.
    class Bitmap_Font : public Font {
      public:
        enum class State { Normal, Pressed };

        Bitmap_Font(State state, const Engine_Font& fallback);
        // Loads fonts/<name>.json from the VFS (base.zip). Returns
        // false when the file is missing or unreadable, leaving
        // any previously loaded font in place; a file that opens
        // but fails to parse unloads the font (see Load_From_Text).
        bool Load(const char* name);
        // Test seam: parses the JSON text directly. Parses into a
        // fresh font and swaps it in only on success; on failure
        // the font is unloaded and false is returned.
        bool Load_From_Text(const char* text, size_t size);
        [[nodiscard]] bool Is_Loaded() const;
        int16_t Width(const char* text) const override;
        int16_t Height() const override;
        void Print(int16_t x, int16_t y, const char* text,
            bool active) const override;

      private:
        struct Glyph {
            int16_t advance = 0;
            int16_t width = 0;
            // Per state: rows of palette indices, -1 = transparent
            // (null in the JSON). Normal rows == cap_height_;
            // pressed rows == cap_height_ + descender_rows_. A
            // pressed row may be wider than width (E's top bar
            // reaches one column into the next cell); the row's own
            // length is that state's width.
            std::vector<std::vector<int16_t>> normal;
            std::vector<std::vector<int16_t>> pressed;
        };

        bool Renderable(const char* text) const;
        const std::vector<std::vector<int16_t>>* Rows(
            const Glyph& glyph) const;

        State state_;
        const Engine_Font& fallback_;
        bool loaded_ = false;
        int16_t cap_height_ = 0;
        int16_t descender_rows_ = 0;
        int16_t space_advance_ = 0;
        std::map<char, Glyph> glyphs_;
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
        // Native size of the frame (0 when null): the screen sizes
        // an image button from its frame, like the colony sizes its
        // RETURN field from anims[1] (85x26).
        [[nodiscard]] int16_t Width() const;
        [[nodiscard]] int16_t Height() const;
        // Draws the frame at its native size at (x, y).
        void Draw(int16_t x, int16_t y) const;
        void Draw_Stretched(int16_t x, int16_t y, int16_t width,
            int16_t left_slice, int16_t right_slice) const;
        void Draw_Stretched_Resampled(int16_t x, int16_t y, int16_t width,
            int16_t left_slice, int16_t right_slice) const;
        // Draws the frame as three slices: the left_slice columns
        // as-is, a single middle_column tiled across the remaining
        // width, the right_slice columns as-is. The middle column
        // is a text-free column of the frame's face, so one frame
        // serves any button width with the label rendered at
        // runtime.
        void Draw_Three_Slice(int16_t x, int16_t y, int16_t width,
            int16_t left_slice, int16_t middle_column,
            int16_t right_slice) const;
        // Draws a sub-rectangle of the frame at (dst_x, dst_y): the
        // (src_x, src_y, width, height) region of its pixels.
        // Nothing when the frame is null or the region is outside it.
        void Draw_Region(int16_t src_x, int16_t src_y, int16_t width,
            int16_t height, int16_t dst_x, int16_t dst_y) const;

      private:
        const s_animation_header* frame_ = nullptr;
    };

    // One tab frame: the derived sprite plus how its middle is
    // treated when the tab is wider than the frame. Both states
    // three-slice on one text-free face column (middle_column,
    // like a metal button): the colony's sort frames carry fixed
    // bright ramps on each side and a flat middle, so tiling one
    // face column keeps the ramps 1:1 at any width. resample_middle
    // is the generic alternative for a frame whose middle is a
    // gradient that must span the width once, not tile (the colony
    // theme no longer uses it: the selected sort frame's field
    // turned out to be flat in the middle once the ramps are fixed
    // slices, not a gradient). middle_column < 0 means the theme
    // names no single face column: the whole middle tiles, as
    // before.
    struct Tab_Frame {
        Frame frame;
        bool resample_middle = false;
        int16_t middle_column = -1;
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

    // Window frame: the bevelled metal border that contains a
    // colony-style window, measured across the game's full-window
    // screens (colony summary COLSUM, colony build COLBLDG, race
    // relations RACES, leaders OFFICER, score) and its floating
    // dialogs. The band is 9-12 px of bevelled metal on every side;
    // the content starts ~11 px inside the outer edge; the title
    // (or tab bar) sits in the top band's zone; the bottom bar is
    // 24 px tall ending ~10 px above the bottom edge. The starmap
    // and main menu use a different, heavier pipe frame -- a second
    // Theme supplies that family's style through this same struct;
    // nothing here names a screen. The bevel profile is one palette
    // index per pixel of band, outside -> inside: the outer edge,
    // the bright ridge, the mid metal, the groove, the inner light
    // lip, the inner edge. A side thicker than the profile repeats
    // its last entry; a side thinner keeps the profile's outer run
    // and its last entry (the inner edge), so the inner edge is
    // never lost.
    struct Frame_Style {
        int16_t thickness_left = 0;
        int16_t thickness_right = 0;
        int16_t thickness_top = 0;
        int16_t thickness_bottom = 0;
        int16_t inset_left = 0; // outer edge -> content, per side
        int16_t inset_right = 0;
        int16_t inset_top = 0;
        int16_t inset_bottom = 0;
        std::vector<uint8_t> profile; // outside -> inside
        const Font* title_font = nullptr; // non-owning; null = no title
        int16_t title_y = 0; // title print y relative to window top
        int16_t bottom_bar_h = 0; // the bottom bar's height
    };

    // Spacing: the empty space between and around the bands of a
    // colony-style window -- list rows, description box, bottom bar.
    // Measured from the colony summary screen (colsum.cpp and the
    // baked COLSUM background, 640x480 game pixels): rows are 30 px
    // cells on a 31 px pitch, so 1 px of empty background shows
    // between adjacent rows; the header plate's bottom edge sits
    // 3 px above the first row; the panel ends 4 px below the last
    // row (a shade band and a bevel) and the description box starts
    // 4 px below the panel and ends 7 px above the bottom bar.
    struct Spacing {
        int16_t row_height = 30;         // one list row's cell height
        int16_t row_gap = 1;             // empty background between rows
        int16_t header_to_first_row = 3; // header bottom -> first row top
        int16_t list_footing = 4;        // panel below the last row: shade band + bevel
        int16_t list_to_info = 4;        // panel bottom -> info box top
        int16_t info_to_bar = 7;         // info box bottom -> bottom bar top
        int16_t Row_Pitch() const {
            return (int16_t)(row_height + row_gap);
        }
    };

    // List line colours: the colony list's framed cells, in the
    // theme's palette. Measured from the colony summary screen
    // (the baked COLSUM background, 640x480 game pixels, rendered
    // with COLSUM.LBX entry 0's embedded palette): each 30 px row
    // is a raised cell -- a light line at its top and bottom
    // edges, a shade line just inside each, and a shade line just
    // below its bottom edge (the cell's drop shadow) -- and the
    // columns are divided by 3 px vertical grooves, a mid line
    // with darker flanks. Colony greys, against its interior (20):
    // frame light 112/120 (dithered, avg 116), shade 60/68 (avg
    // 64), divider 44/68/36. The settings screen renders in the
    // menu palette, whose list interior is brighter (36, index
    // 230), so the values below are the colony's steps ABOVE its
    // interior, re-anchored above the menu interior and taken to
    // the nearest menu index (the same nearest-RGB remap
    // tools/extract_sprite.py uses): light +96 -> 241, shade
    // +44 -> 235, divider +24/+48/+16 -> 233/235/232. All in the
    // menu's blue-grey ramp (230-249), the family the panel, the
    // window frame and the derived tab bevels already use. The
    // dark row separator (229) the settings rows used before is
    // retired: on the dark panel (230) it read as a subtle dark
    // divider, the inverse of the colony's light bevel lines.
    struct List_Style {
        uint8_t interior = 0;
        uint8_t border = 0;
        uint8_t frame_light = 0;
        uint8_t frame_shade = 0;
        uint8_t divider_left = 0;
        uint8_t divider_mid = 0;
        uint8_t divider_right = 0;
        // Header plate face: the colony's plates are raised, their
        // face lighter than the list interior (colony face ~48
        // against its interior 20, the same delta remap as the
        // dividers above).
        uint8_t plate_face = 0;
    };

    // One button's state, in the colony's own terms. The colony's
    // buttons are two-frame sprites (fields.cpp Draw_Visible_Fields
    // and Draw_Field: a held button draws frame 1, a selected
    // multi-button draws frame 1): rest is frame 0, active and
    // pressed are both frame 1 -- active is the latched selection
    // (the sort column, the blue selector), pressed the momentary
    // hold (the RETURN's orange text). A button the colony never
    // disables has no disabled frame: it renders as rest, and a
    // disabled metal button is the flat engine-drawn form below.
    enum class Button_State {
        Rest,
        Pressed,
        Active,
        Disabled,
    };

    // What a button is, visually. The colony has two button
    // families, and the settings screen's value cells are a third
    // use of the same click-a-cell idea:
    // - Selector: a derived COLSUM sort-button frame (Tab_Frames),
    //   cut into three slices at the button width -- the bevel as-is,
    //   one text-free face column tiled across a flat middle --
    //   label rendered at runtime.
    // - Metal: a RETURN cut into three slices -- the derived
    //   frame's domes as-is, one text-free face column tiled
    //   across the middle, the label rendered at runtime -- so
    //   one frame serves any metal button at any width, and the
    //   button's chrome is pixel-identical to the RETURN's. Blue,
    //   disabled and missing-frame buttons keep the engine-drawn
    //   form (the colony scrollbar thumb's bevel, colsum.cpp
    //   Draw_Bar_Indicator: body 229, light 230 top/left, dark
    //   228 bottom/right, in the theme's colours): there is no
    //   blue or disabled RETURN frame to cut. A metal button is
    //   sized from its frame's native height, like an Image
    //   button; the label centres on the frame, not the rect.
    // - Image: a derived sprite drawn at native size, rest and
    //   pressed frames (the colony RETURN, entry 1: grey bevel and
    //   dark text at rest, orange text pressed), falling back to
    //   Metal with the same label when the frames are missing.
    // - Toggle: a colony-list square value cell, bright when on,
    //   grey when off, bare when disabled (the colony's no-button
    //   state: a colony that cannot buy gets no live button, only
    //   the dim disc baked into its background).
    // - Cell: a rectangular value cell with a down chevron, the
    //   dropdown trigger; active while its popup is open (blue,
    //   like the active selector), chevronless when disabled.
    enum class Button_Kind {
        Metal,
        Selector,
        Image,
        Toggle,
        Cell,
    };

    // Button colours and label fonts, in the theme's palette. The
    // metal face/highlight/shadow are the derived RETURN's greys
    // (238/243/232 -- its modal face grey, its brightest grey, its
    // dark edge), which the scrollbar thumb shares: the colony
    // thumb itself is blue (229/230/228 in the colony palette),
    // and the menu palette has no close blue, so the kit's metal
    // is grey by design -- a colony-palette theme would supply
    // the blue instead. The corner dots are the colony's index 80
    // (black in the colony palette; the menu remap is 0), single
    // pixels at the scroll track's ends. The primary (blue) face
    // is the active selector's blue
    // (138, highlight 154), the same blue the colony's "No Enemy
    // Presence" selector and the settings' Show more already use.
    // The toggle fills are the settings squares' own colours (on:
    // 154 base, 247 centre; off: 236 base, 246 centre), and the
    // cell is the list panel's own border/interior (234/230).
    struct Button_Style {
        uint8_t face = 0;
        uint8_t highlight = 0;
        uint8_t shadow = 0;
        uint8_t primary_face = 0;
        uint8_t primary_highlight = 0;
        uint8_t disabled_face = 0;
        uint8_t cell_interior = 0;
        uint8_t cell_border = 0;
        uint8_t toggle_on_base = 0;
        uint8_t toggle_on_bright = 0;
        uint8_t toggle_off_base = 0;
        uint8_t toggle_off_bright = 0;
        // Scrollbar corner dots (see Scroll_Bar): the colony's
        // index 80, black in the colony palette, remapped to 0.
        uint8_t scroll_corner = 0;
        // Metal labels: dark text on the metal face, light text on
        // the blue face. Value-cell labels: the colony cyan row
        // text at rest, light text on the blue active fill. Both
        // are style 4 (the screen's own text style); the tab font
        // stays style 2. Non-owning; null = no label.
        const Font* label_font = nullptr;
        // Pressed metal label: the orange the real RETURN lights
        // up with when held (menu palette 221, the pressed
        // frame's own orange). A pressed metal button prints its
        // label in this font; null falls back to label_font.
        const Font* label_pressed_font = nullptr;
        const Font* cell_font = nullptr;
    };

    // The RETURN button's derived frames: rest and pressed (orange
    // text). Both null falls back to the engine-drawn metal
    // button; a missing pressed frame falls back to the rest
    // frame, like the sprite's own single-frame case.
    struct Image_Button_Frames {
        Frame normal;
        Frame pressed;
    };

    // The colony BUILDING toggle's derived frames: off is the grey
    // toggle (frame 0), on the blue toggle with its white starburst
    // (frame 1). Both null falls back to the engine-drawn toggle;
    // a missing on frame falls back to the off frame, like the
    // sprite's own single-frame case.
    struct Toggle_Frames {
        Frame off;
        Frame on;
    };

    // A theme is everything about a window's look that varies with
    // the art set: fonts, tab frames, window chrome, tab metrics.
    // Screens take a const Theme&; a hi-res asset set is a second
    // Theme implementation.
    class Theme {
      public:
        virtual ~Theme() = default;
        virtual const Font& Tab_Font() const = 0;
        // The header-plate label font: the colony's large display
        // font (Moo2Display, cap 8), for the header plates' labels
        // (NAME, FARMERS, ... on the colony screen; SETTING, VALUE
        // on the settings screen). The baked labels are ~14 px per
        // character; no engine font renders them at the right width.
        virtual const Font& Header_Font() const = 0;
        // Qualified: the member function name would otherwise hide
        // the same-named struct in the return type.
        virtual ui_kit::Tab_Frames Tab_Frames() const = 0;
        // Per-tab frames: the colony's sort buttons are per-tab
        // frames (Name, Population, Food ...), each with its own
        // bevel and ramp widths, so a tab draws its own tab's
        // frames, not the shared first tab's. The default is the
        // shared frames for every tab, so a theme with one frame
        // pair needs no override; a tab's index is its position in
        // the bar, set by the bar (see Button::Set_Tab_Index).
        virtual ui_kit::Tab_Frames Tab_Frames_For_Tab(
            int16_t tab_index) const {
            (void)tab_index;
            return Tab_Frames();
        }
        // The window-band background the gaps between adjacent tabs
        // show: one palette index per tab row, top -> bottom. The
        // colony's sort strip is one continuous band, so the gaps
        // show the band's recessed channel, not black. Empty means
        // the theme names no band: the bar draws no gap fill. The
        // profile is per gap: the band's top row follows the
        // strip-wide arch, so it differs per gap.
        virtual std::vector<uint8_t> Tab_Gap_Profile(
            int16_t gap_index) const {
            (void)gap_index;
            return {};
        }
        // Qualified: the member function name would otherwise hide
        // the same-named struct in the return type.
        virtual ui_kit::Frame_Style Frame_Style() const = 0;
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
        // The button colours and label fonts (see Button_Style).
        virtual ui_kit::Button_Style Button_Style() const = 0;
        // The RETURN button's derived frames (see
        // Image_Button_Frames).
        virtual ui_kit::Image_Button_Frames Return_Frames() const = 0;
        // The colony BUILDING toggle's derived frames (see
        // Toggle_Frames). Both null when the theme carries none:
        // the toggle falls back to the engine-drawn one.
        virtual ui_kit::Toggle_Frames Toggle_Frames() const {
            // Qualified: the member function name hides the
            // same-named struct, so an unqualified Toggle_Frames()
            // would recurse into this method.
            return ui_kit::Toggle_Frames();
        }
        // The bottom band's label font: the colony's SORT label font
        // (style 2, grey), for the label at the start of the band
        // (SETTINGS on the settings screen, SORT on the colony
        // screen). Defaults to the tab font; a theme that carries
        // the grey label font overrides it.
        virtual const Font& Band_Label_Font() const { return Tab_Font(); }
        // The colony screen's baked background (640x480), for
        // screens that render on it instead of the kit's drawn
        // frame: the colony background is the base, the screen's
        // content draws on top of it. Null when the theme carries
        // no background (the screen falls back to the kit frame).
        // The colony theme loads it from the game data at run time
        // (sprite_art::Load_Colony_Background); other themes
        // return null.
        virtual ui_kit::Frame Background() const {
            return Frame();
        }
    };

    // The colony look: derived COLSUM sort-button frames, font
    // style 2 tab labels (the baked colony labels measure as
    // style 2), engine-drawn window chrome in the settings colours.
    // The title font keeps the screen's own title colours, passed
    // in; the tab label ramps are the theme's own, measured from the
    // baked labels (menu palette): inactive is a white face with a
    // dark-grey edge, active a blue-white face with a mid-blue edge.
    // The sort frames are per tab: the settings screen has three
    // categories, so the theme carries the first three colony sort
    // frames (Name, Population, Food), grabbed from the rendered
    // colony screen -- see tools/sprites/README.md. Tab_Frames()
    // returns the first tab's frames, for a Selector with no tab
    // index; the bar hands each tab its own frames through
    // Tab_Frames_For_Tab.
    class Colony_Theme : public Theme {
      public:
        Colony_Theme(Frame name_normal, Frame name_active,
            Frame population_normal, Frame population_active,
            Frame food_normal, Frame food_active,
            Frame return_normal, Frame return_pressed,
            const s_colors& title_colors,
            Frame background = Frame(),
            Frame toggle_off = Frame(), Frame toggle_on = Frame());
        const Font& Tab_Font() const override;
        const Font& Header_Font() const override;
        const Font& Band_Label_Font() const override;
        ui_kit::Tab_Frames Tab_Frames() const override;
        ui_kit::Tab_Frames Tab_Frames_For_Tab(
            int16_t tab_index) const override;
        std::vector<uint8_t> Tab_Gap_Profile(
            int16_t gap_index) const override;
        ui_kit::Frame_Style Frame_Style() const override;
        ui_kit::Spacing Spacing() const override;
        ui_kit::List_Style List_Style() const override;
        ui_kit::Button_Style Button_Style() const override;
        ui_kit::Image_Button_Frames Return_Frames() const override;
        ui_kit::Toggle_Frames Toggle_Frames() const override;
        ui_kit::Frame Background() const override;
        int16_t Tab_Padding() const override;
        int16_t Tab_Gap() const override;
        int16_t Tab_Height() const override;
        int16_t Tab_Label_Y_Offset() const override;

      private:
        Engine_Font tab_font_;
        Engine_Font title_font_;
        Engine_Font button_font_;
        Engine_Font button_pressed_font_;
        // The button-label bitmap fonts over the same file: normal
        // glyphs for the rest label, pressed glyphs for the held
        // label. Loaded in the constructor body; when the file is
        // missing or malformed they stay unloaded and the engine
        // fonts above render instead, so the theme never ships a
        // blank label. Declared after the fallbacks they borrow.
        Bitmap_Font button_bitmap_font_;
        Bitmap_Font button_pressed_bitmap_font_;
        // The header-plate label font over Moo2Display.json: the
        // colony's large display font for the header plates. Loaded
        // in the constructor body; when the file is missing or
        // malformed it stays unloaded and the title font renders
        // instead, so the theme never ships a blank label. Declared
        // after the fallback it borrows.
        Bitmap_Font header_font_;
        Engine_Font cell_font_;
        // The three sort frames, by tab index: 0 Name, 1 Population,
        // 2 Food. Tab_Frames_For_Tab clamps an out-of-range index to
        // the last tab, so a bar with more tabs than frames still
        // draws.
        Frame tab_normal_[3];
        Frame tab_active_[3];
        Frame return_normal_;
        Frame return_pressed_;
        Frame toggle_off_;
        Frame toggle_on_;
        // The bottom band's label font (style 2, grey): the colony's
        // SORT label renders in the tab font's grey, not its white.
        Engine_Font band_label_font_;
        // The colony screen's baked background (COLSUM.LBX entry 0,
        // remapped to the menu palette at run time). Null when it
        // failed to load; the screen falls back to the kit frame.
        Frame background_;
    };

    // Loads the colony theme's frames (the three derived sort-tab
    // sprites, both states, and the derived RETURN sprite, rest and
    // pressed) from the screen's allocation base and returns the
    // theme. The screen never names a sprite id and never sees a
    // frame pointer: the theme owns its assets, so a hi-res asset
    // set plugs in here, not in the screens. A frame that fails to
    // load stays null, and the tab bar falls back to the
    // engine-drawn button.
    std::unique_ptr<Colony_Theme> Load_Colony_Theme(
        void* allocation_base, const s_colors& title_colors);

    // One button: a rect, a kind, a state, an optional label, drawn
    // by the theme. The screen builds one per clickable button and
    // draws it; the field rects come from the same geometry, so the
    // click fields and the drawing can never drift apart. A label
    // must outlive the button: Set_Label keeps the caller's
    // pointer, it does not copy the text. Metal labels are centred
    // vertically by the field system's own rule (fields.cpp
    // Draw_Visible_Fields: cy = y + (y_end - y)/2 -
    // (font_height-1)/2); selector labels use the theme's
    // Tab_Label_Y_Offset, like the colony's baked labels (ink rows
    // 8-16 of the 24 px frame). A cell's label sits on the row's
    // text baseline instead: the colony settings screen prints a
    // row's value text on the same line as its title, so the
    // screen sets the label's offset from the cell top with
    // Set_Label_Y_Offset (trigger_y+4 here: the row text is y+9,
    // the cell starts at y+5); without it the cell centres like a
    // metal button. Primary is the blue face: a primary action, or
    // a latched active button. Pressed inverts the metal bevel;
    // active is the latched selection.
    class Button {
      public:
        Button(int16_t x, int16_t y, int16_t w, int16_t h,
            Button_Kind kind, const Theme& theme);
        void Set_Label(const char* label);
        void Set_Label_Y_Offset(int16_t dy);
        void Set_State(Button_State state);
        void Set_Primary(bool primary);
        // The tab's position in its bar, for a Selector: the theme
        // hands a Selector its own tab's frames through
        // Tab_Frames_For_Tab. -1 (the default) means no tab: the
        // shared Tab_Frames(). Only the bar sets it.
        void Set_Tab_Index(int16_t tab_index);
        void Draw() const;

      private:
        void Draw_Metal() const;
        void Draw_Selector() const;
        void Draw_Image() const;
        void Draw_Toggle() const;
        void Draw_Cell() const;
        // Centres the label on height (the frame's height for a
        // frame-drawn button, the rect height for an engine-drawn
        // one), by the field system's own rule.
        void Print_Centred_Label(const Font& font, bool active,
            int16_t height) const;

        int16_t x_;
        int16_t y_;
        int16_t width_;
        int16_t height_;
        Button_Kind kind_;
        const Theme& theme_;
        const char* label_ = nullptr;
        int16_t label_y_offset_ = 0;
        bool label_y_offset_set_ = false;
        Button_State state_ = Button_State::Rest;
        bool primary_ = false;
        int16_t tab_index_ = -1;
    };

    // A row of tabs: one continuous strip filling [x, right),
    // layout (widths in proportion to label + padding, gap between
    // tabs) and drawing, one Selector Button per tab, so the tabs
    // are kit buttons with the same frame, text and pressed/active
    // states as every other button. Owns the geometry, so the click
    // fields and the drawing can never drift apart: the screen
    // builds its fields from Tab_Bar::tabs(). A tab's label must
    // outlive the bar: Add_Tab keeps the caller's pointer, it does
    // not copy the text.
    class Tab_Bar {
      public:
        struct Tab {
            const char* label = nullptr;
            bool active = false;
            bool pressed = false;
            int16_t x = 0;
            int16_t width = 0;
            int16_t natural_width = 0; // label + padding, pre-fill
        };

        Tab_Bar(int16_t x, int16_t y, int16_t right, const Theme& theme);
        // The label at the start of the band (SETTINGS on the
        // settings screen, like SORT on the colony's sort band):
        // drawn in the theme's band-label font at the bar's x, and
        // the tabs lay out after it. Null (the default) means no
        // label: the tabs fill the whole strip, as before.
        void Set_Label(const char* label);
        void Add_Tab(const char* label, bool active,
            bool pressed = false);
        // A fixed-size tab: the colony's sort buttons are
        // fixed-size sprites (Name 50, Population 78, Food 42,
        // Industry 63, Science 66, Producing 86, BC 50), so the
        // bar places them at their own widths instead of dividing
        // the strip by label widths -- the last of them, BC, is
        // wider than its label plus padding, and the proportional
        // fill would shrink it and drift every tab after Name.
        // The widths plus the gaps must fill [x, right), like the
        // colony's strip (89..530); otherwise the bar still fills
        // the strip proportionally from these widths.
        void Add_Fixed_Tab(const char* label, int16_t width,
            bool active, bool pressed = false);
        const std::vector<Tab>& tabs() const;
        int16_t y() const;
        int16_t height() const;
        int16_t End_X() const; // x past the last tab
        void Draw() const;

      private:
        void Relayout();

        int16_t x_;
        int16_t y_;
        int16_t right_; // exclusive end of the strip
        const Theme& theme_;
        std::vector<Tab> tabs_;
        const char* label_ = nullptr;
        // The label's width plus the gap to the first tab; 0 when
        // there is no label.
        int16_t label_width_ = 0;
    };

    // A window's frame: the border that contains everything, drawn
    // by the theme. The frame positions the contents: the grid, the
    // panels and the bottom bar are derived from its content rect,
    // never from hard-coded screen constants. A screen builds one
    // for its window rect and asks it where things go; a dialog is
    // the same component at a smaller rect. The bands are mitred:
    // each band line stops one pixel further in per offset, so the
    // corners meet on a 45-degree seam, like the baked frames'.
    class Window_Frame {
      public:
        Window_Frame(int16_t x, int16_t y, int16_t w, int16_t h,
            const Theme& theme);
        int16_t x() const { return x_; }
        int16_t y() const { return y_; }
        int16_t w() const { return w_; }
        int16_t h() const { return h_; }
        int16_t content_x() const;
        int16_t content_y() const;
        int16_t content_w() const;
        int16_t content_h() const;
        // Exclusive right/bottom edges of the content rect.
        int16_t content_right() const;
        int16_t content_bottom() const;
        // y for a bottom bar of height bar_h: its bottom sits on
        // the content bottom, so the frame's bottom band shows
        // below it.
        int16_t bottom_bar_y(int16_t bar_h) const;
        // Title print y, absolute.
        int16_t title_y() const;
        void Draw(const char* title) const;

      private:
        int16_t x_;
        int16_t y_;
        int16_t w_;
        int16_t h_;
        const Theme& theme_;
    };

    // Geometry of a colony-style list: panel rect, header rect, row
    // rects, scroll track, description-box top. Derived from the
    // theme's Spacing plus the panel's position, so the screen's
    // fields, drawing and scrolling can never drift apart. Row i
    // occupies [first_row_y + i*pitch, +row_height); the row_gap
    // pixels between rows show the panel background -- the colony's
    // empty space between cells. The panel ends list_footing below
    // the last row's bottom edge; the description box starts
    // list_to_info below the panel.
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

    // One column of a list: the x-range its cells occupy. The
    // settings list's columns are its header plates (SETTING /
    // VALUE); the colony list's are the column interiors (NAME
    // 11..97, FARMERS 101..232, WORKERS 236..374, SCIENTISTS
    // 378..508, BUILDING 512..598).
    struct Column {
        int16_t x = 0;
        int16_t width = 0;
    };

    // Column geometry for a list: the vertical dividers between
    // adjacent columns, derived from the column rects. The divider
    // between two columns is a 3 px band starting right after the
    // left column's right edge, like the colony's (NAME ends at
    // 97, the divider runs 98..100, FARMERS starts at 101); any
    // leftover gap stays panel interior. The dividers run the
    // rows' height only, from the first row top to the last row
    // bottom -- the header plates carry their own frames above
    // them.
    class Column_Layout {
      public:
        Column_Layout(const std::vector<Column>& columns, int16_t top,
            int16_t bottom);
        const std::vector<Column>& columns() const;
        int16_t top() const { return top_; }
        int16_t bottom() const { return bottom_; }
        int divider_count() const {
            return columns_.empty() ? 0 : (int)columns_.size() - 1;
        }
        // Left x of the divider between column index and index+1.
        int16_t divider_x(int index) const;

      private:
        std::vector<Column> columns_;
        int16_t top_;
        int16_t bottom_;
    };

    // A colony-list scrollbar: the drag thumb in its track, like
    // COLSUM's Draw_Bar_Indicator (colsum.cpp) -- a filled bar
    // with a bright top/left edge and a dark bottom/right edge,
    // and single dots marking the track ends. The thumb spans the
    // fraction of the list that is visible: y1 = track_y +
    // track_h * top / total, y2 = track_y + track_h *
    // (top + visible) / total, colsum's own formula, with no
    // minimum height (the thumb can be tiny with many rows). The
    // track is the caller's: the colony's is y 40..311 (271, the
    // strip between the scroll arrows at y 15..34 and y 316..337),
    // the settings screen's is the layout's scroll track (first
    // row top to last row bottom), so the fields, the drawing and
    // the scrolling never drift apart per screen. The scroll
    // model is the colony's inverted slider (COLSUM::Update_First):
    // slider at max shows the first rows, slider at 0 the last.
    // Colours come from the theme's Button_Style (the metal face /
    // highlight / shadow and the corner dots); the track itself
    // has no background -- the panel shows through, like the
    // colony's black strip.
    class Scroll_Bar {
      public:
        // x / width: the strip the thumb fills (the colony's is
        // 621..626, 6 wide; the settings screen's 621..625, 5
        // wide). track_y / track_h: the track's endpoint span.
        Scroll_Bar(int16_t x, int16_t width, int16_t track_y,
            int16_t track_h, const Theme& theme);
        // True while the list is at least as long as the window,
        // colsum's rule (num_colonies >= 10): at exactly visible
        // the thumb spans the whole track.
        static bool Visible(int total, int visible);
        // Maximum first-visible-row index: 0 when everything
        // fits, otherwise the top row of the last window.
        static int Max_Scroll(int total, int visible);
        // The inverted slider for a scroll top, and the scroll
        // top for a slider (clamped into range). Round-trips:
        // Top_From_Slider(Slider_From_Top(t)) == t.
        static int Slider_From_Top(int top, int total, int visible);
        static int Top_From_Slider(int slider, int total,
            int visible);
        // Thumb endpoints for a scroll top, colsum's formula.
        int16_t Thumb_Top(int top, int total, int visible) const;
        int16_t Thumb_Bottom(int top, int total, int visible) const;
        int16_t track_y() const { return track_y_; }
        // Track bottom endpoint (track_y + track_h): the last
        // pixel the thumb can reach, and the bottom dots' row.
        int16_t track_bottom() const {
            return (int16_t)(track_y_ + track_h_);
        }
        // Draws the thumb and the corner dots; nothing when the
        // list fits (see Visible).
        void Draw(int top, int total, int visible) const;
        // The scroll arrows at the track's ends, like COLSUM's
        // (anims[2]/[3]): the up-arrow above the track, the
        // down-arrow below it. The arrows are code-drawn triangles
        // in the theme's colours, not the baked sprites: the
        // sprites' pressed frames carry a vivid colony blue with
        // no close hue in the menu palette, and the baked arrows
        // leave wrong pixels against a kit-drawn strip. The
        // up-arrow sits 25 px above the track (y 15..34 for the
        // colony's track), the down-arrow 5 px below its bottom
        // (y 316..337); both are 10 px wide, starting 2 px left
        // of the strip. The arrows always draw, even when the
        // list fits: the colony shows both arrows with a single
        // colony.
        void Draw_Arrows() const;

      private:
        int16_t x_;
        int16_t width_;
        int16_t track_y_;
        int16_t track_h_;
        const Theme& theme_;
    };

    // List empty space: panel fill and border, plus the framed row
    // slots, like the colony's cells -- so an empty row still
    // reads as a framed cell, and the row_gap between two frame
    // lines is empty panel background. Each row gets a light line
    // at its top and bottom edges, a shade line just inside each,
    // and a shade line just below its bottom edge; the last row's
    // trailing shade is the panel border itself. Row frame lines
    // stop at content_right (the list edge), short of the scrollbar
    // strip, like the colony list's cells stop short of its bar.
    // fill_interior false skips the panel's flat interior fill:
    // a screen rendering on its baked background leaves the
    // background's own panel texture showing and draws only the
    // border and the row frame lines on top of it. True (the
    // default) fills the panel, as before. draw_row_frames false
    // skips the per-row frame lines: the rows are text lines on
    // the panel, not framed cells -- the settings screen renders
    // its rows that way, and paints the background's own baked
    // grid out itself. True (the default) frames every row.
    void Draw_List_Background(const Theme& theme,
        const List_Layout& layout, bool fill_interior = true,
        bool draw_row_frames = true);

    // Column dividers over a list: the 3 px vertical grooves
    // between adjacent columns, from the first row top to the last
    // row bottom. Drawn after the background, so the dividers
    // cross the row frame lines the way the colony's grooves cut
    // through its horizontal lines.
    void Draw_List_Columns(const Theme& theme,
        const Column_Layout& columns);

} // namespace ui_kit
