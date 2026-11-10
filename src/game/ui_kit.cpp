#include "pch.h"

#include "game/fonts.h"
#include "game/graphics.h"
#include "game/sprite_art.h"
#include "game/ui_kit.h"
#include "game/vfs.h"
#include "yyjson.h"

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

        // The colony sort-button frames, grabbed from the rendered
        // colony screen (tools/sprites/derive_sort_tabs.py): one
        // per sort tab -- Name 50 px, Population 78 px, Food 42 px,
        // all 24 tall -- each with a fixed ~20 px bright ramp on
        // each side and a flat dark-navy middle whose width grows
        // with the tab. The draw three-slices every frame with 20 px
        // fixed slices and one tiled middle column per tab (the
        // middle of that tab's flat run), never a resample: the
        // colony's widest sort tab is 86 px with ~20 px ramps, so
        // resampling would stretch the ramps, which the colony
        // never does. The selected frame's field is flat in the
        // middle once the ramps are fixed slices -- the gradient is
        // the ramps, not the field -- so both states three-slice.
        constexpr int16_t kSort_Left_Slice = 20;
        constexpr int16_t kSort_Right_Slice = 20;
        // Middle columns, by tab index (0 Name, 1 Population,
        // 2 Food): the middle of each tab's flat run on the
        // label-free row 5. The two states differ: the normal
        // frame's gradient runs bright blue at the ramps to dark
        // navy (menu 50) in the middle, so the normal middle is the
        // dark-navy centre; the active frame's gradient runs the
        // other way (bright blue in the middle), so the active
        // middle is the bright-blue (menu 138) centre. The dark
        // column is right for the colony's narrow tabs (the bright
        // ramps dominate) but wrong for the settings' wide tabs
        // (the dark centre would dominate and the active tab would
        // read dark navy instead of bright blue).
        constexpr int16_t kSort_Middle_Columns[] = {26, 38, 21};
        constexpr int16_t kSort_Middle_Columns_Active[] = {7, 4, 3};
        constexpr int16_t kSort_Tab_Count = 3;

        // The bottom band's label colours (style 2, grey): the
        // colony's SORT label is menu 243 (152,152,160), the grey
        // of the colony's (152,152,152), with the tab font's dark
        // shadow (29). The tab labels stay white (248); the band
        // label is the grey.
        const s_colors kBand_Label = {{29, 243, 243, 243, 243, 243, 243, 243}};

        // The window-band background the gaps between tabs show,
        // in menu-palette indices, one per tab row (top -> bottom):
        // the recessed channel -- a light top bevel stepping down
        // to the dark-navy face, stepping back up to a light bottom
        // edge -- measured from the rendered colony screen's five
        // sort-tab gaps (tools/sprites/derive_sort_tabs.py). All
        // five gaps agree on rows 1..23; row 0 follows the
        // strip-wide arch, so it differs per gap: 239 after tab 0,
        // 240 after tab 1 (the colony's gaps read 239, 240, 240,
        // 241, 240 left to right). A bar with more gaps than
        // profiles reuses the last profile.
        constexpr uint8_t kSort_Gap_Profiles[][24] = {
            {239, 234, 15, 84, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
             50, 50, 50, 50, 50, 50, 84, 94, 29, 239},
            {240, 234, 15, 84, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
             50, 50, 50, 50, 50, 50, 84, 94, 29, 239},
        };

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
        // Scrollbar corner dots: the colony's index 80 is black
        // in the colony palette (COLSUM.LBX entry 0's embedded
        // palette), so the dots are black-on-black on the colony's
        // own black track; the nearest menu index is 0, exact.
        constexpr uint8_t kScroll_Corner = 0;
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

        // Bitmap-font JSON limits (see data/base/fonts/README.md):
        // the file is a small hand-drawn font, not arbitrary data,
        // so anything outside these ranges is a malformed file and
        // the font stays unloaded.
        constexpr int16_t kBitmap_Font_Max_Cap_Height = 32;
        constexpr int16_t kBitmap_Font_Max_Descender_Rows = 8;
        constexpr int16_t kBitmap_Font_Max_Advance = 64;
        constexpr size_t kBitmap_Font_Max_Glyphs = 128;
        constexpr int32_t kBitmap_Font_Max_File_Size = 1024 * 1024;
    } // namespace

    Engine_Font::Engine_Font(int16_t style, const s_colors& normal_colors,
        const s_colors& active_colors, int16_t spacing)
        : style_(style), spacing_(spacing), normal_colors_(normal_colors),
          active_colors_(active_colors) {}

    int16_t Engine_Font::Width(const char* text) const {
        fonts::Set_Font_Style(style_, &normal_colors_);
        if (spacing_ >= 0) {
            fonts::Set_Font_Spacing(spacing_);
        }
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
        if (spacing_ >= 0) {
            fonts::Set_Font_Spacing(spacing_);
        }
        fonts::Print(x, y, text);
    }

    Bitmap_Font::Bitmap_Font(State state, const Engine_Font& fallback)
        : state_(state), fallback_(fallback) {}

    bool Bitmap_Font::Load(const char* name) {
        if (name == nullptr || *name == '\0') {
            return false;
        }
        const std::string path = std::string("fonts/") + name + ".json";
        std::unique_ptr<s_vfs_file, decltype(&vfs::fclose)> file(
            vfs::fopen(path.c_str(), "rb"), &vfs::fclose);
        if (file == nullptr) {
            return false;
        }
        const int32_t size = vfs::fsize(file.get());
        if (size <= 0 || size > kBitmap_Font_Max_File_Size) {
            return false;
        }
        std::string text((size_t)size, '\0');
        if (vfs::fread(text.data(), 1, text.size(), file.get())
            != text.size()) {
            return false;
        }
        return Load_From_Text(text.data(), text.size());
    }

    bool Bitmap_Font::Load_From_Text(const char* text, size_t size) {
        // A failed parse unloads the font: the caller falls back to
        // the engine font rather than rendering a half-parsed one.
        // The new font is built in locals and swapped in only on
        // success.
        loaded_ = false;
        glyphs_.clear();
        cap_height_ = 0;
        descender_rows_ = 0;
        space_advance_ = 0;
        if (text == nullptr || size == 0) {
            return false;
        }
        // yyjson reads without modifying the input (no INSITU
        // flag), but it takes a mutable pointer: parse a copy.
        // Non-const so data() yields the char* it wants.
        std::string copy(text, size);
        yyjson_read_err error{};
        std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> doc(
            yyjson_read_opts(copy.data(), copy.size(), 0, nullptr,
                &error),
            yyjson_doc_free);
        if (doc == nullptr) {
            return false;
        }
        const yyjson_val* root = yyjson_doc_get_root(doc.get());
        if (!yyjson_is_obj(root)) {
            return false;
        }
        const auto get_uint = [](const yyjson_val* obj, const char* key,
            uint64_t* out) {
            const yyjson_val* value = yyjson_obj_get(obj, key);
            if (value == nullptr || !yyjson_is_uint(value)) {
                return false;
            }
            *out = yyjson_get_uint(value);
            return true;
        };
        // One state's bitmap rows: exactly expected_rows rows,
        // every row an array of the same length, each cell null or
        // a palette index 0..255. Normal rows must be the glyph's
        // width (expected_width >= 0); pressed rows carry their own
        // width (expected_width < 0).
        const auto parse_bitmap = [](const yyjson_val* bitmap,
            const char* state_name, size_t expected_rows,
            int16_t expected_width,
            std::vector<std::vector<int16_t>>* out) {
            const yyjson_val* rows = yyjson_obj_get(bitmap, state_name);
            if (rows == nullptr || !yyjson_is_arr(rows)
                || yyjson_arr_size(rows) != expected_rows) {
                return false;
            }
            out->clear();
            size_t state_width = 0;
            size_t i = 0, count = 0;
            yyjson_val* row = nullptr;
            yyjson_arr_foreach(rows, i, count, row) {
                if (!yyjson_is_arr(row)) {
                    return false;
                }
                const size_t width = yyjson_arr_size(row);
                if (width == 0
                    || width > (size_t)kBitmap_Font_Max_Advance) {
                    return false;
                }
                if (i == 0) {
                    state_width = width;
                    if (expected_width >= 0
                        && (int16_t)width != expected_width) {
                        return false;
                    }
                } else if (width != state_width) {
                    return false;
                }
                std::vector<int16_t> parsed;
                parsed.reserve(width);
                size_t j = 0, cell_count = 0;
                yyjson_val* cell = nullptr;
                yyjson_arr_foreach(row, j, cell_count, cell) {
                    if (yyjson_is_null(cell)) {
                        parsed.push_back(-1);
                    } else if (yyjson_is_uint(cell)
                        && yyjson_get_uint(cell) <= 255) {
                        parsed.push_back((int16_t)yyjson_get_uint(cell));
                    } else {
                        return false;
                    }
                }
                out->push_back(std::move(parsed));
            }
            return true;
        };
        uint64_t value = 0;
        if (!get_uint(root, "cap_height", &value) || value < 1
            || value > kBitmap_Font_Max_Cap_Height) {
            return false;
        }
        const int16_t cap_height = (int16_t)value;
        if (!get_uint(root, "descender_rows", &value)
            || value > kBitmap_Font_Max_Descender_Rows) {
            return false;
        }
        const int16_t descender_rows = (int16_t)value;
        if (!get_uint(root, "space_advance", &value) || value < 1
            || value > kBitmap_Font_Max_Advance) {
            return false;
        }
        const int16_t space_advance = (int16_t)value;
        const yyjson_val* glyphs_val = yyjson_obj_get(root, "glyphs");
        if (glyphs_val == nullptr || !yyjson_is_obj(glyphs_val)
            || yyjson_obj_size(glyphs_val) == 0
            || yyjson_obj_size(glyphs_val) > kBitmap_Font_Max_Glyphs) {
            return false;
        }
        std::map<char, Glyph> glyphs;
        size_t i = 0, count = 0;
        yyjson_val* key = nullptr;
        yyjson_val* glyph_val = nullptr;
        yyjson_obj_foreach(glyphs_val, i, count, key, glyph_val) {
            if (!yyjson_is_str(key) || yyjson_get_len(key) != 1) {
                return false;
            }
            const char ch = yyjson_get_str(key)[0];
            if (ch == ' ' || (unsigned char)ch >= 128) {
                return false;
            }
            if (!yyjson_is_obj(glyph_val)) {
                return false;
            }
            Glyph glyph;
            if (!get_uint(glyph_val, "advance", &value) || value < 1
                || value > kBitmap_Font_Max_Advance) {
                return false;
            }
            glyph.advance = (int16_t)value;
            if (!get_uint(glyph_val, "width", &value) || value < 1
                || value > kBitmap_Font_Max_Advance) {
                return false;
            }
            glyph.width = (int16_t)value;
            const yyjson_val* bitmap = yyjson_obj_get(glyph_val, "bitmap");
            if (bitmap == nullptr || !yyjson_is_obj(bitmap)) {
                return false;
            }
            if (!parse_bitmap(bitmap, "normal", (size_t)cap_height,
                    glyph.width, &glyph.normal)) {
                return false;
            }
            if (!parse_bitmap(bitmap, "pressed",
                    (size_t)(cap_height + descender_rows), -1,
                    &glyph.pressed)) {
                return false;
            }
            glyphs[ch] = std::move(glyph);
        }
        cap_height_ = cap_height;
        descender_rows_ = descender_rows;
        space_advance_ = space_advance;
        glyphs_ = std::move(glyphs);
        loaded_ = true;
        return true;
    }

    bool Bitmap_Font::Is_Loaded() const {
        return loaded_;
    }

    bool Bitmap_Font::Renderable(const char* text) const {
        if (!loaded_ || text == nullptr) {
            return false;
        }
        for (const char* p = text; *p != '\0'; ++p) {
            if (*p != ' ' && glyphs_.find(*p) == glyphs_.end()) {
                return false;
            }
        }
        return true;
    }

    const std::vector<std::vector<int16_t>>* Bitmap_Font::Rows(
        const Glyph& glyph) const {
        return state_ == State::Pressed ? &glyph.pressed : &glyph.normal;
    }

    int16_t Bitmap_Font::Width(const char* text) const {
        if (!Renderable(text)) {
            return fallback_.Width(text);
        }
        int16_t width = 0;
        for (const char* p = text; *p != '\0'; ++p) {
            if (*p == ' ') {
                width = (int16_t)(width + space_advance_);
            } else {
                const Glyph& glyph = glyphs_.at(*p);
                width = (int16_t)(width + (*(p + 1) == '\0'
                    // The last glyph counts its bitmap width, not
                    // its advance: the trailing gap belongs to no
                    // following letter.
                    ? (int16_t)Rows(glyph)->front().size()
                    : glyph.advance));
            }
        }
        return width;
    }

    int16_t Bitmap_Font::Height() const {
        if (!loaded_) {
            return fallback_.Height();
        }
        return (int16_t)(cap_height_ + descender_rows_);
    }

    void Bitmap_Font::Print(int16_t x, int16_t y, const char* text,
        bool active) const {
        // All-or-nothing per string: a string the bitmap font
        // cannot render whole (unloaded, unknown glyph, active
        // blue face) goes to the engine font entire, never mixed.
        if (active || !Renderable(text)) {
            fallback_.Print(x, y, text, active);
            return;
        }
        int16_t cx = x;
        for (const char* p = text; *p != '\0'; ++p) {
            if (*p == ' ') {
                cx = (int16_t)(cx + space_advance_);
                continue;
            }
            const Glyph& glyph = glyphs_.at(*p);
            const std::vector<std::vector<int16_t>>& rows = *Rows(glyph);
            for (size_t row = 0; row < rows.size(); ++row) {
                for (size_t col = 0; col < rows[row].size(); ++col) {
                    const int16_t value = rows[row][col];
                    if (value >= 0) {
                        graphics::Dot((int16_t)(cx + col),
                            (int16_t)(y + row), (uint8_t)value);
                    }
                }
            }
            cx = (int16_t)(cx + glyph.advance);
        }
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

    void Frame::Draw_Region(int16_t src_x, int16_t src_y, int16_t width,
        int16_t height, int16_t dst_x, int16_t dst_y) const {
        if (frame_ == nullptr) {
            return;
        }
        sprite_art::Draw_Sprite_Frame_Region(frame_, src_x, src_y, width,
            height, dst_x, dst_y);
    }

    Colony_Theme::Colony_Theme(Frame name_normal, Frame name_active,
        Frame population_normal, Frame population_active,
        Frame food_normal, Frame food_active,
        Frame return_normal, Frame return_pressed,
        const s_colors& title_colors, Frame background,
        Frame toggle_off, Frame toggle_on)
        // Tab labels are engine font style 2: the baked colony
        // labels measure as style 2 (widths within 1-2 px per
        // label, ink 8-9 rows tall). Button labels are the
        // Moo2Button bitmap font (the baked RETURN text is
        // hand-drawn pixel art, not a font render: style 2 with
        // spacing 1 only reaches IoU 0.626 against it), loaded in
        // the body below; the style-2 engine fonts stay as the
        // fallback for a missing or malformed font file. The title
        // and cell labels keep style 4.
        : tab_font_(2, kTab_Label_Normal, kTab_Label_Active),
          title_font_(4, title_colors, title_colors),
          button_font_(2, kButton_Label_Dark, kButton_Label_Light, 1),
          button_pressed_font_(2, kButton_Label_Pressed,
              kButton_Label_Pressed, 1),
          button_bitmap_font_(Bitmap_Font::State::Normal, button_font_),
          button_pressed_bitmap_font_(Bitmap_Font::State::Pressed,
              button_pressed_font_),
          header_font_(Bitmap_Font::State::Normal, title_font_),
          cell_font_(4, kCell_Label_Normal, kCell_Label_Active),
          tab_normal_{name_normal, population_normal, food_normal},
          tab_active_{name_active, population_active, food_active},
          return_normal_(return_normal),
          return_pressed_(return_pressed),
          toggle_off_(toggle_off),
          toggle_on_(toggle_on),
          band_label_font_(2, kBand_Label, kBand_Label),
          background_(background) {
        // Loading is soft: a missing or malformed file leaves the
        // bitmap fonts unloaded and the engine fonts above render
        // instead (vfs::fopen returns null in the tests, where no
        // base.zip is mounted, so the theme falls back there too).
        button_bitmap_font_.Load("Moo2Button");
        button_pressed_bitmap_font_.Load("Moo2Button");
        header_font_.Load("Moo2Display");
    }

    const Font& Colony_Theme::Tab_Font() const {
        return tab_font_;
    }

    const Font& Colony_Theme::Header_Font() const {
        return header_font_;
    }

    const Font& Colony_Theme::Band_Label_Font() const {
        return band_label_font_;
    }

    ui_kit::Toggle_Frames Colony_Theme::Toggle_Frames() const {
        ui_kit::Toggle_Frames frames;
        frames.off = toggle_off_;
        frames.on = toggle_on_;
        return frames;
    }

    namespace {
        // One tab's frames: the tab's own sort frame in both
        // states, three-sliced with the fixed 20 px ramps and that
        // tab's flat middle column.
        ui_kit::Tab_Frames Make_Tab_Frames(const Frame& normal,
            const Frame& active, int16_t middle_column,
            int16_t middle_column_active) {
            ui_kit::Tab_Frames frames;
            frames.normal.frame = normal;
            frames.normal.resample_middle = false;
            frames.normal.middle_column = middle_column;
            frames.active.frame = active;
            frames.active.resample_middle = false;
            frames.active.middle_column = middle_column_active;
            frames.left_slice = kSort_Left_Slice;
            frames.right_slice = kSort_Right_Slice;
            frames.fallback_fill_normal = kTab_Fallback_Fill_Normal;
            frames.fallback_fill_active = kTab_Fallback_Fill_Active;
            frames.fallback_border = kTab_Fallback_Border;
            return frames;
        }
    } // namespace

    ui_kit::Tab_Frames Colony_Theme::Tab_Frames() const {
        return Tab_Frames_For_Tab(0);
    }

    ui_kit::Tab_Frames Colony_Theme::Tab_Frames_For_Tab(
        int16_t tab_index) const {
        if (tab_index < 0) {
            tab_index = 0;
        }
        if (tab_index >= kSort_Tab_Count) {
            tab_index = kSort_Tab_Count - 1;
        }
        return Make_Tab_Frames(tab_normal_[(size_t)tab_index],
            tab_active_[(size_t)tab_index],
            kSort_Middle_Columns[(size_t)tab_index],
            kSort_Middle_Columns_Active[(size_t)tab_index]);
    }

    std::vector<uint8_t> Colony_Theme::Tab_Gap_Profile(
        int16_t gap_index) const {
        if (gap_index < 0) {
            gap_index = 0;
        }
        const size_t profile_count =
            sizeof(kSort_Gap_Profiles) / sizeof(kSort_Gap_Profiles[0]);
        if ((size_t)gap_index >= profile_count) {
            gap_index = (int16_t)(profile_count - 1);
        }
        const uint8_t* profile = kSort_Gap_Profiles[(size_t)gap_index];
        return std::vector<uint8_t>(profile, profile + kTab_Height);
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
        style.scroll_corner = kScroll_Corner;
        style.label_font = &button_bitmap_font_;
        style.label_pressed_font = &button_pressed_bitmap_font_;
        style.cell_font = &cell_font_;
        return style;
    }

    ui_kit::Frame Colony_Theme::Background() const {
        return background_;
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
        // The baked colony background: extracted from the user's own
        // game data at run time first (works in a fresh build, where
        // no .bin is bundled), falling back to the derived .bin when
        // it is bundled. A null background means the screen falls
        // back to the kit's drawn frame.
        Frame background(
            sprite_art::Load_Colony_Background(allocation_base));
        if (background.Is_Null()) {
            background = Frame(sprite_art::Load_Sprite_Frame(
                "colony-background", "normal", allocation_base));
        }
        // The colony BUILDING toggle, extracted at run time from
        // the user's own game data (COLSUM entry 11). Both null when
        // the entry is missing: the toggle falls back to the
        // engine-drawn one.
        const sprite_art::Buy_Button_Frames toggle =
            sprite_art::Load_Buy_Button_Frames(allocation_base);
        return std::make_unique<Colony_Theme>(
            Frame(sprite_art::Load_Sprite_Frame("sort-name", "normal",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("sort-name", "active",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("sort-population",
                "normal", allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("sort-population",
                "active", allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("sort-food", "normal",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("sort-food", "active",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("return", "normal",
                allocation_base)),
            Frame(sprite_art::Load_Sprite_Frame("return", "pressed",
                allocation_base)),
            title_colors, background,
            Frame(toggle.off), Frame(toggle.on));
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

    void Button::Set_Tab_Index(int16_t tab_index) {
        tab_index_ = tab_index;
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
        // A tab draws its own tab's frames; a Selector with no tab
        // index draws the shared frames.
        const Tab_Frames frames = tab_index_ >= 0
            ? theme_.Tab_Frames_For_Tab(tab_index_)
            : theme_.Tab_Frames();
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
            } else if (frame.middle_column >= 0) {
                // Flat middle, one face column tiled: the same
                // three-slice cut as a metal button. Tiling the
                // whole middle would repeat the bevel tail the
                // source frame carries past its left slice.
                frame.frame.Draw_Three_Slice(x_, y_, width_,
                    frames.left_slice, frame.middle_column,
                    frames.right_slice);
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
        // The derived toggle frames, when the theme carries them:
        // the colony's BUILDING toggle (grey off, blue on), drawn
        // at its native 19x29. Pressed renders as rest: the
        // colony's toggle has no pressed frame (its two-frame buy
        // button's frame 1 means "bought", not "held"). Disabled
        // stays the bare cell: the colony's no-button state.
        const Toggle_Frames frames = theme_.Toggle_Frames();
        if (state_ != Button_State::Disabled &&
            (!frames.off.Is_Null() || !frames.on.Is_Null())) {
            const bool on = state_ == Button_State::Active;
            const Frame& frame = on && !frames.on.Is_Null()
                ? frames.on : frames.off;
            if (!frame.Is_Null()) {
                frame.Draw(x_, y_);
                return;
            }
        }
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

    Tab_Bar::Tab_Bar(int16_t x, int16_t y, int16_t right,
        const Theme& theme)
        : x_(x), y_(y), right_(right), theme_(theme) {}

    void Tab_Bar::Add_Tab(const char* label, bool active, bool pressed) {
        Tab tab;
        tab.label = label;
        tab.active = active;
        tab.pressed = pressed;
        tab.natural_width = (int16_t)(theme_.Tab_Font().Width(label) +
            2 * theme_.Tab_Padding());
        tabs_.push_back(tab);
        Relayout();
    }

    void Tab_Bar::Add_Fixed_Tab(const char* label, int16_t width,
        bool active, bool pressed) {
        Tab tab;
        tab.label = label;
        tab.active = active;
        tab.pressed = pressed;
        tab.natural_width = width;
        tabs_.push_back(tab);
        Relayout();
    }

    void Tab_Bar::Set_Label(const char* label) {
        label_ = label;
        // The label plus the gap to the first tab: the colony's
        // SORT label ends ~15 px before its first tab (the sort
        // icon takes ~12 px of that; the settings label has no
        // icon, so 12 px of gap).
        label_width_ = label_ == nullptr ? 0
            : (int16_t)(theme_.Band_Label_Font().Width(label_) + 12);
        Relayout();
    }

    void Tab_Bar::Relayout() {
        // The strip fills [x_, right_): the label (when set) takes
        // the start, and the tabs divide the rest in proportion to
        // their natural widths (label + padding, like the colony's
        // sort buttons), with the theme's gap between them, so the
        // strip reads as one continuous row ending exactly at
        // right_. The last tab absorbs the rounding remainder; a
        // strip narrower than the natural widths shrinks the tabs
        // proportionally, never below 1 px.
        const int16_t count = (int16_t)tabs_.size();
        if (count == 0) {
            return;
        }
        const int16_t gap = theme_.Tab_Gap();
        int32_t total_natural = 0;
        for (const Tab& tab : tabs_) {
            total_natural += tab.natural_width;
        }
        const int16_t available =
            (int16_t)(right_ - x_ - label_width_ - (count - 1) * gap);
        int16_t x = (int16_t)(x_ + label_width_);
        int32_t used = 0;
        for (int16_t index = 0; index < count; ++index) {
            int16_t width;
            if (total_natural <= 0) {
                // No natural width to proportion (empty labels, no
                // padding): split the strip equally.
                width = (int16_t)(available / count);
            } else if (index + 1 < count) {
                width = (int16_t)(tabs_[(size_t)index].natural_width *
                    available / total_natural);
            } else {
                width = (int16_t)(available - used);
            }
            if (width < 1) {
                width = 1;
            }
            tabs_[(size_t)index].width = width;
            tabs_[(size_t)index].x = x;
            x = (int16_t)(x + width + gap);
            used += width;
        }
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
        // The band label first, at the bar's start, in the theme's
        // band-label font (the colony's SORT label position).
        if (label_ != nullptr) {
            theme_.Band_Label_Font().Print(x_,
                (int16_t)(y_ + theme_.Tab_Label_Y_Offset()), label_,
                false);
        }
        // The band first: the gaps between adjacent tabs show the
        // window band's recessed channel, one profile entry per row,
        // coalesced into one fill per run of equal colour. A theme
        // with no gap profile draws no fill, and the gaps stay the
        // background -- the old behaviour. The strip after the last
        // tab has no colony counterpart (the colony's strip ends at
        // its last tab; the settings strip ends 8 px short of MORE)
        // and is left alone.
        for (size_t i = 0; i + 1 < tabs_.size(); ++i) {
            const std::vector<uint8_t> profile =
                theme_.Tab_Gap_Profile((int16_t)i);
            if (profile.size() != (size_t)theme_.Tab_Height()) {
                continue;
            }
            const int16_t gap_x =
                (int16_t)(tabs_[i].x + tabs_[i].width);
            const int16_t gap_w =
                (int16_t)(tabs_[i + 1].x - gap_x);
            if (gap_w < 1) {
                continue;
            }
            int16_t run_start = 0;
            for (int16_t row = 1; row <= theme_.Tab_Height(); ++row) {
                if (row < theme_.Tab_Height() &&
                    profile[(size_t)row] == profile[(size_t)run_start]) {
                    continue;
                }
                graphics::Fill(gap_x, (int16_t)(y_ + run_start),
                    (int16_t)(gap_x + gap_w - 1),
                    (int16_t)(y_ + row - 1),
                    profile[(size_t)run_start]);
                run_start = row;
            }
        }
        for (size_t i = 0; i < tabs_.size(); ++i) {
            const Tab& tab = tabs_[i];
            Button button(tab.x, y_, tab.width, theme_.Tab_Height(),
                Button_Kind::Selector, theme_);
            button.Set_Label(tab.label);
            button.Set_State(tab.pressed ? Button_State::Pressed
                : (tab.active ? Button_State::Active
                              : Button_State::Rest));
            button.Set_Tab_Index((int16_t)i);
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

    Scroll_Bar::Scroll_Bar(int16_t x, int16_t width, int16_t track_y,
        int16_t track_h, const Theme& theme)
        : x_(x), width_(width), track_y_(track_y), track_h_(track_h),
          theme_(theme) {}

    bool Scroll_Bar::Visible(int total, int visible) {
        return total >= visible;
    }

    int Scroll_Bar::Max_Scroll(int total, int visible) {
        if (total <= visible) {
            return 0;
        }
        return total - visible;
    }

    int Scroll_Bar::Slider_From_Top(int top, int total, int visible) {
        return Max_Scroll(total, visible) - top;
    }

    int Scroll_Bar::Top_From_Slider(int slider, int total,
        int visible) {
        const int max_scroll = Max_Scroll(total, visible);
        if (slider < 0) {
            slider = 0;
        }
        if (slider > max_scroll) {
            slider = max_scroll;
        }
        return max_scroll - slider;
    }

    int16_t Scroll_Bar::Thumb_Top(int top, int total, int visible) const {
        return (int16_t)(track_y_ +
            (int32_t)track_h_ * (int32_t)top / (int32_t)total);
    }

    int16_t Scroll_Bar::Thumb_Bottom(int top, int total, int visible) const {
        return (int16_t)(track_y_ +
            (int32_t)track_h_ * (int32_t)(top + visible) /
                (int32_t)total);
    }

    void Scroll_Bar::Draw(int top, int total, int visible) const {
        if (!Visible(total, visible)) {
            return;
        }
        const int16_t y1 = Thumb_Top(top, total, visible);
        const int16_t y2 = Thumb_Bottom(top, total, visible);
        const Button_Style style = theme_.Button_Style();
        // The thumb body, then its bevel: bright top/left, dark
        // bottom/right -- colsum's Fill plus its four line::Line
        // edges, the axis-aligned lines drawn as fills like the
        // settings screen already did.
        graphics::Fill(x_, y1, (int16_t)(x_ + width_ - 1), y2,
            style.face);
        graphics::Fill(x_, y1, (int16_t)(x_ + width_ - 1), y1,
            style.highlight);
        graphics::Fill(x_, y1, x_, y2, style.highlight);
        graphics::Fill(x_, y2, (int16_t)(x_ + width_ - 1), y2,
            style.shadow);
        graphics::Fill((int16_t)(x_ + width_ - 1), y1,
            (int16_t)(x_ + width_ - 1), y2, style.shadow);
        // Corner dots marking the track ends, single pixels at
        // the track's corners -- not full rows across the strip.
        // 1x1 fills, the same pixels colsum's Dot calls draw.
        const int16_t right = (int16_t)(x_ + width_ - 1);
        const int16_t bottom = track_bottom();
        graphics::Fill(x_, track_y_, x_, track_y_,
            style.scroll_corner);
        graphics::Fill(right, track_y_, right, track_y_,
            style.scroll_corner);
        graphics::Fill(x_, bottom, x_, bottom, style.scroll_corner);
        graphics::Fill(right, bottom, right, bottom,
            style.scroll_corner);
    }

    void Draw_List_Background(const Theme& theme,
        const List_Layout& layout, bool fill_interior,
        bool draw_row_frames) {
        const List_Style style = theme.List_Style();
        const int16_t x = layout.panel_x();
        const int16_t y = layout.panel_y();
        if (fill_interior) {
            graphics::Fill(x, y, (int16_t)(x + layout.panel_w() - 1),
                (int16_t)(y + layout.panel_h() - 1), style.interior);
        }
        graphics::Box(x, y, layout.panel_w(), layout.panel_h(), 1,
            style.border);
        if (draw_row_frames) {
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
        } // draw_row_frames
        // The panel footing: the colony's shade band and bevel
        // below the last row -- two shade lines and a light line;
        // the panel border box above is the fourth, dark line. The
        // band sits against the border, so a taller footing leaves
        // plain panel interior above it. It draws in both modes:
        // a frameless list still keeps the panel's bottom chrome.
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
        // The panel's right edge is a 3 px bevel in the colony: a
        // bright middle line one pixel inside the border (the
        // colony's is 152 at the strip's right edge; the kit renders
        // it in the theme's frame light). The border itself is the
        // outer line and the panel interior the inner one -- the
        // colony's inner dark line (28) is darker than the menu
        // palette's darkest grey, so it needs no separate line.
        // Drawn last: the row frame lines and the footing stop at
        // the content edge, so nothing crosses it, and the scrollbar
        // draws over it where the two overlap.
        const int16_t panel_right = (int16_t)(x + layout.panel_w() - 1);
        graphics::Fill((int16_t)(panel_right - 1), y,
            (int16_t)(panel_right - 1),
            (int16_t)(y + layout.panel_h() - 1), style.frame_light);
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
