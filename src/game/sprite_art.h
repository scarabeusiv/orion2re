#pragma once

#include <cstddef>
#include <cstdint>

// orion2re-owned derived sprites.
//
// The colony screens' separable sprites cannot be drawn on a menu
// screen as-is: they are authored for palette 1, and some carry their
// label baked in. tools/extract_sprite.py derives redeployable frames
// from them (palette remap, label blanking, per-state overrides) and
// ships them in base.zip as native bitmap frames:
//
//     data/base/sprites/<sprite-id>/<state>.bin
//
// Each file is the in-memory layout bitmap::Create_Bitmap_Header
// produces: a 12-byte s_animation_header (one frame, draw mode
// BITMAP), one frame offset of 16, then width*height palette-index
// bytes, padded to a multiple of 4 bytes. Loading is soft: a missing
// or malformed file returns null and the caller keeps its previous
// rendering, so the screen works with a stale base.zip that predates
// a sprite's frames.
//
// Drawing writes every pixel, including index 0: unlike
// draw::Draw_Bitmap_Sprite (which skips 0), a derived sprite keeps
// the authored black pixels, so the frame on screen is the sprite as
// authored, not the sprite with holes where the game drew black.

struct s_animation_header;

namespace sprite_art {
    /**
     * Loads one state of a derived sprite from base.zip.
     * @param sprite_id Sprite directory under data/base/sprites/.
     * @param state State file under that directory ("normal", "pressed",
     *   "active", ...). Sprites carry no text, so there is no language
     *   dimension: the label is rendered at runtime, not baked in.
     * @param allocation_base Pool the frame is allocated from; the frame
     *   lives as long as the pool does (MOX::screen_seg, like the
     *   button frames reloaded with the menu).
     * @return The frame, or nullptr when the file is missing or
     *   malformed.
     */
    s_animation_header* Load_Sprite_Frame(const char* sprite_id,
        const char* state, void* allocation_base);

    /**
     * Draws a frame at its native size at (x, y), writing every pixel
     * including index 0. Clipped to the screen (and to the animate clip
     * window when clipping is on). The caller selects the page first
     * (video::Set_Page_Off / Set_Page_Back, as every Draw function
     * does); the blit writes to video::current_video_page.
     */
    void Draw_Sprite_Frame(const s_animation_header* frame, int16_t x,
        int16_t y);

    /**
     * Draws a frame stretched horizontally to width at (x, y), at its
     * native height: the left_slice columns are drawn as-is, the middle
     * columns are tiled across the remaining width, and the right_slice
     * columns close the frame. For a frame whose middle is flat (the
     * selector frame's blanked interior) the tiling is invisible, so
     * one derived frame serves any tab width. A width at or below
     * left_slice + right_slice draws the two slices cropped to it, with
     * no middle. Returns silently when the frame is null or the slices
     * do not fit inside it.
     */
    void Draw_Sprite_Frame_Stretched(const s_animation_header* frame,
        int16_t x, int16_t y, int16_t width, int16_t left_slice,
        int16_t right_slice);

    /**
     * Like Draw_Sprite_Frame_Stretched, but the middle band is
     * resampled (nearest neighbour) to fill the target width instead
     * of tiled: the first middle column maps to the first target
     * column and the last to the last, so a gradient middle spans
     * the width once. For a frame whose middle is a gradient (the
     * selected sort tab's blue field) tiling would repeat the
     * gradient like stripes; for a flat middle the two draws are
     * identical. Same slice, clipping, and silent-return rules.
     */
    void Draw_Sprite_Frame_Stretched_Resampled(
        const s_animation_header* frame, int16_t x, int16_t y,
        int16_t width, int16_t left_slice, int16_t right_slice);

    /**
     * Draws a frame as three slices at (x, y), at its native
     * height: the left_slice columns are drawn as-is, a single
     * middle_column is tiled across the remaining width, and the
     * right_slice columns close the frame. For a frame whose
     * middle is a flat face with a label baked in (the colony
     * RETURN), the middle column is a text-free column of that
     * face, so one derived frame serves any button width with
     * the label rendered at runtime -- the caps are the real
     * frame's pixels, and every middle pixel is a real pixel of
     * the frame's face. A width at or below left_slice +
     * right_slice draws the two slices cropped to it, with no
     * middle. Returns silently when the frame is null, the
     * slices do not fit inside it, or the middle column is
     * outside the frame.
     */
    void Draw_Sprite_Frame_Three_Slice(const s_animation_header* frame,
        int16_t x, int16_t y, int16_t width, int16_t left_slice,
        int16_t middle_column, int16_t right_slice);
} // namespace sprite_art
