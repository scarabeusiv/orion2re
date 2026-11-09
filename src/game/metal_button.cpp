#include "pch.h"

#include "game/graphics.h"
#include "game/metal_button.h"
#include "game/sprite_art.h"

namespace metal_button {
    namespace {
        // The generated frames, 85x26 with a 2 px bevel (see
        // tools/generate_metal_buttons.py). The gradient is vertical,
        // so the middle tiles invisibly and the bevel slices are the
        // bevel width on each side.
        constexpr int16_t kButton_Height = 26;
        constexpr int16_t kBevel_Slice = 2;
        // Label padding per side, like the colony buttons' 8-10 px.
        constexpr int16_t kLabel_Padding = 10;

        // Engine-drawn fallback colours, in menu-palette indices --
        // the same metal face, highlight and shadow the settings
        // screen's Draw_Metal_Button already uses, so a stale
        // base.zip renders the same button, just without the
        // generated gradient.
        constexpr uint8_t kFallback_Face = 242;
        constexpr uint8_t kFallback_Highlight = 247;
        constexpr uint8_t kFallback_Shadow = 232;
        // Disabled fallback: the generated disabled frame's own
        // colours (flat 236 face, 235/233 bevel).
        constexpr uint8_t kFallback_Disabled_Face = 236;
        constexpr uint8_t kFallback_Disabled_Light = 235;
        constexpr uint8_t kFallback_Disabled_Dark = 233;

        void Draw_Fallback(int16_t x, int16_t y, int16_t width,
                           State state) {
            uint8_t face = kFallback_Face;
            uint8_t top = kFallback_Highlight;
            uint8_t bottom = kFallback_Shadow;
            if (state == State::kPressed) {
                // Pressed inverts the bevel (dark top/left, light
                // bottom/right).
                top = kFallback_Shadow;
                bottom = kFallback_Highlight;
            } else if (state == State::kDisabled) {
                face = kFallback_Disabled_Face;
                top = kFallback_Disabled_Light;
                bottom = kFallback_Disabled_Dark;
            }
            graphics::Fill(x, y, (int16_t)(x + width - 1),
                           (int16_t)(y + kButton_Height - 1), face);
            graphics::Fill(x, y, (int16_t)(x + width - 1), y, top);
            graphics::Fill(x, y, x, (int16_t)(y + kButton_Height - 1),
                           top);
            graphics::Fill(x, (int16_t)(y + kButton_Height - 1),
                           (int16_t)(x + width - 1),
                           (int16_t)(y + kButton_Height - 1), bottom);
            graphics::Fill((int16_t)(x + width - 1), y,
                           (int16_t)(x + width - 1),
                           (int16_t)(y + kButton_Height - 1), bottom);
        }
    } // namespace

    Frames Load_Frames(void* allocation_base) {
        Frames frames;
        frames.left_slice = kBevel_Slice;
        frames.right_slice = kBevel_Slice;
        frames.fallback_face = kFallback_Face;
        frames.fallback_highlight = kFallback_Highlight;
        frames.fallback_shadow = kFallback_Shadow;
        frames.normal = ui_kit::Frame(sprite_art::Load_Sprite_Frame(
            "metal-button", "normal", allocation_base));
        frames.pressed = ui_kit::Frame(sprite_art::Load_Sprite_Frame(
            "metal-button", "pressed", allocation_base));
        frames.disabled = ui_kit::Frame(sprite_art::Load_Sprite_Frame(
            "metal-button", "disabled", allocation_base));
        return frames;
    }

    Button::Button(Frames frames, const ui_kit::Font& label_font,
                   const ui_kit::Font& disabled_font)
        : frames_(frames), label_font_(label_font),
          disabled_font_(disabled_font) {}

    int16_t Button::Width_For_Label(const char* label) const {
        return (int16_t)(label_font_.Width(label) + 2 * kLabel_Padding);
    }

    int16_t Button::Height() const {
        return kButton_Height;
    }

    void Button::Draw(int16_t x, int16_t y, int16_t width,
                      const char* label, State state) const {
        const ui_kit::Frame* frame = nullptr;
        switch (state) {
            case State::kNormal:
                frame = &frames_.normal;
                break;
            case State::kPressed:
                frame = &frames_.pressed;
                break;
            case State::kDisabled:
                frame = &frames_.disabled;
                break;
        }
        if (frame != nullptr && !frame->Is_Null()) {
            frame->Draw_Stretched(x, y, width, frames_.left_slice,
                                  frames_.right_slice);
        } else {
            Draw_Fallback(x, y, width, state);
        }
        // The label is interposed at runtime, centred on the frame.
        // Disabled takes the dimmer font; normal and pressed take
        // the label font (the pressed frame already reads as lit).
        const ui_kit::Font& font = state == State::kDisabled
                                       ? disabled_font_
                                       : label_font_;
        const int16_t label_x =
            (int16_t)(x + (width - font.Width(label)) / 2);
        const int16_t label_y =
            (int16_t)(y + (kButton_Height - font.Height()) / 2);
        font.Print(label_x, label_y, label, false);
    }
} // namespace metal_button
