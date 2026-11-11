#include "pch.h"

#include "game/settings_sprites.h"

namespace SETTINGS_SPRITES {

    namespace {

        struct s_sprite_cache {
            bool loaded = false;
            SETTINGS_PNG::s_rgba_image frame;
            SETTINGS_PNG::s_rgba_image panel_face;
            SETTINGS_PNG::s_rgba_image control_face;
            SETTINGS_PNG::s_rgba_image button[4];
            SETTINGS_PNG::s_rgba_image star[2];
            SETTINGS_PNG::s_rgba_image thumb;
            SETTINGS_PNG::s_rgba_image arrow_face[3];
            SETTINGS_PNG::s_rgba_image triangle[2];
            SETTINGS_PNG::s_rgba_image chevron[2];
            SETTINGS_PNG::s_rgba_image glow_orange;
            SETTINGS_PNG::s_rgba_image bloom_blue;
        };

        s_sprite_cache g_cache;

        const char* kBase = "sprites/hires/";

        SETTINGS_PNG::s_rgba_image Load(const char* name) {
            char path[96];
            if (snprintf(path, sizeof(path), "%s%s", kBase, name) >=
                (int)sizeof(path)) {
                return {};
            }
            return SETTINGS_PNG::Load_Png(path);
        }

        const SETTINGS_PNG::s_rgba_image* As_Ptr(
            const SETTINGS_PNG::s_rgba_image& image) {
            return image.empty() ? nullptr : &image;
        }

    } // namespace

    void Ensure_Loaded() {
        if (g_cache.loaded) {
            return;
        }
        g_cache.loaded = true;
        g_cache.frame = Load("frame.png");
        g_cache.panel_face = Load("panel-face.png");
        g_cache.control_face = Load("control-face.png");
        g_cache.button[BUTTON_NORMAL] = Load("button-normal.png");
        g_cache.button[BUTTON_HOVER] = Load("button-hover.png");
        g_cache.button[BUTTON_PRESSED] = Load("button-pressed.png");
        g_cache.button[BUTTON_ACTIVE] = Load("button-active.png");
        g_cache.star[0] = Load("star-off.png");
        g_cache.star[1] = Load("star-on.png");
        g_cache.thumb = Load("thumb.png");
        g_cache.arrow_face[BUTTON_NORMAL] = Load("arrow-face-normal.png");
        g_cache.arrow_face[BUTTON_HOVER] = Load("arrow-face-hover.png");
        g_cache.arrow_face[BUTTON_PRESSED] = Load("arrow-face-pressed.png");
        g_cache.triangle[0] = Load("triangle-up.png");
        g_cache.triangle[1] = Load("triangle-down.png");
        g_cache.chevron[0] = Load("chevron-white.png");
        g_cache.chevron[1] = Load("chevron-blue.png");
        g_cache.glow_orange = Load("glow-orange.png");
        g_cache.bloom_blue = Load("bloom-blue.png");
    }

    void Reset() {
        g_cache = s_sprite_cache{};
    }

    bool Loaded() {
        return g_cache.loaded && !g_cache.frame.empty();
    }

    const SETTINGS_PNG::s_rgba_image* Frame() {
        return As_Ptr(g_cache.frame);
    }

    const SETTINGS_PNG::s_rgba_image* Panel_Face() {
        return As_Ptr(g_cache.panel_face);
    }

    const SETTINGS_PNG::s_rgba_image* Control_Face() {
        return As_Ptr(g_cache.control_face);
    }

    const SETTINGS_PNG::s_rgba_image* Button(int state) {
        if (state < 0 || state > 3) {
            return nullptr;
        }
        return As_Ptr(g_cache.button[state]);
    }

    const SETTINGS_PNG::s_rgba_image* Star(bool on) {
        return As_Ptr(g_cache.star[on ? 1 : 0]);
    }

    const SETTINGS_PNG::s_rgba_image* Thumb() {
        return As_Ptr(g_cache.thumb);
    }

    const SETTINGS_PNG::s_rgba_image* Arrow_Face(int state) {
        if (state < 0 || state > 2) {
            return nullptr;
        }
        return As_Ptr(g_cache.arrow_face[state]);
    }

    const SETTINGS_PNG::s_rgba_image* Triangle(bool up) {
        return As_Ptr(g_cache.triangle[up ? 0 : 1]);
    }

    const SETTINGS_PNG::s_rgba_image* Chevron(bool open) {
        return As_Ptr(g_cache.chevron[open ? 1 : 0]);
    }

    const SETTINGS_PNG::s_rgba_image* Glow_Orange() {
        return As_Ptr(g_cache.glow_orange);
    }

    const SETTINGS_PNG::s_rgba_image* Bloom_Blue() {
        return As_Ptr(g_cache.bloom_blue);
    }

} // namespace SETTINGS_SPRITES
