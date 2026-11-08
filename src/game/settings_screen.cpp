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

        constexpr int16_t TITLE_X = 16;
        constexpr int16_t TITLE_Y = 8;
        constexpr int16_t TAB_Y = 30;
        constexpr int16_t TAB_HEIGHT = 18;
        constexpr int16_t TITLE_WIDTH = 348;
        constexpr int16_t VALUE_X = 372;
        constexpr int16_t VALUE_WIDTH = 244;
        constexpr int16_t FIRST_ROW_Y = 58;
        constexpr int16_t ROW_HEIGHT = 22;
        constexpr int ROWS_PER_PAGE = 14;
        constexpr int16_t HELP_Y = 376;
        constexpr int16_t HELP_LINE_HEIGHT = 14;
        constexpr int16_t STATUS_Y = 412;
        constexpr int16_t FOOTER_Y = 436;
        constexpr int16_t NAV_Y = 430;
        constexpr int16_t NAV_Y_END = 452;
        constexpr int16_t TEXT_WIDTH = 608;

        // The standard UI text palette, as used by the council screen.
        s_colors g_normal_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
        s_colors g_highlight_colors = {{150, 155, 155, 152, 153, 155, 155, 155}};

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
                if (config::Settings_Category_For_Path(g_state.rows[index].path) ==
                    wanted) {
                    g_state.visible_rows.push_back((int)index);
                }
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
            std::vector<char> items(type->value_count * stride, '\0');
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
            fonts::Set_Font_Style(4, &g_normal_colors);
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
                meta += entry.enabled ? "; Enabled" : "; Disabled";
            }
            return help + " | " + meta;
        }

        void Draw_Settings_Screen() {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 639, 479, 0);

            fonts::Set_Font_Style(4, &g_normal_colors);
            const char* title = "Settings";
            fonts::Print((640 - fonts::Get_String_Width(title)) / 2, TITLE_Y, title);
            char page_text[32];
            snprintf(page_text, sizeof(page_text), "Page %d of %d",
                g_state.page + 1, Page_Count());
            fonts::Print(640 - 16 - fonts::Get_String_Width(page_text), TITLE_Y, page_text);

            int16_t tab_x = TITLE_X;
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                fonts::Set_Font_Style(4,
                    category == g_state.category ? &g_highlight_colors : &g_normal_colors);
                fonts::Print(tab_x + 8, TAB_Y + 2, CATEGORY_LABELS[category]);
                tab_x += (int16_t)(fonts::Get_String_Width(CATEGORY_LABELS[category]) + 16) + 8;
            }

            const bool mods_tab = g_state.category == CATEGORY_MODS;
            const int first = g_state.page * ROWS_PER_PAGE;
            const int count = std::min(ROWS_PER_PAGE, Visible_Count() - first);
            for (int index = 0; index < count; ++index) {
                const int visible = first + index;
                const int16_t y = FIRST_ROW_Y + (int16_t)index * ROW_HEIGHT;
                const bool selected = visible == g_state.selected;
                fonts::Set_Font_Style(4,
                    selected ? &g_highlight_colors : &g_normal_colors);
                if (Is_Mod_Row(visible)) {
                    const config::s_mod_catalogue_entry& entry =
                        g_state.mods[(size_t)Mod_Index(visible)];
                    std::string title_text = entry.name;
                    if (!entry.version_text.empty()) {
                        title_text += " " + entry.version_text;
                    }
                    fonts::Print(TITLE_X, y,
                        Truncate_To_Width(title_text, TITLE_WIDTH).c_str());
                    fonts::Set_Font_Style(4,
                        selected ? &g_highlight_colors : &g_normal_colors);
                    const std::string value_text = entry.broken
                        ? "Broken"
                        : (entry.enabled ? "[x] Enabled" : "[ ] Disabled");
                    fonts::Print(VALUE_X, y,
                        Truncate_To_Width(value_text, VALUE_WIDTH).c_str());
                    continue;
                }
                const config::s_settings_row& row =
                    g_state.rows[(size_t)Visible_Row_Index(visible)];
                const std::string title_text = Truncate_To_Width(row.title, TITLE_WIDTH);
                fonts::Print(TITLE_X, y, title_text.c_str());
                if (row.restart_required) {
                    const int16_t mark_x = (int16_t)(TITLE_X +
                        fonts::Get_String_Width(title_text.c_str()) + 2);
                    if (mark_x + fonts::Get_String_Width("*") < VALUE_X - 4) {
                        fonts::Set_Font_Style(4, &g_highlight_colors);
                        fonts::Print(mark_x, y, "*");
                    }
                }
                // The edit field draws its own text while it is open.
                if (visible == g_state.editing_row) {
                    continue;
                }
                fonts::Set_Font_Style(4,
                    selected ? &g_highlight_colors : &g_normal_colors);
                std::string value_text;
                if (row.widget == config::SETTINGS_WIDGET_INFO) {
                    value_text = row.info;
                } else if (row.widget == config::SETTINGS_WIDGET_TOGGLE) {
                    // Checkbox-style booleans, as in the original GAME
                    // SETTINGS dialog: [x] on, [ ] off.
                    config::s_typed_value value;
                    value_text = config::Get_Settings_Row_Value(row, &value)
                        ? (value.bool_value ? "[x]" : "[ ]")
                        : "?";
                } else {
                    config::s_typed_value value;
                    value_text = config::Get_Settings_Row_Value(row, &value)
                        ? config::Format_Leaf_Value(row.variable_id, value)
                        : "?";
                }
                fonts::Print(VALUE_X, y,
                    Truncate_To_Width(value_text, VALUE_WIDTH).c_str());
            }

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
            fonts::Set_Font_Style(4, &g_normal_colors);
            fonts::Print(TITLE_X, HELP_Y, help_line1.c_str());
            if (!help_line2.empty()) {
                fonts::Print(TITLE_X, HELP_Y + HELP_LINE_HEIGHT, help_line2.c_str());
            }
            fonts::Print(TITLE_X, STATUS_Y,
                Truncate_To_Width(g_state.status, TEXT_WIDTH).c_str());

            if (g_state.prev_field != -1) {
                fonts::Print(TITLE_X, FOOTER_Y, "Prev");
            }
            if (g_state.next_field != -1) {
                fonts::Print(110, FOOTER_Y, "Next");
            }
            fonts::Print(556, FOOTER_Y, "Done");
            const char* footer_note = mods_tab
                ? "Mod changes apply immediately"
                : "* = takes effect after restart";
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
