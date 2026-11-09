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
  button renders it the same way. The same frame also serves
  every metal button at any width, cut into three slices at
  runtime (sprite_art::Draw_Sprite_Frame_Three_Slice, via
  ui_kit::Button): the left dome (columns 0..16) and the right
  dome (columns 60..84) as-is, and a single text-free face column
  tiled across the middle -- column 58 for the rest frame, column
  59 for the pressed frame -- with the label rendered at runtime
  in the game font. Column 58 is the only column that is the flat
  face (238) for the whole text body (rows 8..16) in the rest
  frame; column 59 is the same for the pressed frame, and column
  58 in the pressed frame catches the orange text's fringe
  (221/223) and must not be used there. The caps are copied
  exactly, so a metal button's chrome is pixel-identical to the
  RETURN's; the middle differs from the RETURN's middle only
  where the RETURN has text (correctly omitted) and in the face's
  dithered bands (rows 1, 7, 17, 20 rest; row 17 pressed), where
  the tiled column holds one shade of the dither. Face-match of
  the tiled middle against the RETURN's non-text face pixels:
  622/897 (69.3%) rest, 710/731 (97.1%) pressed -- the rest
  mismatches are mostly text-antialiasing pixels that a blank
  button must not have.

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
