#include "pch.h"

#include "game/fonts.h"
#include "game/graphics.h"
#include "game/sprite_art.h"
#include "game/ui_kit.h"

namespace ui_kit {
    namespace {
        // Tab label ramps, in menu-palette indices (MAINMENU.LBX
        // entry 2's embedded palette). Only entries 0 (glyph edge)
        // and 1 (glyph face) are ever used; the rest repeat the
        // face. Measured from the baked colony sort labels, rendered
        // with the COLSUM background's embedded palette:
        // - Inactive: face 104 (196, 196, 196) white, edge 87/89
        //   (68-80 grey). Menu 248 (196, 196, 204) is the white,
        //   menu 29 (76, 72, 64) the dark-grey edge.
        // - Active: face 43 (196, 208, 252) blue-white, edge the
        //   mid-blues 53/60/62 (12-60, 8-56, 136-184). Menu 250
        //   (220, 220, 228) is the blue-white face, menu 138
        //   (52, 48, 136) the mid-blue edge.
        const s_colors kTab_Label_Normal = {{29, 248, 248, 248, 248, 248, 248, 248}};
        const s_colors kTab_Label_Active = {{138, 250, 250, 250, 250, 250, 250, 250}};

        // The colony sort-button frame, COLSUM.LBX anims[4], 50x24:
        // bevelled on the left and top, flat black on the right, so
        // the stretched draw keeps a 4 px left slice and no right
        // slice. The bevel itself is 4 px wide (grey ramp plus the
        // teal line on the rest frame, blue ramp on the selected).
        constexpr int16_t kSelector_Left_Slice = 4;
        constexpr int16_t kSelector_Right_Slice = 0;

        // Tab metrics, measured from the colony sort buttons
        // (colsum.cpp: y 446, 24 tall; next x = x + width + 1).
        constexpr int16_t kTab_Padding = 8;
        constexpr int16_t kTab_Gap = 1;
        constexpr int16_t kTab_Height = 24;
        constexpr int16_t kTab_Label_Y_Offset = 8;

        // Window frame colours, in menu-palette indices: the thin
        // light-grey outer line and dark inner line baked into the
        // colony screens' backgrounds.
        constexpr uint8_t kWindow_Frame_Outer = 244;
        constexpr uint8_t kWindow_Frame_Inner = 230;
        constexpr int16_t kWindow_Title_Y = 8;

        // Fallback fills when a derived frame is missing (stale
        // base.zip): the derived frames' own interior colours, so
        // the tab stays readable -- rest dark navy, active menu blue.
        constexpr uint8_t kTab_Fallback_Fill_Normal = 50;
        constexpr uint8_t kTab_Fallback_Fill_Active = 138;
        constexpr uint8_t kTab_Fallback_Border = 234;
    } // namespace

    Engine_Font::Engine_Font(int16_t style, const s_colors& normal_colors,
        const s_colors& active_colors)
        : style_(style), normal_colors_(normal_colors),
          active_colors_(active_colors) {}

    int16_t Engine_Font::Width(const char* text) const {
        fonts::Set_Font_Style(style_, &normal_colors_);
        return fonts::Get_String_Width(text);
    }

    int16_t Engine_Font::Height() const {
        fonts::Set_Font_Style(style_, &normal_colors_);
        return fonts::Get_Font_Height();
    }

    void Engine_Font::Print(int16_t x, int16_t y, const char* text,
        bool active) const {
        fonts::Set_Font_Style(style_,
            active ? &active_colors_ : &normal_colors_);
        fonts::Print(x, y, text);
    }

    void Frame::Draw_Stretched(int16_t x, int16_t y, int16_t width,
        int16_t left_slice, int16_t right_slice) const {
        if (frame_ == nullptr) {
            return;
        }
        sprite_art::Draw_Sprite_Frame_Stretched(frame_, x, y, width,
            left_slice, right_slice);
    }

    void Frame::Draw_Stretched_Resampled(int16_t x, int16_t y,
        int16_t width, int16_t left_slice, int16_t right_slice) const {
        if (frame_ == nullptr) {
            return;
        }
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(frame_, x, y,
            width, left_slice, right_slice);
    }

    Colony_Theme::Colony_Theme(Frame tab_normal, Frame tab_active,
        const s_colors& title_colors)
        // Tab labels are engine font style 2: the baked colony
        // labels measure as style 2 (widths within 1-2 px per
        // label, ink 8-9 rows tall). The title keeps style 4.
        : tab_font_(2, kTab_Label_Normal, kTab_Label_Active),
          title_font_(4, title_colors, title_colors),
          tab_normal_(tab_normal), tab_active_(tab_active) {}

    const Font& Colony_Theme::Tab_Font() const {
        return tab_font_;
    }

    ui_kit::Tab_Frames Colony_Theme::Tab_Frames() const {
        ui_kit::Tab_Frames frames;
        frames.normal.frame = tab_normal_;
        frames.normal.resample_middle = false;
        frames.active.frame = tab_active_;
        // The selected frame's middle is a blue gradient, brightest
        // at the edges and dark in the middle: resample it across
        // the tab width, or the gradient repeats like stripes.
        frames.active.resample_middle = true;
        frames.left_slice = kSelector_Left_Slice;
        frames.right_slice = kSelector_Right_Slice;
        frames.fallback_fill_normal = kTab_Fallback_Fill_Normal;
        frames.fallback_fill_active = kTab_Fallback_Fill_Active;
        frames.fallback_border = kTab_Fallback_Border;
        return frames;
    }

    ui_kit::Window_Style Colony_Theme::Window_Style() const {
        ui_kit::Window_Style style;
        style.frame_outer = kWindow_Frame_Outer;
        style.frame_inner = kWindow_Frame_Inner;
        style.title_font = &title_font_;
        style.title_y = kWindow_Title_Y;
        return style;
    }

    int16_t Colony_Theme::Tab_Padding() const {
        return kTab_Padding;
    }

    int16_t Colony_Theme::Tab_Gap() const {
        return kTab_Gap;
    }

    int16_t Colony_Theme::Tab_Height() const {
        return kTab_Height;
    }

    int16_t Colony_Theme::Tab_Label_Y_Offset() const {
        return kTab_Label_Y_Offset;
    }

    std::unique_ptr<Colony_Theme> Load_Colony_Theme(
        void* allocation_base, const s_colors& title_colors) {
        return std::make_unique<Colony_Theme>(
            Frame(sprite_art::Load_Sprite_Frame("selector-frame", "normal",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("selector-frame", "active",
                allocation_base)),
            title_colors);
    }

    Tab_Bar::Tab_Bar(int16_t x, int16_t y, const Theme& theme)
        : x_(x), y_(y), theme_(theme) {}

    void Tab_Bar::Add_Tab(const char* label, bool active) {
        Tab tab;
        tab.label = label;
        tab.active = active;
        tab.width = (int16_t)(theme_.Tab_Font().Width(label) +
            2 * theme_.Tab_Padding());
        tab.x = tabs_.empty() ? x_ : (int16_t)(End_X() + theme_.Tab_Gap());
        tabs_.push_back(tab);
    }

    const std::vector<Tab_Bar::Tab>& Tab_Bar::tabs() const {
        return tabs_;
    }

    int16_t Tab_Bar::y() const {
        return y_;
    }

    int16_t Tab_Bar::height() const {
        return theme_.Tab_Height();
    }

    int16_t Tab_Bar::End_X() const {
        if (tabs_.empty()) {
            return x_;
        }
        const Tab& last = tabs_.back();
        return (int16_t)(last.x + last.width);
    }

    void Tab_Bar::Draw() const {
        const Tab_Frames frames = theme_.Tab_Frames();
        const Font& font = theme_.Tab_Font();
        for (const Tab& tab : tabs_) {
            const Tab_Frame& frame = tab.active ? frames.active : frames.normal;
            if (!frame.frame.Is_Null()) {
                if (frame.resample_middle) {
                    frame.frame.Draw_Stretched_Resampled(tab.x, y_,
                        tab.width, frames.left_slice, frames.right_slice);
                } else {
                    frame.frame.Draw_Stretched(tab.x, y_, tab.width,
                        frames.left_slice, frames.right_slice);
                }
            } else {
                graphics::Fill(tab.x, y_, (int16_t)(tab.x + tab.width - 1),
                    (int16_t)(y_ + theme_.Tab_Height() - 1),
                    tab.active ? frames.fallback_fill_active
                               : frames.fallback_fill_normal);
                graphics::Box(tab.x, y_, tab.width, theme_.Tab_Height(), 1,
                    frames.fallback_border);
            }
            const int16_t label_x = (int16_t)(tab.x +
                (tab.width - font.Width(tab.label)) / 2);
            font.Print(label_x,
                (int16_t)(y_ + theme_.Tab_Label_Y_Offset()), tab.label,
                tab.active);
        }
    }

    void Draw_Window_Frame(const Theme& theme, int16_t x, int16_t y,
        int16_t w, int16_t h, const char* title) {
        const Window_Style style = theme.Window_Style();
        graphics::Box(x, y, w, h, 1, style.frame_outer);
        graphics::Box((int16_t)(x + 1), (int16_t)(y + 1),
            (int16_t)(w - 2), (int16_t)(h - 2), 1, style.frame_inner);
        if (style.title_font != nullptr && title != nullptr) {
            const int16_t label_x = (int16_t)(x +
                (w - style.title_font->Width(title)) / 2);
            style.title_font->Print(label_x,
                (int16_t)(y + style.title_y), title, false);
        }
    }
} // namespace ui_kit
