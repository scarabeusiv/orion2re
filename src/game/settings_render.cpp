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

        // Frame palette (menu palette; the background ramp owns
        // 160-223, so these stay clear of it): 244 light steel,
        // 234 dark steel.
        constexpr uint8_t kFrame_Light = 244;
        constexpr uint8_t kFrame_Dark = 234;

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

        // The 10 vertices of a 5-point star (outer radius R, inner
        // 0.45 R), starting at the top point.
        void Star_Vertices(int16_t cx, int16_t cy, int16_t radius,
            double xs[10], double ys[10]) {
            constexpr double kPi = 3.14159265358979323846;
            for (int i = 0; i < 10; ++i) {
                const double angle = -kPi / 2.0 + (double)i * kPi / 5.0;
                const double r = (i % 2 == 0) ? (double)radius : (double)radius * 0.45;
                xs[i] = (double)cx + r * std::cos(angle);
                ys[i] = (double)cy + r * std::sin(angle);
            }
        }

        // The nearest palette index for a colour, cached by RGB:
        // the sprites use a small fixed colour set, so the cache
        // stays tiny, and the per-pixel opaque path does not pay
        // a 256-entry palette scan per pixel. Cleared by Reset(),
        // with the blend tables, when the palette changes.
        std::map<uint32_t, uint8_t> g_nearest;

        uint8_t Nearest_Index_Cached(const s_rgb& color) {
            const uint32_t key = ((uint32_t)color.r << 16) |
                ((uint32_t)color.g << 8) | (uint32_t)color.b;
            auto found = g_nearest.find(key);
            if (found != g_nearest.end()) {
                return found->second;
            }
            const uint8_t index = Nearest_Index(color);
            g_nearest.emplace(key, index);
            return index;
        }

        // A one-entry cache for the last blend table: adjacent
        // pixels usually share a colour/alpha (a dithered face
        // uses a few shades, a halo a few alpha steps), so the
        // map lookup is skipped for most pixels.
        struct s_blend_cache {
            uint32_t key = 0xFFFFFFFF;
            const std::array<uint8_t, 256>* table = nullptr;

            uint8_t Blend(uint8_t dst, const s_rgb& color, int alpha) {
                const uint32_t k = ((uint32_t)color.r << 24) |
                    ((uint32_t)color.g << 16) |
                    ((uint32_t)color.b << 8) | (uint32_t)(alpha & 0xFF);
                if (k != key || table == nullptr) {
                    table = &Blend_Table(color, alpha);
                    key = k;
                }
                return (*table)[dst];
            }
        };

        // Map a destination position to a source position,
        // preserving the first `preserve` source pixels 1:1 and
        // scaling the rest. The frame's light outer edge and dark
        // inner line are the first two source rows/columns, so
        // preserve 2 keeps them as 1 px lines however the border
        // scales down; the speckle band scales.
        int Scale_Preserve(int dst, int dst_len, int src_len,
            int preserve) {
            if (dst < preserve) {
                return dst < src_len ? dst : src_len - 1;
            }
            const int src_rest = src_len - preserve;
            const int dst_rest = dst_len - preserve;
            if (src_rest <= 0 || dst_rest <= 0) {
                return src_len - 1;
            }
            return preserve + (dst - preserve) * src_rest / dst_rest;
        }

        // Blend one sprite pixel over the destination pixel.
        // Alpha is quantized to multiples of 16 (the sprites are
        // authored with quantized alphas), so the blend-table
        // cache stays bounded.
        void Blend_Sprite_Pixel(uint8_t& dst, const uint8_t* pixel,
            s_blend_cache& cache) {
            int alpha = (pixel[3] + 8) & ~15;
            if (alpha <= 0) {
                return;
            }
            if (alpha > 255) {
                alpha = 255;
            }
            const s_rgb color{pixel[0], pixel[1], pixel[2]};
            if (alpha >= 255) {
                dst = Nearest_Index_Cached(color);
                return;
            }
            dst = cache.Blend(dst, color, alpha);
        }

        // Clip a destination rect to the screen; false when empty.
        bool Clip_Dst(const SETTINGS_LAYOUT::s_rect& dst,
            int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1) {
            const int32_t stride = video::g_screen_res_x;
            const int32_t screen_h = video::g_screen_res_y;
            x0 = (int16_t)std::max<int32_t>(0, dst.x);
            y0 = (int16_t)std::max<int32_t>(0, dst.y);
            x1 = (int16_t)std::min<int32_t>(stride - 1, dst.x + dst.w - 1);
            y1 = (int16_t)std::min<int32_t>(screen_h - 1, dst.y + dst.h - 1);
            return x1 >= x0 && y1 >= y0;
        }

        // Scale a source sub-rect into a destination rect by
        // nearest neighbour, alpha-blended. `preserve_x/y` keep
        // that many leading source pixels 1:1 (the frame edges).
        void Blit_Scaled(const SETTINGS_LAYOUT::s_rect& dst,
            const SETTINGS_PNG::s_rgba_image& image,
            int src_x, int src_y, int src_w, int src_h,
            int preserve_x, int preserve_y) {
            if (dst.w <= 0 || dst.h <= 0 || src_w <= 0 || src_h <= 0 ||
                image.empty()) {
                return;
            }
            s_page_lock lock;
            if (video::current_video_page == nullptr) {
                return;
            }
            int16_t x0 = 0;
            int16_t y0 = 0;
            int16_t x1 = 0;
            int16_t y1 = 0;
            if (!Clip_Dst(dst, x0, y0, x1, y1)) {
                return;
            }
            const int32_t stride = video::g_screen_res_x;
            s_blend_cache cache;
            for (int16_t py = y0; py <= y1; ++py) {
                const int sy = src_y + Scale_Preserve(py - dst.y,
                    dst.h, src_h, preserve_y);
                uint8_t* row = video::current_video_page +
                    (size_t)py * (size_t)stride + (size_t)x0;
                for (int16_t px = x0; px <= x1; ++px) {
                    const int sx = src_x + Scale_Preserve(px - dst.x,
                        dst.w, src_w, preserve_x);
                    const uint8_t* pixel = image.Pixel(sx, sy);
                    if (pixel != nullptr) {
                        Blend_Sprite_Pixel(*row, pixel, cache);
                    }
                    ++row;
                }
            }
        }

        // Tile a source sub-rect 1:1 across a destination rect,
        // alpha-blended. A dithered face tiles: the speckle stays
        // the same grain at any resolution.
        void Blit_Tiled(const SETTINGS_LAYOUT::s_rect& dst,
            const SETTINGS_PNG::s_rgba_image& image,
            int src_x, int src_y, int src_w, int src_h) {
            if (dst.w <= 0 || dst.h <= 0 || src_w <= 0 || src_h <= 0 ||
                image.empty()) {
                return;
            }
            s_page_lock lock;
            if (video::current_video_page == nullptr) {
                return;
            }
            int16_t x0 = 0;
            int16_t y0 = 0;
            int16_t x1 = 0;
            int16_t y1 = 0;
            if (!Clip_Dst(dst, x0, y0, x1, y1)) {
                return;
            }
            const int32_t stride = video::g_screen_res_x;
            s_blend_cache cache;
            for (int16_t py = y0; py <= y1; ++py) {
                const int sy = src_y + (py - dst.y) % src_h;
                uint8_t* row = video::current_video_page +
                    (size_t)py * (size_t)stride + (size_t)x0;
                for (int16_t px = x0; px <= x1; ++px) {
                    const int sx = src_x + (px - dst.x) % src_w;
                    const uint8_t* pixel = image.Pixel(sx, sy);
                    if (pixel != nullptr) {
                        Blend_Sprite_Pixel(*row, pixel, cache);
                    }
                    ++row;
                }
            }
        }

        // True when a source sub-rect has any non-transparent
        // pixel: the 9-slice centre tiles only when it is visible
        // (the frame sprite's centre is transparent -- the border
        // is the whole sprite -- so tiling it would be a no-op
        // over the whole interior).
        bool Region_Visible(const SETTINGS_PNG::s_rgba_image& image,
            int src_x, int src_y, int src_w, int src_h) {
            for (int y = src_y; y < src_y + src_h; ++y) {
                for (int x = src_x; x < src_x + src_w; ++x) {
                    const uint8_t* pixel = image.Pixel(x, y);
                    if (pixel != nullptr && pixel[3] > 0) {
                        return true;
                    }
                }
            }
            return false;
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

    void Reset() {
        g_blend_tables.clear();
        g_nearest.clear();
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

    void Draw_Sprite(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image) {
        if (image.empty()) {
            return;
        }
        Blit_Scaled(rect, image, 0, 0, image.width, image.height, 0, 0);
    }

    void Draw_Sprite_Tiled(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image) {
        if (image.empty()) {
            return;
        }
        Blit_Tiled(rect, image, 0, 0, image.width, image.height);
    }

    void Draw_Sprite_9Slice(const SETTINGS_LAYOUT::s_rect& rect,
        const SETTINGS_PNG::s_rgba_image& image, int16_t border) {
        if (rect.w <= 0 || rect.h <= 0 || image.empty() || border <= 0) {
            return;
        }
        // The target border is the frame thickness for this
        // screen width (2 px at 640, 4 at 1280, 6 at 1920), so
        // the sprite frame matches the vector frame's weight.
        const int16_t t = Frame_Thickness(video::g_screen_res_x);
        const int bw = image.width;
        const int bh = image.height;
        const int b = std::min<int>(border, std::min(bw, bh) / 2);
        if (b <= 0) {
            return;
        }
        // Corners: the source border corners scale to the target
        // border, preserving the light edge + dark line (2 px).
        Blit_Scaled(SETTINGS_LAYOUT::s_rect{rect.x, rect.y, t, t},
            image, 0, 0, b, b, 2, 2);
        Blit_Scaled(SETTINGS_LAYOUT::s_rect{
            (int16_t)(rect.x + rect.w - t), rect.y, t, t},
            image, bw - b, 0, b, b, 2, 2);
        Blit_Scaled(SETTINGS_LAYOUT::s_rect{rect.x,
            (int16_t)(rect.y + rect.h - t), t, t},
            image, 0, bh - b, b, b, 2, 2);
        Blit_Scaled(SETTINGS_LAYOUT::s_rect{
            (int16_t)(rect.x + rect.w - t),
            (int16_t)(rect.y + rect.h - t), t, t},
            image, bw - b, bh - b, b, b, 2, 2);
        // Edges: the top/bottom strips stretch horizontally, the
        // left/right strips vertically; the thickness axis keeps
        // the light edge + dark line.
        if (rect.w > 2 * t) {
            Blit_Scaled(SETTINGS_LAYOUT::s_rect{
                (int16_t)(rect.x + t), rect.y,
                (int16_t)(rect.w - 2 * t), t},
                image, b, 0, bw - 2 * b, b, 0, 2);
            Blit_Scaled(SETTINGS_LAYOUT::s_rect{
                (int16_t)(rect.x + t), (int16_t)(rect.y + rect.h - t),
                (int16_t)(rect.w - 2 * t), t},
                image, b, bh - b, bw - 2 * b, b, 0, 2);
        }
        if (rect.h > 2 * t) {
            Blit_Scaled(SETTINGS_LAYOUT::s_rect{rect.x,
                (int16_t)(rect.y + t), t, (int16_t)(rect.h - 2 * t)},
                image, 0, b, b, bh - 2 * b, 2, 0);
            Blit_Scaled(SETTINGS_LAYOUT::s_rect{
                (int16_t)(rect.x + rect.w - t), (int16_t)(rect.y + t),
                t, (int16_t)(rect.h - 2 * t)},
                image, bw - b, b, b, bh - 2 * b, 2, 0);
        }
        // Centre: the dithered face tiles, so the speckle never
        // stretches into streaks. Skipped when the centre is
        // fully transparent (the frame sprite: the border is the
        // whole sprite, and the caller tiles the face under it).
        if (rect.w > 2 * t && rect.h > 2 * t &&
            Region_Visible(image, b, b, bw - 2 * b, bh - 2 * b)) {
            Blit_Tiled(SETTINGS_LAYOUT::s_rect{
                (int16_t)(rect.x + t), (int16_t)(rect.y + t),
                (int16_t)(rect.w - 2 * t), (int16_t)(rect.h - 2 * t)},
                image, b, b, bw - 2 * b, bh - 2 * b);
        }
    }

    void Frame(const SETTINGS_LAYOUT::s_rect& rect, int16_t thickness) {
        if (rect.w <= 0 || rect.h <= 0 || thickness <= 0) {
            return;
        }
        graphics::Box(rect.x, rect.y, rect.w, rect.h, thickness, kFrame_Light);
        if (rect.w > 2 * thickness + 2 && rect.h > 2 * thickness + 2) {
            graphics::Box((int16_t)(rect.x + thickness), (int16_t)(rect.y + thickness),
                (int16_t)(rect.w - 2 * thickness), (int16_t)(rect.h - 2 * thickness),
                1, kFrame_Dark);
        }
    }

    void Panel(const SETTINGS_LAYOUT::s_rect& rect, const s_rgb& fill, int alpha) {
        Blend_Fill(rect, fill, alpha);
        Frame(rect, Frame_Thickness(video::g_screen_res_x));
    }

    void Button(const SETTINGS_LAYOUT::s_rect& rect, const char* label,
        e_button_state state, int cell_height) {
        s_rgb fill = kControl_Fill;
        int alpha = kControl_Alpha;
        s_rgb text_color = kText_Bright;
        switch (state) {
            case BUTTON_HOVER:
                fill = kControl_Hover_Fill;
                alpha = kControl_Hover_Alpha;
                break;
            case BUTTON_PRESSED:
                fill = kControl_Pressed_Fill;
                alpha = kControl_Pressed_Alpha;
                break;
            case BUTTON_ACTIVE:
                fill = kActive_Fill;
                alpha = kActive_Alpha;
                text_color = kText_Header;
                break;
            case BUTTON_DISABLED:
                fill = kControl_Fill;
                alpha = kPanel_Alpha;
                text_color = kText_Dim;
                break;
            case BUTTON_NORMAL:
            default:
                break;
        }
        Blend_Fill(rect, fill, alpha);
        const int16_t thickness = Frame_Thickness(video::g_screen_res_x);
        Frame(rect, thickness);
        if (state != BUTTON_PRESSED) {
            // 1 px top highlight, just inside the frame: the metal sheen.
            const SETTINGS_LAYOUT::s_rect highlight{
                (int16_t)(rect.x + thickness), (int16_t)(rect.y + thickness),
                (int16_t)(rect.w - 2 * thickness), 1};
            Blend_Fill(highlight, s_rgb{255, 255, 255}, 40);
        }
        if (label != nullptr && label[0] != '\0') {
            const int16_t text_y = (int16_t)(rect.y + (rect.h - cell_height) / 2 +
                (state == BUTTON_PRESSED ? 1 : 0));
            Draw_Text((int16_t)(rect.x + rect.w / 2), text_y, label, text_color,
                cell_height, 1);
        }
    }

    void Star(int16_t cx, int16_t cy, int16_t radius, const s_rgb& color,
        bool filled) {
        if (radius < 3) {
            return;
        }
        const uint8_t index = Nearest_Index(color);
        double xs[10];
        double ys[10];
        Star_Vertices(cx, cy, radius, xs, ys);
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        if (filled) {
            for (int16_t y = (int16_t)(cy - radius); y <= (int16_t)(cy + radius); ++y) {
                double crossings[10];
                int count = 0;
                for (int i = 0; i < 10; ++i) {
                    const int j = (i + 1) % 10;
                    const double y1 = ys[i];
                    const double y2 = ys[j];
                    if ((y1 <= (double)y && y2 > (double)y) ||
                        (y2 <= (double)y && y1 > (double)y)) {
                        crossings[count++] =
                            xs[i] + ((double)y - y1) * (xs[j] - xs[i]) / (y2 - y1);
                    }
                }
                if (count < 2) {
                    continue;
                }
                std::sort(crossings, crossings + count);
                for (int i = 0; i + 1 < count; i += 2) {
                    Fill_Span((int16_t)std::ceil(std::min(crossings[i], crossings[i + 1])),
                        (int16_t)std::floor(std::max(crossings[i], crossings[i + 1])),
                        y, index);
                }
            }
            return;
        }
        // Outline: plot the polygon edges.
        for (int i = 0; i < 10; ++i) {
            const int j = (i + 1) % 10;
            const double dx = xs[j] - xs[i];
            const double dy = ys[j] - ys[i];
            const int steps = (int)std::max(std::abs(dx), std::abs(dy));
            for (int s = 0; s <= steps; ++s) {
                const int16_t x = (int16_t)std::lround(xs[i] + dx * s / std::max(1, steps));
                const int16_t y = (int16_t)std::lround(ys[i] + dy * s / std::max(1, steps));
                Write_Pixel(x, y, index);
            }
        }
    }

    void Triangle_Down(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color) {
        if (w <= 0 || h <= 0) {
            return;
        }
        const uint8_t index = Nearest_Index(color);
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        for (int16_t dy = 0; dy < h; ++dy) {
            const int16_t half = (int16_t)(((int32_t)w / 2) * (h - dy) / h);
            Fill_Span((int16_t)(cx - half), (int16_t)(cx + half), (int16_t)(y + dy), index);
        }
    }

    void Triangle_Up(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color) {
        if (w <= 0 || h <= 0) {
            return;
        }
        const uint8_t index = Nearest_Index(color);
        s_page_lock lock;
        if (video::current_video_page == nullptr) {
            return;
        }
        for (int16_t dy = 0; dy < h; ++dy) {
            const int16_t half = (int16_t)(((int32_t)w / 2) * dy / h);
            Fill_Span((int16_t)(cx - half), (int16_t)(cx + half), (int16_t)(y + dy), index);
        }
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
