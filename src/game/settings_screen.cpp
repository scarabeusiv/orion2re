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
#include "game/mouse.h"
#include "game/palette.h"
#include "game/platform.h"
#include "game/russ.h"
#include "game/settings_background.h"
#include "game/settings_hd_font.h"
#include "game/settings_layout.h"
#include "game/settings_render.h"
#include "game/settings_screen.h"
#include "game/timer.h"
#include "game/user_paths.h"
#include "game/vfs.h"
#include "game/video.h"

namespace SETTINGS_SCREEN {

    namespace {

        // Hi-res rewrite, steps 1-3: the pixel rendering is stripped.
        // What stays is the settings LOGIC -- the config writer,
        // the mod catalogue + staging/reload, the metadata rows,
        // the categories, the XDG fallback, the wheel forwarding
        // (via the field system), the screen's data flow (which
        // settings are shown, primary/secondary, the Show all
        // toggle). What is gone: the wireframe/kit rendering
        // (ui_kit: Frame/Tab_Bar/Scroll_Bar/Button, the dithered
        // fills and boxes, the Bayer/hash speckle, the vivid-blue
        // palette-253 patch, the Moo2 pixel fonts as rendered, the
        // verbatim COLSUM arrow sprites and the code-drawn
        // triangles/bevels). The rendering text helpers
        // (Truncate_To_Width, Wrap_Text, Mod_Help_Text) go with
        // it; step 3/4 re-adds the ones the hi-res layout needs.
        // Step 3: the field rects come from the percentage-based
        // layout (settings_layout), computed for the current game
        // resolution -- the same rects draw the placeholder frames
        // and take the clicks, so drawing and hit-testing agree.
        // The screen renders the background plus placeholders
        // until step 4 renders the elements themselves.
        // Slot geometry for wrapped rows (defined with the draw
        // helpers, after the font helpers): a row whose label
        // wraps takes two slots, so the label renders at full
        // size. Declared here because Rebuild_Fields and the
        // popups use them before their definitions.
        void Rebuild_Row_Lines();
        int Page_Count();
        SETTINGS_LAYOUT::s_rect Visible_Row_Rect(int offset);
        SETTINGS_LAYOUT::s_rect Visible_Value_Rect(int offset);

        constexpr int ROWS_PER_PAGE = 10;

        // The standard UI text palette, as used by the council screen.
        s_colors g_normal_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};

        // Colony-list palette, as rendered on the settings screen.
        // The colony screen loads palette 1 (colony.cpp), the
        // settings screen the menu palette, so the colony's own
        // indices do NOT map to its blue here -- the restyle uses
        // the menu palette's own bright cyan (150-155) as the
        // colony-blue equivalent, with a bright white for
        // secondary info.
        s_colors g_colony_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
        s_colors g_colony_dim_colors = {{244, 249, 248, 249, 248, 249, 248, 249}};

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
            // Lines each visible row takes (1, or 2 for a row whose
            // label wraps onto a second line), parallel to the
            // visible indices (settings rows, then catalogue
            // entries on the Mods tab). Rebuilt with the visible
            // rows: a wrapped row takes two slots, so its label
            // renders at full size.
            std::vector<int> row_lines;
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
            // COLSUM::Update_First. A fresh tab opens at the top:
            // its slider starts at its maximum on first visit
            // (slider_initialised), and keeps its position on
            // later visits.
            int16_t slider_position[CATEGORY_COUNT] = {};
            int scroll_top[CATEGORY_COUNT] = {};
            bool slider_initialised[CATEGORY_COUNT] = {};
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

        // Layout (step 3 of the hi-res rewrite): every element
        // rect comes from the percentage-based layout
        // (settings_layout), computed for the current game
        // resolution. The same rects draw the placeholder frames
        // and take the clicks, so drawing and hit-testing agree.
        // The tabs sit in the bottom bar with MORE and RETURN:
        // the header is the first band, and the list takes the
        // space the old top tab bar used.
        SETTINGS_LAYOUT::s_settings_layout Layout() {
            int lengths[CATEGORY_COUNT];
            for (int c = 0; c < CATEGORY_COUNT; ++c) {
                lengths[c] = (int)strlen(CATEGORY_LABELS[c]);
            }
            return SETTINGS_LAYOUT::Compute_Layout(
                video::g_screen_res_x, video::g_screen_res_y,
                ROWS_PER_PAGE, lengths);
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

        // Maximum first-visible-row index on the current tab:
        // the smallest first index whose settings to the end fit
        // in the page's row slots (a wrapped row takes two).
        int Max_Scroll() {
            return SETTINGS_LAYOUT::First_For_Last_Page(
                g_state.row_lines.data(), Visible_Count(),
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
            // A fresh tab opens at the top: the first visit starts
            // the slider at its maximum (the first rows), and later
            // visits keep the position the user left. The bound
            // value stays inverted -- the field system's drag and
            // wheel need that -- only the starting position changes.
            if (!g_state.slider_initialised[g_state.category]) {
                slider = (int16_t)max_scroll;
                g_state.slider_initialised[g_state.category] = true;
            }
            if (slider < 0) {
                slider = 0;
            }
            if (slider > max_scroll) {
                slider = (int16_t)max_scroll;
            }
            // COLSUM's inverted slider: the top row is the
            // maximum minus the slider.
            g_state.scroll_top[g_state.category] = max_scroll - slider;
        }

        // First visible row on the current tab (after Sync_Scroll).
        int Scroll_Top() {
            return g_state.scroll_top[g_state.category];
        }

        // Rows index for a visible index in a settings category.
        int Visible_Row_Index(int visible) {
            return g_state.visible_rows[(size_t)visible];
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

        // The seeded user config must be a proper readable file:
        // the writer copies the target's permissions onto each
        // rewrite, so a seeded copy inherits the source's mode --
        // a write-only (or read-only) source would poison every
        // later write. Normalize to 0644 (owner read/write,
        // group/others read) regardless of what the source's
        // mode was.
        bool Normalize_User_Config_Permissions(std::string* error) {
            std::error_code ec;
            std::filesystem::permissions(g_state.target_path,
                std::filesystem::perms::owner_read |
                    std::filesystem::perms::owner_write |
                    std::filesystem::perms::group_read |
                    std::filesystem::perms::others_read,
                std::filesystem::perm_options::replace, ec);
            if (ec) {
                *error = "could not set permissions on the configuration file";
                return false;
            }
            return true;
        }

        // Seeds the user config from the settings the game is
        // running with, when no file is available to copy: every
        // settings row's live value, plus the enabled mods. Never
        // an empty file -- an empty user file shadows the live
        // settings on the next reload (the loader reads the user
        // file first), so a toggle that "worked" would silently
        // drop every other setting.
        bool Write_Live_Settings_Seed(std::string* error) {
            std::string content =
                "# Orion2 Redone configuration.\n"
                "# Seeded from the settings the game is running with;\n"
                "# edit the values in the Settings screen.\n";
            for (const config::s_settings_row& row : g_state.rows) {
                if (row.variable_id == config::CONFIG_NO_VARIABLE) {
                    continue;
                }
                config::s_typed_value value;
                if (!config::Get_Settings_Row_Value(row, &value)) {
                    continue;
                }
                content += row.path + " = " +
                    config::Format_Leaf_Value_For_Config(row.variable_id, value) +
                    ";\n";
            }
            for (const std::string& id : g_state.enabled_ids) {
                content += "enable " + id + ";\n";
            }
            std::ofstream output(g_state.target_path,
                std::ios::binary | std::ios::trunc);
            if (!output) {
                *error = "could not create the configuration file";
                return false;
            }
            output.write(content.data(), (std::streamsize)content.size());
            output.flush();
            if (!output) {
                *error = "could not write the configuration file";
                return false;
            }
            return Normalize_User_Config_Permissions(error);
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
                return Normalize_User_Config_Permissions(error);
            }
            // No file to copy from: seed from the live settings,
            // never an empty file (it would shadow the live
            // settings on the next reload).
            return Write_Live_Settings_Seed(error);
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
            Rebuild_Row_Lines();
        }

        void Refresh_Mods() {
            g_state.mods = config::Build_Mod_Catalogue(g_state.enabled_ids);
            if (g_state.selected >= Visible_Count()) {
                g_state.selected = Visible_Count() - 1;
            }
            if (g_state.selected < 0) {
                g_state.selected = 0;
            }
            Rebuild_Row_Lines();
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
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            // Category tabs in the bottom bar: one hidden field
            // per category, left to right, each keeping its own
            // scroll position.
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                const SETTINGS_LAYOUT::s_rect tab = layout.tabs[category];
                g_state.tab_fields[category] = fields::Add_Hidden_Field(
                    tab.x, tab.y, (int16_t)(tab.x + tab.w - 1),
                    (int16_t)(tab.y + tab.h - 1), "", 0);
            }
            Sync_Scroll();
            const int first = Scroll_Top();
            const int count = Page_Count();
            for (int index = 0; index < count; ++index) {
                // The field spans the row's slots: two for a
                // wrapped label, one otherwise.
                const SETTINGS_LAYOUT::s_rect row =
                    Visible_Row_Rect(index);
                // Row fields span the list area, stopping short of
                // the scrollbar strip, so clicks on the bar reach
                // the bar.
                g_state.row_fields[index] = fields::Add_Hidden_Field(
                    row.x, row.y, (int16_t)(row.x + row.w - 1),
                    (int16_t)(row.y + row.h - 1), "", 0);
            }
            for (int index = count; index < ROWS_PER_PAGE; ++index) {
                g_state.row_fields[index] = -1;
            }
            // Scrollbar drag field, bound to this tab's slider
            // position, like COLSUM's Add_Scroll_Field. Only when
            // the list is longer than the window; the field
            // system's lone-scrollbar rule makes the wheel scroll
            // it from anywhere over the list.
            const int total = Visible_Count();
            if (total > ROWS_PER_PAGE) {
                g_state.scroll_field = fields::Add_Scroll_Field(
                    layout.scrollbar.x, layout.scrollbar.y, 0,
                    (int16_t)total, 0, (int16_t)Max_Scroll(),
                    layout.scrollbar.w, layout.scrollbar.h,
                    &g_state.slider_position[g_state.category], "", 0);
            } else {
                g_state.scroll_field = -1;
            }
            // MORE and RETURN sit side by side in the bottom
            // bar at the right end. They share the bar's single
            // line and never wrap.
            g_state.show_all_field = fields::Add_Hidden_Field(
                layout.show_more.x, layout.show_more.y,
                (int16_t)(layout.show_more.x + layout.show_more.w - 1),
                (int16_t)(layout.show_more.y + layout.show_more.h - 1),
                "", 0);
            g_state.done_field = fields::Add_Hidden_Field(
                layout.done.x, layout.done.y,
                (int16_t)(layout.done.x + layout.done.w - 1),
                (int16_t)(layout.done.y + layout.done.h - 1), "", 0);
        }

        void Toggle_Show_All() {
            g_state.show_all = !g_state.show_all;
            g_state.selected = 0;
            Rebuild_Visible_Rows();
            // The row set changed, so the scroll resets to the top:
            // the slider at its maximum shows the first rows.
            g_state.slider_position[g_state.category] = (int16_t)Max_Scroll();
            g_state.slider_initialised[g_state.category] = true;
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
            if (category == CATEGORY_MODS) {
                Read_Enabled_Set();
                Refresh_Mods();
                g_state.status =
                    "Mod changes apply immediately. Click a mod to enable or disable it.";
            } else {
                g_state.status = "Click a setting to change it.";
            }
            // Sync after the Mods catalogue loads: the catalogue
            // changes the visible count, so the first-visit
            // initialisation must see the final count -- otherwise
            // the Mods tab initialises against the two settings
            // rows alone (max scroll 0) and opens at the bottom
            // once the catalogue lands.
            Sync_Scroll();
            // The category keeps its own scroll position (per-category slider
            // and top row); the selection moves to the first visible
            // row so the highlight stays on screen.
            g_state.selected = g_state.scroll_top[category];
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
            // The edit field is the row's value-control rect: the
            // same rect the placeholder draws, so the click that
            // opened the edit and the typing land in the same place.
            const SETTINGS_LAYOUT::s_rect value_rect =
                Visible_Value_Rect(visible - Scroll_Top());
            g_state.edit_field = fields::Add_Continuous_String_Input_Field(
                value_rect.x, value_rect.y, value_rect.w, value_rect.h,
                g_state.edit_buffer,
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
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            int16_t x = layout.header_value.x;
            const int16_t content_right =
                SETTINGS_LAYOUT::Rect_Right(layout.content);
            if (x + width > content_right) {
                x = (int16_t)(content_right - width);
            }
            const int16_t y_step = (int16_t)(fonts::Get_Font_Height() +
                fonts::Get_Font_Vertical_Spacing());
            const int16_t list_height =
                (int16_t)((type->value_count + 1) * (size_t)y_step);
            int16_t y = Visible_Row_Rect(visible - Scroll_Top()).y;
            const int16_t content_bottom =
                SETTINGS_LAYOUT::Rect_Bottom(layout.content);
            if (y + list_height > content_bottom) {
                y = (int16_t)(content_bottom - list_height);
            }
            if (y < layout.content.y) {
                y = layout.content.y;
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

            // Geometry: the list is sized to the longest label, in
            // the font it prints in (style 4 -- measuring in the
            // font active at open time, style 1 for a row click,
            // under-measures: style 4's "3840x2880" is ~111 px
            // against style 1's ~63, so the text ran past the
            // popup's border and the panel's edge, into the frame).
            // The list is right-aligned to the dropdown button's
            // right edge (the closed control's right edge, sized
            // to the current value in style 1 like the row draw),
            // and clamped to the frame's content bounds, so the
            // widest item always fits on screen.
            fonts::Set_Font_Style(4, &g_colony_colors);
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
            // The dropdown button's right edge, like the row draw:
            // the current value in style 1, plus the button chrome
            // (padding, chevron, padding), inside the row's
            // value-control rect.
            fonts::Set_Font_Style(1, &g_colony_dim_colors);
            const std::string current_label =
                config::Format_Resolution(current_width, current_height);
            const int16_t chevron_w = fonts::Get_String_Width("v");
            const int16_t button_w = (int16_t)(
                fonts::Get_String_Width(current_label.c_str()) +
                4 + 4 + chevron_w + 4);
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const SETTINGS_LAYOUT::s_rect value_rect =
                Visible_Value_Rect(visible - Scroll_Top());
            const int16_t button_right =
                (int16_t)(value_rect.x + button_w - 1);
            int16_t x = (int16_t)(button_right + 1 - popup_w);
            if (x < layout.content.x) {
                x = layout.content.x;
            }
            if (x + popup_w > SETTINGS_LAYOUT::Rect_Right(layout.content)) {
                x = (int16_t)(SETTINGS_LAYOUT::Rect_Right(layout.content) -
                    popup_w);
            }
            int16_t y = value_rect.y;
            const int16_t content_bottom =
                SETTINGS_LAYOUT::Rect_Bottom(layout.content);
            if (y + popup_h > content_bottom) {
                y = (int16_t)(content_bottom - popup_h);
            }
            if (y < layout.content.y) {
                y = layout.content.y;
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

        // Step 4 of the hi-res rewrite: the elements themselves,
        // rendered in hi-res on the generated background. Every
        // element draws its OWN border/frame as part of itself
        // (multi-pixel industrial frames); panels are genuinely
        // translucent -- a real per-pixel alpha blend over the
        // background (settings_render), never a repaint of the
        // background onto the panel. Text is the HD font: the
        // engine font's own glyphs, decoded into coverage grids
        // and rendered upscaled with bilinear smoothing, so the
        // letterforms stay the game's own and the edges come out
        // antialiased instead of doubled square pixels.
        //
        // Draw order: the background, then the field system's own
        // drawing (the edit field's pixel-font text, when editing),
        // then the elements -- the edit overpaint below erases
        // that pixel text with an opaque fill and draws the HD
        // text over it, so no pixel-font text ever shows.
        //
        // The VALUE header cell keeps its own frame: it is the
        // VALUE column header, and step 4 labels it "Value" (with
        // "Setting" over the name column) instead of leaving it
        // the bare square step 3 showed. That is the top-right
        // corner square, resolved by labelling it, not by
        // removing it: the header row labels the two columns,
        // and the value column needs its header cell.

        // Glyph-cell height for text in a rect of the given height.
        int Text_Cell_For(int rect_h, int percent) {
            return std::max(8, rect_h * percent / 100);
        }

        // Glyph-cell height for text sized as a permille of the
        // screen height. The description text uses this, not a
        // fraction of the description panel: the panel grows by a
        // row so the longest description fits, and the text must
        // stay the same size -- a fraction of the panel would grow
        // the text with the panel, and the line count would never
        // reach four (it asymptotes to 3.6 as the panel grows).
        int Text_Cell_For_Permille(int height, int permille) {
            return std::max(8, height * permille / 1000);
        }

        // Horizontal text padding inside a rect, scaled with the screen.
        int16_t Text_Pad(int16_t screen_width) {
            return (int16_t)std::max<int32_t>(4, screen_width / 100);
        }

        // The value text of a settings row, for the rows that show
        // one (dropdowns and edits; info rows show none, toggles
        // show the star in step 4e). Empty when the value cannot
        // be read.
        std::string Row_Value_Text(const config::s_settings_row& row) {
            if (Is_Resolution_Row(row)) {
                int width = 0;
                int height = 0;
                if (!config::Get_Current_Resolution(&width, &height)) {
                    return std::string();
                }
                return config::Format_Resolution(width, height);
            }
            config::s_typed_value value;
            if (!config::Get_Settings_Row_Value(row, &value)) {
                return std::string();
            }
            return config::Format_Leaf_Value(row.variable_id, value);
        }

        // The name shown for a visible row: a setting's title, or
        // a mod's name (its id when the manifest names nothing).
        std::string Visible_Row_Name(int visible) {
            if (Is_Mod_Row(visible)) {
                const config::s_mod_catalogue_entry& entry =
                    g_state.mods[(size_t)Mod_Index(visible)];
                return entry.name.empty() ? entry.id : entry.name;
            }
            return g_state.rows[(size_t)Visible_Row_Index(visible)].title;
        }

        // The description shown for a visible row in the info box:
        // a setting's metadata description, or a mod's description
        // (its id when the manifest names nothing). `restart_required`
        // is set for a settings row whose change takes effect only
        // after a restart; mods never need one.
        std::string Visible_Row_Description(int visible, bool* restart_required) {
            *restart_required = false;
            if (Is_Mod_Row(visible)) {
                const config::s_mod_catalogue_entry& entry =
                    g_state.mods[(size_t)Mod_Index(visible)];
                return entry.description.empty() ? entry.id : entry.description;
            }
            const config::s_settings_row& row =
                g_state.rows[(size_t)Visible_Row_Index(visible)];
            *restart_required = row.restart_required;
            return row.description;
        }

        // Lines each visible row takes (1, or 2 for a row whose
        // label wraps onto a second line). Rebuilt with the
        // visible rows: the wrap is measured at the full row
        // cell, the same measure the draw uses, so the cache and
        // the draw always agree.
        void Rebuild_Row_Lines() {
            const int total = Visible_Count();
            g_state.row_lines.assign((size_t)total, 1);
            if (total <= 0) {
                return;
            }
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            if (layout.width <= 0) {
                return;
            }
            const int16_t pad = Text_Pad(layout.width);
            const int row_cell = Text_Cell_For(
                SETTINGS_LAYOUT::Row_Rect(layout, 0).h, 62);
            const SETTINGS_LAYOUT::s_rect name_rect =
                SETTINGS_LAYOUT::Row_Name_Rect(layout, 0);
            auto measure = [&](const std::string& text) {
                return SETTINGS_RENDER::Text_Width(text.c_str(), row_cell);
            };
            for (int visible = 0; visible < total; ++visible) {
                const std::vector<std::string> lines =
                    SETTINGS_HD_FONT::Wrap_Text(
                        Visible_Row_Name(visible),
                        name_rect.w - 2 * pad, measure, 2);
                g_state.row_lines[(size_t)visible] =
                    lines.size() > 1 ? 2 : 1;
            }
        }

        // Cached lines for a visible index (1 when unknown).
        int Row_Lines(int visible) {
            if (visible < 0 ||
                visible >= (int)g_state.row_lines.size()) {
                return 1;
            }
            return g_state.row_lines[(size_t)visible];
        }

        // Settings visible on the current page: the page is
        // ROWS_PER_PAGE slots, and a wrapped row takes two.
        int Page_Count() {
            return SETTINGS_LAYOUT::Settings_On_Page(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top(), ROWS_PER_PAGE);
        }

        // First slot of the setting at visible offset `offset`
        // on the current page: the slots the earlier settings
        // on the page take.
        int Visible_Slot(int offset) {
            const int first = Scroll_Top();
            int slot = 0;
            for (int i = 0; i < offset; ++i) {
                slot += Row_Lines(first + i);
            }
            return slot;
        }

        // The row rect for the setting at visible offset
        // `offset` on the current page: its slots merged, so a
        // wrapped label's row is two slots tall.
        SETTINGS_LAYOUT::s_rect Visible_Row_Rect(int offset) {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const int first = Scroll_Top();
            return SETTINGS_LAYOUT::Row_Rect_For_Span(layout,
                Visible_Slot(offset), Row_Lines(first + offset));
        }

        // The value-control rect for the setting at visible
        // offset `offset`: vertically centred in its (possibly
        // two-slot) row.
        SETTINGS_LAYOUT::s_rect Visible_Value_Rect(int offset) {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            return SETTINGS_LAYOUT::Row_Value_Rect_For_Row(layout,
                Visible_Row_Rect(offset));
        }

        // Total lines in the list, first visible line, and lines
        // on the current page (for the scrollbar thumb).
        int Total_Lines() {
            return SETTINGS_LAYOUT::Lines_Before(
                g_state.row_lines.data(), Visible_Count(),
                Visible_Count());
        }

        int First_Line() {
            return SETTINGS_LAYOUT::Lines_Before(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top());
        }

        int Visible_Lines() {
            return SETTINGS_LAYOUT::Lines_On_Page(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top(), ROWS_PER_PAGE);
        }

        void Draw_Elements() {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const int16_t pad = Text_Pad(layout.width);
            const int16_t thickness =
                SETTINGS_RENDER::Frame_Thickness(layout.width);
            using namespace SETTINGS_RENDER;

            // Header: the panel, with the VALUE column's own
            // framed cell inside it, both labelled.
            Panel(layout.header, kPanel_Fill, kPanel_Alpha);
            Blend_Fill(layout.header_value, kControl_Fill, kControl_Alpha);
            Frame(layout.header_value, thickness);
            const int header_cell = Text_Cell_For(layout.header.h, 62);
            const int16_t header_y =
                (int16_t)(layout.header.y + (layout.header.h - header_cell) / 2);
            Draw_Text(
                (int16_t)(layout.header_setting.x +
                    layout.header_setting.w / 2),
                header_y, "SETTING", kText_Header, header_cell, 1);
            Draw_Text(
                (int16_t)(layout.header_value.x + layout.header_value.w / 2),
                header_y, "VALUE", kText_Header, header_cell, 1);

            // List rows: each its own translucent panel; the
            // selected row a brighter fill with a bright left bar.
            // A row whose label wraps takes two slots, so the
            // label renders at full size on two lines.
            const int first = Scroll_Top();
            const int count = Page_Count();
            const int row_cell = Text_Cell_For(
                SETTINGS_LAYOUT::Row_Rect(layout, 0).h, 62);
            const int value_cell = Text_Cell_For(
                SETTINGS_LAYOUT::Row_Value_Rect(layout, 0).h, 80);
            for (int index = 0; index < count; ++index) {
                // The row spans its slots: two for a wrapped
                // label, one otherwise.
                const SETTINGS_LAYOUT::s_rect row =
                    Visible_Row_Rect(index);
                const bool is_selected =
                    first + index == g_state.selected;
                Panel(row, is_selected ? s_rgb{30, 38, 70} : kPanel_Fill,
                    is_selected ? 185 : kPanel_Alpha);
                if (is_selected) {
                    // Bright left bar inside the frame.
                    const SETTINGS_LAYOUT::s_rect bar{
                        (int16_t)(row.x + thickness),
                        (int16_t)(row.y + thickness), 3,
                        (int16_t)(row.h - 2 * thickness)};
                    Solid_Fill(bar, s_rgb{180, 200, 230});
                }
                const int visible = first + index;
                const std::string name = Visible_Row_Name(visible);
                const SETTINGS_LAYOUT::s_rect name_rect =
                    SETTINGS_LAYOUT::Row_Name_Rect_For_Row(layout, row);
                auto measure = [&](const std::string& text) {
                    return Text_Width(text.c_str(), row_cell);
                };
                // The label colour matches the game: cyan, orange
                // on the hovered row. A long label wraps onto a
                // second line at full size: the row grew to two
                // slots for it, so it never truncates with "..."
                // and never shrinks.
                const s_rgb label_color =
                    Is_Hover(row) ? kText_Hover : kText_Label;
                const std::vector<std::string> name_lines =
                    SETTINGS_HD_FONT::Wrap_Text(name,
                        name_rect.w - 2 * pad, measure, 2);
                if (name_lines.size() <= 1) {
                    Draw_Text((int16_t)(name_rect.x + pad),
                        (int16_t)(row.y + (row.h - row_cell) / 2),
                        name_lines.empty() ? "" : name_lines[0].c_str(),
                        label_color, row_cell, 0);
                } else {
                    const int16_t two_y = (int16_t)(row.y +
                        (row.h - (row_cell + Line_Height(row_cell))) / 2);
                    Draw_Text((int16_t)(name_rect.x + pad), two_y,
                        name_lines[0].c_str(), label_color, row_cell, 0);
                    Draw_Text((int16_t)(name_rect.x + pad),
                        (int16_t)(two_y + Line_Height(row_cell)),
                        name_lines[1].c_str(), label_color, row_cell, 0);
                }
                const SETTINGS_LAYOUT::s_rect value_rect =
                    Visible_Value_Rect(index);
                Blend_Fill(value_rect, kControl_Fill, kControl_Alpha);
                Frame(value_rect, thickness);
                // The value: toggles and mods show the star (4e);
                // dropdowns and edits show their text (dropdowns
                // become framed buttons with a chevron in 4d).
                std::string value_text;
                if (!Is_Mod_Row(visible)) {
                    const config::s_settings_row& row_data =
                        g_state.rows[(size_t)Visible_Row_Index(visible)];
                    if (row_data.widget == config::SETTINGS_WIDGET_DROPDOWN ||
                        row_data.widget == config::SETTINGS_WIDGET_EDIT) {
                        value_text = Row_Value_Text(row_data);
                    }
                }
                if (!value_text.empty()) {
                    auto value_measure = [&](const std::string& text) {
                        return Text_Width(text.c_str(), value_cell);
                    };
                    Draw_Text((int16_t)(value_rect.x + pad),
                        (int16_t)(value_rect.y +
                            (value_rect.h - value_cell) / 2),
                        SETTINGS_HD_FONT::Truncate_To_Width(value_text,
                            value_rect.w - 2 * pad, value_measure).c_str(),
                        kText_Bright, value_cell, 0);
                }
            }
            // Slots past the last visible row keep their empty
            // panels, so the list area stays fully panelled.
            for (int slot = Visible_Slot(count); slot < ROWS_PER_PAGE;
                ++slot) {
                Panel(SETTINGS_LAYOUT::Row_Rect(layout, slot),
                    kPanel_Fill, kPanel_Alpha);
            }

            // Scrollbar: the strip's own panel, with the thumb
            // inside it when the list scrolls (4f adds the arrows
            // and insets the thumb between them).
            Panel(layout.scrollbar, kPanel_Fill, kPanel_Alpha);
            const SETTINGS_LAYOUT::s_rect thumb =
                SETTINGS_LAYOUT::Scrollbar_Thumb_Rect(layout,
                    Total_Lines(), First_Line(), Visible_Lines());
            if (thumb.w > 0) {
                Blend_Fill(thumb, kThumb_Fill, kThumb_Alpha);
            }

            // Description box: the selected row's description,
            // wrapped to fit the panel, with a "Requires restart."
            // line for the settings it applies to and the status
            // line below it -- the old screen's info box, in the
            // hi-res font. The lines stack from the top, so no
            // blank line opens up when the description is short.
            // The description is the selected row's (not the
            // hovered row's): hover only recolours the label.
            Panel(layout.description, kPanel_Fill, kPanel_Alpha);
            {
                const int16_t desc_x =
                    (int16_t)(layout.description.x + thickness + pad);
                const int desc_w = layout.description.w -
                    2 * (thickness + pad);
                // The description text is a fixed fraction of the
                // screen height (25 permille, the status line 21):
                // the sizes 4b used, when the panel was 140
                // permille tall (18% / 15% of it). Fixed to the
                // screen so growing the panel adds lines instead
                // of growing the text.
                const int desc_cell =
                    Text_Cell_For_Permille(layout.height, 25);
                const int status_cell =
                    Text_Cell_For_Permille(layout.height, 21);
                const int desc_lh = Line_Height(desc_cell);
                const int status_lh = Line_Height(status_cell);
                bool restart_required = false;
                std::string description;
                if (Visible_Count() > 0 && g_state.selected >= 0 &&
                    g_state.selected < Visible_Count()) {
                    description = Visible_Row_Description(
                        g_state.selected, &restart_required);
                }
                // Lines the description may take: the panel's
                // interior, less the status line and the restart
                // note (when one applies). At least one, so a
                // description always shows.
                const int restart_lh = restart_required ? desc_lh : 0;
                const int available = layout.description.h -
                    2 * thickness - pad - status_lh - restart_lh;
                const int max_lines =
                    std::max(1, available / desc_lh);
                auto desc_measure = [&](const std::string& text) {
                    return Text_Width(text.c_str(), desc_cell);
                };
                std::vector<std::string> desc_lines;
                if (!description.empty()) {
                    desc_lines = SETTINGS_HD_FONT::Wrap_Text(
                        description, desc_w, desc_measure, max_lines);
                }
                int16_t text_y = (int16_t)(layout.description.y +
                    thickness + pad / 2);
                for (const std::string& line : desc_lines) {
                    Draw_Text(desc_x, text_y, line.c_str(),
                        kText_Normal, desc_cell, 0);
                    text_y = (int16_t)(text_y + desc_lh);
                }
                if (restart_required) {
                    Draw_Text(desc_x, text_y, "Requires restart.",
                        kText_Bright, desc_cell, 0);
                    text_y = (int16_t)(text_y + desc_lh);
                }
                auto status_measure = [&](const std::string& text) {
                    return Text_Width(text.c_str(), status_cell);
                };
                Draw_Text(desc_x, text_y,
                    SETTINGS_HD_FONT::Truncate_To_Width(g_state.status,
                        desc_w, status_measure).c_str(),
                    kText_Dim, status_cell, 0);
            }

            // Bottom bar: the panel, with the tabs on the left and
            // MORE and RETURN at the right end. 4a draws the labels
            // (the active tab blue); 4c wires the hover / pressed
            // states.
            Panel(layout.bottom_bar, kPanel_Fill, kPanel_Alpha);
            const int button_cell =
                Text_Cell_For(layout.show_more.h, 60);
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                Button(layout.tabs[category], CATEGORY_LABELS[category],
                    category == g_state.category ? BUTTON_ACTIVE : BUTTON_NORMAL,
                    button_cell);
            }
            Button(layout.show_more, "MORE", BUTTON_NORMAL, button_cell);
            Button(layout.done, "RETURN", BUTTON_NORMAL, button_cell);

            // The edit overpaint: while a row is being edited, the
            // field system has drawn the edit text in the engine
            // pixel font over the value rect. Erase it with an
            // opaque fill and draw the live text (the field
            // system's continuous string, cursor "_" included) in
            // the HD font, showing the tail when the text is wider
            // than the rect so the cursor stays visible.
            if (g_state.editing_row >= 0) {
                const int edit_index = g_state.editing_row - Scroll_Top();
                if (edit_index >= 0 && edit_index < Page_Count()) {
                    const SETTINGS_LAYOUT::s_rect edit_rect =
                        Visible_Value_Rect(edit_index);
                    Solid_Fill(edit_rect, kEdit_Fill);
                    Frame(edit_rect, thickness);
                    std::string text = fields::continuous_string;
                    auto edit_measure = [&](const std::string& s) {
                        return Text_Width(s.c_str(), value_cell);
                    };
                    while (!text.empty() &&
                        edit_measure(text) > edit_rect.w - 2 * pad) {
                        text.erase(text.begin());
                    }
                    Draw_Text((int16_t)(edit_rect.x + pad),
                        (int16_t)(edit_rect.y +
                            (edit_rect.h - value_cell) / 2),
                        text.c_str(), kText_Bright, value_cell, 0);
                }
            }
        }

        void Draw_Settings_Screen() {
            video::Set_Page_Off();
            SETTINGS_BACKGROUND::Draw_Background();
            fields::Draw_Visible_Fields();
            Draw_Elements();
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
        SETTINGS_BACKGROUND::Install_Background();
        // The render caches (blend tables, scaled glyphs) are built
        // against the palette the background just installed.
        SETTINGS_RENDER::Reset();
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
                        const int count = Page_Count();
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
        SETTINGS_BACKGROUND::Remove_Background();
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

} // namespace SETTINGS_SCREEN
