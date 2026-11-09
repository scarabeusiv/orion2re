#pragma once

// A metallic button, generated from scratch.
//
// The game's metallic buttons (RETURN, the sort buttons, ACCEPT) are
// the visual reference, but these frames are ORIGINAL WORK: they are
// computed by tools/generate_metal_buttons.py from a design spec --
// a dark border and a domed face gradient in menu-palette indices --
// not extracted from the LBX files, and not grabbed from a
// screenshot of a rendered button either. The frames carry no text:
// the label is interposed at runtime, centred, so the same button
// serves any label -- MORE, LESS, RETURN, ACCEPT -- and a short
// label keeps space on the line.
//
// The button lives behind the ui_kit abstraction: it takes opaque
// ui_kit::Frame handles and ui_kit::Font references, never a sprite
// id, a palette index, or a font style number. A hi-res asset set is
// frames of the same shape at 2x, loaded the same way -- the kit
// draws whatever handles it is given, and the screens never see the
// type at all.
//
// States: normal (dark border, domed face), pressed (the face's top
// rows go into shadow -- a latched toggle stays in this state while
// it is on -- and the label lights up orange, like the real RETURN
// does when held), disabled (flat, low contrast). A frame that fails
// to load stays null and the button falls back to the engine-drawn
// metal button, so the screen works with a stale base.zip that
// predates the frames.

#include <cstdint>

#include "game/ui_kit.h"

namespace metal_button {
    enum class State {
        kNormal,
        kPressed,
        kDisabled,
    };

    // The three generated frames for one metallic button, plus how
    // the middle stretches and what the engine-drawn fallback uses
    // when a frame is missing (stale base.zip). The generated frames
    // are 85x26 with a 2 px border; the gradient is vertical, so the
    // middle tiles invisibly and one frame serves any button width.
    struct Frames {
        ui_kit::Frame normal;
        ui_kit::Frame pressed;
        ui_kit::Frame disabled;
        int16_t left_slice = 0;
        int16_t right_slice = 0;
        uint8_t fallback_face = 0;
        uint8_t fallback_highlight = 0;
        uint8_t fallback_shadow = 0;
    };

    // Loads the three generated metal-button frames from base.zip
    // (sprites/metal-button/<state>.bin) into the screen's allocation
    // base. Soft failure: a frame that fails to load stays null.
    Frames Load_Frames(void* allocation_base);

    // One metallic button: a generated frame plus a label interposed
    // at runtime, centred. The button owns its geometry: the caller
    // sizes it from Width_For_Label and draws it at that width, so
    // the click field and the drawing can never drift apart. The
    // label font is the caller's (style 4, dark text, for the
    // settings screen); the pressed font is the orange the real
    // RETURN lights up with when held; the disabled font is a dimmer
    // ramp for the disabled state.
    class Button {
      public:
        Button(Frames frames, const ui_kit::Font& label_font,
               const ui_kit::Font& pressed_font,
               const ui_kit::Font& disabled_font);
        // Width for a label: label width plus padding on each side.
        // The colony buttons carry ~10 px per side.
        int16_t Width_For_Label(const char* label) const;
        // Height of the button (the generated frame's height).
        int16_t Height() const;
        // Draws the frame for the state, stretched to width, plus the
        // label centred on it. Falls back to the engine-drawn metal
        // button when the frame for the state is missing. Pressed
        // takes the orange pressed font, so the label lights up like
        // the real RETURN does when held.
        void Draw(int16_t x, int16_t y, int16_t width, const char* label,
                  State state) const;

      private:
        Frames frames_;
        const ui_kit::Font& label_font_;
        const ui_kit::Font& pressed_font_;
        const ui_kit::Font& disabled_font_;
    };
} // namespace metal_button
