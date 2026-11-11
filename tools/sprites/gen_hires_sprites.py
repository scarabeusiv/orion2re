#!/usr/bin/env python3
"""Generate the hi-res PNG sprites for the settings screen's tie-together.

The settings screen renders in hi-res with bitmap sprites, not scalable
vector vertices (Tom, 2026-11-11): the elements are hi-res PNGs, like the
original game's sprites but hi-res, with borders that integrate to each
other into one continuous frame, close to how the rest of the interfaces
are done. The blue switches carry a bloom (well-used analog display
buttons); a pressed knob glows orange; the buttons are rustical, not
flat Star Trek / enterprise touch-stream design.

The PNGs are the source of truth, committed under data/base/sprites/hires/
(they ship in base.zip, which globs data/base/*). The game decodes them at
run time with the vendored stb_image loader (settings_png) and blits them
through the blend path (real alpha blend, so the background shows through
the translucent faces).

Authored at 2x the 640x480 base: 1 sprite px is 0.5 game px at 640x480.
At draw time the sprites scale by nearest neighbour to the target rect,
so the bitmap feel survives at any resolution (no blur).

Colour discipline: the sprites use a LIMITED colour set and quantized
alpha levels, so the runtime blend-table cache (one table per
(rgb, alpha)) stays bounded. Dither/speckle uses a hash-based speckle
(the pixel-era point 12: uniform 0..15, no repeat distance) over a few
discrete shades, never a smooth gradient with hundreds of colours.
The bloom/glow halos use a few alpha steps (0, 48, 96, 160) for a soft
falloff.
"""

import hashlib
import os
import random
import sys

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "..", "data", "base", "sprites", "hires")

# --- palette (8-bit display space, matching settings_render.h) ---
PANEL_FACE = (16, 20, 38)
PANEL_FACE_LIGHT = (22, 27, 48)
PANEL_FACE_DARK = (11, 14, 28)
CONTROL_FACE = (24, 30, 58)
CONTROL_FACE_LIGHT = (32, 40, 74)
CONTROL_FACE_DARK = (17, 21, 42)
BUTTON_HOVER_FACE = (34, 42, 78)
BUTTON_PRESSED_FACE = (14, 18, 36)
ACTIVE_BLUE = (24, 12, 252)
ARROW_BLUE = (52, 48, 136)
ARROW_BLUE_HOVER = (72, 68, 170)
ARROW_BLUE_PRESSED = (36, 32, 96)
STEEL_LIGHT = (180, 182, 190)
STEEL_MID = (140, 142, 150)
STEEL_DARK = (90, 92, 100)
STEEL_DARKEST = (60, 60, 68)
WHITE = (225, 228, 235)
GOLD = (235, 200, 90)
GOLD_DARK = (180, 150, 60)
GREY_STAR = (96, 100, 110)
ORANGE = (252, 136, 0)
ORANGE_DEEP = (200, 90, 0)

# Alpha levels (quantized, so the runtime blend-table cache stays small).
A_FACE = 150          # panel face
A_CONTROL = 170       # value-box / control face
A_BUTTON = 200        # button face
A_SOLID = 255         # opaque ink (text-facing pixels, star, triangle)


def speckle(x, y, seed=0):
    """Hash-based speckle, uniform 0..15, no repeat distance (pixel-era 12)."""
    h = hashlib.md5(f"{seed}:{x}:{y}".encode()).digest()
    return h[0] & 0x0F


def dither_face(img, box, base, light, dark, alpha, seed=0):
    """Fill box with the base colour, speckled with light/dark at ~13%.

    The speckle density matches the colony's borders (~13%): a pixel
    is light when speckle < 1, dark when speckle > 13, else base.
    """
    px = img.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            s = speckle(x, y, seed)
            if s < 1:
                c = light
            elif s > 13:
                c = dark
            else:
                c = base
            px[x, y] = (c[0], c[1], c[2], alpha)


def frame_border(img, box, thickness, alpha=A_SOLID):
    """Industrial frame: light outer border, dark inner line, speckled.

    The border is `thickness` px wide. The outer 1px is the light
    steel edge, the next 1px is the dark line, the rest of the border
    is the speckled steel face. Corners are square (the frame is a
    machine panel, not a rounded web button).
    """
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    # Outer light edge (1px).
    d.rectangle([x0, y0, x1 - 1, y0], fill=STEEL_LIGHT + (alpha,))
    d.rectangle([x0, y1 - 1, x1 - 1, y1 - 1], fill=STEEL_LIGHT + (alpha,))
    d.rectangle([x0, y0, x0, y1 - 1], fill=STEEL_LIGHT + (alpha,))
    d.rectangle([x1 - 1, y0, x1 - 1, y1 - 1], fill=STEEL_LIGHT + (alpha,))
    # Dark inner line (1px inside the light edge).
    if thickness >= 2:
        d.rectangle([x0 + 1, y0 + 1, x1 - 2, y0 + 1],
                    fill=STEEL_DARKEST + (alpha,))
        d.rectangle([x0 + 1, y1 - 2, x1 - 2, y1 - 2],
                    fill=STEEL_DARKEST + (alpha,))
        d.rectangle([x0 + 1, y0 + 1, x0 + 1, y1 - 2],
                    fill=STEEL_DARKEST + (alpha,))
        d.rectangle([x1 - 2, y0 + 1, x1 - 2, y1 - 2],
                    fill=STEEL_DARKEST + (alpha,))
    # Speckle the remaining border band (between the dark line and
    # the face) so the frame reads as worn metal, not flat vector.
    if thickness >= 3:
        dither_face(img, (x0 + 2, y0 + 2, x1 - 2, y0 + thickness - 1),
                    STEEL_MID, STEEL_LIGHT, STEEL_DARK, alpha, seed=1)
        dither_face(img, (x0 + 2, y1 - thickness + 1, x1 - 2, y1 - 2),
                    STEEL_MID, STEEL_LIGHT, STEEL_DARK, alpha, seed=2)
        dither_face(img, (x0 + 2, y0 + 2, x0 + thickness - 1, y1 - 2),
                    STEEL_MID, STEEL_LIGHT, STEEL_DARK, alpha, seed=3)
        dither_face(img, (x1 - thickness + 1, y0 + 2, x1 - 2, y1 - 2),
                    STEEL_MID, STEEL_LIGHT, STEEL_DARK, alpha, seed=4)


def halo(img, cx, cy, radius, colour, peak_alpha):
    """Soft radial halo (bloom) around (cx, cy): a few alpha steps.

    The halo is the analog glow: a well-used display button blooms
    softly, like an old CRT / lit machine button, not a crisp
    digital edge. Alpha steps are quantized (0, 48, 96, 160, peak)
    so the runtime blend tables stay bounded.
    """
    px = img.load()
    w, h = img.size
    steps = [(1.0, peak_alpha), (0.75, 160), (0.5, 96), (0.25, 48)]
    for y in range(max(0, cy - radius), min(h, cy + radius + 1)):
        for x in range(max(0, cx - radius), min(w, cx + radius + 1)):
            dist = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 / radius
            if dist >= 1.0:
                continue
            for frac, a in steps:
                if dist <= frac:
                    break
            old = px[x, y]
            # src-over: the halo adds its colour over what's there.
            na = min(255, old[3] + a)
            px[x, y] = (colour[0], colour[1], colour[2], na)


def nine_slice_base(size, border):
    """A transparent canvas with the frame border; the face is filled
    by the caller. `border` is the frame thickness in sprite px."""
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    frame_border(img, (0, 0, size[0], size[1]), border)
    return img


def button_sprite(face, face_light, face_dark, alpha, size=(128, 64), border=8):
    """A 9-slice button face tile with a dithered metal face and an
    industrial frame. The glow/bloom halo is a SEPARATE sprite
    (glow-orange / bloom-blue), drawn behind the button, so the
    9-slice stays a clean rectangle: no halo pad inside the tile."""
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    # Face (inside the frame).
    dither_face(img, (border, border, size[0] - border, size[1] - border),
                face, face_light, face_dark, alpha, seed=7)
    frame_border(img, (0, 0, size[0], size[1]), border)
    return img


def glow_sprite(colour, peak_alpha, size=64):
    """A soft radial halo (bloom / glow), drawn behind a button.

    The halo is the analog glow: a well-used display button blooms
    softly, like an old CRT / lit machine button, not a crisp
    digital edge. Alpha steps are quantized so the runtime blend
    tables stay bounded."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    halo(img, size // 2, size // 2, size // 2, colour, peak_alpha)
    return img


def star_sprite(filled, size=28):
    """A five-point star: filled gold when on, grey outline when off.
    The star is a toggle, so it reads at a glance; the filled star
    carries a speckle so it is a worn metal star, not a flat glyph."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = size / 2
    r_out = size / 2 - 1
    r_in = r_out * 0.45
    import math
    pts = []
    for i in range(10):
        r = r_out if i % 2 == 0 else r_in
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    if filled:
        d.polygon(pts, fill=GOLD + (A_SOLID,))
        # Speckle the gold so it reads as worn metal.
        px = img.load()
        for y in range(size):
            for x in range(size):
                if px[x, y][3] == A_SOLID and speckle(x, y, 11) > 13:
                    px[x, y] = GOLD_DARK + (A_SOLID,)
    else:
        d.polygon(pts, outline=GREY_STAR + (A_SOLID,), width=2)
    return img


def triangle_sprite(up, colour, size=32):
    """A bold triangle (scrollbar arrow / chevron), white on transparent."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if up:
        pts = [(size / 2, 2), (size - 3, size - 3), (3, size - 3)]
    else:
        pts = [(3, 3), (size - 3, 3), (size / 2, size - 3)]
    d.polygon(pts, fill=colour + (A_SOLID,))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    written = []

    def save(img, name):
        path = os.path.join(OUT, name)
        img.save(path)
        written.append(name)

    # Panel face tile (tiled across panel interiors): the dithered
    # dark face, so a panel reads as a textured bitmap, not a flat
    # vector fill.
    face = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    dither_face(face, (0, 0, 32, 32), PANEL_FACE, PANEL_FACE_LIGHT,
                PANEL_FACE_DARK, A_FACE, seed=5)
    save(face, "panel-face.png")

    # Control face tile (value boxes, hardened displays).
    cface = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    dither_face(cface, (0, 0, 32, 32), CONTROL_FACE, CONTROL_FACE_LIGHT,
                CONTROL_FACE_DARK, A_CONTROL, seed=6)
    save(cface, "control-face.png")

    # 9-slice frame tile (the integrated frame: corners/edges with
    # the industrial border). 48x48, border 8px (4px at 640x480,
    # scaling to the frame thickness at any resolution). The centre
    # is TRANSPARENT: the 9-slice draws only the border, and the
    # caller tiles the appropriate face (panel or control) under
    # it, so a value box keeps its own face instead of the
    # panel's.
    frame = nine_slice_base((48, 48), 8)
    save(frame, "frame.png")

    # Buttons (9-slice, 128x64). The pressed and active states get
    # their glow from the separate halo sprites, drawn behind.
    save(button_sprite(CONTROL_FACE, CONTROL_FACE_LIGHT, CONTROL_FACE_DARK,
                       A_BUTTON), "button-normal.png")
    save(button_sprite(BUTTON_HOVER_FACE, (44, 54, 96), (26, 32, 60),
                       A_BUTTON), "button-hover.png")
    # Pressed: darker face; the ORANGE GLOW of a depressed knob is
    # the glow-orange halo behind it.
    save(button_sprite(BUTTON_PRESSED_FACE, (20, 25, 48), (9, 11, 24),
                       A_BUTTON), "button-pressed.png")
    # Active tab: vivid blue face; the analog BLOOM of a well-used
    # display button is the bloom-blue halo behind it.
    save(button_sprite(ACTIVE_BLUE, (60, 48, 255), (12, 6, 180),
                       A_BUTTON), "button-active.png")

    # Halos (drawn behind a button): the orange glow of a depressed
    # knob, and the blue bloom of a well-used analog display button.
    save(glow_sprite(ORANGE, 180), "glow-orange.png")
    save(glow_sprite((80, 70, 255), 160), "bloom-blue.png")

    # Stars (toggles): filled gold when on, grey outline when off.
    save(star_sprite(True), "star-on.png")
    save(star_sprite(False), "star-off.png")

    # Scrollbar thumb (9-slice vertical bar): bevelled, like the
    # colony's indicator bar, with a speckled steel face and the
    # industrial frame (8 px border, like the other 9-slice
    # sprites, so the 9-slice border mapping is uniform).
    thumb = Image.new("RGBA", (26, 48), (0, 0, 0, 0))
    dither_face(thumb, (8, 8, 18, 40), STEEL_MID, STEEL_LIGHT, STEEL_DARK,
                230, seed=9)
    frame_border(thumb, (0, 0, 26, 48), 8, alpha=230)
    save(thumb, "thumb.png")

    # Scrollbar arrows: a framed blue button face (9-slice, no
    # triangle) plus a standalone white triangle, drawn centred
    # over it. Split, because the triangle is a centred glyph:
    # a 9-slice would tile it. The face is the arrow-blue (the
    # colony's dark blue arrow field); hover brightens, pressed
    # darkens and gets the orange glow from the halo behind it.
    # Authored 26x52 (2x the 13x26 arrow at 640x480), so the
    # 1:2 arrow aspect is the sprite's own aspect.
    def arrow_face(face):
        return button_sprite(face, (70, 66, 160), (38, 34, 100),
                             A_BUTTON, size=(26, 52), border=6)
    save(arrow_face(ARROW_BLUE), "arrow-face-normal.png")
    save(arrow_face(ARROW_BLUE_HOVER), "arrow-face-hover.png")
    save(arrow_face(ARROW_BLUE_PRESSED), "arrow-face-pressed.png")
    save(triangle_sprite(True, WHITE, size=26), "triangle-up.png")
    save(triangle_sprite(False, WHITE, size=26), "triangle-down.png")

    # Dropdown chevrons (white closed, blue open).
    save(triangle_sprite(False, WHITE, size=24), "chevron-white.png")
    save(triangle_sprite(False, ARROW_BLUE, size=24), "chevron-blue.png")

    for name in written:
        print("wrote", name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
