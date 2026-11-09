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

        // Window frame, in menu-palette indices: the bevelled metal
        // band baked into the colony screens' backgrounds, measured
        // as the median left-band profile of the COLSUM background
        // (640x480 game pixels, colony palette) and remapped into
        // the menu palette by preserving each pixel's offset above
        // the interior (see Frame_Style in ui_kit.h for why the
        // remap is a delta, not a nearest-colour lookup): the outer
        // edge, the bright ridge, the mid metal, the groove, the
        // inner light lip, the inner edge. Thickness and inset are
        // per side, in pixels: the left band is 11 px, the right
        // and top bands 12 px, the bottom band 10 px; the content
        // starts the same distance inside each edge. The title
        // prints at y 8; the bottom bar is 24 px tall and its bottom
        // sits on the content bottom (y 446..469).
        constexpr uint8_t kFrame_Profile[] = {234, 235, 238, 239, 238,
            234, 234, 231, 234, 236, 234};
        constexpr int16_t kFrame_Thickness_Left = 11;
        constexpr int16_t kFrame_Thickness_Right = 12;
        constexpr int16_t kFrame_Thickness_Top = 12;
        constexpr int16_t kFrame_Thickness_Bottom = 10;
        constexpr int16_t kFrame_Inset_Left = 11;
        constexpr int16_t kFrame_Inset_Right = 12;
        constexpr int16_t kFrame_Inset_Top = 12;
        constexpr int16_t kFrame_Inset_Bottom = 10;
        constexpr int16_t kFrame_Title_Y = 8;
        constexpr int16_t kFrame_Bottom_Bar_H = 24;

        // Fallback fills when a derived frame is missing (stale
        // base.zip): the derived frames' own interior colours, so
        // the tab stays readable -- rest dark navy, active menu blue.
        constexpr uint8_t kTab_Fallback_Fill_Normal = 50;
        constexpr uint8_t kTab_Fallback_Fill_Active = 138;
        constexpr uint8_t kTab_Fallback_Border = 234;

        // List spacing, measured from the colony summary screen
        // (colsum.cpp: rows are 30 px cells, y_row_end advancing
        // 31 px per row; the baked header plates end at y 32 and
        // the first row starts at y 35; the last row ends at y 343,
        // the panel's shade band and bevel run y 344..347 and the
        // bottom info area starts at y 350; the info area ends at
        // y 438 and the sort bar starts at y 446).
        constexpr int16_t kList_Row_Height = 30;
        constexpr int16_t kList_Row_Gap = 1;
        constexpr int16_t kList_Header_To_First_Row = 3;
        constexpr int16_t kList_Footing = 4;
        constexpr int16_t kList_To_Info = 4;
        constexpr int16_t kInfo_To_Bar = 7;

        // List background colours, in menu-palette indices: the
        // colony list's dark panel interior and mid-grey border --
        // the same indices the settings screen's own panels use.
        constexpr uint8_t kList_Interior = 230;
        constexpr uint8_t kList_Border = 234;

        // List line colours, in menu-palette indices: the colony
        // list's framed cells, re-anchored above the menu
        // interior (see List_Style in ui_kit.h for the
        // measurement). The dark row separator (229) the settings
        // rows used before is retired: on the dark panel (230) it
        // read as a subtle dark divider, the inverse of the
        // colony's light bevel lines.
        constexpr uint8_t kList_Frame_Light = 241;
        constexpr uint8_t kList_Frame_Shade = 235;
        constexpr uint8_t kList_Divider_Left = 233;
        constexpr uint8_t kList_Divider_Mid = 235;
        constexpr uint8_t kList_Divider_Right = 232;
        constexpr uint8_t kList_Plate_Face = 233;
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

    ui_kit::Frame_Style Colony_Theme::Frame_Style() const {
        ui_kit::Frame_Style style;
        style.thickness_left = kFrame_Thickness_Left;
        style.thickness_right = kFrame_Thickness_Right;
        style.thickness_top = kFrame_Thickness_Top;
        style.thickness_bottom = kFrame_Thickness_Bottom;
        style.inset_left = kFrame_Inset_Left;
        style.inset_right = kFrame_Inset_Right;
        style.inset_top = kFrame_Inset_Top;
        style.inset_bottom = kFrame_Inset_Bottom;
        for (const uint8_t index : kFrame_Profile) {
            style.profile.push_back(index);
        }
        style.title_font = &title_font_;
        style.title_y = kFrame_Title_Y;
        style.bottom_bar_h = kFrame_Bottom_Bar_H;
        return style;
    }

    ui_kit::Spacing Colony_Theme::Spacing() const {
        ui_kit::Spacing spacing;
        spacing.row_height = kList_Row_Height;
        spacing.row_gap = kList_Row_Gap;
        spacing.header_to_first_row = kList_Header_To_First_Row;
        spacing.list_footing = kList_Footing;
        spacing.list_to_info = kList_To_Info;
        spacing.info_to_bar = kInfo_To_Bar;
        return spacing;
    }

    ui_kit::List_Style Colony_Theme::List_Style() const {
        ui_kit::List_Style style;
        style.interior = kList_Interior;
        style.border = kList_Border;
        style.frame_light = kList_Frame_Light;
        style.frame_shade = kList_Frame_Shade;
        style.divider_left = kList_Divider_Left;
        style.divider_mid = kList_Divider_Mid;
        style.divider_right = kList_Divider_Right;
        style.plate_face = kList_Plate_Face;
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

    Window_Frame::Window_Frame(int16_t x, int16_t y, int16_t w,
        int16_t h, const Theme& theme)
        : x_(x), y_(y), w_(w), h_(h), theme_(theme) {}

    int16_t Window_Frame::content_x() const {
        return (int16_t)(x_ + theme_.Frame_Style().inset_left);
    }

    int16_t Window_Frame::content_y() const {
        return (int16_t)(y_ + theme_.Frame_Style().inset_top);
    }

    int16_t Window_Frame::content_w() const {
        const Frame_Style style = theme_.Frame_Style();
        return (int16_t)(w_ - style.inset_left - style.inset_right);
    }

    int16_t Window_Frame::content_h() const {
        const Frame_Style style = theme_.Frame_Style();
        return (int16_t)(h_ - style.inset_top - style.inset_bottom);
    }

    int16_t Window_Frame::content_right() const {
        return (int16_t)(content_x() + content_w());
    }

    int16_t Window_Frame::content_bottom() const {
        return (int16_t)(content_y() + content_h());
    }

    int16_t Window_Frame::bottom_bar_y(int16_t bar_h) const {
        return (int16_t)(content_bottom() - bar_h);
    }

    int16_t Window_Frame::title_y() const {
        return (int16_t)(y_ + theme_.Frame_Style().title_y);
    }

    void Window_Frame::Draw(const char* title) const {
        const Frame_Style style = theme_.Frame_Style();
        const size_t profile_len = style.profile.size();
        if (profile_len > 0) {
            // The profile entry for band offset i on a side of the
            // given thickness: a side thicker than the profile
            // repeats its last entry; a side thinner keeps the
            // profile's outer run and its last entry (the inner
            // edge), so the inner edge is never lost.
            auto band = [&](int16_t offset, int16_t thickness) {
                if ((size_t)offset + 1 < (size_t)thickness &&
                    (size_t)offset < profile_len) {
                    return style.profile[(size_t)offset];
                }
                return style.profile[profile_len - 1];
            };
            // Top and bottom bands, mitred: each line stops one
            // pixel further in per offset, so the corners meet on a
            // 45-degree seam.
            for (int16_t offset = 0; offset < style.thickness_top;
                ++offset) {
                graphics::Fill((int16_t)(x_ + offset),
                    (int16_t)(y_ + offset),
                    (int16_t)(x_ + w_ - 1 - offset),
                    (int16_t)(y_ + offset),
                    band(offset, style.thickness_top));
            }
            for (int16_t offset = 0; offset < style.thickness_bottom;
                ++offset) {
                const int16_t yy = (int16_t)(y_ + h_ - 1 - offset);
                graphics::Fill((int16_t)(x_ + offset), yy,
                    (int16_t)(x_ + w_ - 1 - offset), yy,
                    band(offset, style.thickness_bottom));
            }
            for (int16_t offset = 0; offset < style.thickness_left;
                ++offset) {
                graphics::Fill((int16_t)(x_ + offset),
                    (int16_t)(y_ + offset),
                    (int16_t)(x_ + offset),
                    (int16_t)(y_ + h_ - 1 - offset),
                    band(offset, style.thickness_left));
            }
            for (int16_t offset = 0; offset < style.thickness_right;
                ++offset) {
                const int16_t xx = (int16_t)(x_ + w_ - 1 - offset);
                graphics::Fill(xx, (int16_t)(y_ + offset), xx,
                    (int16_t)(y_ + h_ - 1 - offset),
                    band(offset, style.thickness_right));
            }
        }
        if (style.title_font != nullptr && title != nullptr) {
            const int16_t label_x = (int16_t)(x_ +
                (w_ - style.title_font->Width(title)) / 2);
            style.title_font->Print(label_x, title_y(), title, false);
        }
    }

    List_Layout::List_Layout(int16_t panel_x, int16_t panel_y,
        int16_t panel_w, int16_t content_right, int16_t header_y,
        int16_t header_h, int rows, const Spacing& spacing)
        : panel_x_(panel_x), panel_y_(panel_y), panel_w_(panel_w),
          content_right_(content_right), header_y_(header_y),
          header_h_(header_h), rows_(rows), spacing_(spacing) {
        first_row_y_ = (int16_t)(header_y + header_h +
            spacing.header_to_first_row);
        // The panel ends list_footing below the last row's bottom
        // edge: first row top plus all pitches, minus the trailing
        // gap (there is no gap after the last row), plus the footing
        // -- the colony's shade band and bevel below its last row.
        panel_h_ = (int16_t)(first_row_y_ + rows * spacing.Row_Pitch() -
            spacing.row_gap - panel_y + spacing.list_footing);
        scroll_track_h_ = (int16_t)(rows * spacing.Row_Pitch() -
            spacing.row_gap);
        info_y_ = (int16_t)(panel_y + panel_h_ + spacing.list_to_info);
    }

    void Draw_List_Background(const Theme& theme,
        const List_Layout& layout) {
        const List_Style style = theme.List_Style();
        const int16_t x = layout.panel_x();
        const int16_t y = layout.panel_y();
        graphics::Fill(x, y, (int16_t)(x + layout.panel_w() - 1),
            (int16_t)(y + layout.panel_h() - 1), style.interior);
        graphics::Box(x, y, layout.panel_w(), layout.panel_h(), 1,
            style.border);
        for (int index = 0; index < layout.rows(); ++index) {
            const int16_t row_y = layout.row_y(index);
            const int16_t bottom =
                (int16_t)(row_y + layout.row_height() - 1);
            // A framed cell: light lines at the top and bottom
            // edges, shade lines just inside each. The bottom
            // light line sits one pixel above the cell's last row:
            // that last pixel is the shade below the light line
            // (the cell's drop shadow), on every row including the
            // last: the colony's last row has its shade too, and
            // the footing below it is drawn next.
            graphics::Fill((int16_t)(x + 1), row_y,
                layout.content_right(), row_y, style.frame_light);
            graphics::Fill((int16_t)(x + 1), (int16_t)(row_y + 1),
                layout.content_right(), (int16_t)(row_y + 1),
                style.frame_shade);
            graphics::Fill((int16_t)(x + 1), (int16_t)(bottom - 2),
                layout.content_right(), (int16_t)(bottom - 2),
                style.frame_shade);
            graphics::Fill((int16_t)(x + 1), (int16_t)(bottom - 1),
                layout.content_right(), (int16_t)(bottom - 1),
                style.frame_light);
            graphics::Fill((int16_t)(x + 1), bottom,
                layout.content_right(), bottom, style.frame_shade);
        }
        // The panel footing: the colony's shade band and bevel
        // below the last row -- two shade lines and a light line;
        // the panel border box above is the fourth, dark line. The
        // band sits against the border, so a taller footing leaves
        // plain panel interior above it.
        const int16_t panel_bottom = (int16_t)(y + layout.panel_h());
        graphics::Fill((int16_t)(x + 1), (int16_t)(panel_bottom - 4),
            layout.content_right(), (int16_t)(panel_bottom - 4),
            style.frame_shade);
        graphics::Fill((int16_t)(x + 1), (int16_t)(panel_bottom - 3),
            layout.content_right(), (int16_t)(panel_bottom - 3),
            style.frame_shade);
        graphics::Fill((int16_t)(x + 1), (int16_t)(panel_bottom - 2),
            layout.content_right(), (int16_t)(panel_bottom - 2),
            style.frame_light);
    }

    Column_Layout::Column_Layout(const std::vector<Column>& columns,
        int16_t top, int16_t bottom)
        : columns_(columns), top_(top), bottom_(bottom) {}

    const std::vector<Column>& Column_Layout::columns() const {
        return columns_;
    }

    int16_t Column_Layout::divider_x(int index) const {
        const Column& left = columns_[(size_t)index];
        return (int16_t)(left.x + left.width);
    }

    void Draw_List_Columns(const Theme& theme,
        const Column_Layout& columns) {
        const List_Style style = theme.List_Style();
        for (int index = 0; index < columns.divider_count(); ++index) {
            const int16_t x = columns.divider_x(index);
            graphics::Fill(x, columns.top(), x, columns.bottom(),
                style.divider_left);
            graphics::Fill((int16_t)(x + 1), columns.top(),
                (int16_t)(x + 1), columns.bottom(), style.divider_mid);
            graphics::Fill((int16_t)(x + 2), columns.top(),
                (int16_t)(x + 2), columns.bottom(), style.divider_right);
        }
    }
} // namespace ui_kit
