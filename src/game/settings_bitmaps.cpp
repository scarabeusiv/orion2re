#include "pch.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game/settings_bitmaps.h"
#include "game/settings_render.h"

namespace SETTINGS_BITMAPS {

    namespace {

        // The destination buffer for one blit: palette indices,
        // row-major, -1 for a transparent pixel (the panel shows
        // through). Built first, handed to the render in one call,
        // so the page lock is held once per element.
        struct s_buffer {
            int16_t w = 0;
            int16_t h = 0;
            std::vector<int16_t> pixels;
        };

        int Clamp_Tone(int value) {
            if (value < 0) {
                return 0;
            }
            if (value > 255) {
                return 255;
            }
            return value;
        }

        s_rgb Shifted(const s_rgb& color, int shift) {
            if (shift == 0) {
                return color;
            }
            return s_rgb{(uint8_t)Clamp_Tone((int)color.r + shift),
                (uint8_t)Clamp_Tone((int)color.g + shift),
                (uint8_t)Clamp_Tone((int)color.b + shift)};
        }

        bool Key_In(const char* keys, char key) {
            if (keys == nullptr || key == '\0') {
                return false;
            }
            for (const char* p = keys; *p != '\0'; ++p) {
                if (*p == key) {
                    return true;
                }
            }
            return false;
        }

        // The palette index for a tone character of `bitmap`:
        // `shift_keys` (a string of tone keys, or null) names
        // the tones `shift` brightens (positive) or darkens
        // (negative) -- hover and pressed states shift the
        // face/glow tones only, the hand-drawn pixels stay --
        // and `center_override` replaces the centre tone when
        // given (the popup frame's panel-fill face). -1 for
        // transparent ('.') or an unknown key.
        int16_t Tone_Index(const s_bitmap& bitmap, char key,
            const char* shift_keys, int shift,
            char center_key, const s_rgb* center_override) {
            if (key == '.') {
                return -1;
            }
            for (int i = 0; i < bitmap.tone_count; ++i) {
                if (bitmap.tones[i].key != key) {
                    continue;
                }
                s_rgb color = bitmap.tones[i].color;
                if (center_override != nullptr && key == center_key) {
                    color = *center_override;
                } else if (Key_In(shift_keys, key)) {
                    color = Shifted(color, shift);
                }
                return (int16_t)SETTINGS_RENDER::Palette_Index(color);
            }
            return -1;
        }

        // Nearest-neighbour stretch of `bitmap` into a buffer of
        // `w` x `h`: a source pixel becomes a block of screen
        // pixels, so the bitmap keeps its square pixel-art feel.
        // Each destination pixel samples the centre of its
        // source span, so the last row/column samples the
        // bitmap's last row/column instead of dropping it.
        s_buffer Stretch_Buffer(const s_bitmap& bitmap,
            int16_t w, int16_t h, const char* shift_keys, int shift,
            char center_key, const s_rgb* center_override) {
            s_buffer buffer;
            buffer.w = w;
            buffer.h = h;
            buffer.pixels.assign((size_t)w * (size_t)h, -1);
            if (w <= 0 || h <= 0 || bitmap.width <= 0 ||
                bitmap.height <= 0) {
                return buffer;
            }
            for (int16_t y = 0; y < h; ++y) {
                const int16_t sy = (int16_t)(((int32_t)(2 * y + 1) *
                    bitmap.height) / (2 * h));
                for (int16_t x = 0; x < w; ++x) {
                    const int16_t sx = (int16_t)(((int32_t)(2 * x + 1) *
                        bitmap.width) / (2 * w));
                    const char key = bitmap.rows[sy][sx];
                    buffer.pixels[(size_t)y * (size_t)w + (size_t)x] =
                        Tone_Index(bitmap, key, shift_keys, shift,
                            center_key, center_override);
                }
            }
            return buffer;
        }

        // One cell of a 9-slice frame: copy the source cell
        // (sx0..sx1, sy0..sy1) into the buffer at (dx0..dx1,
        // dy0..dy1), stretching along the edge's length only
        // (a corner copies 1:1 -- its source and destination
        // sizes are equal).
        void Blit_Cell(s_buffer& buffer, const s_bitmap& bitmap,
            int sx0, int sy0, int sx1, int sy1,
            int dx0, int dy0, int dx1, int dy1,
            const char* shift_keys, int shift,
            char center_key, const s_rgb* center_override) {
            const int sw = sx1 - sx0;
            const int sh = sy1 - sy0;
            const int dw = dx1 - dx0;
            const int dh = dy1 - dy0;
            if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
                return;
            }
            for (int y = 0; y < dh; ++y) {
                const int sy = sy0 +
                    (sh == dh ? y : (int)(((int32_t)(2 * y + 1) * sh) /
                        (2 * dh)));
                for (int x = 0; x < dw; ++x) {
                    const int sx = sx0 +
                        (sw == dw ? x
                                  : (int)(((int32_t)(2 * x + 1) * sw) /
                                      (2 * dw)));
                    const char key = bitmap.rows[sy][sx];
                    buffer.pixels[(size_t)(dy0 + y) * (size_t)buffer.w +
                        (size_t)(dx0 + x)] = Tone_Index(bitmap, key,
                        shift_keys, shift, center_key,
                        center_override);
                }
            }
        }

        // The 9-slice blit: corners 1:1, edges stretched along
        // their length, the centre a flat fill of the centre
        // tone (or its override). A rect too small for the
        // border falls back to a plain stretch.
        s_buffer Frame_Buffer(const s_frame_bitmap& frame,
            int16_t w, int16_t h, int face_shift,
            const s_rgb* center_override) {
            s_buffer buffer;
            buffer.w = w;
            buffer.h = h;
            buffer.pixels.assign((size_t)w * (size_t)h, -1);
            if (w <= 0 || h <= 0) {
                return buffer;
            }
            const s_bitmap& bitmap = frame.bitmap;
            const int bx = frame.border_x;
            const int by = frame.border_y;
            // The face tone is the frame's centre key: shifting
            // it shifts the face (and the border's inner face
            // pixels) and nothing else.
            const char face_key[2] = {frame.center, '\0'};
            if (w <= 2 * bx || h <= 2 * by) {
                return Stretch_Buffer(bitmap, w, h, face_key,
                    face_shift, frame.center, center_override);
            }
            // The centre first: a flat fill of the centre tone.
            const int16_t center_index = Tone_Index(bitmap,
                frame.center, face_key, face_shift,
                frame.center, center_override);
            for (int16_t y = 0; y < h; ++y) {
                for (int16_t x = 0; x < w; ++x) {
                    buffer.pixels[(size_t)y * (size_t)w + (size_t)x] =
                        center_index;
                }
            }
            const int bw = bitmap.width;
            const int bh = bitmap.height;
            // Corners (1:1).
            Blit_Cell(buffer, bitmap, 0, 0, bx, by, 0, 0, bx, by,
                face_key, face_shift, frame.center, center_override);
            Blit_Cell(buffer, bitmap, bw - bx, 0, bw, by, w - bx, 0,
                w, by, face_key, face_shift, frame.center,
                center_override);
            Blit_Cell(buffer, bitmap, 0, bh - by, bx, bh, 0, h - by,
                bx, h, face_key, face_shift, frame.center,
                center_override);
            Blit_Cell(buffer, bitmap, bw - bx, bh - by, bw, bh,
                w - bx, h - by, w, h, face_key, face_shift,
                frame.center, center_override);
            // Edges (stretched along their length only).
            Blit_Cell(buffer, bitmap, bx, 0, bw - bx, by, bx, 0,
                w - bx, by, face_key, face_shift, frame.center,
                center_override);
            Blit_Cell(buffer, bitmap, bx, bh - by, bw - bx, bh,
                bx, h - by, w - bx, h, face_key, face_shift,
                frame.center, center_override);
            Blit_Cell(buffer, bitmap, 0, by, bx, bh - by, 0, by,
                bx, h - by, face_key, face_shift, frame.center,
                center_override);
            Blit_Cell(buffer, bitmap, bw - bx, by, bw, bh - by,
                w - bx, by, w, h - by, face_key, face_shift,
                frame.center, center_override);
            return buffer;
        }

        void Flush(const s_rect& rect, const s_buffer& buffer) {
            if (buffer.w <= 0 || buffer.h <= 0) {
                return;
            }
            SETTINGS_RENDER::Blit_Indices(rect.x, rect.y, buffer.w,
                buffer.h, buffer.pixels.data());
        }

        // Punch rounded corners into a stretched buffer: the
        // pixels outside the rounded span go transparent, so
        // the panel shows through -- the band's outer corners
        // round while its shared edges stay square.
        void Punch_Corners(s_buffer& buffer, const s_rect& rect,
            int16_t radius, uint8_t corners) {
            if (radius <= 0 || corners == 0) {
                return;
            }
            for (int16_t y = 0; y < buffer.h; ++y) {
                int16_t x0 = 0;
                int16_t x1 = 0;
                if (!SETTINGS_RENDER::Rounded_Row_Span(rect, radius,
                    corners, (int16_t)(rect.y + y), x0, x1)) {
                    continue;
                }
                const int16_t lx0 = (int16_t)(x0 - rect.x);
                const int16_t lx1 = (int16_t)(x1 - rect.x);
                for (int16_t x = 0; x < lx0; ++x) {
                    buffer.pixels[(size_t)y * (size_t)buffer.w +
                        (size_t)x] = -1;
                }
                for (int16_t x = lx1; x < buffer.w; ++x) {
                    buffer.pixels[(size_t)y * (size_t)buffer.w +
                        (size_t)x] = -1;
                }
            }
        }

    } // namespace

    void Draw_Tab(const s_rect& rect, bool active, uint8_t corners,
        int16_t radius, int glow_shift) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        const s_bitmap& bitmap = active ? kTab_Active : kTab_Inactive;
        // Pressed darkens the glow tones only (vivid, light,
        // bright core) -- the navy edges stay, so the tab still
        // reads as the active tab while pressed.
        const char* shift_keys = active ? "BLW" : nullptr;
        s_buffer buffer = Stretch_Buffer(bitmap, rect.w, rect.h,
            shift_keys, glow_shift, '\0', nullptr);
        // Only the band's outer corners round (the radius the
        // caller passes, scaled to the screen); the shared edges
        // between tabs stay square so the band reads as one piece.
        Punch_Corners(buffer, rect, radius, corners);
        Flush(rect, buffer);
    }

    void Draw_Switch(const s_rect& rect, bool on) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        // The switch keeps its own aspect (40x14) and caps at
        // 56 px wide, centred in the value box: a 55x19 switch
        // in a 115x25 box at 640 -- big enough to read the knob
        // position at a glance, small enough to sit inside the
        // sunken value box with room around it.
        int16_t w = (int16_t)std::min<int32_t>(56,
            rect.w * 54 / 100);
        if (w < 8) {
            w = (int16_t)std::min<int32_t>(rect.w, 8);
        }
        int16_t h = (int16_t)(w * 14 / 40);
        if (h > rect.h) {
            h = rect.h;
            w = (int16_t)(h * 40 / 14);
        }
        const s_rect target{(int16_t)(rect.x + (rect.w - w) / 2),
            (int16_t)(rect.y + (rect.h - h) / 2), w, h};
        const s_bitmap& bitmap = on ? kSwitch_On : kSwitch_Off;
        Flush(target, Stretch_Buffer(bitmap, w, h, nullptr, 0, '\0',
            nullptr));
    }

    void Draw_Sunken_Box(const s_rect& rect,
        const s_rgb* center_override) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        Flush(rect, Frame_Buffer(kSunken_Box, rect.w, rect.h, 0,
            center_override));
    }

    void Draw_Plate(const s_rect& rect) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        Flush(rect, Frame_Buffer(kPlate, rect.w, rect.h, 0, nullptr));
    }

    void Draw_Grey_Button(const s_rect& rect, int face_shift) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        Flush(rect, Frame_Buffer(kGrey_Button, rect.w, rect.h,
            face_shift, nullptr));
    }

    void Draw_Thumb(const s_rect& rect) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        Flush(rect, Frame_Buffer(kThumb, rect.w, rect.h, 0, nullptr));
    }

    void Draw_Frame(const s_rect& rect, const s_rgb& center) {
        Draw_Frame(rect, kGrey_Button.bitmap, kGrey_Button.border_x,
            kGrey_Button.border_y, center);
    }

    void Draw_Frame(const s_rect& rect, const s_bitmap& bitmap,
        int border_x, int border_y, const s_rgb& center) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        const s_frame_bitmap frame{bitmap, border_x, border_y, 'F'};
        Flush(rect, Frame_Buffer(frame, rect.w, rect.h, 0, &center));
    }

    void Draw_Arrow(const s_rect& rect, bool up, int face_shift) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        const s_bitmap& bitmap = up ? kArrow_Up : kArrow_Down;
        Flush(rect, Stretch_Buffer(bitmap, rect.w, rect.h, "D",
            face_shift, '\0', nullptr));
    }

    void Draw_Chevron(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color) {
        if (w <= 0 || h <= 0) {
            return;
        }
        const int16_t index = (int16_t)SETTINGS_RENDER::Palette_Index(
            color);
        s_buffer buffer;
        buffer.w = w;
        buffer.h = h;
        buffer.pixels.assign((size_t)w * (size_t)h, -1);
        for (int16_t py = 0; py < h; ++py) {
            const int16_t sy = (int16_t)(((int32_t)(2 * py + 1) *
                kChevron_Mask.height) / (2 * h));
            for (int16_t px = 0; px < w; ++px) {
                const int16_t sx = (int16_t)(((int32_t)(2 * px + 1) *
                    kChevron_Mask.width) / (2 * w));
                if (kChevron_Mask.rows[sy][sx] == 'X') {
                    buffer.pixels[(size_t)py * (size_t)w +
                        (size_t)px] = index;
                }
            }
        }
        const s_rect rect{(int16_t)(cx - w / 2), y, w, h};
        Flush(rect, buffer);
    }

    void Draw_Outer_Frame(const s_rect& content,
        int16_t screen_width, int16_t screen_height) {
        if (content.w <= 0 || content.h <= 0 || screen_width <= 0 ||
            screen_height <= 0) {
            return;
        }
        const int16_t left_t = content.x;
        const int16_t right_t =
            (int16_t)(screen_width - (content.x + content.w));
        const int16_t top_t = content.y;
        const int16_t bottom_t =
            (int16_t)(screen_height - (content.y + content.h));
        const int16_t cx1 = (int16_t)(content.x + content.w);
        const int16_t cy1 = (int16_t)(content.y + content.h);
        // The four mottled strips: the top and bottom span the
        // full width (their ends are the screen's corners); the
        // sides run between them. A strip is stretched, never
        // tiled, so its hand-placed mottling has no repeat.
        Flush(s_rect{0, 0, screen_width, top_t},
            Stretch_Buffer(kFrame_Top, screen_width, top_t, nullptr,
                0, '\0', nullptr));
        Flush(s_rect{0, cy1, screen_width, bottom_t},
            Stretch_Buffer(kFrame_Bottom, screen_width, bottom_t,
                nullptr, 0, '\0', nullptr));
        Flush(s_rect{0, content.y, left_t, content.h},
            Stretch_Buffer(kFrame_Left, left_t, content.h, nullptr,
                0, '\0', nullptr));
        Flush(s_rect{cx1, content.y, right_t, content.h},
            Stretch_Buffer(kFrame_Right, right_t, content.h, nullptr,
                0, '\0', nullptr));
        // Rivets: one at each corner, then evenly spaced along
        // each edge -- the sample's ~78 px pitch at 640 (7 along
        // the top and bottom, 5 along each side). The bitmap
        // blits stretched to the rivet's diameter, so it scales
        // with the screen like the vector disc did.
        const int16_t rivet_r = std::max<int16_t>(2,
            (int16_t)(3 * screen_width / 640));
        const int16_t rivet_d = (int16_t)(2 * rivet_r + 1);
        const int16_t rcx0 = (int16_t)(left_t / 2);
        const int16_t rcx1 = (int16_t)(cx1 + right_t / 2);
        const int16_t rcy0 = (int16_t)(top_t / 2);
        const int16_t rcy1 = (int16_t)(cy1 + bottom_t / 2);
        auto rivet = [&](int16_t cx, int16_t cy) {
            const s_rect target{(int16_t)(cx - rivet_r),
                (int16_t)(cy - rivet_r), rivet_d, rivet_d};
            Flush(target, Stretch_Buffer(kRivet, rivet_d, rivet_d,
                nullptr, 0, '\0', nullptr));
        };
        rivet(rcx0, rcy0);
        rivet(rcx1, rcy0);
        rivet(rcx0, rcy1);
        rivet(rcx1, rcy1);
        for (int i = 1; i <= 7; ++i) {
            const int16_t x =
                (int16_t)(rcx0 + (rcx1 - rcx0) * i / 8);
            rivet(x, rcy0);
            rivet(x, rcy1);
        }
        for (int i = 1; i <= 5; ++i) {
            const int16_t y =
                (int16_t)(rcy0 + (rcy1 - rcy0) * i / 6);
            rivet(rcx0, y);
            rivet(rcx1, y);
        }
    }

} // namespace SETTINGS_BITMAPS
