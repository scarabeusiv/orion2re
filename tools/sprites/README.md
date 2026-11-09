# orion2re-owned derived sprites

The colony screens' separable sprites cannot be drawn on a menu
screen as-is: they are authored for the colony palette -- the palette
embedded in COLSUM.LBX entry 0, which the colony screen installs
before its sprites draw -- and some carry their label baked in.
(That palette is not FONTS.LBX entry 2: the two agree on the grey
ramp but differ completely in the blue range, where the colony
screen's blue field and labels live.) `../extract_sprite.py`
derives redeployable frames from them (palette remap, label blanking,
per-state overrides); its module docstring documents the full
pipeline. The derived frames ship in
`data/base/sprites/<sprite-id>/<state>.bin` and are part of
`base.zip`; the runtime loads them with
`sprite_art::Load_Sprite_Frame` and draws them with
`sprite_art::Draw_Sprite_Frame` /
`sprite_art::Draw_Sprite_Frame_Stretched` (flat middles) or
`sprite_art::Draw_Sprite_Frame_Stretched_Resampled` (gradient
middles, which must span the width once, not tile).

Contents:

- `sprites.json` — the manifest: one entry per sprite, with the
  source LBX entry, the source/target palette ids, the expected size,
  the frame each state takes, and (when the label is baked in) the
  blank rectangle. This is the source of truth for the derivation.
  Adding a sprite means adding a manifest entry and re-running the
  tool; the game only needs the sprite id wired to
  `sprite_art::Load_Sprite_Frame`.

- `return` — COLSUM.LBX entry 1, the colony RETURN button, 85x26,
  colony palette -> menu palette. Frame 0 is the button at rest (grey
  bevel, dark "RETURN"); frame 1 is its pressed state (the text turns
  orange). No blanking: the baked label is the label the settings
  screen wants. The button's right edge is flat black as authored
  (it blends into the colony background there); the derived frame
  keeps it, and the settings screen's black background behind the
  button renders it the same way.

- `selector-frame` — COLSUM.LBX entry 4, the colony sort button
  ("Name"), 50x24, colony palette -> menu palette. The baked label
  is blanked per state so the settings screen can render its own
  labels with the game font: `normal` takes frame 0, blanked with a
  flat fill of the dark-navy interior (rows 4-19, columns 4 to the
  right edge); `active` takes frame 1 -- the selected sort column --
  whose field is a blue gradient (bright at both edges, dark navy in
  the centre) with a blue bevel, blanked by copying each pixel from
  the same column at label-free row 5, so the gradient continues
  behind the new label. The blue ramp has no close hue in the menu
  palette, so the active state overrides it onto the menu's own
  blues (50/84/103/138). The sort button is asymmetric as authored
  — bevelled on the left and top, flat black on the right — so the
  stretched draw keeps a 4 px left slice and no right slice. The
  rest frame's flat middle tiles; the selected frame's gradient
  middle is resampled to the tab width
  (sprite_art::Draw_Sprite_Frame_Stretched_Resampled), or the
  gradient would repeat like stripes. The tabs sit 1 px apart at
  y=446, like the colony's sort buttons, and their labels are font
  style 2: the baked colony labels measure as style 2, with a white
  face at rest and a blue-white face when active (see
  src/game/ui_kit.h, Colony_Theme).

- `metal-button` — NOT derived from game art, and NOT grabbed
  from a screenshot of a rendered button either. The metallic button
  frames (blank, pressed, disabled) are original work, created from
  scratch by observing how the game's metallic buttons look: a 2 px
  dark border (the real button's top and bottom edges are dark, not
  a light raised bevel) around a domed face gradient -- dark at the
  top edge, rising to a bright plateau in the middle, a one-row
  specular highlight at the vertical centre, then falling to dark
  at the bottom edge -- in the menu palette's grey ramp.
  `tools/generate_metal_buttons.py` computes them from that design
  spec — it reads no game data and no game output — and writes
  `data/base/sprites/metal-button/{normal,pressed,disabled}.bin`
  (85x26, the RETURN size). Reproduce with
  `tools/generate_metal_buttons.py`, verify with the same command
  plus `--check`. The frames carry no label: the button component
  (`src/game/metal_button.h`) interposes the label at runtime with
  the game font, so one frame serves any short label (MORE, LESS,
  RETURN). The pressed state darkens the face's top rows into
  shadow and lights the label up orange (palette 221, the pressed
  RETURN text's own orange); the disabled state is a flat face with
  a low-contrast bevel. The component draws the frame stretched
  (2 px slices preserved at both edges) and falls back to an
  engine-drawn button in the same colours when a frame is missing,
  so a stale base.zip still renders.
  A 3-slice grab of the rendered RETURN button was considered and
  rejected: the button is a sprite (COLSUM.LBX entry 1), so grabbing
  its render would be a verbatim copy via screenshot, and blanking
  its baked text cannot be done cleanly -- vertical interpolation
  of the face behind the text misses by 2-6 palette steps, and the
  specular band is unrecoverable that way.

## What cannot be derived from game art

- Border, list panel, header plates, description box, toggle
  squares, scrollbar track/thumb: baked into the 640x480 colony
  background art (COLSUM.LBX entry 0); `colsum.cpp` draws no boxes
  itself, so no separable sprite exists. They stay engine-drawn
  Fill/Box in menu-palette indices, in the settings chrome colours
  the restyle already uses.
- Dropdown popup: no popup-window sprite exists; the game's own
  modal lists are engine-drawn. The resolution popup stays
  engine-drawn.
- Star buy buttons (COLSUM.LBX entries 11-20): purchase actions
  with two frames (not bought / bought), not on/off toggles. Not
  used; the settings toggles stay the engine-drawn squares.
- Scrollbar arrows (COLSUM.LBX entries 2/3): real sprites, but the
  settings scrollbar is a drag thumb with no arrow buttons. Not used.

Reproduce everything with `tools/extract_sprite.py --lbx COLSUM.LBX
--fonts-lbx FONTS.LBX --mainmenu-lbx MAINMENU.LBX`, and verify with
the same command plus `--check`. The tool needs the user's original
game data; the committed .bin files are the checked-in result.
