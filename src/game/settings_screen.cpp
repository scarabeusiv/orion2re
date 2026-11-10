#include "pch.h"
#include "string_overrides.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "config/config_loader.h"
#include "config/config_runtime.h"
#include "config/config_writer.h"
#include "config/mod_catalogue.h"
#include "config/settings_value.h"
#include "game/fields.h"
#include "game/fonts.h"
#include "game/graphics.h"
#include "game/mox2.h"
#include "game/mouse.h"
#include "game/palette.h"
#include "game/platform.h"
#include "game/russ.h"
#include "game/settings_screen.h"
#include "game/timer.h"
#include "game/ui_kit.h"
#include "game/user_paths.h"
#include "game/vfs.h"
#include "game/video.h"

namespace SETTINGS_SCREEN {

    namespace {

        // Full-window layout, mapped to the colony screens
        // (COLBLDG.LBX entry 0, colbldg.cpp; COLSUM.LBX, colsum.cpp):
        // the screen fills the whole 640x480 window. The category
        // selector bar sits at the bottom, like the colony summary's
        // sort buttons (Name / Population / Food ...), with MORE
        // and RETURN beside it on the same line. The list fills the
        // main panel above it, with SETTING / VALUE header plates
        // like the colony list's header boxes, and the description
        // box spans the whole width between the list and the bottom
        // bar, like the colony screen's bottom info area. The colony
        // screens' panels are baked into their 640x480 background
        // art, so the frame here is the kit's Window_Frame: the
        // bevelled metal band baked into the colony screens'
        // backgrounds, 9-12 px per side, drawn in theme colours,
        // with thin grey panel borders and framed row slots inside.
        //
        // Asset sourcing: the colony sprites were audited against
        // COLSUM.LBX (rendered and measured, 2026-11-09), and the
        // separable ones are derived into orion2re-owned frames by
        // tools/extract_sprite.py (see tools/sprites/README.md): a
        // palette remap from the colony palette to the menu palette,
        // with the baked label blanked where a new label is needed.
        // Everything else stays engine-drawn Fill/Box in
        // menu-palette indices, the same technique the game itself
        // uses for its chrome:
        // - Border, list panel, header plates, description box,
        //   scrollbar track/thumb: baked into the 640x480
        //   background (anims[0]); colsum.cpp draws no boxes
        //   itself, so there is no separable sprite for any of them.
        //   The toggle squares and dropdown cells are kit buttons
        //   instead (game/ui_kit.h: Button, Toggle/Cell kinds):
        //   the colony list's value cell, engine-drawn in the
        //   panel's own border/interior colours.
        // - Category selectors: COLSUM anims[4..10] are the sort
        //   tabs' frames (Name, Population, Food, ...), but their
        //   labels are baked in and do not match Game Internals /
        //   Gameplay / Mods; the frame without text is not a
        //   separate sprite. The settings screen's three tabs use
        //   the first three sort frames, grabbed from the rendered
        //   colony screen and label-blanked (tools/sprites/
        //   derive_sort_tabs.py; the screen grab, not an LBX
        //   extraction, so the frames are the colony's own pixels).
        //   Each tab draws its own frame in both states, cut into
        //   three slices at the tab width -- fixed ~20 px ramps on
        //   each side, one flat face column tiled across the middle
        //   -- with the label rendered at runtime (game/ui_kit.h:
        //   Tab_Bar over the colony theme). The two states are
        //   different frames, not a recolour: frame 0 rests on a
        //   flat dark-navy field with a grey bevel; frame 1 is the
        //   selected tab, a blue field (bright at both edges, dark
        //   navy in the centre) with a blue bevel and a blue-white
        //   label. The gaps between tabs show the window band's
        //   recessed channel, not black, like the colony's sort
        //   strip. The labels are font style 2 (the colony tabs'
        //   baked labels measure as style 2, not the style 4 the
        //   rest of this screen uses), white at rest and blue-white
        //   when active, and the tabs sit 1 px apart at y=446,
        //   like the colony's sort buttons. The frames are remapped
        //   from the colony screen's own palette -- the palette
        //   embedded in COLSUM.LBX entry 0, which
        //   animate::Draw_Palette installs before the colony
        //   sprites draw -- to the menu palette; see
        //   tools/sprites/README.md.
        // - Star buy buttons (anims[11..20]) are purchase actions,
        //   not on/off toggles, so they cannot stand in for the
        //   toggle squares (the diamond is a buy button, not a value).
        // - Scrollbar arrows (anims[2]/[3]) are real sprites, but the
        //   scrollbar here is a drag thumb with no arrow buttons, so
        //   adding them would be a design change, not a visual swap.
        // - Dropdown popup: no popup-window sprite exists; the game's
        //   own modal lists are engine-drawn.
        // - RETURN (COLSUM anims[1], 85x26, "RETURN" baked in) is the
        //   one sprite that matches. It is authored for the colony
        //   palette (the embedded palette in COLSUM.LBX entry 0;
        //   indices 80-98 are a grey ramp there) while this screen
        //   uses the menu palette, so drawing it directly renders a
        //   multicoloured button; the derived return asset is the
        //   sprite remapped to the menu palette's grey family (see
        //   tools/sprites/README.md for why the menu palette is
        //   MAINMENU.LBX entry 2's embedded palette, not FONTS.LBX
        //   entry 1). Loading palette 1 for the whole screen is
        //   not an alternative: it turns the settings text
        //   pink/red and the panels blue/green/red. The engine-
        //   drawn metallic RETURN is the fallback when the derived
        //   frames are missing. The sprite's pressed frame (orange
        //   text) is derived too, and the button is a kit Image
        //   button (game/ui_kit.h: Button): while the mouse is held
        //   on RETURN, the field system holds
        //   fields::down_mouse_button on the RETURN field and the
        //   auto function redraws the screen on every hold tick, so
        //   the draw picks the pressed frame until release.
        constexpr int16_t TITLE_X = 19;
        constexpr int16_t TITLE_Y = 8;
        constexpr int16_t TITLE_WIDTH = 348;
        constexpr int16_t VALUE_X = 372;
        constexpr int16_t VALUE_WIDTH = 244;
        // List panel: its x and width come from the window frame's
        // content rect (Make_List_Layout); its y is this screen's
        // (below the title), and its height, first-row y, row pitch
        // and scroll track come from the kit layout
        // (ui_kit::List_Layout), which derives them from the
        // theme's Spacing -- the colony's 30 px rows on a 31 px
        // pitch, header 3 px above the first row. The panel ends
        // list_footing below the last row's bottom edge.
        constexpr int16_t LIST_PANEL_Y = 30;
        // SETTING / VALUE header plates, like the colony list's
        // raised header boxes (NAME, FARMERS, ...), at the top of
        // the list panel, aligned to the name and value columns.
        // The SETTING plate starts at the panel's left edge plus 2
        // and ends 4 px before the VALUE plate, like the colony's
        // plates (its face is lighter than the list interior).
        constexpr int16_t HEADER_Y = 32;
        constexpr int16_t HEADER_HEIGHT = 22;
        constexpr int16_t HEADER_SETTING_X = 13;
        constexpr int16_t HEADER_SETTING_W = 351;
        constexpr int16_t HEADER_VALUE_X = 368;
        constexpr int16_t HEADER_VALUE_W = 248;
        constexpr int ROWS_PER_PAGE = 10;
        constexpr int16_t ROW_TEXT_Y_OFFSET = 8;
        // Colony-list scrollbar, like COLSUM's: a 5-wide vertical
        // bar in its own strip at the right edge of the list panel.
        // The colony's bar is at x=621, 5 wide, in a 271-tall
        // track; here the track spans the list's rows, from the
        // layout. Row fields stop short of the strip, like the colony
        // list's row fields stop short of its scrollbar.
        constexpr int16_t SCROLL_X = 621;
        constexpr int16_t SCROLL_W = 5;
        constexpr int16_t ROW_FIELD_X_END = SCROLL_X - 2;

        // Selector column: every row's selector (toggle square,
        // dropdown cell, value) ends at this right edge, like the
        // colony list's cells all end at their column's right edge.
        // The name stays left at TITLE_X; the selector is right.
        constexpr int16_t SELECTOR_RIGHT_X = 616;
        // Square selector cell size: the colony list's grid cell,
        // a bordered square holding one value.
        constexpr int16_t SQUARE_SIZE = 20;
        // Description box: spans the whole width of the window,
        // like the colony screen's bottom info area — the selected
        // row's description lives here, with a "Requires restart."
        // line for the settings it applies to and the status line
        // below it. No permanent restart legend anywhere else.
        // Its top comes from the kit layout (4 px below the panel,
        // the colony's list-to-info gap) and its bottom sits
        // the theme's info-to-bar gap above the bottom bar, so the
        // box fills the space between the list and the bar. The
        // text keeps a 6 px top pad inside the box. Its x and width
        // come from the window frame's content rect, at the draw site.
        constexpr int16_t HELP_PAD_TOP = 6;
        constexpr int16_t HELP_LINE_HEIGHT = 15;
        // Bottom bar: the category selectors on the left, MORE
        // and RETURN side by side on the right, all on one line —
        // MORE and RETURN never wrap to a second line. The bar
        // is the frame's bottom bar (24 tall, like the colony's sort
        // buttons), so its y and end come from the frame
        // (Bottom_Bar_Y/Bottom_Bar_End), never from constants here.
        // MORE and RETURN, side by side at the right end of the
        // bottom bar. MORE is the short label (the game keeps one
        // button for the inverse activity: the label stays MORE and
        // the lit state says it is on), 69 px wide at x 471 —
        // right-aligned against RETURN with an 8 px gap, the width
        // the verified metal-button version measured for its labels
        // plus padding. "RETURN" measures ~64 px, so the button is
        // 76 px wide (the old 88/68 px widths clipped both labels).
        constexpr int16_t SHOW_MORE_X = 471;
        constexpr int16_t SHOW_MORE_W = 69;
        constexpr int16_t RETURN_X = 548;
        constexpr int16_t RETURN_W = 76;
        constexpr int16_t TEXT_WIDTH = 608;

        // Colony chrome, in menu-palette indices (MAINMENU.LBX
        // entries 1/2 palette: 230-249 is a blue-grey ramp).
        // 230 is the dark panel interior, 234 the mid-grey panel
        // border. The window frame colours and the list line
        // colours (row frames, column dividers) live in the theme
        // (ui_kit::Colony_Theme).
        constexpr uint8_t PANEL_INTERIOR = 230;
        constexpr uint8_t PANEL_BORDER = 234;

        // The standard UI text palette, as used by the council screen.
        s_colors g_normal_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
        s_colors g_highlight_colors = {{150, 155, 155, 152, 153, 155, 155, 155}};

        // Light-grey title palette: glyph pixel value v maps to
        // colors[v], so colors[7] is the bright face and colors[0]
        // the darker outline (same shape as science_high_color).
        s_colors g_title_colors = {{234, 238, 240, 242, 243, 244, 246, 247}};

        // Colony-list palette, as rendered on the settings screen.
        // The colony screen loads palette 1 (colony.cpp), the
        // settings screen the menu palette, so the colony's own
        // indices (245/253/252) do NOT map to its blue here — 253
        // renders black and the name indices render dark olive.
        // The restyle therefore uses the menu palette's own bright
        // cyan (150-155, the old row color) as the colony-blue
        // equivalent, with the orange highlight for the selected
        // row and a bright white for secondary info. The colony
        // look is carried by the list structure (frame lines,
        // buttons) as much as by the hue.
        s_colors g_colony_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
        s_colors g_colony_highlight_colors = {{150, 155, 155, 152, 153, 155, 155, 155}};
        s_colors g_colony_dim_colors = {{244, 249, 248, 249, 248, 249, 248, 249}};

        // Flat light text colour for the blue selection bar in the
        // Resolution dropdown popup. A font style maps glyph shade
        // to a palette index, so a flat array renders a
        // single-colour label. (The metal buttons' dark/light label
        // colours live in the theme now, ui_kit::Button_Style.)
        s_colors g_light_text_colors = {{247, 247, 247, 247, 247, 247, 247, 247}};

        // The colony listing's squares are the grid cells of the
        // colony summary list (colsum.cpp / COLSUM.LBX): each data
        // item sits in its own bordered square/rectangular cell —
        // a thin metallic border around a dark interior — baked
        // into the COLSUM.LBX background art (colsum.cpp draws no
        // cell boxes itself; the background carries the grid). The
        // header labels (NAME, FARMERS, WORKERS, SCIENTISTS,
        // BUILDING) sit in the same bordered tab boxes, and each
        // row's far-right element is the round buy button.
        //
        // The diamond is that buy button, not a value container:
        // COLSUM.LBX entries 11-20, a 19x28-30 two-frame sprite, a
        // dark plaque with a grey ring around a four-point star.
        // fields.cpp draws frame 0 while *selection == 0 and frame
        // 1 once it is set, and selection is the colony's
        // bought_outright — so frame 0 (grey star) is "not bought",
        // frame 1 (blue star) is "bought". A colony that cannot buy
        // gets no live button at all, only the dim disc baked into
        // the colony background. A settings toggle is a value, not
        // a buy, so it takes the square — the colony list's value
        // cell — and the diamond is retired.
        //
        // The squares are engine-drawn in the list panel's own
        // border/interior colours (the colony's cells are baked
        // art and cannot be drawn here), by the kit now
        // (game/ui_kit.h: Button, Toggle/Cell kinds): a toggle is
        // a square cell -- on fills it bright (pale cyan-blue body
        // 154, near-white centre 247), off fills it in the menu
        // grey ramp (236 body, 246 centre), disabled is the bare
        // cell with no fill, like the colony's no-button state.
        // Dropdowns take a rectangular cell of the same
        // border/interior, sized to their text, and all selectors
        // end at SELECTOR_RIGHT_X.

        enum e_apply_result {
            APPLY_LIVE,
            APPLY_SAVED,
            APPLY_FAILED,
        };

        // Categories, in selector order. A category button is the
        // cleanest fit for the existing field system: the screen is
        // already a scrolling list with RETURN/MORE fields, so a
        // category is one more row of hidden fields, now in the
        // bottom selector bar like the colony summary's sort buttons,
        // and each category keeps its own scroll position and
        // selection. Sections (one long list with headers) would
        // break the scrolling and edit-field geometry instead.
        constexpr int CATEGORY_INTERNALS = 0;
        constexpr int CATEGORY_GAMEPLAY = 1;
        constexpr int CATEGORY_MODS = 2;
        constexpr int CATEGORY_COUNT = 3;

        const char* CATEGORY_LABELS[CATEGORY_COUNT] = {
            "Game Internals",
            "Gameplay",
            "Mods",
        };

        config::e_settings_category Settings_Category_For_Tab(int category) {
            switch (category) {
                case CATEGORY_GAMEPLAY:
                    return config::SETTINGS_CATEGORY_GAMEPLAY;
                case CATEGORY_MODS:
                    return config::SETTINGS_CATEGORY_MODS;
                case CATEGORY_INTERNALS:
                default:
                    return config::SETTINGS_CATEGORY_GAME_INTERNALS;
            }
        }

        struct s_settings_screen_state {
            std::vector<config::s_settings_row> rows;
            // Indices into rows for the current settings category.
            std::vector<int> visible_rows;
            // Catalogue entries for the Mods category.
            std::vector<config::s_mod_catalogue_entry> mods;
            // Working enabled set, in file order: the `enable` lines
            // of the config file. A toggle previews, writes and
            // applies from this set, then refreshes it from the
            // preview's resolved order (the order written to the file).
            std::vector<std::string> enabled_ids;
            int category = CATEGORY_INTERNALS;
            // Colony-list scroll, per tab: the scrollbar's bound
            // value (colsum.cpp's slider_bar_position) and the
            // first visible row (colsum.cpp's `first`). The slider
            // is inverted against the top row, like the colony's:
            // the slider at its maximum shows the first rows, the
            // slider at 0 shows the last. Sync_Scroll derives the
            // top row from the slider, mirroring
            // COLSUM::Update_First.
            int16_t slider_position[CATEGORY_COUNT] = {};
            int scroll_top[CATEGORY_COUNT] = {};
            // Index within the current category's list (visible rows
            // for settings, catalogue entries for mods).
            int selected = 0;
            std::string status;
            std::string target_path;
            std::string seed_path;
            bool target_ready = false;
            // Visible index of the row being edited, or -1.
            int editing_row = -1;
            char edit_buffer[512] = {};
            int16_t edit_field = -1;
            int16_t scroll_field = -1;
            int16_t done_field = -1;
            int16_t show_all_field = -1;
            // MORE is global for the screen, not per category: the user
            // is either browsing the most-used settings or everything.
            // It resets when the screen reopens (fresh state).
            bool show_all = false;
            int16_t tab_fields[CATEGORY_COUNT];
            int16_t row_fields[ROWS_PER_PAGE];
            // Resolution dropdown popup (fleet-popup shape): a small
            // window over the screen, the values as rows, a blue bar
            // on the selected one. Open while the popup is up.
            bool resolution_popup_open = false;
            std::vector<config::s_resolution_option> resolution_options;
            int resolution_popup_selected = 0;
            int16_t resolution_popup_x = 0;
            int16_t resolution_popup_y = 0;
            int16_t resolution_popup_w = 0;
            int16_t resolution_popup_h = 0;
            int16_t resolution_popup_first_row_y = 0;
            int16_t resolution_popup_row_step = 0;
            std::vector<int16_t> resolution_popup_fields;
        };

        s_settings_screen_state g_state;

        // The colony theme for this screen's tab bar, buttons and
        // window frame (game/ui_kit.h): the tab font, frames,
        // metrics and window frame of the colony screens, so a
        // hi-res asset set can replace the theme without touching
        // the screen. Built in Settings_Screen() by
        // Load_Colony_Theme, which loads the theme's frames --
        // including the derived RETURN frames (tools/
        // extract_sprite.py) -- from MOX::screen_seg on every
        // entry; the screen never names a sprite id or sees a
        // frame pointer. A frame that fails to load stays null and
        // its button falls back to the engine-drawn equivalent.
        std::unique_ptr<ui_kit::Colony_Theme> g_theme;

        // The window frame for this screen: the whole 640x480
        // window, drawn by the kit's Window_Frame. Every rect inside
        // the window -- the list panel, the tab bar, the help box,
        // the bottom-bar buttons -- is derived from this frame's
        // content rect, never from hard-coded screen constants.
        // Only call once g_theme is built (Settings_Screen builds
        // it before any field rebuild or draw).
        ui_kit::Window_Frame Make_Window_Frame() {
            return ui_kit::Window_Frame(0, 0, 640, 480, *g_theme);
        }

        // The bottom bar's y and exclusive end: the frame's bottom
        // bar, Tab_Height tall, its bottom on the content bottom.
        int16_t Bottom_Bar_Y() {
            return Make_Window_Frame().bottom_bar_y(
                g_theme->Tab_Height());
        }

        int16_t Bottom_Bar_End() {
            return (int16_t)(Bottom_Bar_Y() + g_theme->Tab_Height());
        }

        // The tab bar for the current category set: the rects come
        // from the bar, so the click fields and the draw cannot drift
        // apart. With pressed states, a tab draws its pressed frame
        // while the field system holds its field down (a mouse hold
        // or a keyboard press); the field build passes false, so a
        // rebuild never flashes a stale hold.
        ui_kit::Tab_Bar Make_Tab_Bar(bool with_pressed = false) {
            // The strip fills the bottom bar from the content edge
            // to MORE, like the colony summary's sort buttons fill
            // their bar to RETURN: 8 px clear of MORE, the same gap
            // MORE keeps from RETURN.
            ui_kit::Tab_Bar bar(Make_Window_Frame().content_x(),
                Bottom_Bar_Y(), (int16_t)(SHOW_MORE_X - 8), *g_theme);
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                bar.Add_Tab(CATEGORY_LABELS[category],
                    category == g_state.category,
                    with_pressed &&
                        g_state.tab_fields[category] >= 0 &&
                        fields::down_mouse_button ==
                            g_state.tab_fields[category]);
            }
            return bar;
        }

        // The list layout for this screen: the panel rect, header
        // rect, row rects and scroll track, derived from the
        // theme's Spacing (game/ui_kit.h), so the fields, the draw
        // and the scrolling all share one geometry. The panel's x
        // and width come from the window frame's content rect.
        // Only call once g_theme is built (Settings_Screen builds
        // it before any field rebuild or draw).
        ui_kit::List_Layout Make_List_Layout() {
            const ui_kit::Window_Frame frame = Make_Window_Frame();
            return ui_kit::List_Layout(frame.content_x(), LIST_PANEL_Y,
                frame.content_w(), ROW_FIELD_X_END, HEADER_Y, HEADER_HEIGHT,
                ROWS_PER_PAGE, g_theme->Spacing());
        }

        // The list's columns, from the header plates: SETTING /
        // VALUE, like the colony list's NAME / FARMERS / ... The
        // divider between them follows the SETTING plate's right
        // edge, like the colony's dividers follow their columns.
        // Only call once g_theme is built (Settings_Screen builds
        // it before any field rebuild or draw).
        ui_kit::Column_Layout Make_List_Columns(
            const ui_kit::List_Layout& layout) {
            return ui_kit::Column_Layout(
                {ui_kit::Column{HEADER_SETTING_X, HEADER_SETTING_W},
                 ui_kit::Column{HEADER_VALUE_X, HEADER_VALUE_W}},
                layout.first_row_y(),
                (int16_t)(layout.panel_y() + layout.panel_h() - 1));
        }

        // MORE button geometry follows the theme's derived frame
        // like RETURN's: a metal button is a RETURN cut into three
        // slices, so it is the frame's height (26) and
        // bottom-aligned with the bar, like RETURN -- never the
        // 24 px tab height, or the frame would overhang the field
        // by 2 px and the label would centre 1 px high. With no
        // frame it falls back to the tab-height engine button.
        int16_t More_Button_Y() {
            const ui_kit::Frame frame =
                g_theme->Return_Frames().normal;
            return !frame.Is_Null()
                ? (int16_t)(Bottom_Bar_End() - frame.Height() + 1)
                : Bottom_Bar_Y();
        }

        int16_t More_Button_H() {
            const ui_kit::Frame frame =
                g_theme->Return_Frames().normal;
            return !frame.Is_Null() ? frame.Height()
                                    : g_theme->Tab_Height();
        }

        // RETURN button geometry follows the theme's derived frame
        // when it loaded (85x26, bottom-aligned with the bar),
        // else the engine-drawn button's 76x24.
        int16_t Return_Button_Y() {
            const ui_kit::Frame frame =
                g_theme->Return_Frames().normal;
            return !frame.Is_Null()
                ? (int16_t)(Bottom_Bar_End() - frame.Height() + 1)
                : Bottom_Bar_Y();
        }

        int16_t Return_Button_W() {
            const ui_kit::Frame frame =
                g_theme->Return_Frames().normal;
            return !frame.Is_Null() ? frame.Width() : RETURN_W;
        }

        // True while the field system holds a field down (a mouse
        // hold, or a keyboard press -- Push_Field_Down sets
        // fields::down_mouse_button and the auto function redraws
        // on every hold tick): the button draws its pressed state
        // until release.
        bool Field_Held(int16_t field) {
            return field >= 0 && fields::down_mouse_button == field;
        }

        // Rows on the current tab. The Mods tab lists its settings
        // rows first (the two mod-adjacent settings) and then the
        // catalogue entries, so every row stays reachable.
        int Visible_Count() {
            if (g_state.category == CATEGORY_MODS) {
                return (int)g_state.visible_rows.size() + (int)g_state.mods.size();
            }
            return (int)g_state.visible_rows.size();
        }

        // True when a visible index on the Mods tab names a catalogue
        // entry rather than a settings row.
        bool Is_Mod_Row(int visible) {
            return g_state.category == CATEGORY_MODS &&
                visible >= (int)g_state.visible_rows.size();
        }

        // Catalogue index for a visible index on the Mods tab.
        int Mod_Index(int visible) {
            return visible - (int)g_state.visible_rows.size();
        }

        // Maximum first-visible-row index on the current tab: the
        // kit scroll model (game/ui_kit.h: Scroll_Bar), the
        // colony's `num_colonies - 10` bound.
        int Max_Scroll() {
            return ui_kit::Scroll_Bar::Max_Scroll(Visible_Count(),
                ROWS_PER_PAGE);
        }

        // Derive the current tab's first visible row from its
        // slider position, the kit scroll model (COLSUM's inverted
        // slider: the top row is the maximum minus the slider).
        // Clamps the slider into range (the field system also
        // clamps it, but the count can shrink under it when MORE
        // toggles or the category changes).
        void Sync_Scroll() {
            const int max_scroll = Max_Scroll();
            int16_t& slider = g_state.slider_position[g_state.category];
            if (slider < 0) {
                slider = 0;
            }
            if (slider > max_scroll) {
                slider = (int16_t)max_scroll;
            }
            g_state.scroll_top[g_state.category] =
                ui_kit::Scroll_Bar::Top_From_Slider(slider,
                    Visible_Count(), ROWS_PER_PAGE);
        }

        // First visible row on the current tab (after Sync_Scroll).
        int Scroll_Top() {
            return g_state.scroll_top[g_state.category];
        }

        // Colony-list scrollbar, the kit's Scroll_Bar
        // (game/ui_kit.h, element 5): colsum's thumb formula, its
        // bevel edges and its single-dot corner markers, in the
        // theme's colours, on the layout's track (first row top to
        // last row bottom). This screen shows the bar only when
        // the list is longer than the window; colsum's own rule
        // also draws a full-track thumb at exactly 10, which this
        // screen omits.
        void Draw_Scroll_Bar(const ui_kit::List_Layout& layout) {
            const int total = Visible_Count();
            if (total <= ROWS_PER_PAGE) {
                return;
            }
            const ui_kit::Scroll_Bar bar(SCROLL_X, SCROLL_W,
                layout.scroll_track_y(), layout.scroll_track_h(),
                *g_theme);
            bar.Draw(Scroll_Top(), total, ROWS_PER_PAGE);
        }

        // Rows index for a visible index in a settings category.
        int Visible_Row_Index(int visible) {
            return g_state.visible_rows[(size_t)visible];
        }

        std::string Truncate_To_Width(const std::string& text, int16_t max_width) {
            if (fonts::Get_String_Width(text.c_str()) <= max_width) {
                return text;
            }
            std::string truncated = text;
            while (truncated.size() > 3 &&
                fonts::Get_String_Width((truncated + "...").c_str()) > max_width) {
                truncated.pop_back();
            }
            return truncated + "...";
        }

        // Greedy word wrap into at most two lines of the given pixel
        // width. Anything past the second line is truncated with "...".
        void Wrap_Text(const std::string& text, int16_t max_width,
            std::string* line1, std::string* line2) {
            *line1 = "";
            *line2 = "";
            std::string current;
            size_t word_start = 0;
            while (word_start <= text.size()) {
                size_t word_end = text.find(' ', word_start);
                if (word_end == std::string::npos) {
                    word_end = text.size();
                }
                const std::string word = text.substr(word_start, word_end - word_start);
                const std::string candidate = current.empty() ? word : current + " " + word;
                if (fonts::Get_String_Width(candidate.c_str()) <= max_width) {
                    current = candidate;
                } else if (current.empty()) {
                    current = Truncate_To_Width(word, max_width);
                } else if (line1->empty()) {
                    *line1 = current;
                    current = word;
                } else {
                    *line2 = Truncate_To_Width(current, max_width);
                    return;
                }
                if (word_end == text.size()) {
                    break;
                }
                word_start = word_end + 1;
            }
            if (line1->empty()) {
                *line1 = current;
            } else {
                *line2 = current;
            }
        }

        std::string Join_Ids(const std::vector<std::string>& ids) {
            std::string joined;
            for (const std::string& id : ids) {
                if (!joined.empty()) {
                    joined += ", ";
                }
                joined += id;
            }
            return joined;
        }

        // The file the settings UI writes: the one config. When the user
        // has no
        // orion2re.cfg yet, the first write seeds it from the file the
        // game loaded (usually the shipped data file): creating a fresh
        // empty user file would shadow the shipped file's live settings
        // on the next reload. When there is no user config directory
        // (portable mode), the loaded file itself is the target: the loader
        // records its filesystem path on the active config at load time, so
        // the file the game loaded is the file saved to. The entry path
        // itself is a VFS key, not a file path, so it must never be used
        // as the target.
        void Resolve_Target() {
            g_state.target_ready = false;
            g_state.target_path.clear();
            g_state.seed_path.clear();
            const config::s_active_config* active = config::Get_Active_Config();
            if (active == nullptr) {
                g_state.status =
                    "Settings cannot be saved: the configuration is not initialized.";
                return;
            }
            const char* config_dir_raw =
                user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG);
            const std::string config_dir =
                config_dir_raw != nullptr ? config_dir_raw : "";
            if (!config_dir.empty()) {
                g_state.target_path = config_dir + "orion2re.cfg";
                if (std::filesystem::exists(g_state.target_path)) {
                    g_state.target_ready = true;
                    return;
                }
                if (!active->entry_filesystem_path.empty() &&
                    std::filesystem::is_regular_file(active->entry_filesystem_path)) {
                    g_state.seed_path = active->entry_filesystem_path;
                } else {
                    char resolved[4096] = {};
                    if (vfs::Resolve_Read_Path_No_Zip("orion2re.cfg", resolved,
                            sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    } else if (!active->entry_path.empty() &&
                        vfs::Resolve_Read_Path_No_Zip(active->entry_path.c_str(),
                            resolved, sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    }
                }
                g_state.target_ready = true;
                return;
            }
            // Portable mode: no user configuration directory. Save to the
            // file the game loaded.
            if (!active->entry_filesystem_path.empty() &&
                std::filesystem::is_regular_file(active->entry_filesystem_path)) {
                g_state.target_path = active->entry_filesystem_path;
                g_state.target_ready = true;
                return;
            }
            // No real file was loaded (defaults, or a zip-only entry): the
            // one config becomes a file next to the executable, seeded from
            // the loaded file when one can be resolved. Portable mode only
            // runs in a writable directory, and the VFS finds the
            // executable-directory file on the next load when no data file
            // shadows it.
            const char* base_path = SDL_GetBasePath();
            if (base_path != nullptr && base_path[0] != 0) {
                g_state.target_path = std::string(base_path) + "orion2re.cfg";
                if (!active->entry_filesystem_path.empty()) {
                    g_state.seed_path = active->entry_filesystem_path;
                } else {
                    char resolved[4096] = {};
                    if (vfs::Resolve_Read_Path_No_Zip("orion2re.cfg", resolved,
                            sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    } else if (!active->entry_path.empty() &&
                        vfs::Resolve_Read_Path_No_Zip(active->entry_path.c_str(),
                            resolved, sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    }
                }
                g_state.target_ready = true;
                return;
            }
            g_state.status =
                "Settings cannot be saved: no user configuration directory is available.";
        }

        bool Ensure_User_Config(std::string* error) {
            if (std::filesystem::exists(g_state.target_path)) {
                return true;
            }
            std::error_code ec;
            const std::filesystem::path parent =
                std::filesystem::path(g_state.target_path).parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    *error = "could not create the configuration directory";
                    return false;
                }
            }
            if (!g_state.seed_path.empty()) {
                std::filesystem::copy_file(g_state.seed_path, g_state.target_path, ec);
                if (ec) {
                    *error = "could not copy the loaded configuration file";
                    return false;
                }
                return true;
            }
            std::ofstream created(g_state.target_path, std::ios::binary);
            if (!created) {
                *error = "could not create the configuration file";
                return false;
            }
            return true;
        }

        // The working enabled set: the `enable` lines of the file the
        // screen writes, falling back to the file the game loaded when
        // the target does not exist yet (first session, nothing seeded),
        // and to the active package order when neither file can be read.
        void Read_Enabled_Set() {
            g_state.enabled_ids.clear();
            config::s_cfg_diagnostic diagnostic;
            if (!g_state.target_path.empty() &&
                config::Read_Enabled_Mods(g_state.target_path,
                    &g_state.enabled_ids, &diagnostic)) {
                return;
            }
            const config::s_active_config* active = config::Get_Active_Config();
            if (active != nullptr && !active->entry_filesystem_path.empty() &&
                config::Read_Enabled_Mods(active->entry_filesystem_path,
                    &g_state.enabled_ids, &diagnostic)) {
                return;
            }
            if (active != nullptr) {
                g_state.enabled_ids = active->package_order;
            }
        }

        void Rebuild_Visible_Rows() {
            g_state.visible_rows.clear();
            const config::e_settings_category wanted =
                Settings_Category_For_Tab(g_state.category);
            for (size_t index = 0; index < g_state.rows.size(); ++index) {
                if (config::Settings_Category_For_Path(g_state.rows[index].path) !=
                    wanted) {
                    continue;
                }
                // Secondary settings stay hidden until MORE. The
                // catalogue on the Mods tab is not a setting row, so
                // it is never filtered.
                if (!g_state.show_all &&
                    !config::Settings_Is_Primary_For_Path(g_state.rows[index].path)) {
                    continue;
                }
                g_state.visible_rows.push_back((int)index);
            }
        }

        void Refresh_Mods() {
            g_state.mods = config::Build_Mod_Catalogue(g_state.enabled_ids);
            if (g_state.selected >= Visible_Count()) {
                g_state.selected = Visible_Count() - 1;
            }
            if (g_state.selected < 0) {
                g_state.selected = 0;
            }
        }

        e_apply_result Apply_Row(const config::s_settings_row& row,
            const std::string& config_text, const std::string& display_text) {
            if (!g_state.target_ready) {
                g_state.status = "Settings cannot be saved: no configuration file target.";
                return APPLY_FAILED;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save '" + row.title + "': " + error + ".";
                return APPLY_FAILED;
            }
            config::s_cfg_diagnostic diagnostic;
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path(row.path), config_text, &diagnostic)) {
                g_state.status = "Could not save '" + row.title + "': " +
                    diagnostic.message;
                return APPLY_FAILED;
            }
            // The screen runs from the main menu with no game loaded, so
            // every settings group reloads. Reload also follows the launch
            // config while no game is loaded, so the change survives
            // returning to the menu after a game.
            if (!config::Reload_Active_Config_Groups(false, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live reload failed (" +
                    diagnostic.message +
                    "). The new value takes effect after a restart.";
                return APPLY_SAVED;
            }
            if (row.restart_required) {
                g_state.status = "'" + row.title +
                    "' saved; restart the game for it to take effect.";
            } else {
                g_state.status = "'" + row.title + "' set to " + display_text + ".";
            }
            return APPLY_LIVE;
        }

        bool Is_Resolution_Row(const config::s_settings_row& row) {
            return row.path == config::kResolutionRowPath;
        }

        // The Resolution row writes both window settings together,
        // then reloads once, mirroring Apply_Row's save/reload/status
        // shape. The width is written first; a failed height write
        // after a saved width reports the partial save.
        e_apply_result Apply_Resolution(int width, int height,
            const std::string& display_text) {
            if (!g_state.target_ready) {
                g_state.status = "Settings cannot be saved: no configuration file target.";
                return APPLY_FAILED;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save 'Resolution': " + error + ".";
                return APPLY_FAILED;
            }
            config::s_cfg_diagnostic diagnostic;
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path("interface.window_width"),
                    std::to_string(width), &diagnostic)) {
                g_state.status = "Could not save 'Resolution': " + diagnostic.message;
                return APPLY_FAILED;
            }
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path("interface.window_height"),
                    std::to_string(height), &diagnostic)) {
                g_state.status = "Saved the width, but could not save the height (" +
                    diagnostic.message + ").";
                return APPLY_SAVED;
            }
            if (!config::Reload_Active_Config_Groups(false, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live reload failed (" +
                    diagnostic.message +
                    "). The new value takes effect after a restart.";
                return APPLY_SAVED;
            }
            g_state.status = "'Resolution' set to " + display_text +
                "; restart the game for it to take effect.";
            return APPLY_LIVE;
        }

        // Rebuilds the category, row and navigation fields for the
        // current page. Only call when no edit field is open: the edit
        // field is added separately by Open_Edit and removed by
        // Commit/Cancel_Edit.
        void Rebuild_Fields() {
            fields::Clear_Fields();
            fonts::Set_Font_Style(4, &g_normal_colors);
            // Category selectors in the bottom bar, like the colony
            // summary's sort buttons: one hidden field per category,
            // left to right, each keeping its own scroll position.
            // The rects come from the tab bar, so the fields match
            // the drawn tabs exactly.
            const ui_kit::Tab_Bar tab_bar = Make_Tab_Bar();
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                const ui_kit::Tab_Bar::Tab& tab =
                    tab_bar.tabs()[(size_t)category];
                g_state.tab_fields[category] = fields::Add_Hidden_Field(
                    tab.x, tab_bar.y(), tab.x + tab.width - 1,
                    tab_bar.y() + tab_bar.height() - 1, "", 0);
            }
            const ui_kit::List_Layout layout = Make_List_Layout();
            Sync_Scroll();
            const int first = Scroll_Top();
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const int16_t y = layout.row_y(index);
                // Row fields stop short of the scrollbar strip, like
                // the colony list's row fields stop short of its
                // scrollbar, so clicks on the bar reach the bar.
                g_state.row_fields[index] = fields::Add_Hidden_Field(
                    8, y, ROW_FIELD_X_END, y + layout.row_height() - 1, "", 0);
            }
            for (int index = count; index < ROWS_PER_PAGE; ++index) {
                g_state.row_fields[index] = -1;
            }
            // Colony-list scrollbar, in its own strip at the right
            // edge of the list panel: a drag-operated Scroll_Field
            // bound to this tab's slider position, like COLSUM's
            // Add_Scroll_Field. Only when the list is longer than
            // the window; the field system's lone-scrollbar rule
            // makes the wheel scroll it from anywhere over the list.
            const int total = Visible_Count();
            if (total > ROWS_PER_PAGE) {
                g_state.scroll_field = fields::Add_Scroll_Field(SCROLL_X,
                    layout.scroll_track_y(), 0, (int16_t)total, 0,
                    (int16_t)Max_Scroll(), SCROLL_W,
                    layout.scroll_track_h(),
                    &g_state.slider_position[g_state.category], "", 0);
            } else {
                g_state.scroll_field = -1;
            }
            // MORE and RETURN sit side by side in the bottom
            // bar at the right end: the categories occupy the left
            // end, so SHOW_MORE_X..RETURN_X+RETURN_W is clear in
            // every category. They share the bar's single line and
            // never wrap.
            g_state.show_all_field = fields::Add_Hidden_Field(
                SHOW_MORE_X, More_Button_Y(), SHOW_MORE_X + SHOW_MORE_W - 1,
                Bottom_Bar_End(), "", 0);
            g_state.done_field = fields::Add_Hidden_Field(
                RETURN_X, Return_Button_Y(),
                RETURN_X + Return_Button_W() - 1, Bottom_Bar_End(), "", 0);
        }

        void Toggle_Show_All() {
            g_state.show_all = !g_state.show_all;
            g_state.selected = 0;
            Rebuild_Visible_Rows();
            // The row set changed, so the scroll resets to the top:
            // the slider at its maximum shows the first rows.
            g_state.slider_position[g_state.category] = (int16_t)Max_Scroll();
            g_state.scroll_top[g_state.category] = 0;
            g_state.status = g_state.show_all
                ? "Showing all settings, including advanced and ordering options."
                : "Showing the most-used settings. Use MORE for the rest.";
            Rebuild_Fields();
        }

        void Switch_Category(int category) {
            if (category == g_state.category) {
                return;
            }
            if (g_state.editing_row >= 0) {
                g_state.editing_row = -1;
                g_state.edit_field = -1;
            }
            g_state.category = category;
            Rebuild_Visible_Rows();
            Sync_Scroll();
            // The category keeps its own scroll position (per-category slider
            // and top row); the selection moves to the first visible
            // row so the highlight stays on screen.
            g_state.selected = g_state.scroll_top[category];
            if (category == CATEGORY_MODS) {
                Read_Enabled_Set();
                Refresh_Mods();
                g_state.status =
                    "Mod changes apply immediately. Click a mod to enable or disable it.";
            } else {
                g_state.status = "Click a setting to change it.";
            }
            Rebuild_Fields();
        }

        void Cancel_Edit() {
            g_state.editing_row = -1;
            g_state.edit_field = -1;
            Rebuild_Fields();
        }

        bool Commit_Edit() {
            const int row_index = Visible_Row_Index(g_state.editing_row);
            const config::s_settings_row& row = g_state.rows[row_index];
            config::s_typed_value value;
            std::string config_text;
            std::string error;
            if (!config::Parse_Leaf_Edit_Text(row.variable_id, g_state.edit_buffer,
                    &value, &config_text, &error)) {
                // Keep the edit open so the value can be fixed.
                g_state.status = error;
                return false;
            }
            const std::string display = config::Format_Leaf_Value(row.variable_id, value);
            Apply_Row(row, config_text, display);
            g_state.editing_row = -1;
            g_state.edit_field = -1;
            Rebuild_Fields();
            return true;
        }

        void Open_Edit(int visible) {
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            config::s_typed_value value;
            if (!config::Get_Settings_Row_Value(row, &value)) {
                g_state.status = "Could not read the current value of '" + row.title + "'.";
                return;
            }
            std::string text = config::Format_Leaf_Value(row.variable_id, value);
            if (text.size() >= sizeof(g_state.edit_buffer)) {
                text.resize(sizeof(g_state.edit_buffer) - 1);
            }
            strlcpy(g_state.edit_buffer, text.c_str(), sizeof(g_state.edit_buffer));
            const config::s_type_descriptor* type =
                config::Get_Variable_Type(row.variable_id);
            int16_t max_length = 20;
            if (type != nullptr && type->value_type == config::CONFIG_VALUE_STRING) {
                max_length = type->has_max_length
                    ? (int16_t)std::min<size_t>(type->max_length,
                        sizeof(g_state.edit_buffer) - 1)
                    : 64;
            }
            g_state.editing_row = visible;
            fonts::Set_Font_Style(4, &g_normal_colors);
            const ui_kit::List_Layout layout = Make_List_Layout();
            const int16_t y =
                layout.row_y(visible - Scroll_Top());
            g_state.edit_field = fields::Add_Continuous_String_Input_Field(
                VALUE_X, y, VALUE_WIDTH, layout.row_height() - 2, g_state.edit_buffer,
                max_length, 0, 0, 0, 0);
            // The row's full-width hidden field always wins Scan_Field
            // over the edit field added after it, so a click can never
            // focus the edit field through the field system: every
            // click in the value area cancels and reopens the edit
            // instead, and typing does nothing. Activate it directly,
            // mirroring the field system's own click activation for a
            // continuous input field (fields.cpp,
            // Interpret_Mouse_Input): the buffer becomes the working
            // text, the first Backspace clears it, and Enter copies it
            // back to the buffer for Commit_Edit.
            strlcpy(fields::continuous_string, g_state.edit_buffer,
                sizeof(fields::continuous_string));
            fields::input_field_active = 1;
            fields::active_input_field_first = 1;
            fields::cursor_position = 0;
            fields::cursor_on = 0;
            fields::active_input_field_number = g_state.edit_field;
        }

        void Open_Dropdown(int visible) {
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            const config::s_type_descriptor* type =
                config::Get_Variable_Type(row.variable_id);
            if (type == nullptr || type->values == nullptr || type->value_count == 0) {
                g_state.status = "'" + row.title + "' has no list of values.";
                return;
            }
            config::s_typed_value value;
            size_t current_index = 0;
            if (config::Get_Settings_Row_Value(row, &value)) {
                for (size_t index = 0; index < type->value_count; ++index) {
                    if (type->values[index].value == value.integer_value) {
                        current_index = index;
                    }
                }
            }
            size_t stride = 1;
            for (size_t index = 0; index < type->value_count; ++index) {
                stride = std::max(stride, strlen(type->values[index].name) + 1);
            }
            // One extra slot: Get_List_Field reads items until an
            // empty entry, so the list needs a terminator.
            std::vector<char> items((type->value_count + 1) * stride, '\0');
            std::vector<uint8_t> flags(type->value_count, 1);
            int16_t width = 16;
            for (size_t index = 0; index < type->value_count; ++index) {
                strlcpy(&items[index * stride], type->values[index].name, stride);
                width = (int16_t)std::max<int32_t>(width,
                    fonts::Get_String_Width(type->values[index].name) + 16);
            }
            int16_t x = VALUE_X;
            if (x + width > 632) {
                x = 632 - width;
            }
            const int16_t y_step = (int16_t)(fonts::Get_Font_Height() +
                fonts::Get_Font_Vertical_Spacing());
            const int16_t list_height =
                (int16_t)((type->value_count + 1) * (size_t)y_step);
            const ui_kit::List_Layout layout = Make_List_Layout();
            int16_t y = layout.row_y(visible - Scroll_Top());
            if (y + list_height > 470) {
                y = 470 - list_height;
            }
            if (y < 30) {
                y = 30;
            }
            int8_t selected_index = (int8_t)current_index;
            // The list items render in the colony-list color, so the
            // dropdown reads as part of the same list. The list
            // itself is the shared fields::Get_List_Field picker; its
            // frame is that picker's, not the settings screen's.
            fonts::Set_Font_Style(4, &g_colony_colors);
            fonts::Set_Font_LF(4);
            // The selection list clears the field set while it runs, so
            // the screen's fields are stashed and restored around it.
            fields::Mark_Fields();
            const int16_t choice = fields::Get_List_Field(x, y, width,
                row.title.c_str(), items.data(), (int32_t)stride, &selected_index,
                flags.data(), 17, nullptr, 0, nullptr, 0, 0);
            fields::Release_Fields();
            Rebuild_Fields();
            if (choice < 0) {
                return;
            }
            config::s_typed_value chosen;
            chosen.type = type->value_type;
            chosen.integer_value = type->values[choice].value;
            Apply_Row(row,
                config::Format_Leaf_Value_For_Config(row.variable_id, chosen),
                type->values[choice].name);
        }

        // The Resolution dropdown opens as a small popup window, in
        // the shape of the fleet-selection popup (fleetpop.cpp): a
        // small window over the screen, the values as rows, a blue
        // bar on the selected one. The fleet box's own frame cannot
        // be reused as-is -- it is fixed-width BUFFER0.LBX bitmap
        // art (top/middle/bottom caps sized to the fleet box), and
        // its selection is a ship-icon sprite -- so the popup uses
        // the settings screen's own colony chrome (thin border,
        // dark interior) for the frame and a filled blue bar for the
        // selection, the same blue as the active category and RETURN button.
        // The shared fields::Get_List_Field picker is not used here:
        // it draws its own unframed list and cannot render this shape.
        void Open_Resolution_Popup(int visible) {
            int current_width = 0;
            int current_height = 0;
            if (!config::Get_Current_Resolution(&current_width, &current_height)) {
                g_state.status = "Could not read the current window size.";
                return;
            }
            g_state.resolution_options =
                config::Resolution_Options(current_width, current_height);
            int selected = 0;
            for (size_t index = 0; index < g_state.resolution_options.size(); ++index) {
                if (g_state.resolution_options[index].width == current_width &&
                    g_state.resolution_options[index].height == current_height) {
                    selected = (int)index;
                }
            }
            g_state.resolution_popup_selected = selected;

            // Geometry: anchored to the row's value area, sized to
            // the longest label, clamped to the screen like the
            // dropdown in Open_Dropdown.
            const int16_t y_step = (int16_t)(fonts::Get_Font_Height() +
                fonts::Get_Font_Vertical_Spacing());
            int16_t content_w = 16;
            for (const config::s_resolution_option& option :
                g_state.resolution_options) {
                content_w = (int16_t)std::max<int32_t>(content_w,
                    fonts::Get_String_Width(option.label.c_str()));
            }
            const int16_t pad_x = 8;
            const int16_t pad_top = 6;
            const int16_t pad_bottom = 6;
            const int16_t popup_w = (int16_t)(content_w + pad_x * 2 + 2);
            const int16_t popup_h = (int16_t)(
                (int16_t)g_state.resolution_options.size() * y_step +
                pad_top + pad_bottom + 2);
            int16_t x = VALUE_X;
            if (x + popup_w > 632) {
                x = 632 - popup_w;
            }
            const ui_kit::List_Layout layout = Make_List_Layout();
            int16_t y = layout.row_y(visible - Scroll_Top());
            if (y + popup_h > 470) {
                y = 470 - popup_h;
            }
            if (y < 30) {
                y = 30;
            }
            g_state.resolution_popup_x = x;
            g_state.resolution_popup_y = y;
            g_state.resolution_popup_w = popup_w;
            g_state.resolution_popup_h = popup_h;
            g_state.resolution_popup_first_row_y = (int16_t)(y + 1 + pad_top);
            g_state.resolution_popup_row_step = y_step;

            // One hidden field per option row, so clicks land on rows
            // and hover moves the blue bar. The screen's fields are
            // stashed and restored around the popup, as in Open_Dropdown.
            fields::Mark_Fields();
            fields::Clear_Fields();
            g_state.resolution_popup_fields.clear();
            for (size_t index = 0; index < g_state.resolution_options.size(); ++index) {
                const int16_t row_y = (int16_t)(
                    g_state.resolution_popup_first_row_y + (int16_t)index * y_step);
                g_state.resolution_popup_fields.push_back(
                    fields::Add_Hidden_Field(x + 1, row_y,
                        (int16_t)(x + popup_w - 2),
                        (int16_t)(row_y + y_step - 1), "", 0));
            }
            g_state.resolution_popup_open = true;

            // Modal loop, modelled on fields::Get_List_Field: hover
            // moves the blue bar, a click chooses, Esc cancels, and
            // a click outside the popup dismisses it too.
            bool popup_armed = false;
            int choice = -1;
            while (true) {
                platform::Check_Exit();
                timer::Mark_Time();
                const int16_t input = fields::Get_Input();
                if (input == -1) {
                    choice = -1;
                    break;
                }
                if (input != 0) {
                    for (size_t index = 0;
                        index < g_state.resolution_popup_fields.size(); ++index) {
                        if (input == g_state.resolution_popup_fields[index]) {
                            choice = (int)index;
                            break;
                        }
                    }
                    if (choice >= 0) {
                        break;
                    }
                }
                // A click outside the popup dismisses it, like Esc.
                // Arm once the opening click is released, so the
                // click that opened the popup does not dismiss it.
                // (The loop samples the live button state every
                // iteration, so a held press is seen while it is
                // still down; Get_Input above has already consumed
                // any click that landed on a popup field.)
                if (!popup_armed) {
                    if (mouse::Mouse_Button() == 0) {
                        popup_armed = true;
                    }
                } else if (mouse::Mouse_Button() != 0) {
                    const int16_t px = mouse::Pointer_X();
                    const int16_t py = mouse::Pointer_Y();
                    if (px < g_state.resolution_popup_x
                        || px >= g_state.resolution_popup_x + g_state.resolution_popup_w
                        || py < g_state.resolution_popup_y
                        || py >= g_state.resolution_popup_y + g_state.resolution_popup_h) {
                        choice = -1;
                        break;
                    }
                }
                const int16_t scan = fields::Scan_Input();
                if (scan > 0) {
                    for (size_t index = 0;
                        index < g_state.resolution_popup_fields.size(); ++index) {
                        if (scan == g_state.resolution_popup_fields[index]) {
                            g_state.resolution_popup_selected = (int)index;
                            break;
                        }
                    }
                }
                video::Set_Page_Off();
                fields::Invoke_Auto_Function();
                palette::Refresh_Palette();
                platform::Toggle_Pages_Stub();
                timer::Release_Time(3);
            }
            fields::Release_Fields();
            g_state.resolution_popup_open = false;
            g_state.resolution_popup_fields.clear();
            Rebuild_Fields();
            if (choice < 0) {
                return;
            }
            const config::s_resolution_option& option =
                g_state.resolution_options[(size_t)choice];
            Apply_Resolution(option.width, option.height, option.label);
        }

        void Handle_Row_Click(int visible) {
            g_state.selected = visible;
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            switch (row.widget) {
                case config::SETTINGS_WIDGET_TOGGLE: {
                    config::s_typed_value value;
                    if (!config::Get_Settings_Row_Value(row, &value)) {
                        g_state.status = "Could not read the current value of '" +
                            row.title + "'.";
                        return;
                    }
                    const std::string next = value.bool_value ? "false" : "true";
                    Apply_Row(row, next, next);
                    return;
                }
                case config::SETTINGS_WIDGET_DROPDOWN:
                    if (Is_Resolution_Row(row)) {
                        Open_Resolution_Popup(visible);
                    } else {
                        Open_Dropdown(visible);
                    }
                    return;
                case config::SETTINGS_WIDGET_EDIT:
                    Open_Edit(visible);
                    return;
                case config::SETTINGS_WIDGET_INFO:
                default:
                    g_state.status = row.info;
                    return;
            }
        }

        // A mod toggle: dry-run the candidate set through the staging
        // path, then write the resolved order to the config file and
        // apply it with a full load. The file is written before the
        // apply, so a failed apply still leaves the picked set as the
        // set the next start loads. Nothing is written when the
        // preview fails.
        // `mod_index` is the catalogue index, not the visible row:
        // the Mods tab lists its settings rows first.
        void Handle_Mod_Click(int mod_index) {
            g_state.selected = (int)g_state.visible_rows.size() + mod_index;
            // Copy the id out: Refresh_Mods() below replaces the
            // catalogue vector, so a reference into it would dangle.
            const config::s_mod_catalogue_entry entry = g_state.mods[(size_t)mod_index];
            if (entry.broken) {
                g_state.status = "'" + entry.id + "' is broken: " + entry.reason;
                return;
            }
            if (!g_state.target_ready) {
                g_state.status = "Mods cannot be saved: no configuration file target.";
                return;
            }
            const std::vector<std::string> previous_enabled = g_state.enabled_ids;
            const auto found = std::find(previous_enabled.begin(),
                previous_enabled.end(), entry.id);
            const bool enabling = found == previous_enabled.end();
            std::vector<std::string> candidate;
            if (enabling) {
                // Newly enabled mods append after the order already in
                // the file; the resolver orders the written set.
                candidate = previous_enabled;
                candidate.push_back(entry.id);
            } else {
                // A dependency chain is a unit: disabling a mod also
                // disables its dependents and any dependency nothing
                // remaining needs (see Prune_Disabled_Mod_Set), so no
                // partial chain is left behind.
                candidate = config::Prune_Disabled_Mod_Set(g_state.mods,
                    previous_enabled, entry.id);
            }
            config::s_cfg_diagnostic diagnostic;
            config::s_mod_set_preview preview;
            if (!config::Preview_Enabled_Mods(candidate, &preview, &diagnostic)) {
                g_state.status = "Cannot " + std::string(enabling ? "enable '" : "disable '") +
                    entry.id + "': " + diagnostic.message;
                return;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save mods: " + error + ".";
                return;
            }
            if (!config::Write_Enabled_Mods(g_state.target_path,
                    preview.package_order, &diagnostic)) {
                g_state.status = "Could not save mods: " + diagnostic.message;
                return;
            }
            g_state.enabled_ids = preview.package_order;
            Refresh_Mods();
            if (!config::Apply_Enabled_Mods(candidate, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live apply failed (" +
                    diagnostic.message +
                    "). The new set takes effect after a restart.";
                return;
            }
            const std::string active = preview.package_order.empty()
                ? "no mods"
                : Join_Ids(preview.package_order);
            std::string extra;
            if (enabling) {
                // Dependencies the resolver pulled in alongside the mod.
                std::vector<std::string> pulled;
                for (const std::string& id : preview.package_order) {
                    if (std::find(candidate.begin(), candidate.end(), id) ==
                        candidate.end()) {
                        pulled.push_back(id);
                    }
                }
                if (!pulled.empty()) {
                    extra = ". Also enabled: " + Join_Ids(pulled);
                }
            } else {
                // Chain members the disable took with it.
                std::vector<std::string> removed;
                for (const std::string& id : previous_enabled) {
                    if (id != entry.id &&
                        std::find(candidate.begin(), candidate.end(), id) ==
                            candidate.end()) {
                        removed.push_back(id);
                    }
                }
                if (!removed.empty()) {
                    extra = ". Also disabled: " + Join_Ids(removed);
                }
            }
            g_state.status = "'" + entry.id + "' " +
                (enabling ? "enabled" : "disabled") + extra +
                ". Active mods: " + active + ".";
        }

        std::string Mod_Help_Text(const config::s_mod_catalogue_entry& entry,
            const std::vector<config::s_mod_catalogue_entry>& catalogue) {
            std::string help = entry.description.empty() ? entry.id : entry.description;
            std::string meta;
            if (entry.broken) {
                meta = "Broken: " + entry.reason;
            } else {
                meta = entry.source == config::MOD_SOURCE_USER ? "Source: user mods"
                    : "Source: bundled";
                if (!entry.dependencies.empty()) {
                    std::vector<std::string> ids;
                    for (const config::s_mod_dependency& dependency : entry.dependencies) {
                        ids.push_back(dependency.id);
                    }
                    meta += "; Needs: " + Join_Ids(ids);
                }
                // Enabled mods that need this one: disabling it disables
                // them too, so the chain is visible before the click.
                // (The row's [x]/[ ] already states this mod's own
                // state, so it is not repeated here in words.)
                std::vector<std::string> required_by;
                for (const config::s_mod_catalogue_entry& other : catalogue) {
                    if (!other.enabled || other.id == entry.id) {
                        continue;
                    }
                    for (const config::s_mod_dependency& dependency : other.dependencies) {
                        if (dependency.id == entry.id) {
                            required_by.push_back(other.id);
                            break;
                        }
                    }
                }
                if (!required_by.empty()) {
                    meta += "; Required by: " + Join_Ids(required_by);
                }
                if (!entry.conflicts.empty()) {
                    meta += "; Conflicts: " + Join_Ids(entry.conflicts);
                }
            }
            return help + " | " + meta;
        }

        // The Resolution dropdown popup, in the fleet-popup shape:
        // a small window over the screen, the values as rows, a blue
        // bar on the selected one. Drawn by Draw_Settings_Screen
        // while the popup is open.
        void Draw_Resolution_Popup() {
            if (!g_state.resolution_popup_open) {
                return;
            }
            const int16_t x = g_state.resolution_popup_x;
            const int16_t y = g_state.resolution_popup_y;
            const int16_t w = g_state.resolution_popup_w;
            const int16_t h = g_state.resolution_popup_h;
            graphics::Fill(x, y, (int16_t)(x + w - 1), (int16_t)(y + h - 1),
                PANEL_INTERIOR);
            graphics::Box(x, y, w, h, 1, PANEL_BORDER);
            const int16_t text_x = (int16_t)(x + 1 + 8);
            const int16_t text_dy = (int16_t)(
                (g_state.resolution_popup_row_step - fonts::Get_Font_Height()) / 2);
            for (size_t index = 0; index < g_state.resolution_options.size(); ++index) {
                const int16_t row_y = (int16_t)(
                    g_state.resolution_popup_first_row_y +
                    (int16_t)index * g_state.resolution_popup_row_step);
                const int16_t row_end = (int16_t)(row_y +
                    g_state.resolution_popup_row_step - 1);
                if ((int)index == g_state.resolution_popup_selected) {
                    // Blue bar around the selected value, the same blue
                    // as the active category and the RETURN button: the
                    // theme's primary face.
                    graphics::Fill(x + 1, row_y, (int16_t)(x + w - 2), row_end,
                        g_theme->Button_Style().primary_face);
                    fonts::Set_Font_Style(4, &g_light_text_colors);
                } else {
                    fonts::Set_Font_Style(4, &g_colony_colors);
                }
                fonts::Print(text_x, (int16_t)(row_y + text_dy),
                    g_state.resolution_options[index].label.c_str());
            }
        }

        void Draw_Settings_Screen() {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 639, 479, 0);

            // Full-window colony chrome (game/ui_kit.h): the
            // bevelled metal frame baked into the colony screens'
            // backgrounds, drawn by the kit's Window_Frame. No
            // title: the colony screens carry no dialog title, and
            // the category tabs in the bottom bar say what this
            // screen is. The frame positions everything inside it:
            // the list panel, the tab bar, the help box.
            const ui_kit::Window_Frame frame = Make_Window_Frame();
            frame.Draw(nullptr);
            fonts::Set_Font_Style(4, &g_normal_colors);

            // List panel, like the colony list: the kit draws the
            // dark interior, the thin grey border and the framed
            // row slots -- light bevel lines, like the colony's
            // cells -- with 1 px of empty background between them,
            // and the column divider between SETTING and VALUE,
            // like the colony list's grooves between its columns.
            const ui_kit::List_Layout layout = Make_List_Layout();
            ui_kit::Draw_List_Background(*g_theme, layout);
            ui_kit::Draw_List_Columns(*g_theme, Make_List_Columns(layout));

            // Header plates, like the colony list's raised header
            // boxes (NAME, FARMERS, WORKERS, ...): bevelled plates
            // over the name and value columns at the top of the list
            // panel, their face lighter than the list interior.
            // Drawn after the panel fill, so the fill does not cover them.
            const ui_kit::List_Style list_style = g_theme->List_Style();
            const auto draw_header_plate = [&](int16_t x, int16_t w, const char* label) {
                graphics::Fill(x, HEADER_Y, x + w - 1, HEADER_Y + HEADER_HEIGHT - 1,
                    list_style.plate_face);
                // Raised bevel: light on the top and left, shade on
                // the bottom and right, like the colony's plates.
                graphics::Fill(x, HEADER_Y, x + w - 1, HEADER_Y,
                    list_style.frame_light);
                graphics::Fill(x, HEADER_Y, x, HEADER_Y + HEADER_HEIGHT - 1,
                    list_style.frame_light);
                graphics::Fill(x, HEADER_Y + HEADER_HEIGHT - 1, x + w - 1,
                    HEADER_Y + HEADER_HEIGHT - 1, list_style.frame_shade);
                graphics::Fill(x + w - 1, HEADER_Y, x + w - 1,
                    HEADER_Y + HEADER_HEIGHT - 1, list_style.frame_shade);
                fonts::Set_Font_Style(4, &g_title_colors);
                const int16_t label_x = (int16_t)(x +
                    (w - fonts::Get_String_Width(label)) / 2);
                fonts::Print(label_x, HEADER_Y + 4, label);
                fonts::Set_Font_Style(4, &g_normal_colors);
            };
            draw_header_plate(HEADER_SETTING_X, HEADER_SETTING_W, "SETTING");
            draw_header_plate(HEADER_VALUE_X, HEADER_VALUE_W, "VALUE");

            const bool mods_tab = g_state.category == CATEGORY_MODS;
            Sync_Scroll();
            const int first = Scroll_Top();
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const int visible = first + index;
                const int16_t y = layout.row_y(index);
                const int16_t text_y = (int16_t)(y + ROW_TEXT_Y_OFFSET);
                const bool selected = visible == g_state.selected;
                // Colony-list row: name in the colony color, selected
                // row in the bright-blue highlight.
                fonts::Set_Font_Style(4,
                    selected ? &g_colony_highlight_colors : &g_colony_colors);
                if (Is_Mod_Row(visible)) {
                    const config::s_mod_catalogue_entry& entry =
                        g_state.mods[(size_t)Mod_Index(visible)];
                    std::string title_text = entry.name;
                    if (!entry.version_text.empty()) {
                        title_text += " " + entry.version_text;
                    }
                    fonts::Print(TITLE_X, text_y,
                        Truncate_To_Width(title_text, TITLE_WIDTH).c_str());
                    // Kit toggle button, right-aligned to the
                    // selector column: bright when on, grey when
                    // off, bare when the mod is broken and cannot
                    // be enabled.
                    const int16_t toggle_x = SELECTOR_RIGHT_X - SQUARE_SIZE + 1;
                    const int16_t toggle_y = (int16_t)(y + (layout.row_height() - SQUARE_SIZE) / 2);
                    ui_kit::Button toggle(toggle_x, toggle_y,
                        SQUARE_SIZE, SQUARE_SIZE,
                        ui_kit::Button_Kind::Toggle, *g_theme);
                    toggle.Set_State(entry.broken
                        ? ui_kit::Button_State::Disabled
                        : (entry.enabled ? ui_kit::Button_State::Active
                                         : ui_kit::Button_State::Rest));
                    toggle.Draw();
                    if (entry.broken) {
                        fonts::Set_Font_Style(4, &g_colony_dim_colors);
                        const int16_t broken_w = fonts::Get_String_Width("Broken");
                        fonts::Print(toggle_x - 6 - broken_w, text_y, "Broken");
                    }
                    continue;
                }
                const config::s_settings_row& row =
                    g_state.rows[(size_t)Visible_Row_Index(visible)];
                const std::string title_text = Truncate_To_Width(row.title, TITLE_WIDTH);
                fonts::Print(TITLE_X, text_y, title_text.c_str());
                // Restart-required settings carry no marker on the
                // row: the restart note lives in the description box
                // for the selected row instead.
                // The edit field draws its own text while it is open.
                if (visible == g_state.editing_row) {
                    continue;
                }
                if (row.widget == config::SETTINGS_WIDGET_TOGGLE) {
                    // Kit toggle button, right-aligned to the
                    // selector column: bright when on, grey when off.
                    config::s_typed_value value;
                    const bool on = config::Get_Settings_Row_Value(row, &value)
                        && value.bool_value;
                    const int16_t toggle_x = SELECTOR_RIGHT_X - SQUARE_SIZE + 1;
                    const int16_t toggle_y =
                        (int16_t)(y + (layout.row_height() - SQUARE_SIZE) / 2);
                    ui_kit::Button toggle(toggle_x, toggle_y,
                        SQUARE_SIZE, SQUARE_SIZE,
                        ui_kit::Button_Kind::Toggle, *g_theme);
                    toggle.Set_State(on ? ui_kit::Button_State::Active
                                        : ui_kit::Button_State::Rest);
                    toggle.Draw();
                } else if (row.widget == config::SETTINGS_WIDGET_DROPDOWN) {
                    // Dropdown: the value in a kit Cell button with
                    // a down chevron, right-aligned to the selector
                    // column; blue while its popup is open (the
                    // Resolution popup), like the active selector.
                    // The list itself is the shared
                    // fields::Get_List_Field picker; its frame is
                    // that picker's, not the settings screen's.
                    std::string value_text;
                    if (Is_Resolution_Row(row)) {
                        int width = 0;
                        int height = 0;
                        value_text = config::Get_Current_Resolution(&width, &height)
                            ? config::Format_Resolution(width, height)
                            : "?";
                    } else {
                        config::s_typed_value value;
                        value_text = config::Get_Settings_Row_Value(row, &value)
                            ? config::Format_Leaf_Value(row.variable_id, value)
                            : "?";
                    }
                    value_text = Truncate_To_Width(value_text, VALUE_WIDTH - 16);
                    const int16_t text_w = fonts::Get_String_Width(value_text.c_str());
                    int16_t box_w = (int16_t)(text_w + 24);
                    if (box_w > VALUE_WIDTH) {
                        box_w = VALUE_WIDTH;
                    }
                    const int16_t trigger_x = (int16_t)(SELECTOR_RIGHT_X - box_w + 1);
                    const int16_t trigger_y = (int16_t)(y + 5);
                    ui_kit::Button cell(trigger_x, trigger_y, box_w, 20,
                        ui_kit::Button_Kind::Cell, *g_theme);
                    cell.Set_Label(value_text.c_str());
                    // The value text sits on the row's text line
                    // (text_y), like the title and the plain values:
                    // the cell starts 5 px into the row, the text
                    // 9 px in, so the label offset is 4 px.
                    cell.Set_Label_Y_Offset((int16_t)(text_y - trigger_y));
                    cell.Set_State(Is_Resolution_Row(row) &&
                            g_state.resolution_popup_open
                        ? ui_kit::Button_State::Active
                        : ui_kit::Button_State::Rest);
                    cell.Draw();
                } else {
                    // Info and edit values: text in the colony dim
                    // color, right-aligned to the selector column.
                    config::s_typed_value value;
                    std::string value_text;
                    if (row.widget == config::SETTINGS_WIDGET_INFO) {
                        value_text = row.info;
                    } else {
                        value_text = config::Get_Settings_Row_Value(row, &value)
                            ? config::Format_Leaf_Value(row.variable_id, value)
                            : "?";
                    }
                    value_text = Truncate_To_Width(value_text, VALUE_WIDTH);
                    const int16_t value_w =
                        fonts::Get_String_Width(value_text.c_str());
                    fonts::Set_Font_Style(4, &g_colony_dim_colors);
                    fonts::Print(SELECTOR_RIGHT_X - value_w + 1, text_y,
                        value_text.c_str());
                }
            }

            Draw_Scroll_Bar(layout);

            // Description box, spanning the whole width of the
            // window like the colony screen's bottom info area
            // (the "Large Terran" panel): the selected row's
            // description, a "Requires restart." line for the
            // settings it applies to, and the status line below.
            // There is no permanent restart legend: the restart
            // note lives here, only for the selected setting. The
            // box top comes from the layout (4 px below the last
            // row) and its bottom sits the theme's info-to-bar gap
            // above the bottom bar.
            const ui_kit::Spacing spacing = g_theme->Spacing();
            const int16_t desc_y = layout.info_y();
            const int16_t desc_bottom =
                (int16_t)(Bottom_Bar_Y() - 1 - spacing.info_to_bar);
            const int16_t desc_h = (int16_t)(desc_bottom - desc_y + 1);
            graphics::Fill(frame.content_x(), desc_y,
                frame.content_right() - 1, desc_y + desc_h - 1,
                PANEL_INTERIOR);
            graphics::Box(frame.content_x(), desc_y, frame.content_w(),
                desc_h, 1, PANEL_BORDER);
            std::string help_text;
            bool restart_required = false;
            if (mods_tab && Is_Mod_Row(g_state.selected)) {
                help_text = Mod_Help_Text(g_state.mods[(size_t)Mod_Index(g_state.selected)],
                    g_state.mods);
            } else if (Visible_Count() > 0) {
                const config::s_settings_row& selected_row =
                    g_state.rows[(size_t)Visible_Row_Index(g_state.selected)];
                help_text = selected_row.description;
                restart_required = selected_row.restart_required;
            }
            std::string help_line1;
            std::string help_line2;
            Wrap_Text(help_text, TEXT_WIDTH, &help_line1, &help_line2);
            fonts::Set_Font_Style(4, &g_colony_dim_colors);
            const int16_t help_y = (int16_t)(desc_y + HELP_PAD_TOP);
            fonts::Print(TITLE_X, help_y, help_line1.c_str());
            if (!help_line2.empty()) {
                fonts::Print(TITLE_X, help_y + HELP_LINE_HEIGHT, help_line2.c_str());
            }
            // The status sits directly under the restart note when
            // one is shown, so no blank line opens up otherwise.
            int16_t status_y = (int16_t)(help_y + 2 * HELP_LINE_HEIGHT);
            if (restart_required) {
                fonts::Set_Font_Style(4, &g_colony_highlight_colors);
                fonts::Print(TITLE_X, help_y + 2 * HELP_LINE_HEIGHT,
                    "Requires restart.");
                status_y = (int16_t)(help_y + 3 * HELP_LINE_HEIGHT);
            }
            fonts::Set_Font_Style(4, &g_colony_dim_colors);
            fonts::Print(TITLE_X, status_y,
                Truncate_To_Width(g_state.status, TEXT_WIDTH).c_str());

            // Bottom bar: the category selectors on the left, like
            // the colony summary's sort buttons (Name / Population
            // / Food ...); MORE and RETURN side by side on
            // the right, on the same line, never wrapping. All of
            // them are kit buttons: the tabs are Selector buttons
            // (the bar builds them), MORE a Metal toggle and
            // RETURN an Image button, like the colony RETURN. A
            // button held down draws its pressed state until
            // release.
            Make_Tab_Bar(true).Draw();
            const int16_t bar_y =
                frame.bottom_bar_y(g_theme->Tab_Height());
            const int16_t bar_h = g_theme->Tab_Height();
            // MORE is a RETURN-style metal toggle: the metal face
            // (not the blue primary face), latched pressed while
            // the secondary rows are shown, so it stays lit like
            // RETURN before it closes — the label stays MORE in
            // both states, the orange lit state is what says it
            // is on (the game's own choice: one button for the
            // inverse activity, no second button). Clicking it
            // again turns it off. Held down it draws pressed too,
            // like any held button.
            ui_kit::Button show_more(SHOW_MORE_X, More_Button_Y(),
                SHOW_MORE_W, More_Button_H(),
                ui_kit::Button_Kind::Metal, *g_theme);
            show_more.Set_Label("MORE");
            show_more.Set_State(Field_Held(g_state.show_all_field) ||
                    g_state.show_all
                ? ui_kit::Button_State::Pressed
                : ui_kit::Button_State::Rest);
            show_more.Draw();
            const int16_t return_y = Return_Button_Y();
            const ui_kit::Frame return_frame =
                g_theme->Return_Frames().normal;
            const int16_t return_h = !return_frame.Is_Null()
                ? return_frame.Height()
                : bar_h;
            ui_kit::Button ret(RETURN_X, return_y, Return_Button_W(),
                return_h, ui_kit::Button_Kind::Image, *g_theme);
            ret.Set_Label("RETURN");
            ret.Set_State(Field_Held(g_state.done_field)
                ? ui_kit::Button_State::Pressed
                : ui_kit::Button_State::Rest);
            ret.Draw();

            Draw_Resolution_Popup();

            fields::Draw_Visible_Fields();
        }

    } // namespace

    void __cdecl Settings_Screen() {
        g_state = s_settings_screen_state{};
        g_state.rows = config::Build_Settings_Rows();
        if (g_state.rows.empty()) {
            // Nothing to show (the schema did not load); return to the
            // menu rather than drawing an empty screen.
            MOX::current_screen = SCREEN_MAIN_MENU;
            return;
        }
        g_state.status = "Click a setting to change it.";
        Resolve_Target();
        Read_Enabled_Set();
        Rebuild_Visible_Rows();

        video::Set_Refresh_Full();
        fields::Clear_Fields();
        RUSS::Mox_Update();
        fonts::Set_Font_Style(4, &g_normal_colors);
        fonts::Set_Font_LF(4);
        // The theme loads its own frames (MOX::screen_seg is
        // released between screens, so they load on every entry):
        // the selector frames and the RETURN frames. The screen
        // never names a sprite id or sees a frame pointer. A frame
        // that fails to load stays null and its button falls back
        // to the engine-drawn equivalent.
        g_theme = ui_kit::Load_Colony_Theme(MOX::screen_seg,
            g_title_colors);
        Rebuild_Fields();
        fields::Assign_Auto_Function(Draw_Settings_Screen, 2);

        bool screen_exit = false;
        while (!screen_exit) {
            timer::Mark_Time();
            const int16_t input = fields::Get_Input();
            if (input == -1) {
                // Esc: cancel a pending edit, otherwise close the screen.
                if (g_state.editing_row >= 0) {
                    Cancel_Edit();
                } else {
                    screen_exit = true;
                }
            } else if (input != 0) {
                // Anything other than the edit field or RETURN cancels a
                // pending edit (RETURN commits it instead). The cancel
                // rebuilds the field set, but the row and navigation
                // field ids are deterministic — the edit field is always
                // added after them, so the input id still matches below.
                if (g_state.editing_row >= 0 && input != g_state.edit_field &&
                    input != g_state.done_field) {
                    Cancel_Edit();
                }
                if (input == g_state.done_field) {
                    if (g_state.editing_row >= 0) {
                        if (Commit_Edit()) {
                            screen_exit = true;
                        }
                    } else {
                        screen_exit = true;
                    }
                } else if (input == g_state.show_all_field) {
                    Toggle_Show_All();
                } else if (input == g_state.edit_field) {
                    // Enter commits (input_field_active is cleared by the
                    // field system on Enter); a click only focuses the
                    // field and keeps editing.
                    if (fields::input_field_active == 0) {
                        Commit_Edit();
                    }
                } else if (input == g_state.scroll_field) {
                    // The wheel or a scrollbar drag moved the list:
                    // like clicking another row, it cancels a pending
                    // edit, whose field would no longer line up with
                    // its row. The scroll itself already happened.
                    if (g_state.editing_row >= 0) {
                        Cancel_Edit();
                    }
                } else {
                    bool handled = false;
                    for (int category = 0; category < CATEGORY_COUNT; ++category) {
                        if (input == g_state.tab_fields[category]) {
                            Switch_Category(category);
                            handled = true;
                            break;
                        }
                    }
                    if (!handled) {
                        const int first = Scroll_Top();
                        const int count = std::min(ROWS_PER_PAGE,
                            Visible_Count() - first);
                        for (int index = 0; index < count; ++index) {
                            if (input == g_state.row_fields[index]) {
                                const int visible = first + index;
                                if (Is_Mod_Row(visible)) {
                                    Handle_Mod_Click(Mod_Index(visible));
                                } else {
                                    Handle_Row_Click(visible);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            if (!screen_exit) {
                Draw_Settings_Screen();
                video::Copy_Off_To_Back();
                RUSS::Mox_Sync_Update();
            }
            timer::Release_Time(3);
        }

        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

} // namespace SETTINGS_SCREEN
