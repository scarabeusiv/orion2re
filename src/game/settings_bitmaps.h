// Hand-drawn bitmaps for the settings screen (Tom, 2026-11-11).
//
// The settings screen renders as HOME-MADE BITMAPS, not code-drawn
// vector primitives: every element below is pixel art drawn by
// observation from the colonies screen (fullwin-colony-summary.png)
// and Tom's sample, stored as data in this file and blitted by
// settings_bitmaps.cpp -- the same pattern as the HD font
// (settings_hd_font: glyphs stored as data, drawn by code).
// The bitmaps are HAND-DRAWN: each row was placed by hand, with
// slight irregularity (a wobbled edge, a nick, a brighter pixel)
// -- never procedurally generated, never a vector pattern, never
// a mechanical repeat. A bitmap's face is flat where the colony's
// is flat (its metal faces are clean); the hand-drawn character
// lives in the edges, the bevels, the bloom falloff and the
// knob -- and in the mottled frame strips, whose variation is
// authored once per strip and stretched, so it never tiles.
//
// Format: a bitmap is rows of characters, one character per
// pixel; each character is a tone from the bitmap's tone table
// ('.' is transparent -- the panel shows through). A frame
// bitmap is a 9-slice: its border (border_x by border_y) is
// corners (blitted 1:1) and edge strips (stretched along their
// length only); its centre is a flat fill of the centre tone.
// Bitmaps are authored at 640-space size and stretch-blitted
// to the target rect with nearest neighbour, so the pixels stay
// square -- the pixel-art feel survives any game resolution.
//
// DO NOT copy the game's bitmaps here: these are home-made,
// drawn by observation, like the Moo2 fonts.
#pragma once

#include <cstdint>

#include "game/settings_blend.h"
#include "game/settings_layout.h"

namespace SETTINGS_BITMAPS {

    using s_rgb = SETTINGS_BLEND::s_rgb;
    using s_rect = SETTINGS_LAYOUT::s_rect;

    // One tone of a bitmap: the character in the rows, and the
    // colour it blits as (8-bit display space).
    struct s_tone {
        char key;
        s_rgb color;
    };

    // A hand-drawn bitmap: rows of tone characters, row-major.
    // Every row is exactly `width` characters long.
    struct s_bitmap {
        int width;
        int height;
        const char* const* rows;
        const s_tone* tones;
        int tone_count;
    };

    // A 9-slice frame bitmap: the border is `border_x` px wide
    // and `border_y` px tall; the centre (inside the border) is
    // a flat fill of `center`'s tone.
    struct s_frame_bitmap {
        s_bitmap bitmap;
        int border_x;
        int border_y;
        char center;
    };

    // -- Category tabs (32x10, stretch-blitted to the tab rect)
    //
    // The active tab's bloom: a glow emanating from the CENTER
    // of the tab, brightest in the middle, fading outward to the
    // colony's dark navy at the edges -- like the colonies
    // screen's active sort band. The falloff is hand-placed:
    // the band edges wobble a pixel between rows, so the glow
    // is not a mechanical radial gradient.
    //   N dark navy (0,0,36)   n navy (8,8,72)
    //   b blue (24,12,160)     B vivid blue (24,12,252)
    //   L light blue (96,96,255)  W bright core (176,176,255)
    inline const char* const kTab_Active_Rows[] = {
        "NNNNNNNNNNNNNNnnnnNNNNNNNNNNNNNN",
        "NNNNNNNNNNNnnnbbbbnnnNNNNNNNNNNN",
        "NNNNNNNNNNnnbbbbBBBBbbnnNNNNNNNN",
        "NNNNNNNnnbbbBBBBBBBBbbbnNNNNNNNN",
        "NNNNNNnbbbBBBBLLLLBBBBbbnNNNNNNN",
        "NNNNNnnbbBBBLLLLWWLLLBBBbbnnNNNN",
        "NNNNNNnnbbBBBBLLLLLBBBbbnNNNNNNN",
        "NNNNNNNNnbbbbBBBBBBBbbbnnNNNNNNN",
        "NNNNNNNNNNnbbbbBBBBBbbnNNNNNNNNN",
        "NNNNNNNNNNNNNnnnnnNNNNNNNNNNNNNN",
    };
    inline constexpr s_tone kTab_Active_Tones[] = {
        {'N', s_rgb{0, 0, 36}},
        {'n', s_rgb{8, 8, 72}},
        {'b', s_rgb{24, 12, 160}},
        {'B', s_rgb{24, 12, 252}},
        {'L', s_rgb{96, 96, 255}},
        {'W', s_rgb{176, 176, 255}},
    };
    inline constexpr s_bitmap kTab_Active{32, 10, kTab_Active_Rows,
        kTab_Active_Tones, 6};

    // The inactive tab: the colony SORT band's flat dark navy,
    // with a faint sheen patch at the top (the light catching
    // the band) and a few darker nicks in the face -- the hand
    // touch on an otherwise flat face.
    //   N dark navy (0,0,36)   n navy (16,16,52)
    //   d darker nick (0,0,20) s sheen (28,28,72)
    inline const char* const kTab_Inactive_Rows[] = {
        "NNNNNNNNNNssssssNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNdNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
        "NNNNNNNdNNNNNNNNNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNdNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNnnnnNNNNNNNNNNNNNNNN",
        "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
    };
    inline constexpr s_tone kTab_Inactive_Tones[] = {
        {'N', s_rgb{0, 0, 36}},
        {'n', s_rgb{16, 16, 52}},
        {'d', s_rgb{0, 0, 20}},
        {'s', s_rgb{28, 28, 72}},
    };
    inline constexpr s_bitmap kTab_Inactive{32, 10, kTab_Inactive_Rows,
        kTab_Inactive_Tones, 4};

    // -- The flickable switch (40x14, stretch-blitted)
    //
    // A track + knob, approximated from the colonies screen's
    // low-res buying switch: the small round recessed button of
    // the BUILDING column (a dark disc, a light ring, a
    // top-left highlight). The track is a dark recessed pill;
    // the knob is that round button, slid to one end. ON: the
    // knob sits right and the track glows vivid blue behind it.
    // OFF: the knob sits left, the track stays dark, the knob
    // is dim grey. The knob's position AND the track's glow
    // both read the state at a glance.
    //   . transparent  K outline (8,8,8)   D track dark (16,16,32)
    //   B glow vivid (24,12,252)  L glow light (96,96,255)
    //   G knob face (112,112,112) g knob dark (72,72,72)
    //   W knob highlight (200,200,200)  k off-knob shade (48,48,48)
    inline const char* const kSwitch_On_Rows[] = {
        "........................................",
        "........................................",
        ".....KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.....",
        "...KKDDDDDDDDDDDDDDDLLBBBBBBBKGGGKBBK...",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBBKGGGGGKBBK..",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBBKWGGGGKBBK..",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBKGGGGGGGKBK..",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBBKGGGGGKBBK..",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBBKGGGggKBBK..",
        "..KDDDDDDDDDDDDDDDDDLLBBBBBBBKGGgKBBBK..",
        "...KKDDDDDDDDDDDDDDDLLBBBBBBBBBKBBBBK...",
        ".....KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.....",
        "........................................",
        "........................................",
    };
    inline const char* const kSwitch_Off_Rows[] = {
        "........................................",
        "........................................",
        ".....KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.....",
        "...KKDDKgggKDDDDDDDDDDDDDDDDDDDDDDDDK...",
        "..KDDDKgggggKDDDDDDDDDDDDDDDDDDDDDDDDK..",
        "..KDDDKWggggKDDDDDDDDDDDDDDDDDDDDDDDDK..",
        "..KDDKgggggggKDDDDDDDDDDDDDDDDDDDDDDDK..",
        "..KDDDKgggggKDDDDDDDDDDDDDDDDDDDDDDDDK..",
        "..KDDDKggggkKDDDDDDDDDDDDDDDDDDDDDDDDK..",
        "..KDDDDKggkKDDDDDDDDDDDDDDDDDDDDDDDDDK..",
        "...KKDDDDKDDDDDDDDDDDDDDDDDDDDDDDDDDK...",
        ".....KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.....",
        "........................................",
        "........................................",
    };
    inline constexpr s_tone kSwitch_Tones[] = {
        {'K', s_rgb{8, 8, 8}},
        {'D', s_rgb{16, 16, 32}},
        {'B', s_rgb{24, 12, 252}},
        {'L', s_rgb{96, 96, 255}},
        {'G', s_rgb{112, 112, 112}},
        {'g', s_rgb{72, 72, 72}},
        {'W', s_rgb{200, 200, 200}},
        {'k', s_rgb{48, 48, 48}},
    };
    inline constexpr s_bitmap kSwitch_On{40, 14, kSwitch_On_Rows,
        kSwitch_Tones, 8};
    inline constexpr s_bitmap kSwitch_Off{40, 14, kSwitch_Off_Rows,
        kSwitch_Tones, 8};

    // -- Scrollbar arrow buttons (13x26, stretch-blitted)
    //
    // The colony's arrows: a dark-navy button face with a
    // light-grey chunky triangle, like the colony's own up/down
    // arrows. The triangle is hand-stepped (its edges wobble a
    // pixel), the face carries two darker nicks.
    //   D face (20,20,56)  d nick (28,28,72)  T triangle (180,180,180)
    inline const char* const kArrow_Up_Rows[] = {
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDdDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDTDDDDDD",
        "DDDDDTTTDDDDD",
        "DDDDDTTTDDDDD",
        "DDDDTTTTTDDDD",
        "DDDDTTTTTDDDD",
        "DDDTTTTTTTDDD",
        "DDDTTTTTTTDDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDdDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
    };
    inline const char* const kArrow_Down_Rows[] = {
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDTTTTTTTTTDD",
        "DDDTTTTTTTDDD",
        "DDDTTTTTTTDDD",
        "DDDDTTTTTDDDD",
        "DDDDTTTTTDDDD",
        "DDDDDTTTDDDDD",
        "DDDDDTTTDDDDD",
        "DDDDDDTDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDdDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
        "DDDDDDDDDDDDD",
    };
    inline constexpr s_tone kArrow_Tones[] = {
        {'D', s_rgb{20, 20, 56}},
        {'d', s_rgb{28, 28, 72}},
        {'T', s_rgb{180, 180, 180}},
    };
    inline constexpr s_bitmap kArrow_Up{13, 26, kArrow_Up_Rows,
        kArrow_Tones, 3};
    inline constexpr s_bitmap kArrow_Down{13, 26, kArrow_Down_Rows,
        kArrow_Tones, 3};

    // -- Dropdown chevron (9x9 mask, stretch-blitted)
    //
    // A hand-stepped down triangle. A mask: 'X' is ink, drawn
    // in the colour the caller passes (white while the dropdown
    // is closed, the open blue while its popup is up -- the
    // colour shift is the state signal).
    inline const char* const kChevron_Rows[] = {
        "XXXXXXXXX",
        ".XXXXXXXX",
        ".XXXXXXX.",
        "..XXXXXX.",
        "..XXXXX..",
        "...XXXX..",
        "...XXX...",
        "....XX...",
        "....X....",
    };
    inline constexpr s_bitmap kChevron_Mask{9, 9, kChevron_Rows,
        nullptr, 0};

    // -- Sunken value box (22x12, 9-slice, border 3)
    //
    // The recessed slot numbers and dropdowns sit in: a dark
    // outer border with the shadow on the top and left inside
    // it and the lit lip on the bottom and right -- the reverse
    // of a raised frame's lip, so the slot reads as cut into
    // the surface. The top edge carries one lighter nick in
    // its dark, the lip one dimmer nick, the left edge one
    // lighter nick: the hand touch on a clean slot. Corners
    // are cut one pixel (the sample's barely-round boxes).
    //   . transparent  K frame dark (36,36,40)
    //   k nick light (52,52,56)  L lit lip (96,96,104)
    //   l lip nick (80,80,88)    F control face (24,30,58)
    inline const char* const kSunken_Box_Rows[] = {
        ".KKKKKKKKKKKKKKKKKKKK.",
        "KKKKKKKKKKkKKKKKKKKKKK",
        "KKKKKKKKKKKKKKKKKKKKKK",
        "KKKFFFFFFFFFFFFFFFFLKK",
        "KKKFFFFFFFFFFFFFFFFLKK",
        "KKkFFFFFFFFFFFFFFFFLKK",
        "KKKFFFFFFFFFFFFFFFFLKK",
        "KKKFFFFFFFFFFFFFFFFLKK",
        "KKKFFFFFFFFFFFFFFFFLKK",
        "KLLLLLLLLLLLlLLLLLLLLK",
        "KKKKKKKKKKKKKKKKKKKKKK",
        ".KKKKKKKKKKKKKKKKKKKK.",
    };
    inline constexpr s_tone kSunken_Box_Tones[] = {
        {'K', s_rgb{36, 36, 40}},
        {'k', s_rgb{52, 52, 56}},
        {'L', s_rgb{96, 96, 104}},
        {'l', s_rgb{80, 80, 88}},
        {'F', s_rgb{24, 30, 58}},
    };
    inline constexpr s_frame_bitmap kSunken_Box{
        s_bitmap{22, 12, kSunken_Box_Rows, kSunken_Box_Tones, 5},
        3, 3, 'F'};

    // -- Header plate (22x10, 9-slice, border 3)
    //
    // The colony's bevelled grey plate (SETTING / VALUE): a
    // clean grey face, a dark-then-bright top bevel, dark
    // bottom and sides. One face pixel is a touch lighter --
    // the light catching the plate.
    //   . transparent  X top outer (76,76,76)
    //   Y top inner (88,88,88)  D dark (44,44,44)
    //   F face (60,60,60)  f face light (68,68,68)
    inline const char* const kPlate_Rows[] = {
        ".XXXXXXXXXXXXXXXXXXXX.",
        "XYYYYYYYYYYYYYYYYYYYYX",
        "DFFFFFFFFFFFFFFFFFFFFD",
        "DFFFFFFFFFFFFFFFFFFFFD",
        "DFFFFFFFFFFFFFFFFFFFFD",
        "DFFFFFFFFFfFFFFFFFFFFD",
        "DFFFFFFFFFFFFFFFFFFFFD",
        "DFFFFFFFFFFFFFFFFFFFFD",
        "DFFFFFFFFFFFFFFFFFFFFD",
        ".DDDDDDDDDDDDDDDDDDDD.",
    };
    inline constexpr s_tone kPlate_Tones[] = {
        {'X', s_rgb{76, 76, 76}},
        {'Y', s_rgb{88, 88, 88}},
        {'D', s_rgb{44, 44, 44}},
        {'F', s_rgb{60, 60, 60}},
        {'f', s_rgb{68, 68, 68}},
    };
    inline constexpr s_frame_bitmap kPlate{
        s_bitmap{22, 10, kPlate_Rows, kPlate_Tones, 5}, 3, 3, 'F'};

    // -- Grey button (22x12, 9-slice, border 3)
    //
    // MORE / RETURN and the popup frame: the colony's RETURN
    // button / the main menu's buttons -- a grey bevelled metal
    // button. The dark outer ring, the light inner line, a flat
    // grey face; the top light line carries a couple of brighter
    // pixels (the sheen, hand-placed), the face one lighter nick.
    //   . transparent  K dark ring (36,36,40)
    //   L light line (96,96,104)  W sheen (160,160,160)
    //   F face (112,112,112)  f face nick (124,124,124)
    inline const char* const kGrey_Button_Rows[] = {
        ".KKKKKKKKKKKKKKKKKKKK.",
        "KKLKKKKKKKKKKKKKKKKLKK",
        "KLFLLLLLLWLLLLLLLLLFLK",
        "KKLFFFFFFFFFFFFFFFFLKK",
        "KKLFFFFFFFFFFFFFFFFLKK",
        "KKLFFFFFfFFFFFFFFFFLKK",
        "KKLFFFFFFFFFFFFFFFFLKK",
        "KKLFFFFFFFFFFFFFFFFLKK",
        "KKLFFFFFFFFFFFFFFFFLKK",
        "FLKLLLLLLLLLLLLLLLLKLF",
        "LKKKKKKKKKKKKKKKKKKKKL",
        ".KKKKKKKKKKKKKKKKKKKK.",
    };
    inline constexpr s_tone kGrey_Button_Tones[] = {
        {'K', s_rgb{36, 36, 40}},
        {'L', s_rgb{96, 96, 104}},
        {'W', s_rgb{160, 160, 160}},
        {'F', s_rgb{112, 112, 112}},
        {'f', s_rgb{124, 124, 124}},
    };
    inline constexpr s_frame_bitmap kGrey_Button{
        s_bitmap{22, 12, kGrey_Button_Rows, kGrey_Button_Tones, 5},
        3, 3, 'F'};

    // -- Scrollbar thumb (12x12, 9-slice, border 2)
    //
    // The colony's indicator bar: a bevelled handle, narrower
    // than the strip, light top/left and dark right/bottom.
    //   L light edge (180,182,190)  F fill (140,142,150)
    //   D dark edge (90,92,100)
    inline const char* const kThumb_Rows[] = {
        "LLLLLLLLLLLL",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "LFFFFFFFFFFD",
        "DDDDDDDDDDDD",
    };
    inline constexpr s_tone kThumb_Tones[] = {
        {'L', s_rgb{180, 182, 190}},
        {'F', s_rgb{140, 142, 150}},
        {'D', s_rgb{90, 92, 100}},
    };
    inline constexpr s_frame_bitmap kThumb{
        s_bitmap{12, 12, kThumb_Rows, kThumb_Tones, 3}, 2, 2, 'F'};

    // -- Outer frame strips (64x10 horizontal, 10x64 vertical)
    //
    // The thick mechanical frame: a dark outer edge, a mid-grey
    // face, a dark groove, a bright inner line. The face rows
    // carry hand-placed mottling -- a few pixels a touch
    // lighter or darker, scattered irregularly, never in a
    // repeating run -- so the long bands read as hand-finished
    // metal, not a flat vector fill. A strip is authored once
    // and STRETCHED to the band, never tiled: its variation has
    // no repeat period. The side strips also carry the 1 px
    // bright strip inside the face (the light catching it).
    //   O outer (28,28,28)  F face (72,72,72)
    //   f face light (80,80,80)  d face dark (64,64,64)
    //   G groove (44,44,44)  B bright inner line (115,115,115)
    inline const char* const kFrame_Top_Rows[] = {
        "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
        "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        "FFFFFfFFFFFFFFFFdFFFFFFFFFFFFFfFFFFFFFFFFFFFdFFFFFFFFFFFfFFFFFFF",
        "FFFFFFFFFFdFFFFFFFFFFFFFFFFfFFFFFFFFFFFFFFfFFFFFFFFFFFFFdFFFFFFF",
        "FFFFFFFFFFFFFFFFFFFFfFFFFFFFFFFFFFFdFFFFFFFFFFFFFFFFFFFfFFFFFFFF",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
    };
    inline const char* const kFrame_Bottom_Rows[] = {
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        "FFFFFfFFFFFFFFFFdFFFFFFFFFFFFFfFFFFFFFFFFFFFdFFFFFFFFFFFfFFFFFFF",
        "FFFFFFFFFFdFFFFFFFFFFFFFFFFfFFFFFFFFFFFFFFfFFFFFFFFFFFFFdFFFFFFF",
        "FFFFFFFFFFFFFFFFFFFFfFFFFFFFFFFFFFFdFFFFFFFFFFFFFFFFFFFfFFFFFFFF",
        "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
        "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
    };
    inline const char* const kFrame_Left_Rows[] = {
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFdBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
        "OOFfBFGGGB",
    };
    inline const char* const kFrame_Right_Rows[] = {
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBdFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
        "BGGGFBfFOO",
    };
    inline constexpr s_tone kFrame_Tones[] = {
        {'O', s_rgb{28, 28, 28}},
        {'F', s_rgb{72, 72, 72}},
        {'f', s_rgb{80, 80, 80}},
        {'d', s_rgb{64, 64, 64}},
        {'G', s_rgb{44, 44, 44}},
        {'B', s_rgb{115, 115, 115}},
    };
    inline constexpr s_bitmap kFrame_Top{64, 10, kFrame_Top_Rows,
        kFrame_Tones, 6};
    inline constexpr s_bitmap kFrame_Bottom{64, 10, kFrame_Bottom_Rows,
        kFrame_Tones, 6};
    inline constexpr s_bitmap kFrame_Left{10, 64, kFrame_Left_Rows,
        kFrame_Tones, 6};
    inline constexpr s_bitmap kFrame_Right{10, 64, kFrame_Right_Rows,
        kFrame_Tones, 6};

    // -- Rivet (7x7, blitted 1:1)
    //
    // A screw head on the outer frame: a dark rim, a mid-grey
    // face, a light top-left highlight.
    //   K rim (32,32,32)  F face (96,96,96)  W highlight (160,160,160)
    inline const char* const kRivet_Rows[] = {
        "..KKK..",
        ".KFFFK.",
        "KFWFFFK",
        "KFFFFFK",
        "KFFFFFK",
        ".KFFFK.",
        "..KKK..",
    };
    inline constexpr s_tone kRivet_Tones[] = {
        {'K', s_rgb{32, 32, 32}},
        {'F', s_rgb{96, 96, 96}},
        {'W', s_rgb{160, 160, 160}},
    };
    inline constexpr s_bitmap kRivet{7, 7, kRivet_Rows, kRivet_Tones, 3};

    // -- Drawing -------------------------------------------------
    //
    // All draws are nearest-neighbour: a source pixel becomes a
    // block of screen pixels, so the bitmaps keep their square
    // pixel-art feel at any game resolution. `face_shift`
    // brightens (positive) or darkens (negative) the face tone
    // only -- hover (+24) and pressed (-24) on the grey button
    // -- the hand-drawn pixels stay, only their light shifts.

    // A tab: the bloom bitmap when active, the flat navy bitmap
    // when inactive. `corners` selects which corners are
    // rounded (bit 0 top-left, 1 top-right, 2 bottom-right,
    // 3 bottom-left): only the band's outer corners round, the
    // shared edges between tabs stay square so the band reads
    // as one piece. `radius` is the corner radius in screen
    // pixels (the caller's scaled button radius). `glow_shift`
    // darkens (pressed) or brightens the active tab's glow
    // tones only (0 for inactive tabs and unpressed tabs).
    void Draw_Tab(const s_rect& rect, bool active, uint8_t corners,
        int16_t radius, int glow_shift = 0);

    // The flickable switch, centred in `rect` at its own size
    // (the bitmap's aspect, capped by the rect): ON or OFF.
    void Draw_Switch(const s_rect& rect, bool on);

    // The sunken value box / header plate / grey button /
    // scrollbar thumb as 9-slice bitmaps. The popup frame is
    // the grey button's shape with the panel fill as its face:
    // pass `center_override` for a face other than the
    // bitmap's own centre tone. The edit field is the sunken
    // box with its own edit fill as the face.
    void Draw_Sunken_Box(const s_rect& rect,
        const s_rgb* center_override = nullptr);
    void Draw_Plate(const s_rect& rect);
    void Draw_Grey_Button(const s_rect& rect, int face_shift);
    void Draw_Thumb(const s_rect& rect);
    void Draw_Frame(const s_rect& rect, const s_rgb& center);
    void Draw_Frame(const s_rect& rect, const s_bitmap& bitmap,
        int border_x, int border_y, const s_rgb& center);

    // A scrollbar arrow button (face + triangle in one bitmap)
    // and the dropdown chevron (a mask, inked in `color`).
    // `face_shift` brightens (hover) or darkens (pressed) the
    // arrow's face tone only.
    void Draw_Arrow(const s_rect& rect, bool up, int face_shift = 0);
    void Draw_Chevron(int16_t cx, int16_t y, int16_t w, int16_t h,
        const s_rgb& color);

    // The thick mechanical outer frame: the four mottled strips
    // (stretched, never tiled) with bitmap rivets at the
    // corners and evenly spaced along the edges. `content` is
    // the content rect; the bands run from the screen edge to it.
    void Draw_Outer_Frame(const s_rect& content, int16_t screen_width,
        int16_t screen_height);

} // namespace SETTINGS_BITMAPS
