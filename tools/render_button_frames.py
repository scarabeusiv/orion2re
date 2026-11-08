#!/usr/bin/env python3
"""Render orion2re-owned main-menu button frames from a bundled font.

The six original menu buttons are baked art: each button is three opaque
frames (normal / hover / disabled) in MAINMENU.LBX with its label painted
in. There is no blank panel sprite and no seventh button, so a new button
cannot reuse the original art as-is. This tool is the systemic answer: it
renders orion2re-owned button frames from a bundled open font, in the
original button style, for every supported language.

Pipeline (per button, per language, per state):

1.  Template. The tool starts from the original Quit button frames
    (MAINMENU.LBX entries 18/19/20, 153x22 px), stored as indexed PNGs in
    tools/button_art/. Quit is the template because the new button takes
    the row Quit used to occupy: the frames carry one pixel of neighbour
    bleed in their top/bottom rows, and that bleed is only correct for
    that row. The label area (x 13..140, y 5..16) is blanked per state:
    the normal panel is flat index 209, the disabled panel is flat
    index 159, and the hover panel is a regular dither of indices
    139/140/141 whose base value is read per column from label-free row
    17 and whose accent rows are {3, 8, 12, 16, 20} (accent = base + 1).
    The tool asserts the label bounding box and the panel values before
    blanking, so a template that does not match fails loudly instead of
    producing a subtly wrong frame.

2.  Font. Labels are rendered with Archivo Expanded Black (Archivo
    variable font, wght 900, wdth 125), bundled unchanged at
    data/base/fonts/Archivo[wdth,wght].ttf under the SIL Open Font
    License 1.1 (data/base/fonts/OFL.txt). No free font matches the baked
    label at its natural width: at 11 px cap height the closest
    candidates render "QUIT GAME" at 75-114 px against the baked 128 px
    (Archivo Expanded Black is the widest, at 114 px). The tool therefore
    derives the horizontal scale at render time: the scale that makes
    the reference label "QUIT GAME" exactly as wide as the baked label
    (128 px at 11 px cap height). Re-rendering "QUIT GAME" through this
    pipeline reproduces the original Quit frames to within
    anti-aliasing texture (same silhouette, same weight).

3.  Stamp. The rendered label is quantized into the original label's
    ink ramp instead of a flat colour, so it carries the same embossed
    look: the top edge row of each glyph is index 159 and the bottom
    edge row is index 159 in the normal state (in hover the top edge
    is left as panel, matching the original hover frames, and the
    descender is dropped; in disabled both edges are the face index
    19). The face is index 19, partial-coverage side pixels are
    indices 22/37. Labels are uppercased: the original buttons are
    all-caps, and the render matches that style.

4.  Fit. A label that does not fit the 128 px label width is condensed
    horizontally (x-scale reduced, cap height kept at 11 px) until it
    fits; the scale never goes below 0.70, below which the tool fails
    and the label must be shortened. German "EINSTELLUNGEN" (13 caps)
    is the only current label that needs this (x-scale about 0.71).

5.  Output. Each frame is written as a native orion2re bitmap: the
    s_animation_header layout produced by bitmap::Create_Bitmap_Header
    (12-byte header, one frame offset of 16, width*height palette-index
    bytes, padded to a multiple of 4, draw mode BITMAP). The game loads
    these with button_art::Load_Button_Frames, which validates the
    header before use; see src/game/button_art.h. Files land in
    data/base/buttons/<button-id>/<lang>/<state>.bin and ship in
    base.zip; the runtime never needs the font or the templates.

Adding a button: add an entry to tools/button_art/buttons.json with
the button id and one label per language (en/de/fr/es/it, the game's
five languages), then run this tool. No game-code change is needed
beyond wiring the button id to button_art::Load_Button_Frames.

Reproducing: `tools/render_button_frames.py` regenerates every frame.
`--check` regenerates to a temporary directory and byte-compares
against the committed files, so drift in the font, the templates or
the rasterizer is caught instead of silently shipped. Rasterization
depends on Pillow + FreeType, so --check can differ across library
versions; the committed .bin files are the shipped truth.
`--extract-templates MAINMENU.LBX` re-derives the template PNGs from
the user's original game data (entries 18/19/20 plus the palette of
entry 2); the committed templates are the checked-in result of that
extraction, so the tool also runs with no game data present.

Requires Pillow (pip install pillow). Reads no system fonts.
"""

import argparse
import json
import struct
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("render_button_frames: Pillow is required (pip install pillow)",
          file=sys.stderr)
    raise SystemExit(2)

REPO_ROOT = Path(__file__).resolve().parent.parent
TOOL_DIR = REPO_ROOT / "tools" / "button_art"
FONT_PATH = REPO_ROOT / "data" / "base" / "fonts" / "Archivo[wdth,wght].ttf"
MANIFEST_PATH = TOOL_DIR / "buttons.json"
OUTPUT_ROOT = REPO_ROOT / "data" / "base" / "buttons"

# Template geometry, measured from the original Quit frames (see module
# docstring). The tool asserts the label box instead of assuming it.
TEMPLATE_SIZE = (153, 22)
LABEL_BOX = (13, 140, 5, 16)  # x0, x1, y0, y1 inclusive
LABEL_WIDTH = LABEL_BOX[1] - LABEL_BOX[0] + 1  # 128
CAP_HEIGHT = 11
FONT_VARIATION = [900, 125]  # Archivo fvar order is (wght, wdth)
# Reference label the horizontal scale is calibrated against: the
# baked "QUIT GAME" label is LABEL_WIDTH px wide at CAP_HEIGHT px cap
# height. The scale is derived at render time (reference width /
# LABEL_WIDTH), never a magic constant, so it stays correct across
# rasterizers. A label wider than the box after scaling is condensed
# further by the fit step.
REFERENCE_LABEL = "QUIT GAME"
MIN_X_SCALE = 0.70
LANGUAGES = ("en", "de", "fr", "es", "it")
STATES = ("normal", "hover", "disabled")

# Panel indices per state, from the original Quit frames.
PANEL_FILL = {"normal": 209, "disabled": 159}
HOVER_ACCENT_ROWS = {3, 8, 12, 16, 20}
HOVER_PANEL_INDICES = {139, 140, 141}

# Label ink ramp per state: (top edge, bottom edge, face, mid, low).
# Hover has no top edge and no descender, matching the original hover
# frames; disabled renders both edges in the face index.
INK = {
    "normal": (159, 159, 19, 22, 37),
    "hover": (None, 159, 19, 22, 37),
    "disabled": (19, 19, 19, 22, 37),
}

TEMPLATE_FILES = {
    "normal": TOOL_DIR / "template-quit-normal.png",
    "hover": TOOL_DIR / "template-quit-hover.png",
    "disabled": TOOL_DIR / "template-quit-disabled.png",
}

# Native bitmap frame layout (bitmap::Create_Bitmap_Header).
BITMAP_FRAME_DATA_OFFSET = 16
BITMAP_DRAW_MODE = 1


def load_template(state):
    """Load a template frame as a palette-index grid (list of rows)."""
    path = TEMPLATE_FILES[state]
    with Image.open(path) as image:
        if image.size != TEMPLATE_SIZE:
            raise ValueError(f"{path}: expected {TEMPLATE_SIZE}, got {image.size}")
        if image.mode != "P":
            raise ValueError(f"{path}: expected indexed (P) image, got {image.mode}")
        width, height = image.size
        raw = image.tobytes()
        return [list(raw[y * width:(y + 1) * width]) for y in range(height)]


def check_label_box(template):
    """Verify the normal template's label box matches the calibration."""
    fill = PANEL_FILL["normal"]
    rows, cols = len(template), len(template[0])
    found = None
    # Rows 0 and 20/21 carry neighbour bleed and the bottom bevel, not
    # the label; the label lives strictly inside rows 1..19.
    for y in range(1, rows - 2):
        for x in range(2, cols - 2):
            if template[y][x] != fill:
                if found is None:
                    found = [x, x, y, y]
                else:
                    found[0] = min(found[0], x)
                    found[1] = max(found[1], x)
                    found[2] = min(found[2], y)
                    found[3] = max(found[3], y)
    if found is None or tuple(found) != LABEL_BOX:
        raise ValueError(
            f"template label box is {tuple(found) if found else None}, "
            f"expected {LABEL_BOX}; the template does not match the Quit "
            "frames this tool was calibrated against")


def blank(template, state):
    """Return a copy of the template with the label area blanked."""
    out = [row[:] for row in template]
    x0, x1, y0, y1 = LABEL_BOX
    if state in ("normal", "disabled"):
        fill = PANEL_FILL[state]
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                out[y][x] = fill
        return out
    # Hover: regular dither; base per column from label-free row 17,
    # accent rows one step brighter.
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            base = template[17][x]
            if base not in HOVER_PANEL_INDICES:
                raise ValueError(
                    f"hover template row 17 col {x} is {base}, not a panel "
                    "dither index; cannot reconstruct the blanked panel")
            out[y][x] = base + 1 if y in HOVER_ACCENT_ROWS else base
    return out


def render_label(text, font_path=FONT_PATH):
    """Render an uppercased label to a float coverage grid.

    Returns (coverage, size, x_scale, width): coverage rows are cap-top
    aligned, values in [0, 1]. The label is condensed horizontally
    until it fits LABEL_WIDTH.
    """
    text = text.upper()
    if not font_path.is_file():
        raise ValueError(f"font not found: {font_path}")

    def cap_height(size):
        font = ImageFont.truetype(str(font_path), size)
        font.set_variation_by_axes(FONT_VARIATION)
        probe = Image.new("L", (64, 64), 0)
        draw = ImageDraw.Draw(probe)
        box = draw.textbbox((0, 0), "H", font=font)
        return box[3] - box[1]

    low, high = 4, 200
    for _ in range(12):
        mid = (low + high) // 2
        if cap_height(mid) < CAP_HEIGHT:
            low = mid
        else:
            high = mid
    # Smallest size whose cap height reaches CAP_HEIGHT (low is the
    # largest size still short of it).
    size = high
    font = ImageFont.truetype(str(font_path), size)
    font.set_variation_by_axes(FONT_VARIATION)

    def natural_width(label):
        probe = Image.new("L", (1024, 128), 0)
        draw = ImageDraw.Draw(probe)
        draw.text((10, 10), label, font=font, fill=255)
        box = probe.getbbox()
        if box is None:
            raise ValueError(f"label {label!r} renders empty")
        return box[2] - box[0]

    # Horizontal scale: the factor that makes the reference label
    # exactly as wide as the baked label. Derived, not constant, so
    # the calibration survives rasterizer differences.
    x_scale = LABEL_WIDTH / natural_width(REFERENCE_LABEL)

    for _ in range(8):
        width = round(natural_width(text) * x_scale)
        if width <= LABEL_WIDTH:
            break
        x_scale *= LABEL_WIDTH / width
        if x_scale < MIN_X_SCALE:
            raise ValueError(
                f"label {text!r} needs x-scale {x_scale:.2f} to fit "
                f"{LABEL_WIDTH}px, below the {MIN_X_SCALE} floor; shorten it")
    else:
        raise ValueError(f"label {text!r} did not converge to fit")

    canvas = Image.new("L", (1024, 128), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((10, 10), text, font=font, fill=255)
    box = canvas.getbbox()
    crop = canvas.crop(box)
    scaled = crop.resize((width, crop.height), Image.LANCZOS)
    pixels = scaled.tobytes()
    rows = [[pixels[y * width + x] / 255.0 for x in range(width)]
            for y in range(scaled.height)]
    return rows, size, x_scale, width


def stamp(blanked, coverage, state):
    """Stamp a coverage grid into a blanked template, in the label box."""
    top_ink, bottom_ink, face, mid, low = INK[state]
    x0, x1, y0, y1 = LABEL_BOX
    height = len(coverage)
    width = len(coverage[0])
    out = [row[:] for row in blanked]
    x_start = x0 + (LABEL_WIDTH - width) // 2
    y_start = y0  # coverage rows are cap-top aligned
    significant = [[value > 0.08 for value in row] for row in coverage]
    for yy in range(height):
        for xx in range(width):
            if not significant[yy][xx]:
                continue
            gx, gy = x_start + xx, y_start + yy
            if not (x0 <= gx <= x1 and y0 <= gy <= y1):
                continue
            above = significant[yy - 1][xx] if yy > 0 else False
            below = significant[yy + 1][xx] if yy < height - 1 else False
            if not above:
                if top_ink is None:
                    continue  # hover: no top edge
                out[gy][gx] = top_ink
            elif not below:
                if state == "hover" and yy >= CAP_HEIGHT:
                    continue  # hover: no descender
                out[gy][gx] = bottom_ink
            else:
                value = coverage[yy][xx]
                if value >= 0.6:
                    out[gy][gx] = face
                elif value >= 0.3:
                    out[gy][gx] = mid
                else:
                    out[gy][gx] = low
    return out


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


def decode_frame(data):
    """Decode a native bitmap frame back to an index grid (for previews)."""
    width, height = struct.unpack_from("<hh", data, 0)
    offset = struct.unpack_from("<I", data, 12)[0]
    pixels = data[offset:offset + width * height]
    return [list(pixels[y * width:(y + 1) * width]) for y in range(height)]


def load_manifest():
    with open(MANIFEST_PATH, encoding="utf-8") as handle:
        manifest = json.load(handle)
    buttons = manifest.get("buttons", [])
    if not buttons:
        raise ValueError(f"{MANIFEST_PATH}: no buttons defined")
    for button in buttons:
        labels = button.get("labels", {})
        missing = [lang for lang in LANGUAGES if lang not in labels]
        if missing:
            raise ValueError(
                f"button {button.get('id')!r}: missing labels for {missing}")
    return buttons


def render_all(output_root, button_filter=None, lang_filter=None,
               preview_dir=None):
    templates = {state: load_template(state) for state in STATES}
    check_label_box(templates["normal"])
    buttons = load_manifest()
    written = []
    for button in buttons:
        button_id = button["id"]
        if button_filter and button_id != button_filter:
            continue
        for lang in LANGUAGES:
            if lang_filter and lang != lang_filter:
                continue
            label = button["labels"][lang]
            for state in STATES:
                coverage, size, x_scale, width = render_label(label)
                blanked = blank(templates[state], state)
                stamped = stamp(blanked, coverage, state)
                data = frame_bytes(stamped)
                path = output_root / button_id / lang / f"{state}.bin"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                written.append((str(path), width, size, x_scale))
                if preview_dir is not None:
                    with Image.open(TEMPLATE_FILES["normal"]) as template:
                        palette = list(template.getpalette() or [0] * 768)
                    preview = Image.new("P", TEMPLATE_SIZE)
                    preview.putpalette(palette)
                    preview.frombytes(bytes(
                        value for row in decode_frame(data)
                        for value in row))
                    preview_path = (Path(preview_dir) / button_id / lang /
                                    f"{state}.png")
                    preview_path.parent.mkdir(parents=True, exist_ok=True)
                    preview.convert("RGB").save(preview_path)
    return written


def extract_templates(lbx_path):
    """Re-derive the template PNGs from the original MAINMENU.LBX."""
    import hashlib

    data = Path(lbx_path).read_bytes()
    entry_count, magic = struct.unpack_from("<HH", data, 0)
    if magic != 0xFEAD:
        raise ValueError(f"{lbx_path}: not an LBX file")
    if entry_count < 21:
        raise ValueError(f"{lbx_path}: only {entry_count} entries")
    offsets = struct.unpack_from("<510I", data, 8)

    def entry(index):
        return data[offsets[index]:offsets[index + 1]]

    def decode_sprite(payload, width, height):
        frame_offset = struct.unpack_from("<I", payload, 12)[0]
        stream = payload[frame_offset:]
        start_y = struct.unpack_from("<H", stream, 2)[0]
        image = [[0] * width for _ in range(height)]
        pos, y = 4, start_y
        while y < height:
            pixel_count, skip = struct.unpack_from("<hh", stream, pos)
            pos += 4
            if pixel_count == 0:
                y += skip
            else:
                image[y][skip:skip + pixel_count] = stream[pos:pos + pixel_count]
                pos += pixel_count
                if pos & 1:
                    pos += 1
        return image

    def palette():
        background = entry(2)
        palette_offset = 12 + (1 + 1) * 4 + 4
        raw = background[palette_offset:palette_offset + 1024]
        table = []
        for index in range(256):
            # s_palette_entry is {changed, r, g, b} in 6-bit values.
            table.extend((raw[4 * index + 1] * 4,
                          raw[4 * index + 2] * 4,
                          raw[4 * index + 3] * 4))
        return table

    table = palette()
    for lbx_entry, state in ((18, "normal"), (19, "hover"), (20, "disabled")):
        payload = entry(lbx_entry)
        width, height = struct.unpack_from("<hh", payload, 0)
        grid = decode_sprite(payload, width, height)
        image = Image.new("P", (width, height))
        image.putpalette(table)
        image.frombytes(bytes(value for row in grid for value in row))
        path = TEMPLATE_FILES[state]
        image.save(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()[:12]
        print(f"{lbx_path} entry {lbx_entry} -> {path} (sha256 {digest})")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Render orion2re-owned button frames (see module docstring).")
    parser.add_argument("--button", help="render only this button id")
    parser.add_argument("--lang", choices=list(LANGUAGES),
                        help="render only this language")
    parser.add_argument("--check", action="store_true",
                        help="regenerate to a temp dir and byte-compare "
                        "against the committed frames")
    parser.add_argument("--preview-dir", type=Path,
                        help="also write RGB preview PNGs here")
    parser.add_argument("--extract-templates", type=Path, metavar="MAINMENU.LBX",
                        help="re-derive the template PNGs from an LBX file "
                        "and exit")
    arguments = parser.parse_args(argv)
    if arguments.extract_templates:
        extract_templates(arguments.extract_templates)
        return 0
    if arguments.check:
        with tempfile.TemporaryDirectory(prefix="orion2re-buttons-") as tmp:
            render_all(Path(tmp), arguments.button, arguments.lang)
            mismatches = []
            for path in sorted(Path(tmp).rglob("*.bin")):
                committed = OUTPUT_ROOT / path.relative_to(tmp)
                if not committed.is_file():
                    mismatches.append(f"{committed}: missing committed file")
                elif committed.read_bytes() != path.read_bytes():
                    mismatches.append(f"{committed}: differs from regeneration")
            if mismatches:
                print("button frames do not match regeneration:", file=sys.stderr)
                for line in mismatches:
                    print(f"  {line}", file=sys.stderr)
                return 1
            print("button frames match regeneration")
            return 0
    written = render_all(OUTPUT_ROOT, arguments.button, arguments.lang,
                         arguments.preview_dir)
    for path, width, size, x_scale in written:
        print(f"{path}: label {width}px, font size {size}, x-scale {x_scale:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
