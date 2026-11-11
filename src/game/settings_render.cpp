#include "pch.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#include "game/fonts.h"
#include "game/graphics.h"
#include "game/mouse.h"
#include "game/platform.h"
#include "game/settings_blend.h"
#include "game/settings_hd_font.h"
#include "game/settings_render.h"
#include "game/video.h"

namespace SETTINGS_RENDER {

    namespace {

        // The screen's standard text ramp, as used by the settings
        // screen (g_normal_colors): the 8 palette indices the
        // engine font's ink levels map through.
        constexpr uint8_t kFont_Ramp[8] = {150, 154, 155, 151, 152, 153, 154, 155};
        // The engine font style the HD font decodes from.
        constexpr int kFont_Style = 4;

        // Alpha levels for text coverage: coverage is quantized to
        // 32 levels so the blend tables stay bounded (one table per
        // colour per level, built once and reused every frame).
        constexpr int kText_Alpha_Levels = 32;

        // The current palette in 8-bit display space (stored 6-bit
        // values shifted left 2).
        void Palette_RGB(uint8_t palette[256][3]) {
            for (int i = 0; i < 256; ++i) {
                palette[i][0] = (uint8_t)(video::current_palette[i].r << 2);
                palette[i][1] = (uint8_t)(video::current_palette[i].g << 2);
                palette[i][2] = (uint8_t)(video::current_palette[i].b << 2);
            }
        }

        std::map<uint32_t, std::array<uint8_t, 256>> g_blend_tables;

        const std::array<uint8_t, 256>& Blend_Table(const s_rgb& color, int alpha) {
            const uint32_t key = ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) |
                ((uint32_t)color.b << 8) | (uint32_t)(alpha & 0xFF);
            auto found = g_blend_tables.find(key);
            if (found != g_blend_tables.end()) {
                return found->second;
            }
            uint8_t palette[256][3];
            Palette_RGB(palette);
            auto inserted = g_blend_tables.emplace(key,
                SETTINGS_BLEND::Build_Blend_Table(color, alpha, palette));
            return inserted.first->second;
        }

        uint8_t Nearest_Index(const s_rgb& color) {
            uint8_t palette[256][3];
            Palette_RGB(palette);
            return SETTINGS_BLEND::Nearest_Palette_Index(color, palette);
        }

        // The render-buffer lock: Lock_Appropriate_Render_Buffer
        // selects the current page (Unlock is a no-op), mirroring
        // graphics::Fill. Held for one primitive at a time, never
        // across a call into graphics:: (which locks for itself).
        struct s_page_lock {
            s_page_lock() {
                video::g_screen_res_x =
                    (int32_t)platform::Lock_Appropriate_Render_Buffer();
            }
            ~s_page_lock() {
                platform::Unlock_Appropriate_Render_Buffer();
            }
        };

        struct s_font_cache {
            bool loaded = false;
            bool available = false;
            int src_height = 0;
            int spacing = 0;
            int advances[256] = {};
            SETTINGS_HD_FONT::s_hd_glyph glyphs[256];
        };

        s_font_cache g_font;

        struct s_scaled_glyph {
            int width = 0;
            int height = 0;
            int advance = 0;
            std::vector<uint8_t> coverage;
        };

        // Scaled glyphs, keyed by (cell height, char).
        std::map<uint32_t, s_scaled_glyph> g_scaled;

        const s_scaled_glyph* Scaled_Glyph(unsigned char c, int cell_height) {
            if (!g_font.available || cell_height <= 0) {
                return nullptr;
            }
            const uint32_t key = ((uint32_t)cell_height << 8) | (uint32_t)c;
            auto found = g_scaled.find(key);
            if (found != g_scaled.end()) {
                return &found->second;
            }
            const SETTINGS_HD_FONT::s_hd_glyph& src = g_font.glyphs[c];
            s_scaled_glyph scaled;
            scaled.height = cell_height;
            scaled.width = (int)(((int64_t)src.width * cell_height +
                g_font.src_height / 2) / g_font.src_height);
            scaled.advance = (int)(((int64_t)g_font.advances[c] * cell_height +
                g_font.src_height / 2) / g_font.src_height);
            scaled.coverage = SETTINGS_HD_FONT::Scale_Coverage(src.coverage,
                src.width, src.height, std::max(1, scaled.width), cell_height);
            auto inserted = g_scaled.emplace(key, std::move(scaled));
            return &inserted.first->second;
        }

        // Direct pixel write under the page lock, clipped to the screen.
        void Write_Pixel(int16_t x, int16_t y, uint8_t index) {
            if (x < 0 || y < 0 || x >= video::g_screen_res_x ||
                y >= video::g_screen_res_y || video::current_video_page == nullptr) {
                return;
            }
            video::current_video_page[(size_t)y * (size_t)video::g_screen_res_x + (size_t)x] =
                index;
        }

        // Fill one horizontal span with a palette index.
        void Fill_Span(int16_t x1, int16_t x2, int16_t y, uint8_t index) {
            if (video::current_video_page == nullptr || y < 0 ||
                y >= video::g_screen_res_y) {
                return;
            }
            if (x1 < 0) {
                x1 = 0;
            }
            if (x2 >= video::g_screen_res_x) {
                x2 = (int16_t)(video::g_screen_res_x - 1);
            }
            if (x2 < x1) {
                return;
            }
            uint8_t* row = video::current_video_page +
                (size_t)y * (size_t)video::g_screen_res_x + (size_t)x1;
            for (int16_t x = x1; x <= x2; ++x) {
                *row++ = index;
            }
        }

        // The nearest palette index for a colour: the opaque
        // fills and the bitmap blits map through it.

        // The corner inset of a rounded rect: how far the span
        // starts in from the rect's edge, on a corner row `i`
        // rows in from the corner's outer edge (0-based). The arc
        // is the quarter circle of radius r centred at (r, r) in
        // the corner square; a pixel is in when its centre is
        // inside the circle. r 4 cuts 2, 1, 0, 0 px off the four
        // corner rows; r 2 cuts 1, 0 -- the sample's barely-round
        // value boxes.
        int16_t Corner_Inset(int16_t r, int16_t i) {
            if (r <= 1 || i < 0 || i >= r) {
                return 0;
            }
            const double dy = (double)r - (double)i - 0.5;
            const double edge = (double)r -
                std::sqrt((double)r * (double)r - dy * dy);
            const int16_t inset = (int16_t)std::ceil(edge - 0.5);
            if (inset < 0) {
                return 0;
            }
            return inset > r ? r : inset;
        }

    } // namespace

    int16_t Frame_Thickness(int16_t screen_width) {
        const int16_t thickness = (int16_t)(screen_width / 320);
        if (thickness < 2) {
            return 2;
        }
        if (thickness > 6) {
            return 6;
        }
        return thickness;
    }

    int16_t Scaled_Radius(int16_t screen_width, int16_t base_radius) {
        if (base_radius <= 0 || screen_width <= 0) {
            return 1;
        }
        const int16_t radius =
            (int16_t)((base_radius * screen_width + 320) / 640);
        return radius < 1 ? 1 : radius;
    }

    void Reset() {
        g_blend_tables.clear();
        g_scaled.clear();
        g_font = s_font_cache{};
    }

    bool Ensure_Font() {
        if (g_font.loaded) {
            return g_font.available;
        }
        g_font.loaded = true;
        g_font.available = false;
        if (fonts::font_data == nullptr) {
            return false;
        }
        // Save the active style; the decode below selects style 4.
        const int saved_style = fonts::Get_Current_Font_Style();
        const s_colors* current_colors = fonts::Get_Current_Font_Color();
        const s_colors saved_colors =
            current_colors != nullptr ? *current_colors : s_colors{};
        s_colors ramp_colors{};
        for (int i = 0; i < 8; ++i) {
            ramp_colors.colors[i] = kFont_Ramp[i];
        }
        fonts::Set_Font_Style(kFont_Style, &ramp_colors);
        const int src_height = fonts::font_header.current_height;
        if (src_height <= 0) {
            fonts::Set_Font_Style(saved_style, &saved_colors);
            return false;
        }
        g_font.src_height = src_height;
        g_font.spacing = fonts::font_header.font_style_spacing;
        uint8_t palette[256][3];
        Palette_RGB(palette);
        uint8_t ramp_luminance[8];
        for (int i = 0; i < 8; ++i) {
            const uint8_t index = kFont_Ramp[i];
            ramp_luminance[i] = (uint8_t)SETTINGS_BLEND::Luminance(
                palette[index][0], palette[index][1], palette[index][2]);
        }
        for (int c = 0; c < 256; ++c) {
            const int width = fonts::font_header.font_style_widths[c];
            g_font.advances[c] = width + g_font.spacing;
            if (c < 32 || c > 126 || width <= 0) {
                g_font.glyphs[c] = SETTINGS_HD_FONT::s_hd_glyph{};
                g_font.glyphs[c].width = std::max(0, width);
                g_font.glyphs[c].height = src_height;
                g_font.glyphs[c].coverage.assign(
                    (size_t)std::max(0, width) * (size_t)src_height, 0);
                continue;
            }
            const uint32_t offset = fonts::font_header.font_style_offsets[c];
            g_font.glyphs[c] = SETTINGS_HD_FONT::Decode_Glyph(
                fonts::font_data + offset, width, src_height, ramp_luminance);
        }
        // The space advance is the word gap; a missing space width
        // would glue words together.
        if (g_font.advances[' '] <= g_font.spacing) {
            g_font.advances[' '] = src_height / 2 + g_font.spacing;
        }
        fonts::Set_Font_Style(saved_style, &saved_colors);
        g_font.available = true;
        return true;
    }

    int Text_Width(const char* text, int cell_height) {
        if (text == nullptr || cell_height <= 0) {
            return 0;
        }
        if (!Ensure_Font()) {
            return (int)strlen(text) * cell_height / 2;
        }
        int width = 0;
        for (const char* p = text; *p != '\0'; ++p) {
            const s_scaled_glyph* glyph = Scaled_Glyph((unsigned char)*p, cell_height);
            width += glyph != nullptr ? glyph->advance : cell_height / 2;
        }
        return width;
    }

    int Line_Height(int cell_height) {
        return cell_height + std::max(2, cell_height / 4);
    }

    void Draw_Text(int16_t x, int16_t y, const char* text, const s_rgb& color,
        int cell_height, int align) {
        if (text == nullptr || text[0] == '\0' || cell_height <= 0) {
            return;
        }
        if (!Ensure_Font()) {
            // Fallback: the engine pixel font, never a blank label.
            s_colors ramp_colors{};
            for (int i = 0; i < 8; ++i) {
                ramp_colors.colors[i] = kFont_Ramp[i];
            }
            fonts::Set_Font_Style(kFont_Style, &ramp_colors);
            if (align == 1) {
                fonts::Print_Centered(x, y, text);
            } else if (align == 2) {
                fonts::Print_Right(x, y, text);
            } else {
                fonts::Print(x, y, text);
            }
            return;
        }
        const int width = Text_Width(text, cell_height);
        int16_t x0 = x;
        if (align == 1) {
            x0 = (int16_t)(x - width / 2);
        } else if (align == 2) {
            x0 = (int16_t)(x - width);
        }
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        const int32_t stride = video::g_screen_res_x;
        const int32_t screen_h = video::g_screen_res_y;
        int16_t pen_x = x0;
        // One blend table per coverage level; the level changes
        // rarely within a glyph, so cache the last table instead
        // of looking the map up per pixel.
        int last_alpha = -1;
        const std::array<uint8_t, 256>* table = nullptr;
        for (const char* p = text; *p != '\0'; ++p) {
            const unsigned char c = (unsigned char)*p;
            const s_scaled_glyph* glyph = Scaled_Glyph(c, cell_height);
            if (glyph == nullptr) {
                pen_x = (int16_t)(pen_x + cell_height / 2);
                continue;
            }
            const int16_t gx0 = pen_x;
            const int16_t gy0 = y;
            const int16_t gx1 = (int16_t)(gx0 + glyph->width - 1);
            const int16_t gy1 = (int16_t)(gy0 + glyph->height - 1);
            if (gx1 >= 0 && gy1 >= 0 && gx0 < stride && gy0 < screen_h) {
                const int16_t cx0 = (int16_t)std::max<int32_t>(0, gx0);
                const int16_t cy0 = (int16_t)std::max<int32_t>(0, gy0);
                const int16_t cx1 = (int16_t)std::min<int32_t>(stride - 1, gx1);
                const int16_t cy1 = (int16_t)std::min<int32_t>(screen_h - 1, gy1);
                for (int16_t py = cy0; py <= cy1; ++py) {
                    uint8_t* row = video::current_video_page +
                        (size_t)py * (size_t)stride + (size_t)cx0;
                    const size_t src_row =
                        (size_t)(py - gy0) * (size_t)glyph->width;
                    for (int16_t px = cx0; px <= cx1; ++px) {
                        const uint8_t coverage =
                            glyph->coverage[src_row + (size_t)(px - gx0)];
                        if (coverage == 0) {
                            ++row;
                            continue;
                        }
                        const int level = (coverage * (kText_Alpha_Levels - 1) + 127) / 255;
                        const int alpha = level * 255 / (kText_Alpha_Levels - 1);
                        if (alpha != last_alpha) {
                            table = &Blend_Table(color, alpha);
                            last_alpha = alpha;
                        }
                        *row = (*table)[*row];
                        ++row;
                    }
                }
            }
            pen_x = (int16_t)(pen_x + glyph->advance);
        }
    }

    void Blend_Fill(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& color,
        int alpha) {
        if (rect.w <= 0 || rect.h <= 0 || alpha <= 0) {
            return;
        }
        if (alpha >= 255) {
            Solid_Fill(rect, color);
            return;
        }
        const std::array<uint8_t, 256>& table = Blend_Table(color, alpha);
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        const int32_t stride = video::g_screen_res_x;
        const int32_t screen_h = video::g_screen_res_y;
        const int16_t x0 = (int16_t)std::max<int32_t>(0, rect.x);
        const int16_t y0 = (int16_t)std::max<int32_t>(0, rect.y);
        const int16_t x1 = (int16_t)std::min<int32_t>(stride - 1,
            rect.x + rect.w - 1);
        const int16_t y1 = (int16_t)std::min<int32_t>(screen_h - 1,
            rect.y + rect.h - 1);
        for (int16_t py = y0; py <= y1; ++py) {
            uint8_t* row = video::current_video_page +
                (size_t)py * (size_t)stride + (size_t)x0;
            for (int16_t px = x0; px <= x1; ++px) {
                *row = table[*row];
                ++row;
            }
        }
    }

    void Solid_Fill(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& color) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        graphics::Fill(rect.x, rect.y, (int16_t)(rect.x + rect.w - 1),
            (int16_t)(rect.y + rect.h - 1), Nearest_Index(color));
    }

    uint8_t Palette_Index(const s_rgb& color) {
        return Nearest_Index(color);
    }

    void Blit_Indices(int16_t x, int16_t y, int16_t w, int16_t h,
        const int16_t* indices) {
        if (w <= 0 || h <= 0 || indices == nullptr) {
            return;
        }
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        const int32_t stride = video::g_screen_res_x;
        const int32_t screen_h = video::g_screen_res_y;
        const int16_t y0 = (int16_t)std::max<int32_t>(0, y);
        const int16_t y1 = (int16_t)std::min<int32_t>(screen_h - 1,
            y + h - 1);
        const int16_t x0 = (int16_t)std::max<int32_t>(0, x);
        const int16_t x1 = (int16_t)std::min<int32_t>(stride - 1,
            x + w - 1);
        for (int16_t py = y0; py <= y1; ++py) {
            uint8_t* row = video::current_video_page +
                (size_t)py * (size_t)stride + (size_t)x0;
            const int16_t* src = indices +
                (size_t)(py - y) * (size_t)w + (size_t)(x0 - x);
            for (int16_t px = x0; px <= x1; ++px, ++src) {
                if (*src >= 0) {
                    *row = (uint8_t)*src;
                }
                ++row;
            }
        }
    }

    bool Rounded_Row_Span(const SETTINGS_LAYOUT::s_rect& rect,
        int16_t radius, uint8_t corners, int16_t y, int16_t& x0,
        int16_t& x1) {
        if (rect.w <= 0 || rect.h <= 0 || y < rect.y ||
            y >= (int16_t)(rect.y + rect.h)) {
            return false;
        }
        if (radius > rect.w / 2) {
            radius = (int16_t)(rect.w / 2);
        }
        if (radius > rect.h / 2) {
            radius = (int16_t)(rect.h / 2);
        }
        x0 = rect.x;
        x1 = (int16_t)(rect.x + rect.w);
        if (radius > 0) {
            if (y < (int16_t)(rect.y + radius)) {
                const int16_t i = (int16_t)(y - rect.y);
                if ((corners & 0x1) != 0) {
                    x0 = (int16_t)(x0 + Corner_Inset(radius, i));
                }
                if ((corners & 0x2) != 0) {
                    x1 = (int16_t)(x1 - Corner_Inset(radius, i));
                }
            } else if (y >= (int16_t)(rect.y + rect.h - radius)) {
                const int16_t i =
                    (int16_t)(rect.y + rect.h - 1 - y);
                if ((corners & 0x8) != 0) {
                    x0 = (int16_t)(x0 + Corner_Inset(radius, i));
                }
                if ((corners & 0x4) != 0) {
                    x1 = (int16_t)(x1 - Corner_Inset(radius, i));
                }
            }
        }
        return x1 > x0;
    }

    bool Point_In_Rect(int16_t px, int16_t py, const SETTINGS_LAYOUT::s_rect& rect) {
        return px >= rect.x && px < rect.x + rect.w && py >= rect.y &&
            py < rect.y + rect.h;
    }

    bool Is_Hover(const SETTINGS_LAYOUT::s_rect& rect) {
        return Point_In_Rect(mouse::Pointer_X(), mouse::Pointer_Y(), rect);
    }

    bool Is_Pressed(const SETTINGS_LAYOUT::s_rect& rect) {
        return mouse::Mouse_Button() != 0 && Is_Hover(rect);
    }

} // namespace SETTINGS_RENDER
