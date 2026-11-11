#pragma once

#include "game/settings_png.h"

namespace SETTINGS_SPRITES {

    // Button states, matching the sprite files.
    enum e_button_state {
        BUTTON_NORMAL = 0,
        BUTTON_HOVER = 1,
        BUTTON_PRESSED = 2,
        BUTTON_ACTIVE = 3,
    };

    // Load every hi-res PNG sprite once (through the VFS, from
    // base.zip). Soft failure per sprite: a missing PNG leaves that
    // sprite empty and the caller falls back to its vector rendering,
    // so the screen works with a stale base.zip. Idempotent.
    void Ensure_Loaded();

    // Drop the cache (on screen exit). The PNGs are RGBA, not
    // palette indices, so a palette change does not need a reset;
    // this is only to free the memory between screen visits.
    void Reset();

    // True when the frame sprite loaded: the tie-together is active.
    // When false the screen renders with the vector primitives.
    bool Loaded();

    // The 9-slice frame tile (industrial border + dithered face).
    const SETTINGS_PNG::s_rgba_image* Frame();
    // Tiled faces: the panel interior and the value-box (hardened
    // display) interior.
    const SETTINGS_PNG::s_rgba_image* Panel_Face();
    const SETTINGS_PNG::s_rgba_image* Control_Face();
    // A 9-slice button face (normal / hover / pressed with the
    // orange glow / active with the analog bloom).
    const SETTINGS_PNG::s_rgba_image* Button(int state);
    // The toggle star (filled gold when on, grey outline when off).
    const SETTINGS_PNG::s_rgba_image* Star(bool on);
    // The scrollbar thumb (a bevelled indicator bar).
    const SETTINGS_PNG::s_rgba_image* Thumb();
    // A scrollbar arrow: a framed blue button face (9-slice;
    // normal / hover / pressed with the orange glow) plus a
    // standalone white triangle, drawn centred over it. Split,
    // because the triangle is a centred glyph: a 9-slice would
    // tile it.
    const SETTINGS_PNG::s_rgba_image* Arrow_Face(int state);
    const SETTINGS_PNG::s_rgba_image* Triangle(bool up);
    // The dropdown chevron (white when closed, blue when open).
    const SETTINGS_PNG::s_rgba_image* Chevron(bool open);
    // The halos, drawn BEHIND a button: the orange glow of a
    // depressed knob, and the blue bloom of a well-used analog
    // display button.
    const SETTINGS_PNG::s_rgba_image* Glow_Orange();
    const SETTINGS_PNG::s_rgba_image* Bloom_Blue();

} // namespace SETTINGS_SPRITES
