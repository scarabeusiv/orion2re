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
        constexpr int16_t TITLE_WIDTH = 348;
        constexpr int16_t VALUE_X = 372;
        constexpr int16_t VALUE_WIDTH = 244;
        constexpr int16_t FIRST_ROW_Y = 38;
        constexpr int16_t ROW_HEIGHT = 22;
        constexpr int ROWS_PER_PAGE = 15;
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

        struct s_settings_screen_state {
            std::vector<config::s_settings_row> rows;
            int page = 0;
            int selected = 0;
            std::string status;
            std::string target_path;
            std::string seed_path;
            bool target_ready = false;
            int editing_row = -1;
            char edit_buffer[512] = {};
            int16_t edit_field = -1;
            int16_t prev_field = -1;
            int16_t next_field = -1;
            int16_t done_field = -1;
            int16_t row_fields[ROWS_PER_PAGE];
        };

        s_settings_screen_state g_state;

        int Page_Count() {
            return (int)((g_state.rows.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
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

        // Rebuilds the row and navigation fields for the current page.
        // Only call when no edit field is open: the edit field is added
        // separately by Open_Edit and removed by Commit/Cancel_Edit.
        void Rebuild_Fields() {
            fields::Clear_Fields();
            const int first = g_state.page * ROWS_PER_PAGE;
            const int count = std::min(ROWS_PER_PAGE,
                (int)g_state.rows.size() - first);
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

        void Cancel_Edit() {
            g_state.editing_row = -1;
            g_state.edit_field = -1;
            Rebuild_Fields();
        }

        bool Commit_Edit() {
            const int row_index = g_state.editing_row;
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

        void Open_Edit(int row_index) {
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
            g_state.editing_row = row_index;
            fonts::Set_Font_Style(4, &g_normal_colors);
            const int16_t y = FIRST_ROW_Y +
                (int16_t)(row_index - g_state.page * ROWS_PER_PAGE) * ROW_HEIGHT;
            g_state.edit_field = fields::Add_Continuous_String_Input_Field(
                VALUE_X, y, VALUE_WIDTH, ROW_HEIGHT - 2, g_state.edit_buffer,
                max_length, 0, 0, 0, 0);
        }

        void Open_Dropdown(int row_index) {
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
                (int16_t)(row_index - g_state.page * ROWS_PER_PAGE) * ROW_HEIGHT;
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

        void Handle_Row_Click(int row_index) {
            g_state.selected = row_index;
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
                    Open_Dropdown(row_index);
                    return;
                case config::SETTINGS_WIDGET_EDIT:
                    Open_Edit(row_index);
                    return;
                case config::SETTINGS_WIDGET_INFO:
                default:
                    g_state.status = row.info;
                    return;
            }
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

            const int first = g_state.page * ROWS_PER_PAGE;
            const int count = std::min(ROWS_PER_PAGE,
                (int)g_state.rows.size() - first);
            for (int index = 0; index < count; ++index) {
                const int row_index = first + index;
                const config::s_settings_row& row = g_state.rows[row_index];
                const int16_t y = FIRST_ROW_Y + (int16_t)index * ROW_HEIGHT;
                const bool selected = row_index == g_state.selected;
                fonts::Set_Font_Style(4,
                    selected ? &g_highlight_colors : &g_normal_colors);
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
                if (row_index == g_state.editing_row) {
                    continue;
                }
                fonts::Set_Font_Style(4,
                    selected ? &g_highlight_colors : &g_normal_colors);
                std::string value_text;
                if (row.widget == config::SETTINGS_WIDGET_INFO) {
                    value_text = row.info;
                } else {
                    config::s_typed_value value;
                    value_text = config::Get_Settings_Row_Value(row, &value)
                        ? config::Format_Leaf_Value(row.variable_id, value)
                        : "?";
                }
                fonts::Print(VALUE_X, y,
                    Truncate_To_Width(value_text, VALUE_WIDTH).c_str());
            }

            const config::s_settings_row& selected_row =
                g_state.rows[g_state.selected];
            std::string help_line1;
            std::string help_line2;
            Wrap_Text(selected_row.description, TEXT_WIDTH, &help_line1, &help_line2);
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
            const char* restart_note = "* = takes effect after restart";
            fonts::Print((640 - fonts::Get_String_Width(restart_note)) / 2, FOOTER_Y,
                restart_note);

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
                    const int first = g_state.page * ROWS_PER_PAGE;
                    const int count = std::min(ROWS_PER_PAGE,
                        (int)g_state.rows.size() - first);
                    for (int index = 0; index < count; ++index) {
                        if (input == g_state.row_fields[index]) {
                            Handle_Row_Click(first + index);
                            break;
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
