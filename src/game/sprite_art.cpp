#include "pch.h"

#include "sprite_art.h"

#include "game/animate.h"
#include "game/button_art.h"
#include "game/video.h"

namespace sprite_art {
    namespace {
        // Native bitmap frame layout (bitmap::Create_Bitmap_Header).
        constexpr uint32_t kBitmapFrameDataOffset = 16;

        const uint8_t* Frame_Pixels(const s_animation_header* frame) {
            return (const uint8_t*)frame + kBitmapFrameDataOffset;
        }

        // Blits one row of frame pixels to the current video page.
        // (src_x, src_y, width) is the source run; (dst_x, dst_y) its
        // destination. The run is clipped to the screen and, when
        // clipping is on, to the animate clip window; pixels outside
        // are not written.
        void Blit_Row(const uint8_t* pixels, int16_t frame_width,
            int16_t src_x, int16_t src_y, int16_t width, int16_t dst_x,
            int16_t dst_y) {
            if (video::current_video_page == nullptr || width <= 0) {
                return;
            }

            int16_t clip_x1 = 0;
            int16_t clip_x2 = (int16_t)(video::g_screen_res_x - 1);
            int16_t clip_y1 = 0;
            int16_t clip_y2 = (int16_t)(video::g_screen_res_y - 1);
            if (animate::clipping_flag == 1) {
                clip_x1 = video::screen_window_x1;
                clip_x2 = video::screen_window_x2;
                clip_y1 = video::screen_window_y1;
                clip_y2 = video::screen_window_y2;
            }
            if (dst_y < clip_y1 || dst_y > clip_y2 || dst_x > clip_x2 ||
                dst_x + width - 1 < clip_x1) {
                return;
            }

            int16_t start = dst_x;
            int16_t end = (int16_t)(dst_x + width - 1);
            int16_t src_start = src_x;
            if (start < clip_x1) {
                src_start = (int16_t)(src_start + (clip_x1 - start));
                start = clip_x1;
            }
            if (end > clip_x2) {
                end = clip_x2;
            }
            const int16_t count = (int16_t)(end - start + 1);
            if (count <= 0) {
                return;
            }

            uint8_t* dst = video::current_video_page +
                (int32_t)dst_y * video::g_screen_res_x + start;
            const uint8_t* src = pixels + (int32_t)src_y * frame_width + src_start;
            for (int16_t i = 0; i < count; ++i) {
                dst[i] = src[i];
            }
        }
    } // namespace

    s_animation_header* Load_Sprite_Frame(const char* sprite_id,
        const char* state, void* allocation_base) {
        if (sprite_id == nullptr || sprite_id[0] == '\0' ||
            state == nullptr || state[0] == '\0' ||
            allocation_base == nullptr) {
            return nullptr;
        }

        char path[128];
        if (snprintf(path, sizeof(path), "sprites/%s/%s.bin", sprite_id,
                state) >= (int)sizeof(path)) {
            return nullptr;
        }

        s_vfs_file* file = vfs::fopen(path, "rb");
        if (file == nullptr) {
            return nullptr;
        }

        int32_t size = vfs::fsize(file);
        if (size <= 0) {
            vfs::fclose(file);
            return nullptr;
        }

        auto* frame = (s_animation_header*)allocate::Allocate_Next_Block(
            allocation_base, (uint32_t)size);
        if (frame == nullptr) {
            vfs::fclose(file);
            return nullptr;
        }

        if (vfs::fread(frame, (size_t)size, 1, file) != 1) {
            vfs::fclose(file);
            return nullptr;
        }
        vfs::fclose(file);

        if (!button_art::Validate_Button_Frame((const uint8_t*)frame,
                (size_t)size)) {
            return nullptr;
        }
        return frame;
    }

    void Draw_Sprite_Frame(const s_animation_header* frame, int16_t x,
        int16_t y) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        for (int16_t row = 0; row < frame->height; ++row) {
            Blit_Row(pixels, frame->width, 0, row, frame->width, x,
                (int16_t)(y + row));
        }
    }

    void Draw_Sprite_Frame_Stretched(const s_animation_header* frame,
        int16_t x, int16_t y, int16_t width, int16_t left_slice,
        int16_t right_slice) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0 ||
            width <= 0 || left_slice < 0 || right_slice < 0 ||
            left_slice + right_slice >= frame->width) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        const int16_t middle_width =
            (int16_t)(frame->width - left_slice - right_slice);
        for (int16_t row = 0; row < frame->height; ++row) {
            const int16_t dst_y = (int16_t)(y + row);
            int16_t cursor = x;
            // Left slice, as-is.
            const int16_t left_width =
                left_slice < width ? left_slice : width;
            Blit_Row(pixels, frame->width, 0, row, left_width, cursor,
                dst_y);
            cursor = (int16_t)(cursor + left_width);
            // Middle, tiled from the source middle columns.
            const int16_t middle_target =
                (int16_t)(width - left_width - right_slice);
            int16_t middle_done = 0;
            while (middle_done < middle_target) {
                const int16_t chunk = middle_target - middle_done < middle_width
                    ? (int16_t)(middle_target - middle_done)
                    : middle_width;
                Blit_Row(pixels, frame->width, left_slice, row, chunk,
                    (int16_t)(cursor + middle_done), dst_y);
                middle_done = (int16_t)(middle_done + chunk);
            }
            cursor = (int16_t)(cursor + middle_done);
            // Right slice, as-is (cropped when the width runs out).
            const int16_t right_target = (int16_t)(width - (cursor - x));
            if (right_target > 0) {
                const int16_t right_width = right_target < right_slice
                    ? right_target
                    : right_slice;
                Blit_Row(pixels, frame->width,
                    (int16_t)(frame->width - right_slice), row, right_width,
                    cursor, dst_y);
            }
        }
    }
} // namespace sprite_art
