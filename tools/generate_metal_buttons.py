#!/usr/bin/env python3
"""Generate the metallic button frames from scratch.

The game's metallic buttons (RETURN, the sort buttons, ACCEPT) are
the visual reference, but these frames are ORIGINAL WORK: they are
computed here from a design spec -- a bevel geometry and a metallic
gradient in menu-palette indices -- not extracted from the LBX files.
Nothing in this script reads game data. If it quacks like a duck
and looks like a duck by observation, that is fine; a verbatim copy
of the LBX pixels would not be.

Design (observed from the game's buttons, 640x480 game pixels):
the button is a raised metallic slab, 85x26 (the RETURN button's
size, the canonical metallic button). A 2 px bevel frames it: a
light outer edge on the top/left and a dark outer edge on the
bottom/right, with an inner edge one step darker/lighter, and a
vertical metallic gradient between them -- lighter at the top,
darker at the bottom, so the face reads as metal, not flat.

The three states:
- normal: raised bevel (light top/left, dark bottom/right), gradient
  246 -> 240 down the face.
- pressed: the bevel inverts (dark top/left, light bottom/right)
  and the face darkens (242 -> 238) -- the button reads as pushed in.
  A latched toggle stays in this state while it is on.
- disabled: flat, low contrast -- a 235/233 bevel that barely
  reads, and a flat 236 face with no gradient. The label (drawn at
  runtime, in a dim colour) carries the rest of the meaning.

Colours are menu-palette indices (MAINMENU.LBX entry 2's embedded
palette, the palette the settings screen renders in). The grey ramp
there is 228-249, a blue-grey ramp from (16,16,24) to (204,204,212):
  232 (52,52,60)   shadow -- the dark bevel edge
  234 (72,72,80)   inner shade
  236 (88,88,96)   disabled face
  238 (108,108,116) pressed face bottom
  240 (124,124,132) normal face bottom
  242 (144,144,152) the metal face the settings screen already uses
  244 (164,164,172) inner highlight
  246 (180,180,188) normal face top
  247 (188,188,196) highlight -- the light bevel edge

The gradient is a linear interpolation through the ramp indices,
row by row; consecutive rows often share an index, which is how a
24-row gradient reads smooth in a 256-colour palette.

Frames land in data/base/sprites/metal-button/<state>.bin, in the
same native bitmap layout tools/extract_sprite.py produces (12-byte
s_animation_header, one frame offset of 16, width*height
palette-index bytes, padded to a multiple of 4), so the runtime
loads them with sprite_art::Load_Sprite_Frame and draws them with
sprite_art::Draw_Sprite_Frame_Stretched (the gradient is vertical,
so tiling the middle horizontally is invisible -- one frame serves
any button width). The frames carry no text: the label is interposed
at runtime, centred, by the metal_button component.

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
BEVEL = 2  # px of bevel on each edge

# Menu-palette indices (see the module docstring for the RGB values).
HIGHLIGHT = 247  # light bevel edge
INNER_HIGHLIGHT = 244
FACE_TOP = 246
FACE = 242
FACE_BOTTOM = 240
INNER_SHADE = 234
SHADOW = 232  # dark bevel edge
PRESSED_FACE_TOP = 242
PRESSED_FACE_BOTTOM = 238
DISABLED_BEVEL_LIGHT = 235
DISABLED_FACE = 236
DISABLED_BEVEL_DARK = 233

STATES = ("normal", "pressed", "disabled")


def gradient_row(top_index, bottom_index, row, first_row, last_row):
    """Linear interpolation through the ramp indices for one row."""
    if last_row <= first_row:
        return top_index
    t = (row - first_row) / (last_row - first_row)
    return round(top_index + (bottom_index - top_index) * t)


def frame_pixels(state):
    """Compute the index grid for one state. No game data is read."""
    if state == "normal":
        outer_top = outer_left = HIGHLIGHT
        outer_bottom = outer_right = SHADOW
        inner_top = inner_left = INNER_HIGHLIGHT
        inner_bottom = inner_right = INNER_SHADE
        face_top, face_bottom = FACE_TOP, FACE_BOTTOM
    elif state == "pressed":
        outer_top = outer_left = SHADOW
        outer_bottom = outer_right = HIGHLIGHT
        inner_top = inner_left = INNER_SHADE
        inner_bottom = inner_right = INNER_HIGHLIGHT
        face_top, face_bottom = PRESSED_FACE_TOP, PRESSED_FACE_BOTTOM
    elif state == "disabled":
        outer_top = outer_left = DISABLED_BEVEL_LIGHT
        outer_bottom = outer_right = DISABLED_BEVEL_DARK
        inner_top = inner_left = DISABLED_FACE
        inner_bottom = inner_right = DISABLED_FACE
        face_top = face_bottom = DISABLED_FACE
    else:
        raise ValueError(f"unknown state {state!r}")

    grid = [[0] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if y == 0:
                value = outer_top
            elif y == HEIGHT - 1:
                value = outer_bottom
            elif x == 0:
                value = outer_left
            elif x == WIDTH - 1:
                value = outer_right
            elif y == 1:
                value = inner_top
            elif y == HEIGHT - 2:
                value = inner_bottom
            elif x == 1:
                value = inner_left
            elif x == WIDTH - 2:
                value = inner_right
            else:
                # Interior: vertical metallic gradient. The disabled
                # face is flat (face_top == face_bottom), so this is
                # a no-op there.
                value = gradient_row(face_top, face_bottom, y, BEVEL,
                                     HEIGHT - 1 - BEVEL)
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
