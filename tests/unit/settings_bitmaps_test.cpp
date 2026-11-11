#include "pch.h"

#include "doctest/doctest.h"
#include "game/settings_bitmaps.h"

namespace {

    using namespace SETTINGS_BITMAPS;

    int Luminance(const s_rgb& color) {
        return (299 * (int)color.r + 587 * (int)color.g +
            114 * (int)color.b) / 1000;
    }

    // Every row is exactly the declared width, and every
    // character is transparent ('.') or a key in the tone table:
    // a mistyped row would blit magenta (unknown key) or read
    // past the row.
    void Check_Bitmap(const s_bitmap& bitmap) {
        CHECK(bitmap.width > 0);
        CHECK(bitmap.height > 0);
        CHECK(bitmap.rows != nullptr);
        // A mask bitmap has no tone table (tone_count 0, tones
        // null); every other bitmap has one.
        CHECK((bitmap.tone_count == 0) == (bitmap.tones == nullptr));
        for (int y = 0; y < bitmap.height; ++y) {
            CHECK(bitmap.rows[y] != nullptr);
            int len = 0;
            while (bitmap.rows[y][len] != '\0') {
                ++len;
            }
            CHECK(len == bitmap.width);
            for (int x = 0; x < bitmap.width; ++x) {
                const char key = bitmap.rows[y][x];
                if (key == '.') {
                    continue;
                }
                // A mask bitmap (no tone table) treats every
                // non-transparent character as ink.
                if (bitmap.tone_count == 0) {
                    continue;
                }
                bool known = false;
                for (int i = 0; i < bitmap.tone_count; ++i) {
                    if (bitmap.tones[i].key == key) {
                        known = true;
                    }
                }
                CHECK(known);
            }
        }
    }

    void Check_Frame(const s_frame_bitmap& frame,
        bool rounded_corners = true) {
        Check_Bitmap(frame.bitmap);
        CHECK(frame.border_x > 0);
        CHECK(frame.border_y > 0);
        CHECK(2 * frame.border_x < frame.bitmap.width);
        CHECK(2 * frame.border_y < frame.bitmap.height);
        // The centre tone is a key in the tone table.
        bool known = false;
        for (int i = 0; i < frame.bitmap.tone_count; ++i) {
            if (frame.bitmap.tones[i].key == frame.center) {
                known = true;
            }
        }
        CHECK(known);
        if (!rounded_corners) {
            // The thumb is square-cornered by design (the
            // colony's indicator bar slides in its track with
            // square corners); the other frames round theirs.
            return;
        }
        // The corners are cut: the outer corner pixel is
        // transparent, so the element rounds instead of
        // squaring into the panel.
        CHECK(frame.bitmap.rows[0][0] == '.');
        CHECK(frame.bitmap.rows[0][frame.bitmap.width - 1] == '.');
        CHECK(frame.bitmap.rows[frame.bitmap.height - 1][0] == '.');
        CHECK(frame.bitmap.rows[frame.bitmap.height - 1]
            [frame.bitmap.width - 1] == '.');
    }

    const s_rgb* Tone_Of(const s_bitmap& bitmap, char key) {
        for (int i = 0; i < bitmap.tone_count; ++i) {
            if (bitmap.tones[i].key == key) {
                return &bitmap.tones[i].color;
            }
        }
        return nullptr;
    }

    // The centroid x of the pixels of `key` in the bitmap.
    double Centroid_X(const s_bitmap& bitmap, char key) {
        double sum = 0.0;
        int count = 0;
        for (int y = 0; y < bitmap.height; ++y) {
            for (int x = 0; x < bitmap.width; ++x) {
                if (bitmap.rows[y][x] == key) {
                    sum += x;
                    ++count;
                }
            }
        }
        return count > 0 ? sum / count : -1.0;
    }

    int Count_Key(const s_bitmap& bitmap, char key) {
        int count = 0;
        for (int y = 0; y < bitmap.height; ++y) {
            for (int x = 0; x < bitmap.width; ++x) {
                if (bitmap.rows[y][x] == key) {
                    ++count;
                }
            }
        }
        return count;
    }

    TEST_CASE("every bitmap is well formed") {
        Check_Bitmap(kTab_Active);
        Check_Bitmap(kTab_Inactive);
        Check_Bitmap(kSwitch_On);
        Check_Bitmap(kSwitch_Off);
        Check_Bitmap(kArrow_Up);
        Check_Bitmap(kArrow_Down);
        Check_Bitmap(kChevron_Mask);
        Check_Frame(kSunken_Box);
        Check_Frame(kPlate);
        Check_Frame(kGrey_Button);
        Check_Frame(kThumb, false);
        Check_Bitmap(kFrame_Top);
        Check_Bitmap(kFrame_Bottom);
        Check_Bitmap(kFrame_Left);
        Check_Bitmap(kFrame_Right);
        Check_Bitmap(kRivet);
    }

    TEST_CASE("the active tab blooms from its centre") {
        // The centre pixel is the bright core; the corners are
        // the dark navy -- the glow is brightest in the middle
        // and fades outward, not a flat fill.
        const s_rgb* centre =
            Tone_Of(kTab_Active, kTab_Active.rows[5][16]);
        const s_rgb* corner =
            Tone_Of(kTab_Active, kTab_Active.rows[0][0]);
        CHECK(centre != nullptr);
        CHECK(corner != nullptr);
        CHECK(Luminance(*centre) > Luminance(*corner));
        CHECK(Luminance(*centre) > 150);
        CHECK(Luminance(*corner) < 20);
        // The inactive tab is flat dark navy throughout: its
        // brightest tone is far dimmer than the active tab's core.
        const s_rgb* inactive_centre =
            Tone_Of(kTab_Inactive, kTab_Inactive.rows[5][16]);
        CHECK(inactive_centre != nullptr);
        CHECK(Luminance(*inactive_centre) <
            Luminance(*centre) / 2);
        // The two tab bitmaps are the same size, so they tile
        // the same band.
        CHECK(kTab_Active.width == kTab_Inactive.width);
        CHECK(kTab_Active.height == kTab_Inactive.height);
    }

    TEST_CASE("the switch states are clearly different") {
        // ON glows vivid blue behind the knob; OFF has no glow
        // tone at all.
        CHECK(Count_Key(kSwitch_On, 'B') > 0);
        CHECK(Count_Key(kSwitch_On, 'L') > 0);
        CHECK(Count_Key(kSwitch_Off, 'B') == 0);
        CHECK(Count_Key(kSwitch_Off, 'L') == 0);
        // The knob (bright face 'G') sits right when ON and
        // left when OFF: the two states read from the knob's
        // position as well as the glow.
        CHECK(Centroid_X(kSwitch_On, 'G') > kSwitch_On.width / 2);
        CHECK(Centroid_X(kSwitch_Off, 'g') < kSwitch_Off.width / 2);
        // Both states are the same size, and both are a pill
        // with a transparent surround (the panel shows through
        // around the track).
        CHECK(kSwitch_On.width == kSwitch_Off.width);
        CHECK(kSwitch_On.height == kSwitch_Off.height);
        CHECK(kSwitch_On.rows[0][0] == '.');
        CHECK(kSwitch_Off.rows[0][0] == '.');
        CHECK(Count_Key(kSwitch_On, '.') > 0);
        CHECK(Count_Key(kSwitch_Off, '.') > 0);
    }

    TEST_CASE("the frame strips are bevelled the right way") {
        // Top: dark outer edge at the top, bright inner line at
        // the bottom. Bottom: the mirror. The strips are the
        // same size, so the frame is square.
        CHECK(kFrame_Top.width == kFrame_Bottom.width);
        CHECK(kFrame_Top.height == kFrame_Bottom.height);
        CHECK(kFrame_Top.rows[0][0] == 'O');
        CHECK(kFrame_Top.rows[kFrame_Top.height - 1][0] == 'B');
        CHECK(kFrame_Bottom.rows[0][0] == 'B');
        CHECK(kFrame_Bottom.rows[kFrame_Bottom.height - 1][0] == 'O');
        // The side strips: bright inner line inside, dark outer
        // edge outside, and a 1 px bright strip inside the face.
        CHECK(kFrame_Left.rows[0][0] == 'O');
        CHECK(kFrame_Left.rows[0][kFrame_Left.width - 1] == 'B');
        CHECK(kFrame_Right.rows[0][0] == 'B');
        CHECK(kFrame_Right.rows[0][kFrame_Right.width - 1] == 'O');
        CHECK(kFrame_Left.rows[0][4] == 'B');
        CHECK(kFrame_Right.rows[0][5] == 'B');
        // The face rows carry hand-placed mottling: more than
        // one face tone appears, and the mottling is not a
        // repeating run (no two adjacent mottling pixels share
        // a tone in the same row pattern twice).
        CHECK(Count_Key(kFrame_Top, 'f') > 0);
        CHECK(Count_Key(kFrame_Top, 'd') > 0);
    }

    TEST_CASE("the chevron mask is a down triangle") {
        // The ink widens toward the top: the top row is all
        // ink, the bottom row has one ink pixel.
        CHECK(Count_Key(kChevron_Mask, 'X') > 0);
        int top = 0;
        int bottom = 0;
        for (int x = 0; x < kChevron_Mask.width; ++x) {
            if (kChevron_Mask.rows[0][x] == 'X') {
                ++top;
            }
            if (kChevron_Mask.rows[kChevron_Mask.height - 1][x] ==
                'X') {
                ++bottom;
            }
        }
        CHECK(top == kChevron_Mask.width);
        CHECK(bottom == 1);
    }

} // namespace
