#!/usr/bin/env python3
"""Generate the metallic button frames from scratch.

The game's metallic buttons (RETURN, the sort buttons, ACCEPT) are
the visual reference, but these frames are ORIGINAL WORK: they are
computed here from a design spec -- a border geometry and a domed
face gradient in menu-palette indices -- not extracted from the LBX
files, and not grabbed from a screenshot of a rendered button
either. Nothing in this script reads game data or game output. If
it quacks like a duck and looks like a duck by observation, that is
fine; a verbatim copy of the game's pixels would not be.

Design (observed from the game's buttons, 640x480 game pixels):
the button is a dark metallic slab, 85x26 (the RETURN button's
size, the canonical metallic button). A 2 px dark border frames it
-- the real button's top and bottom edges are dark (48, 56, 60),
not a light raised bevel -- and the face between them is a domed
metal gradient: dark at the top edge, rising to a bright plateau
in the middle, with a one-row specular highlight at the vertical
centre, then falling to dark at the bottom edge. The face reads as
a rounded metal bar, light in the middle and dark at the edges,
which is what makes it match the real button sitting next to it:
the earlier light-grey design (face 246 -> 240, light bevel) read
as washed out against the real RETURN, whose face lives in the
72-136 range.

The three states (menu-palette indices, MAINMENU.LBX entry 2's
embedded palette; the grey ramp there is 228-249):
- normal: dark border (15 top/left, 233 bottom/right, 17 inner),
  face dome 234 -> 241, specular band 245 at the centre row,
  falling to 236 at the bottom.
- pressed: the same border; the face's top rows go into shadow
  (230 -> 233 -> 234, the button pushed in) and the dome sits one
  step darker (240 plateau, 243 specular). A latched toggle stays
  in this state while it is on. The label itself lights up orange
  at runtime (palette 221, the pressed RETURN text's own orange) --
  that is the metal_button component's pressed font, not the frame.
- disabled: flat, low contrast -- a 235/233 bevel that barely
  reads, and a flat 236 face with no gradient. The label (drawn at
  runtime, in a dim colour) carries the rest of the meaning.

Colours are menu-palette indices. The grey ramp:
  230 (36,36,40)    pressed shadow top
  233 (60,60,68)    border bottom / pressed shadow
  234 (72,72,80)    face top
  235 (80,80,88)    disabled bevel / pressed face bottom
  236 (88,88,96)    face bottom / disabled face
  238 (108,108,116) face rise
  240 (124,124,132) pressed plateau
  241 (136,136,144) face plateau -- the face's dominant colour
  243 (152,152,160) pressed specular
  245 (172,172,180) specular band
  15  (48,48,44)    border -- the real button's dark top edge
  17  (56,56,48)    border inner

The face profile is a piecewise-linear dome through key points
(row, index), so the rise, plateau, specular band and fall are
each explicit. The gradient is vertical, so tiling the middle
horizontally is invisible -- one frame serves any button width.

Frames land in data/base/sprites/metal-button/<state>.bin, in the
same native bitmap layout tools/extract_sprite.py produces (12-byte
s_animation_header, one frame offset of 16, width*height
palette-index bytes, padded to a multiple of 4), so the runtime
loads them with sprite_art::Load_Sprite_Frame and draws them with
sprite_art::Draw_Sprite_Frame_Stretched. The frames carry no text:
the label is interposed at runtime, centred, by the metal_button
component.

Reproduce everything with `tools/generate_metal_buttons.py`, and
verify with the same command plus `--check`.
"""

import argparse
import os
import struct
import sys

BITMAP_DRAW_MODE = 1
BITMAP_FRAME_DATA_OFFSET = 16

WIDTH = 85
HEIGHT = 26
BORDER = 2  # px of dark border on each edge

# Menu-palette indices (see the module docstring for the RGB values).
BORDER_DARK = 15    # dark border edge
BORDER_INNER = 17   # border inner edge
BORDER_BOTTOM = 233  # dark border bottom/right edge
FACE_TOP = 234
FACE_PLATEAU = 241
FACE_SPECULAR = 245
FACE_BOTTOM = 236
PRESSED_SHADOW_TOP = 230
PRESSED_SHADOW = 233
PRESSED_PLATEAU = 240
PRESSED_SPECULAR = 243
PRESSED_BOTTOM = 235
DISABLED_BEVEL_LIGHT = 235
DISABLED_FACE = 236
DISABLED_BEVEL_DARK = 233

# Face profile: (row, palette index) key points; the face is a
# piecewise-linear dome through them. Rows are absolute (0-25);
# the face itself is rows BORDER..HEIGHT-1-BORDER.
NORMAL_PROFILE = (
    (2, 234), (6, 241), (12, 241), (13, 245), (14, 241), (19, 241),
    (23, 236),
)
PRESSED_PROFILE = (
    (2, 230), (3, 233), (4, 234), (6, 240), (12, 240), (13, 243),
    (14, 240), (19, 240), (23, 235),
)

STATES = ("normal", "pressed", "disabled")


def profile_value(profile, row):
    """Piecewise-linear interpolation through the profile points."""
    if row <= profile[0][0]:
        return profile[0][1]
    if row >= profile[-1][0]:
        return profile[-1][1]
    for (r0, v0), (r1, v1) in zip(profile, profile[1:]):
        if r0 <= row <= r1:
            if r1 == r0:
                return v1
            t = (row - r0) / (r1 - r0)
            return round(v0 + (v1 - v0) * t)
    return profile[-1][1]


def frame_pixels(state):
    """Compute the index grid for one state. No game data is read."""
    if state == "disabled":
        grid = [[DISABLED_FACE] * WIDTH for _ in range(HEIGHT)]
        for y in range(HEIGHT):
            for x in range(WIDTH):
                if y == 0:
                    grid[y][x] = DISABLED_BEVEL_LIGHT
                elif y == HEIGHT - 1:
                    grid[y][x] = DISABLED_BEVEL_DARK
                elif x == 0:
                    grid[y][x] = DISABLED_BEVEL_LIGHT
                elif x == WIDTH - 1:
                    grid[y][x] = DISABLED_BEVEL_DARK
        return grid

    profile = NORMAL_PROFILE if state == "normal" else PRESSED_PROFILE
    grid = [[0] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if y == 0 or x == 0:
                value = BORDER_DARK
            elif y == HEIGHT - 1 or x == WIDTH - 1:
                value = BORDER_BOTTOM
            elif y == 1 or x == 1:
                value = BORDER_INNER
            elif y == HEIGHT - 2 or x == WIDTH - 2:
                value = BORDER_DARK
            else:
                value = profile_value(profile, y)
            grid[y][x] = value
    return grid


def frame_bytes(frame):
    """Encode an index grid as a native orion2re bitmap frame."""
    height = len(frame)
    width = len(frame[0])
    pixel_count = width * height
    size = (pixel_count + BITMAP_FRAME_DATA_OFFSET + 3) // 4 * 4
    header = struct.pack(
        "<hhhhBBBbI", width, height, 0, 1, 0, 0, 0, BITMAP_DRAW_MODE,
        BITMAP_FRAME_DATA_OFFSET)
    pixels = bytes(value for row in frame for value in row)
    return header + pixels + bytes(size - len(header) - len(pixels))


def generate(output_dir):
    """Write the three frames. Returns {state: bytes}."""
    written = {}
    for state in STATES:
        data = frame_bytes(frame_pixels(state))
        path = os.path.join(output_dir, state + ".bin")
        with open(path, "wb") as f:
            f.write(data)
        written[state] = data
        print(f"wrote {path} ({len(data)} bytes)")
    return written


def check(output_dir):
    """Regenerate and byte-compare against the committed files."""
    failed = False
    for state in STATES:
        expected = frame_bytes(frame_pixels(state))
        path = os.path.join(output_dir, state + ".bin")
        try:
            with open(path, "rb") as f:
                actual = f.read()
        except FileNotFoundError:
            print(f"FAIL: {path} is missing")
            failed = True
            continue
        if actual != expected:
            print(f"FAIL: {path} differs from the generator output "
                  f"({len(actual)} vs {len(expected)} bytes)")
            failed = True
        else:
            print(f"ok: {path} matches")
    return 0 if not failed else 1


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate the metallic button frames from scratch.")
    parser.add_argument("--output-dir",
                        default=os.path.join("data", "base", "sprites",
                                             "metal-button"),
                        help="directory the .bin frames land in")
    parser.add_argument("--check", action="store_true",
                        help="verify the committed files match the "
                             "generator instead of writing them")
    args = parser.parse_args(argv)
    if args.check:
        return check(args.output_dir)
    os.makedirs(args.output_dir, exist_ok=True)
    generate(args.output_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
