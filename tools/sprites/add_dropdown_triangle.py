#!/usr/bin/env python3
"""Derive the dropdown value-box sprite: value-box-wide plus a baked-in
down-triangle, so the collapsed dropdown reads as a dropdown.

The overlay triangle drawn at runtime was covered by the button field
being redrawn over it (the field system redraws button fields after
the screen's auto-function), so the triangle has to live in the button
art itself, like the colonies' baked-in labels. This script takes the
derived value-box-wide frame (104x26, menu palette) and stamps a
7x4 header-white (palette index 250) down-triangle on it: right
edge 4 px inside the frame's right edge, vertically centred -- the
same geometry as settings_layout::Dropdown_Triangle_Rect, so the
value text (already truncated to clear the triangle) still clears
it. White, not black: a black triangle sat on the frame's dark
right-edge bevel and was invisible (dark on dark); the value text
("overview", ending at button x=85) leaves no room for a dark
triangle on the light face, so the triangle reads as a light mark
on the dark bevel instead -- the header white, already used for
the SETTING/VALUE header text.

Usage: add_dropdown_triangle.py [--check]
  Writes data/base/sprites/value-box-wide-dropdown/normal.bin.
  --check verifies the output without rewriting it.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "data/base/sprites/value-box-wide/normal.bin"
DST = ROOT / "data/base/sprites/value-box-wide-dropdown/normal.bin"

W, H = 104, 26
TRI_W, TRI_H, TRI_INSET = 7, 4, 4
WHITE = 250  # header white (g_header_white_colors face)


def triangle_rows():
    """(y, x0, x1) inclusive, in sprite coordinates."""
    x = W - TRI_INSET - TRI_W
    y = (H - TRI_H) // 2
    cx = x + TRI_W // 2
    for r in range(TRI_H):
        half = TRI_W // 2 - r
        yield y + r, cx - half, cx + half


def main():
    check = "--check" in sys.argv
    data = bytearray(SRC.read_bytes())
    header, px = data[:16], data[16:]
    assert len(px) == W * H, f"{SRC}: {len(px)} pixels, expected {W * H}"
    for y, x0, x1 in triangle_rows():
        for x in range(x0, x1 + 1):
            px[y * W + x] = WHITE
    out = bytes(header) + bytes(px)
    if check:
        cur = DST.read_bytes()
        assert cur == out, f"{DST} differs from derived output; re-run without --check"
        print("value-box-wide-dropdown: OK (matches derived output)")
        return
    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_bytes(out)
    print(f"wrote {DST} ({len(out)} bytes)")


if __name__ == "__main__":
    main()
