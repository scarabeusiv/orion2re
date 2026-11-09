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

        // Button colours, in menu-palette indices (see
        // Button_Style in ui_kit.h for the measurements): the
        // metal face/highlight/shadow are the colony scrollbar
        // thumb's bevel (colsum.cpp Draw_Bar_Indicator: body 229,
        // light 230 top/left, dark 228 bottom/right), remapped to
        // the menu palette the settings screen already uses for
        // its metal buttons -- but at the colony buttons' own
        // lightness, not the old too-light values: the face is
        // the derived RETURN's modal face grey (238, the most
        // common interior grey of the remapped RETURN sprite),
        // the highlight its brightest grey (243), the shadow the
        // dark edge (232, already matching the RETURN's dark
        // edge). The old face/highlight (242/247) sat above the
        // RETURN's whole face range (brightest 243) and above
        // the colony build buttons' dithered faces (52-112).
        // The blue face is the active selector's blue (138,
        // highlight 154); the disabled face is the panel border
        // mid-grey (234); the toggle fills are the settings
        // squares' own (on 154/247, off 236/246); the cell is
        // the list panel's own border/interior (234/230).
        constexpr uint8_t kMetal_Face = 238;
        constexpr uint8_t kMetal_Highlight = 243;
        constexpr uint8_t kMetal_Shadow = 232;
        // The metal button's three slices, cut from the derived
        // RETURN frame (COLSUM entry 1, 85x26): the left dome
        // (columns 0..16) and the right dome (columns 60..84) are
        // drawn as-is, and a single text-free face column is tiled
        // across the middle. Column 58 is the only column that is
        // the flat face (238) for the whole text body (rows 8..16)
        // in the rest frame -- every other candidate carries a text
        // pixel or an antialiasing shade there. Column 59 is the
        // same for the pressed frame (25/26 rows match the face
        // profile, no orange text pixel); column 58 in the pressed
        // frame catches the orange text's fringe (221/223) and must
        // not be used there. The caps are copied exactly, so the
        // button's chrome is pixel-identical to the RETURN's; the
        // middle differs from the RETURN's middle only where the
        // RETURN has text (correctly omitted) and in the face's
        // dithered bands (rows 1, 7, 17, 20 rest; row 17 pressed),
        // where the tiled column holds one shade of the dither.
        constexpr int16_t kMetal_Left_Slice = 17;
        constexpr int16_t kMetal_Right_Slice = 25;
        constexpr int16_t kMetal_Middle_Column_Normal = 58;
        constexpr int16_t kMetal_Middle_Column_Pressed = 59;
        constexpr uint8_t kPrimary_Face = 138;
        constexpr uint8_t kPrimary_Highlight = 154;
        constexpr uint8_t kButton_Disabled_Face = 234;
        constexpr uint8_t kCell_Interior = 230;
        constexpr uint8_t kCell_Border = 234;
        constexpr uint8_t kToggle_On_Base = 154;
        constexpr uint8_t kToggle_On_Bright = 247;
        constexpr uint8_t kToggle_Off_Base = 236;
        constexpr uint8_t kToggle_Off_Bright = 246;

        // Metal-button label ramps, in menu-palette indices: flat
        // dark text on the metal face, flat light text on the blue
        // face (the settings screen's own g_dark/g_light text
        // colours, now owned by the theme).
        const s_colors kButton_Label_Dark = {{230, 230, 230, 230, 230, 230, 230, 230}};
        const s_colors kButton_Label_Light = {{247, 247, 247, 247, 247, 247, 247, 247}};
        // Pressed metal label: the orange the real RETURN lights
        // up with when held (menu palette 221, (176, 112, 44) —
        // the pressed frame's own orange, the most common ink of
        // the pixels that differ between the derived rest/pressed
        // frames). A pressed metal button's label is this orange,
        // like the sprite's pressed text.
        const s_colors kButton_Label_Pressed = {{221, 221, 221, 221, 221, 221, 221, 221}};
        // Value-cell label ramps: the colony cyan row text at
        // rest, light text on the blue active fill.
        const s_colors kCell_Label_Normal = {{150, 154, 155, 151, 152, 153, 154, 155}};
        const s_colors kCell_Label_Active = {{247, 247, 247, 247, 247, 247, 247, 247}};
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

    int16_t Frame::Width() const {
        return frame_ == nullptr ? 0 : frame_->width;
    }

    int16_t Frame::Height() const {
        return frame_ == nullptr ? 0 : frame_->height;
    }

    void Frame::Draw(int16_t x, int16_t y) const {
        if (frame_ == nullptr) {
            return;
        }
        sprite_art::Draw_Sprite_Frame(frame_, x, y);
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

    void Frame::Draw_Three_Slice(int16_t x, int16_t y, int16_t width,
        int16_t left_slice, int16_t middle_column,
        int16_t right_slice) const {
        if (frame_ == nullptr) {
            return;
        }
        sprite_art::Draw_Sprite_Frame_Three_Slice(frame_, x, y, width,
            left_slice, middle_column, right_slice);
    }

    Colony_Theme::Colony_Theme(Frame tab_normal, Frame tab_active,
        Frame return_normal, Frame return_pressed,
        const s_colors& title_colors)
        // Tab labels are engine font style 2: the baked colony
        // labels measure as style 2 (widths within 1-2 px per
        // label, ink 8-9 rows tall). The title, button labels and
        // cell labels keep style 4.
        : tab_font_(2, kTab_Label_Normal, kTab_Label_Active),
          title_font_(4, title_colors, title_colors),
          button_font_(4, kButton_Label_Dark, kButton_Label_Light),
          button_pressed_font_(4, kButton_Label_Pressed,
              kButton_Label_Pressed),
          cell_font_(4, kCell_Label_Normal, kCell_Label_Active),
          tab_normal_(tab_normal), tab_active_(tab_active),
          return_normal_(return_normal),
          return_pressed_(return_pressed) {}

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

    ui_kit::Button_Style Colony_Theme::Button_Style() const {
        ui_kit::Button_Style style;
        style.face = kMetal_Face;
        style.highlight = kMetal_Highlight;
        style.shadow = kMetal_Shadow;
        style.primary_face = kPrimary_Face;
        style.primary_highlight = kPrimary_Highlight;
        style.disabled_face = kButton_Disabled_Face;
        style.cell_interior = kCell_Interior;
        style.cell_border = kCell_Border;
        style.toggle_on_base = kToggle_On_Base;
        style.toggle_on_bright = kToggle_On_Bright;
        style.toggle_off_base = kToggle_Off_Base;
        style.toggle_off_bright = kToggle_Off_Bright;
        style.label_font = &button_font_;
        style.label_pressed_font = &button_pressed_font_;
        style.cell_font = &cell_font_;
        return style;
    }

    ui_kit::Image_Button_Frames Colony_Theme::Return_Frames() const {
        ui_kit::Image_Button_Frames frames;
        frames.normal = return_normal_;
        frames.pressed = return_pressed_;
        return frames;
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
            Frame(sprite_art::Load_Sprite_Frame("return", "normal",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("return", "pressed",
                allocation_base)),
            title_colors);
    }

    Button::Button(int16_t x, int16_t y, int16_t w, int16_t h,
        Button_Kind kind, const Theme& theme)
        : x_(x), y_(y), width_(w), height_(h), kind_(kind),
          theme_(theme) {}

    void Button::Set_Label(const char* label) {
        label_ = label;
    }

    void Button::Set_Label_Y_Offset(int16_t dy) {
        label_y_offset_ = dy;
        label_y_offset_set_ = true;
    }

    void Button::Set_State(Button_State state) {
        state_ = state;
    }

    void Button::Set_Primary(bool primary) {
        primary_ = primary;
    }

    void Button::Draw() const {
        switch (kind_) {
            case Button_Kind::Metal:
                Draw_Metal();
                break;
            case Button_Kind::Selector:
                Draw_Selector();
                break;
            case Button_Kind::Image:
                Draw_Image();
                break;
            case Button_Kind::Toggle:
                Draw_Toggle();
                break;
            case Button_Kind::Cell:
                Draw_Cell();
                break;
        }
    }

    void Button::Print_Centred_Label(const Font& font, bool active,
        int16_t height) const {
        if (label_ == nullptr) {
            return;
        }
        const int16_t label_x = (int16_t)(x_ +
            (width_ - font.Width(label_)) / 2);
        // The field system's own centring (fields.cpp
        // Draw_Visible_Fields): half the button height, less half
        // the font height rounded down. Not (h - fh)/2: that sits
        // 1 px high (measured: the Show more label inked at y+4,
        // the pre-kit code and the engine both put it at y+5).
        const int16_t label_y = (int16_t)(y_ + height / 2 -
            (font.Height() - 1) / 2);
        font.Print(label_x, label_y, label_, active);
    }

    void Button::Draw_Metal() const {
        const Button_Style style = theme_.Button_Style();
        // Blue face for a primary action or a latched-active
        // button; the flat mid-grey face when disabled.
        const bool blue = primary_ || state_ == Button_State::Active;
        // A metal button is a RETURN cut into three slices: the
        // derived frame's domes as-is, one text-free face column
        // tiled across the middle, the label rendered at runtime.
        // One frame serves any metal button at any width, and the
        // button's chrome is pixel-identical to the RETURN's. Blue,
        // disabled and missing-frame buttons keep the engine-drawn
        // form below: there is no blue or disabled RETURN frame to
        // cut. The label centres on the frame's height: a metal
        // button is sized from its frame, like an Image button.
        const Image_Button_Frames frames = theme_.Return_Frames();
        if (!blue && state_ != Button_State::Disabled
            && !frames.normal.Is_Null()) {
            const bool pressed = state_ == Button_State::Pressed
                && !frames.pressed.Is_Null();
            const Frame& frame = pressed ? frames.pressed : frames.normal;
            const int16_t middle = pressed
                ? kMetal_Middle_Column_Pressed
                : kMetal_Middle_Column_Normal;
            frame.Draw_Three_Slice(x_, y_, width_, kMetal_Left_Slice,
                middle, kMetal_Right_Slice);
            // A pressed metal button's label lights up orange, like
            // the real RETURN's pressed text; every other state keeps
            // the dark/light label. A null pressed font falls back to
            // the plain label font.
            const Font* label_font = style.label_font;
            if (state_ == Button_State::Pressed
                && style.label_pressed_font != nullptr) {
                label_font = style.label_pressed_font;
            }
            if (label_font != nullptr) {
                Print_Centred_Label(*label_font,
                    blue && state_ != Button_State::Disabled,
                    frame.Height());
            }
            return;
        }
        const uint8_t face = state_ == Button_State::Disabled
            ? style.disabled_face
            : (blue ? style.primary_face : style.face);
        graphics::Fill(x_, y_, (int16_t)(x_ + width_ - 1),
            (int16_t)(y_ + height_ - 1), face);
        if (state_ == Button_State::Disabled) {
            // Flat, bordered, no bevel: the colony never disables
            // a button, so there is no disabled bevel to copy.
            graphics::Box(x_, y_, width_, height_, 1, style.shadow);
        } else {
            // Raised bevel: light top/left, dark bottom/right.
            // Pressed inverts it (dark top/left, light
            // bottom/right); a primary button's light edge is its
            // own highlight, not the metal one.
            const uint8_t light = blue ? style.primary_highlight
                                       : style.highlight;
            const uint8_t top =
                state_ == Button_State::Pressed ? style.shadow : light;
            const uint8_t bottom =
                state_ == Button_State::Pressed ? light : style.shadow;
            graphics::Fill(x_, y_, (int16_t)(x_ + width_ - 1), y_, top);
            graphics::Fill(x_, y_, x_, (int16_t)(y_ + height_ - 1), top);
            graphics::Fill(x_, (int16_t)(y_ + height_ - 1),
                (int16_t)(x_ + width_ - 1), (int16_t)(y_ + height_ - 1),
                bottom);
            graphics::Fill((int16_t)(x_ + width_ - 1), y_,
                (int16_t)(x_ + width_ - 1), (int16_t)(y_ + height_ - 1),
                bottom);
        }
        // A pressed metal button's label lights up orange, like
        // the real RETURN's pressed text; every other state keeps
        // the dark/light label. A null pressed font falls back to
        // the plain label font.
        const Font* label_font = style.label_font;
        if (state_ == Button_State::Pressed
            && style.label_pressed_font != nullptr) {
            label_font = style.label_pressed_font;
        }
        if (label_font != nullptr) {
            Print_Centred_Label(*label_font,
                blue && state_ != Button_State::Disabled, height_);
        }
    }

    void Button::Draw_Selector() const {
        const Tab_Frames frames = theme_.Tab_Frames();
        // Active and pressed are both the colony's frame 1 (the
        // selected sort column's blue gradient); a disabled
        // selector has no frame of its own and renders as rest.
        const bool selected = state_ == Button_State::Active ||
            state_ == Button_State::Pressed;
        const Tab_Frame& frame = selected ? frames.active : frames.normal;
        if (!frame.frame.Is_Null()) {
            if (frame.resample_middle) {
                frame.frame.Draw_Stretched_Resampled(x_, y_, width_,
                    frames.left_slice, frames.right_slice);
            } else {
                frame.frame.Draw_Stretched(x_, y_, width_,
                    frames.left_slice, frames.right_slice);
            }
        } else {
            graphics::Fill(x_, y_, (int16_t)(x_ + width_ - 1),
                (int16_t)(y_ + height_ - 1),
                selected ? frames.fallback_fill_active
                         : frames.fallback_fill_normal);
            graphics::Box(x_, y_, width_, height_, 1,
                frames.fallback_border);
        }
        if (label_ != nullptr) {
            const Font& font = theme_.Tab_Font();
            const int16_t label_x = (int16_t)(x_ +
                (width_ - font.Width(label_)) / 2);
            font.Print(label_x,
                (int16_t)(y_ + theme_.Tab_Label_Y_Offset()), label_,
                selected);
        }
    }

    void Button::Draw_Image() const {
        const Image_Button_Frames frames = theme_.Return_Frames();
        const Frame& frame = state_ == Button_State::Pressed &&
                !frames.pressed.Is_Null()
            ? frames.pressed
            : frames.normal;
        if (!frame.Is_Null()) {
            frame.Draw(x_, y_);
            return;
        }
        // No derived frames (stale base.zip): the engine-drawn
        // metal button with the same label.
        Draw_Metal();
    }

    void Button::Draw_Toggle() const {
        const Button_Style style = theme_.Button_Style();
        graphics::Fill(x_, y_, (int16_t)(x_ + width_ - 1),
            (int16_t)(y_ + height_ - 1), style.cell_interior);
        graphics::Box(x_, y_, width_, height_, 1, style.cell_border);
        if (state_ == Button_State::Disabled) {
            // Bare cell, no fill: the colony's no-button state.
            return;
        }
        // Pressed renders as rest: the colony's toggle has no
        // pressed frame (its two-frame buy button's frame 1 means
        // "bought", not "held").
        const bool on = state_ == Button_State::Active;
        const uint8_t base =
            on ? style.toggle_on_base : style.toggle_off_base;
        const uint8_t bright =
            on ? style.toggle_on_bright : style.toggle_off_bright;
        // Filled centre, inset two pixels from the border, with a
        // small pale centre.
        graphics::Fill((int16_t)(x_ + 2), (int16_t)(y_ + 2),
            (int16_t)(x_ + width_ - 3), (int16_t)(y_ + height_ - 3),
            base);
        graphics::Fill((int16_t)(x_ + width_ / 2 - 2),
            (int16_t)(y_ + height_ / 2 - 2),
            (int16_t)(x_ + width_ / 2 + 1),
            (int16_t)(y_ + height_ / 2 + 1), bright);
    }

    void Button::Draw_Cell() const {
        const Button_Style style = theme_.Button_Style();
        const bool active = state_ == Button_State::Active;
        graphics::Fill(x_, y_, (int16_t)(x_ + width_ - 1),
            (int16_t)(y_ + height_ - 1),
            active ? style.primary_face : style.cell_interior);
        graphics::Box(x_, y_, width_, height_, 1, style.cell_border);
        if (label_ != nullptr && style.cell_font != nullptr) {
            const Font& font = *style.cell_font;
            // The value text sits on the row's text baseline when
            // the screen sets a label offset (trigger_y+4 here);
            // otherwise it centres like a metal button.
            const int16_t label_y = label_y_offset_set_
                ? (int16_t)(y_ + label_y_offset_)
                : (int16_t)(y_ + height_ / 2 -
                    (font.Height() - 1) / 2);
            font.Print((int16_t)(x_ + 4), label_y, label_, active);
            if (state_ != Button_State::Disabled) {
                font.Print((int16_t)(x_ + width_ - 12), label_y, "v",
                    active);
            }
        }
    }

    Tab_Bar::Tab_Bar(int16_t x, int16_t y, const Theme& theme)
        : x_(x), y_(y), theme_(theme) {}

    void Tab_Bar::Add_Tab(const char* label, bool active, bool pressed) {
        Tab tab;
        tab.label = label;
        tab.active = active;
        tab.pressed = pressed;
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
        for (const Tab& tab : tabs_) {
            Button button(tab.x, y_, tab.width, theme_.Tab_Height(),
                Button_Kind::Selector, theme_);
            button.Set_Label(tab.label);
            button.Set_State(tab.pressed ? Button_State::Pressed
                : (tab.active ? Button_State::Active
                              : Button_State::Rest));
            button.Draw();
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
