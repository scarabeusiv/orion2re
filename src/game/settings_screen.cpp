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
#include "game/settings_layout.h"
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
        // the colony's `num_colonies - 10` bound.
        int Max_Scroll() {
            const int total = Visible_Count();
            return total <= ROWS_PER_PAGE ? 0 : total - ROWS_PER_PAGE;
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
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const SETTINGS_LAYOUT::s_rect row =
                    SETTINGS_LAYOUT::Row_Rect(layout, index);
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
            // The edit field is the row's value-control rect: the
            // same rect the placeholder draws, so the click that
            // opened the edit and the typing land in the same place.
            const SETTINGS_LAYOUT::s_rect value_rect =
                SETTINGS_LAYOUT::Row_Value_Rect(Layout(),
                    visible - Scroll_Top());
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
            int16_t y = SETTINGS_LAYOUT::Row_Rect(layout,
                visible - Scroll_Top()).y;
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
                SETTINGS_LAYOUT::Row_Value_Rect(layout,
                    visible - Scroll_Top());
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

        // Step 3 of the hi-res rewrite: the background is the
        // generated hi-res surface (settings_background): a smooth
        // navy gradient with two soft glows, a deterministic
        // starfield and a vignette, generated per pixel for the
        // current game resolution -- no baked colony bitmap, no
        // dither, no pixel art. On top of it sit the positioned
        // placeholder elements (Draw_Layout_Placeholders): each
        // element draws its OWN border/frame as part of itself, in
        // hi-res. The background is just a background -- it
        // controls no element part. The elements themselves are
        // NOT yet rendered (no HD font text, no finished
        // buttons/dropdowns/toggles/scrollbar -- step 4); what
        // step 3 proves is the positions and proportions.
        //
        // Placeholder palette (menu palette; the background's
        // transient ramp occupies 160-223, so these stay clear
        // of it): 230 dark panel interior, 232 dark control fill,
        // 234 dim frame, 244 light frame, 138 blue for the active
        // tab.
        constexpr uint8_t kPlaceholder_Panel = 230;
        constexpr uint8_t kPlaceholder_Control = 232;
        constexpr uint8_t kPlaceholder_Dim_Frame = 234;
        constexpr uint8_t kPlaceholder_Frame = 244;
        constexpr uint8_t kPlaceholder_Active_Tab = 138;

        void Draw_Placeholder_Frame(const SETTINGS_LAYOUT::s_rect& rect,
            uint8_t fill, uint8_t border, int16_t thickness) {
            graphics::Fill(rect.x, rect.y,
                (int16_t)(rect.x + rect.w - 1),
                (int16_t)(rect.y + rect.h - 1), fill);
            graphics::Box(rect.x, rect.y, rect.w, rect.h, thickness,
                border);
        }

        void Draw_Layout_Placeholders() {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            // The frame scales with the screen: 1 px at 640 wide,
            // 2 px at 1280, 3 px at 1920.
            const int16_t thickness =
                (int16_t)std::max<int32_t>(1, layout.width / 640);
            // SETTING / VALUE header row at the top: one frame,
            // with the VALUE zone's own frame inside it.
            Draw_Placeholder_Frame(layout.header, kPlaceholder_Panel,
                kPlaceholder_Frame, thickness);
            Draw_Placeholder_Frame(layout.header_value,
                kPlaceholder_Control, kPlaceholder_Dim_Frame, thickness);
            // List area: each row its own frame, with the value
            // control's own frame nested inside it.
            for (int index = 0; index < ROWS_PER_PAGE; ++index) {
                Draw_Placeholder_Frame(
                    SETTINGS_LAYOUT::Row_Rect(layout, index),
                    kPlaceholder_Panel, kPlaceholder_Dim_Frame, thickness);
                Draw_Placeholder_Frame(
                    SETTINGS_LAYOUT::Row_Value_Rect(layout, index),
                    kPlaceholder_Control, kPlaceholder_Frame, thickness);
            }
            // Scrollbar on the right of the list: the strip's own
            // frame, with the thumb inside it when the list scrolls.
            Draw_Placeholder_Frame(layout.scrollbar, kPlaceholder_Panel,
                kPlaceholder_Dim_Frame, thickness);
            const SETTINGS_LAYOUT::s_rect thumb =
                SETTINGS_LAYOUT::Scrollbar_Thumb_Rect(layout,
                    Visible_Count(), Scroll_Top());
            if (thumb.w > 0) {
                graphics::Fill(thumb.x, thumb.y,
                    (int16_t)(thumb.x + thumb.w - 1),
                    (int16_t)(thumb.y + thumb.h - 1),
                    kPlaceholder_Frame);
            }
            // Description box: one frame across the full width.
            Draw_Placeholder_Frame(layout.description, kPlaceholder_Panel,
                kPlaceholder_Frame, thickness);
            // Bottom bar: one frame, with the tabs on the left
            // and MORE and RETURN at the right end, each with its
            // own frame inside it; the active tab reads blue.
            Draw_Placeholder_Frame(layout.bottom_bar, kPlaceholder_Panel,
                kPlaceholder_Frame, thickness);
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                Draw_Placeholder_Frame(layout.tabs[category],
                    category == g_state.category
                        ? kPlaceholder_Active_Tab
                        : kPlaceholder_Control,
                    kPlaceholder_Frame, thickness);
            }
            Draw_Placeholder_Frame(layout.show_more, kPlaceholder_Control,
                kPlaceholder_Frame, thickness);
            Draw_Placeholder_Frame(layout.done, kPlaceholder_Control,
                kPlaceholder_Frame, thickness);
        }

        void Draw_Settings_Screen() {
            video::Set_Page_Off();
            SETTINGS_BACKGROUND::Draw_Background();
            Draw_Layout_Placeholders();
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
        SETTINGS_BACKGROUND::Install_Background();
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
        SETTINGS_BACKGROUND::Remove_Background();
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

} // namespace SETTINGS_SCREEN
