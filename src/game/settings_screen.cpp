#include "pch.h"
#include "string_overrides.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
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
#include "game/russ.h"
#include "game/settings_screen.h"
#include "game/timer.h"
#include "game/user_paths.h"
#include "game/vfs.h"
#include "game/video.h"

namespace SETTINGS_SCREEN {

    namespace {

        // Full-window layout, mapped to the colony build screen
        // (COLBLDG.LBX entry 0, colbldg.cpp): the screen fills the
        // whole 640x480 window, tabs at the top, the list filling
        // the main panel, the help/status panel above the footer
        // buttons at the bottom. The colony build screen's panels
        // are baked into its 640x480 background art, so the frame
        // here is the engine-drawn equivalent: a thin light-grey
        // outer line with a dark inner line and corner rivets,
        // thin grey panel borders, and framed row slots.
        constexpr int16_t TITLE_X = 16;
        constexpr int16_t TITLE_Y = 8;
        constexpr int16_t TAB_Y = 30;
        constexpr int16_t TAB_HEIGHT = 24;
        constexpr int16_t TITLE_WIDTH = 348;
        constexpr int16_t VALUE_X = 372;
        constexpr int16_t VALUE_WIDTH = 244;
        constexpr int16_t LIST_PANEL_X = 8;
        constexpr int16_t LIST_PANEL_Y = 62;
        constexpr int16_t LIST_PANEL_W = 624;
        constexpr int16_t LIST_PANEL_H = 332;
        constexpr int16_t FIRST_ROW_Y = 64;
        constexpr int16_t ROW_HEIGHT = 30;
        constexpr int ROWS_PER_PAGE = 11;
        constexpr int16_t ROW_TEXT_Y_OFFSET = 8;
        constexpr int16_t HELP_PANEL_X = 8;
        constexpr int16_t HELP_PANEL_Y = 398;
        constexpr int16_t HELP_PANEL_W = 624;
        constexpr int16_t HELP_PANEL_H = 50;
        constexpr int16_t HELP_Y = 402;
        constexpr int16_t HELP_LINE_HEIGHT = 15;
        constexpr int16_t STATUS_Y = 433;
        constexpr int16_t FOOTER_Y = 457;
        constexpr int16_t NAV_Y = 452;
        constexpr int16_t NAV_Y_END = 474;
        constexpr int16_t TEXT_WIDTH = 608;

        // Colony chrome, in menu-palette indices (MAINMENU.LBX
        // entries 1/2 palette: 230-249 is a blue-grey ramp).
        // 244 is the light outer frame, 230 the dark panel
        // interior, 234 the mid-grey panel border, 229 the dark
        // row separator.
        constexpr uint8_t PANEL_INTERIOR = 230;
        constexpr uint8_t PANEL_BORDER = 234;
        constexpr uint8_t PANEL_SEPARATOR = 229;
        constexpr uint8_t FRAME_OUTER = 244;
        constexpr uint8_t FRAME_INNER = 230;
        constexpr uint8_t FRAME_RIVET = 244;

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
        // look is carried by the list structure (separators,
        // buttons) as much as by the hue.
        s_colors g_colony_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
        s_colors g_colony_highlight_colors = {{150, 155, 155, 152, 153, 155, 155, 155}};
        s_colors g_colony_dim_colors = {{244, 249, 248, 249, 248, 249, 248, 249}};

        // Flat text colours for the raised metallic buttons: dark
        // text on the light metal face, light text on the blue
        // primary face. A font style maps glyph shade to a palette
        // index, so a flat array renders a single-colour label.
        s_colors g_dark_text_colors = {{230, 230, 230, 230, 230, 230, 230, 230}};
        s_colors g_light_text_colors = {{247, 247, 247, 247, 247, 247, 247, 247}};

        // The colony buy button (COLSUM.LBX entries 11-20, colsum.cpp
        // Add_Fields / fields::Add_Locked_Button_Field) is the
        // diamond Tom means: a 19x28-30 two-frame sprite, a dark
        // plaque with a grey ring around a four-point star.
        // fields.cpp draws frame 0 while *selection == 0 and frame 1
        // once it is set, and selection is the colony's
        // bought_outright — so frame 0 (grey star) is "not bought",
        // frame 1 (blue star) is "bought". A colony that cannot buy
        // gets no live button at all, only the dim disc baked into
        // the colony background. The three states map to the
        // settings toggle as off / on / disabled.
        //
        // The star's exact shape, decoded from COLSUM.LBX entry 11
        // frame 1 (blue indices 38-62 in the colony palette): a
        // vertical diamond 11 wide x 14 tall, rows y 8-21 of the
        // 19x29 sprite, widest (x 4-14) at rows 13-14 and 17, with
        // a bright centre and a bright horizontal bar at row 13.
        // The diamond below is that same shape, 1:1, as row spans.
        //
        // The literal sprite cannot be drawn here: the settings
        // screen runs under the main-menu palette, where the
        // colony palette's blue indices 38-62 are browns and
        // olives (MAINMENU.LBX palette), so the sprite would render
        // brown, not blue. The toggle is therefore an engine-drawn
        // replica in the closest menu-palette equivalents, keeping
        // the sprite's own structure: a darker rim, a brighter
        // body, and a small pale centre. On is the most saturated
        // menu blue, 138 (53,49,138), rimming a pale cyan-blue
        // body, 154 (121,182,186), with a near-white centre, 247;
        // off is the menu grey ramp, 232 rimming 236 with a 246
        // centre; disabled is the bare plaque with no star, like
        // the colony's no-button state. Plaque and ring keep the
        // sprite's 19x28 size, so rows are 30 tall (the colony
        // summary's rows are 31).
        constexpr int16_t TOGGLE_W = 19;
        constexpr int16_t TOGGLE_H = 28;
        constexpr uint8_t TOGGLE_PLAQUE = 230;
        constexpr uint8_t TOGGLE_BORDER = 234;
        constexpr uint8_t TOGGLE_RING = 238;
        constexpr uint8_t TOGGLE_RING_DIM = 231;
        constexpr uint8_t TOGGLE_ON_RIM = 138;
        constexpr uint8_t TOGGLE_ON_BASE = 154;
        constexpr uint8_t TOGGLE_ON_BRIGHT = 247;
        constexpr uint8_t TOGGLE_OFF_RIM = 232;
        constexpr uint8_t TOGGLE_OFF_BASE = 236;
        constexpr uint8_t TOGGLE_OFF_BRIGHT = 246;

        // Diamond row spans (x1, x2), plaque-relative, rows y+7 to
        // y+20 of the 19x28 plaque. Decoded from COLSUM.LBX entry
        // 11 frame 1.
        constexpr int8_t DIAMOND_SPANS[14][2] = {
            {7, 11}, {6, 12}, {5, 13}, {5, 13}, {5, 14}, {4, 14}, {4, 14},
            {5, 14}, {5, 14}, {4, 14}, {5, 13}, {6, 13}, {7, 12}, {10, 10},
        };

        enum e_toggle_state {
            TOGGLE_OFF,
            TOGGLE_ON,
            TOGGLE_DISABLED,
        };

        // Draw the colony buy button's diamond toggle at (x, y):
        // dark plaque, grey ring, four-point star in the on/off
        // colours; disabled draws the bare plaque with no star.
        void Draw_Diamond_Toggle(int16_t x, int16_t y, e_toggle_state state) {
            graphics::Fill(x, y, x + TOGGLE_W - 1, y + TOGGLE_H - 1, TOGGLE_PLAQUE);
            graphics::Box(x, y, TOGGLE_W, TOGGLE_H, 1,
                state == TOGGLE_DISABLED ? TOGGLE_RING_DIM : TOGGLE_BORDER);
            graphics::Fill(x + 1, y + 1, x + TOGGLE_W - 2, y + 1, TOGGLE_RING);
            const uint8_t ring =
                state == TOGGLE_DISABLED ? TOGGLE_RING_DIM : TOGGLE_RING;
            // Ring: octagon outline around the star, as the sprite's
            // grey oval ring.
            line::Line(x + 7, y + 3, x + 11, y + 3, ring);
            line::Line(x + 11, y + 3, x + 15, y + 7, ring);
            line::Line(x + 15, y + 7, x + 15, y + 20, ring);
            line::Line(x + 15, y + 20, x + 11, y + 24, ring);
            line::Line(x + 11, y + 24, x + 7, y + 24, ring);
            line::Line(x + 7, y + 24, x + 3, y + 20, ring);
            line::Line(x + 3, y + 20, x + 3, y + 7, ring);
            line::Line(x + 3, y + 7, x + 7, y + 3, ring);
            if (state == TOGGLE_DISABLED) {
                return;
            }
            const uint8_t rim =
                state == TOGGLE_ON ? TOGGLE_ON_RIM : TOGGLE_OFF_RIM;
            const uint8_t base =
                state == TOGGLE_ON ? TOGGLE_ON_BASE : TOGGLE_OFF_BASE;
            const uint8_t bright =
                state == TOGGLE_ON ? TOGGLE_ON_BRIGHT : TOGGLE_OFF_BRIGHT;
            for (int row = 0; row < 14; ++row) {
                graphics::Fill(x + DIAMOND_SPANS[row][0], y + 7 + row,
                    x + DIAMOND_SPANS[row][1], y + 7 + row, rim);
            }
            // Brighter body, inset one pixel from the rim on every
            // side but the pointed ends.
            for (int row = 1; row < 13; ++row) {
                graphics::Fill(x + DIAMOND_SPANS[row][0] + 1, y + 7 + row,
                    x + DIAMOND_SPANS[row][1] - 1, y + 7 + row, base);
            }
            // Small pale star centre: a 2-pixel vertical bar with
            // the full-width horizontal bar at y+12.
            graphics::Fill(x + 9, y + 10, x + 10, y + 13, bright);
            graphics::Fill(x + 6, y + 12, x + 13, y + 12, bright);
        }

        // Raised metallic button, like the colony build screen's
        // REFIT / DESIGN / CANCEL / OK: light face, bright top and
        // left edges, dark bottom and right edges, dark label.
        // A primary (blue) button takes the Auto Build blue face
        // with a light label instead.
        constexpr uint8_t METAL_FACE = 242;
        constexpr uint8_t METAL_HIGHLIGHT = 247;
        constexpr uint8_t METAL_SHADOW = 232;
        constexpr uint8_t PRIMARY_FACE = 138;
        constexpr uint8_t PRIMARY_HIGHLIGHT = 154;

        void Draw_Metal_Button(int16_t x, int16_t y, int16_t w, int16_t h,
            bool primary) {
            const uint8_t face = primary ? PRIMARY_FACE : METAL_FACE;
            const uint8_t highlight =
                primary ? PRIMARY_HIGHLIGHT : METAL_HIGHLIGHT;
            graphics::Fill(x, y, x + w - 1, y + h - 1, face);
            graphics::Fill(x, y, x + w - 1, y, highlight);
            graphics::Fill(x, y, x, y + h - 1, highlight);
            graphics::Fill(x, y + h - 1, x + w - 1, y + h - 1, METAL_SHADOW);
            graphics::Fill(x + w - 1, y, x + w - 1, y + h - 1, METAL_SHADOW);
        }

        enum e_apply_result {
            APPLY_LIVE,
            APPLY_SAVED,
            APPLY_FAILED,
        };

        // Categories, in tab order. A tab is the cleanest fit for the
        // existing field system: the screen is already a paged list
        // with Prev/Next/Done fields, so a tab is one more row of
        // hidden fields above the list, and each tab keeps its own
        // page and selection. Sections (one long list with headers)
        // would break the paging and edit-field geometry instead.
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
            int page = 0;
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
            int16_t prev_field = -1;
            int16_t next_field = -1;
            int16_t done_field = -1;
            int16_t show_all_field = -1;
            // Show all is global for the screen, not per tab: the user
            // is either browsing the most-used settings or everything.
            // It resets when the screen reopens (fresh state).
            bool show_all = false;
            int16_t tab_fields[CATEGORY_COUNT];
            int16_t row_fields[ROWS_PER_PAGE];
        };

        s_settings_screen_state g_state;

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

        int Page_Count() {
            const int count = Visible_Count();
            if (count == 0) {
                return 1;
            }
            return (count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
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
                // Secondary settings stay hidden until Show all. The
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

        // Rebuilds the tab, row and navigation fields for the current
        // page. Only call when no edit field is open: the edit field is
        // added separately by Open_Edit and removed by Commit/Cancel_Edit.
        void Rebuild_Fields() {
            fields::Clear_Fields();
            fonts::Set_Font_Style(4, &g_normal_colors);
            int16_t tab_x = TITLE_X;
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                const int16_t width = (int16_t)(
                    fonts::Get_String_Width(CATEGORY_LABELS[category]) + 16);
                g_state.tab_fields[category] = fields::Add_Hidden_Field(
                    tab_x, TAB_Y, tab_x + width - 1, TAB_Y + TAB_HEIGHT - 1, "", 0);
                tab_x += width + 8;
            }
            const int first = g_state.page * ROWS_PER_PAGE;
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const int16_t y = FIRST_ROW_Y + (int16_t)index * ROW_HEIGHT;
                g_state.row_fields[index] = fields::Add_Hidden_Field(
                    8, y, 632, y + ROW_HEIGHT - 1, "", 0);
            }
            for (int index = count; index < ROWS_PER_PAGE; ++index) {
                g_state.row_fields[index] = -1;
            }
            g_state.prev_field = g_state.page > 0
                ? fields::Add_Hidden_Field(16, NAV_Y, 90, NAV_Y_END, "P", 0)
                : -1;
            g_state.next_field = g_state.page + 1 < Page_Count()
                ? fields::Add_Hidden_Field(110, NAV_Y, 184, NAV_Y_END, "N", 0)
                : -1;
            // Show all sits in the footer between the centred note and
            // Done: the note ends near x=444 at most, Done starts at
            // 556, so 456-544 is clear on every tab.
            g_state.show_all_field = fields::Add_Hidden_Field(
                456, NAV_Y, 544, NAV_Y_END, "", 0);
            g_state.done_field = fields::Add_Hidden_Field(556, NAV_Y, 624, NAV_Y_END, "", 0);
        }

        void Change_Page(int delta) {
            const int next = g_state.page + delta;
            if (next < 0 || next >= Page_Count() || next == g_state.page) {
                return;
            }
            g_state.page = next;
            g_state.selected = next * ROWS_PER_PAGE;
            Rebuild_Fields();
        }

        void Toggle_Show_All() {
            g_state.show_all = !g_state.show_all;
            g_state.page = 0;
            g_state.selected = 0;
            Rebuild_Visible_Rows();
            g_state.status = g_state.show_all
                ? "Showing all settings, including advanced and ordering options."
                : "Showing the most-used settings. Use Show all for the rest.";
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
            g_state.page = 0;
            g_state.selected = 0;
            Rebuild_Visible_Rows();
            if (category == CATEGORY_MODS) {
                Read_Enabled_Set();
                Refresh_Mods();
                g_state.status =
                    "Click a mod to enable or disable it. Broken mods cannot be enabled.";
            } else {
                g_state.status =
                    "Click a setting to change it. * marks settings that need a restart.";
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
            const int16_t y = FIRST_ROW_Y +
                (int16_t)(visible - g_state.page * ROWS_PER_PAGE) * ROW_HEIGHT;
            g_state.edit_field = fields::Add_Continuous_String_Input_Field(
                VALUE_X, y, VALUE_WIDTH, ROW_HEIGHT - 2, g_state.edit_buffer,
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
            int16_t y = FIRST_ROW_Y +
                (int16_t)(visible - g_state.page * ROWS_PER_PAGE) * ROW_HEIGHT;
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
                    Open_Dropdown(visible);
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

        void Draw_Settings_Screen() {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 639, 479, 0);

            // Full-window colony chrome: a thin light outer line, a
            // dark inner line, and corner rivets, like the frame
            // baked into the colony screens' backgrounds.
            graphics::Box(0, 0, 640, 480, 1, FRAME_OUTER);
            graphics::Box(2, 2, 636, 476, 1, FRAME_INNER);
            graphics::Fill(4, 4, 6, 6, FRAME_RIVET);
            graphics::Fill(633, 4, 635, 6, FRAME_RIVET);
            graphics::Fill(4, 473, 6, 475, FRAME_RIVET);
            graphics::Fill(633, 473, 635, 475, FRAME_RIVET);

            fonts::Set_Font_Style(4, &g_title_colors);
            const char* title = "Settings";
            fonts::Print((640 - fonts::Get_String_Width(title)) / 2, TITLE_Y, title);
            fonts::Set_Font_Style(4, &g_normal_colors);
            char page_text[32];
            snprintf(page_text, sizeof(page_text), "Page %d of %d",
                g_state.page + 1, Page_Count());
            fonts::Print(640 - 16 - fonts::Get_String_Width(page_text), TITLE_Y, page_text);

            // Tabs, like the colony summary's sort buttons: dark
            // buttons, the active tab in the Auto Build blue.
            int16_t tab_x = TITLE_X;
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                const int16_t width = (int16_t)(
                    fonts::Get_String_Width(CATEGORY_LABELS[category]) + 16);
                const bool active = category == g_state.category;
                graphics::Fill(tab_x, TAB_Y, tab_x + width - 1,
                    TAB_Y + TAB_HEIGHT - 1, active ? PRIMARY_FACE : 232);
                graphics::Box(tab_x, TAB_Y, width, TAB_HEIGHT, 1, PANEL_BORDER);
                fonts::Set_Font_Style(4,
                    active ? &g_title_colors : &g_colony_dim_colors);
                const int16_t label_x = (int16_t)(tab_x +
                    (width - fonts::Get_String_Width(CATEGORY_LABELS[category])) / 2);
                fonts::Print(label_x, TAB_Y + 5, CATEGORY_LABELS[category]);
                tab_x += width + 8;
            }

            // List panel, like the colony list: dark interior, thin
            // grey border, framed row slots even when empty.
            graphics::Fill(LIST_PANEL_X, LIST_PANEL_Y,
                LIST_PANEL_X + LIST_PANEL_W - 1, LIST_PANEL_Y + LIST_PANEL_H - 1,
                PANEL_INTERIOR);
            graphics::Box(LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_W, LIST_PANEL_H, 1,
                PANEL_BORDER);
            for (int slot = 0; slot < ROWS_PER_PAGE - 1; ++slot) {
                const int16_t sep_y = (int16_t)(FIRST_ROW_Y + slot * ROW_HEIGHT +
                    ROW_HEIGHT - 1);
                graphics::Fill(LIST_PANEL_X + 1, sep_y,
                    LIST_PANEL_X + LIST_PANEL_W - 2, sep_y, PANEL_SEPARATOR);
            }

            const bool mods_tab = g_state.category == CATEGORY_MODS;
            const int first = g_state.page * ROWS_PER_PAGE;
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const int visible = first + index;
                const int16_t y = FIRST_ROW_Y + (int16_t)index * ROW_HEIGHT;
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
                    // Colony buy-button diamond: blue when on, grey
                    // when off, bare plaque when the mod is broken
                    // and cannot be enabled.
                    const int16_t toggle_y = (int16_t)(y + (ROW_HEIGHT - TOGGLE_H) / 2);
                    if (entry.broken) {
                        Draw_Diamond_Toggle(VALUE_X, toggle_y, TOGGLE_DISABLED);
                        fonts::Set_Font_Style(4, &g_colony_dim_colors);
                        fonts::Print(VALUE_X + TOGGLE_W + 6, text_y, "Broken");
                    } else {
                        Draw_Diamond_Toggle(VALUE_X, toggle_y,
                            entry.enabled ? TOGGLE_ON : TOGGLE_OFF);
                    }
                    continue;
                }
                const config::s_settings_row& row =
                    g_state.rows[(size_t)Visible_Row_Index(visible)];
                const std::string title_text = Truncate_To_Width(row.title, TITLE_WIDTH);
                fonts::Print(TITLE_X, text_y, title_text.c_str());
                if (row.restart_required) {
                    const int16_t mark_x = (int16_t)(TITLE_X +
                        fonts::Get_String_Width(title_text.c_str()) + 2);
                    if (mark_x + fonts::Get_String_Width("*") < VALUE_X - 4) {
                        fonts::Set_Font_Style(4, &g_colony_highlight_colors);
                        fonts::Print(mark_x, text_y, "*");
                    }
                }
                // The edit field draws its own text while it is open.
                if (visible == g_state.editing_row) {
                    continue;
                }
                if (row.widget == config::SETTINGS_WIDGET_TOGGLE) {
                    // Colony buy-button diamond instead of the
                    // "[x]"/"[ ]" text: blue when on, grey when off.
                    config::s_typed_value value;
                    const bool on = config::Get_Settings_Row_Value(row, &value)
                        && value.bool_value;
                    const int16_t toggle_y =
                        (int16_t)(y + (ROW_HEIGHT - TOGGLE_H) / 2);
                    Draw_Diamond_Toggle(VALUE_X, toggle_y,
                        on ? TOGGLE_ON : TOGGLE_OFF);
                } else if (row.widget == config::SETTINGS_WIDGET_DROPDOWN) {
                    // Dropdown: the value in a raised metallic button
                    // with a down-arrow — the game's only missing
                    // widget, restyled to the colony build screen's
                    // buttons. The list itself is the shared
                    // fields::Get_List_Field picker; its frame is
                    // that picker's, not the settings screen's.
                    config::s_typed_value value;
                    std::string value_text = config::Get_Settings_Row_Value(row, &value)
                        ? config::Format_Leaf_Value(row.variable_id, value)
                        : "?";
                    value_text = Truncate_To_Width(value_text, VALUE_WIDTH - 16);
                    const int16_t text_w = fonts::Get_String_Width(value_text.c_str());
                    int16_t box_w = (int16_t)(text_w + 24);
                    if (box_w > VALUE_WIDTH) {
                        box_w = VALUE_WIDTH;
                    }
                    const int16_t trigger_y = (int16_t)(y + 5);
                    Draw_Metal_Button(VALUE_X, trigger_y, box_w, 20, false);
                    fonts::Set_Font_Style(4, &g_dark_text_colors);
                    fonts::Print(VALUE_X + 4, trigger_y + 4, value_text.c_str());
                    fonts::Print(VALUE_X + box_w - 12, trigger_y + 4, "v");
                } else {
                    // Info and edit values: text in the colony dim color.
                    config::s_typed_value value;
                    std::string value_text;
                    if (row.widget == config::SETTINGS_WIDGET_INFO) {
                        value_text = row.info;
                    } else {
                        value_text = config::Get_Settings_Row_Value(row, &value)
                            ? config::Format_Leaf_Value(row.variable_id, value)
                            : "?";
                    }
                    fonts::Set_Font_Style(4, &g_colony_dim_colors);
                    fonts::Print(VALUE_X, text_y,
                        Truncate_To_Width(value_text, VALUE_WIDTH).c_str());
                }
            }

            // Help and status panel, like the colony screens' bottom
            // info panels.
            graphics::Fill(HELP_PANEL_X, HELP_PANEL_Y,
                HELP_PANEL_X + HELP_PANEL_W - 1, HELP_PANEL_Y + HELP_PANEL_H - 1,
                PANEL_INTERIOR);
            graphics::Box(HELP_PANEL_X, HELP_PANEL_Y, HELP_PANEL_W, HELP_PANEL_H, 1,
                PANEL_BORDER);
            std::string help_text;
            if (mods_tab && Is_Mod_Row(g_state.selected)) {
                help_text = Mod_Help_Text(g_state.mods[(size_t)Mod_Index(g_state.selected)],
                    g_state.mods);
            } else if (Visible_Count() > 0) {
                help_text = g_state.rows[(size_t)Visible_Row_Index(g_state.selected)].description;
            }
            std::string help_line1;
            std::string help_line2;
            Wrap_Text(help_text, TEXT_WIDTH, &help_line1, &help_line2);
            fonts::Set_Font_Style(4, &g_colony_dim_colors);
            fonts::Print(TITLE_X, HELP_Y, help_line1.c_str());
            if (!help_line2.empty()) {
                fonts::Print(TITLE_X, HELP_Y + HELP_LINE_HEIGHT, help_line2.c_str());
            }
            fonts::Print(TITLE_X, STATUS_Y,
                Truncate_To_Width(g_state.status, TEXT_WIDTH).c_str());

            // Footer buttons, like the colony build screen's raised
            // buttons: metallic Prev / Next / Show all, blue Done.
            // Each button rect matches its field; the label is centred.
            const auto draw_action_button = [](int16_t x, int16_t w, const char* label,
                bool primary) {
                Draw_Metal_Button(x, NAV_Y, w, NAV_Y_END - NAV_Y, primary);
                fonts::Set_Font_Style(4,
                    primary ? &g_light_text_colors : &g_dark_text_colors);
                const int16_t label_x = (int16_t)(x +
                    (w - fonts::Get_String_Width(label)) / 2);
                fonts::Print(label_x, FOOTER_Y, label);
            };
            if (g_state.prev_field != -1) {
                draw_action_button(TITLE_X, 74, "Prev", false);
            }
            if (g_state.next_field != -1) {
                draw_action_button(110, 74, "Next", false);
            }
            draw_action_button(456, 88, g_state.show_all ? "Show less" : "Show all",
                false);
            draw_action_button(556, 68, "Done", true);
            const char* footer_note = mods_tab
                ? "Mod changes apply immediately"
                : "* = takes effect after restart";
            fonts::Set_Font_Style(4, &g_colony_dim_colors);
            fonts::Print((640 - fonts::Get_String_Width(footer_note)) / 2, FOOTER_Y,
                footer_note);

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
        g_state.status =
            "Click a setting to change it. * marks settings that need a restart.";
        Resolve_Target();
        Read_Enabled_Set();
        Rebuild_Visible_Rows();

        video::Set_Refresh_Full();
        fields::Clear_Fields();
        RUSS::Mox_Update();
        fonts::Set_Font_Style(4, &g_normal_colors);
        fonts::Set_Font_LF(4);
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
                // Anything other than the edit field or Done cancels a
                // pending edit (Done commits it instead). The cancel
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
                } else if (input == g_state.prev_field) {
                    Change_Page(-1);
                } else if (input == g_state.next_field) {
                    Change_Page(1);
                } else if (input == g_state.show_all_field) {
                    Toggle_Show_All();
                } else if (input == g_state.edit_field) {
                    // Enter commits (input_field_active is cleared by the
                    // field system on Enter); a click only focuses the
                    // field and keeps editing.
                    if (fields::input_field_active == 0) {
                        Commit_Edit();
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
                        const int first = g_state.page * ROWS_PER_PAGE;
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
