# orion2re-owned derived sprites

The colony screens' separable sprites cannot be drawn on a menu
screen as-is: they are authored for palette 1, and some carry their
label baked in. `../extract_sprite.py` derives redeployable frames
from them (palette remap, label blanking, per-state overrides); its
module docstring documents the full pipeline. The derived frames ship
in `data/base/sprites/<sprite-id>/<state>.bin` and are part of
`base.zip`; the runtime loads them with
`sprite_art::Load_Sprite_Frame` and draws them with
`sprite_art::Draw_Sprite_Frame` /
`sprite_art::Draw_Sprite_Frame_Stretched`.

Contents:

- `sprites.json` — the manifest: one entry per sprite, with the
  source LBX entry, the source/target palette ids, the expected size,
  the frame each state takes, and (when the label is baked in) the
  blank rectangle. This is the source of truth for the derivation.
  Adding a sprite means adding a manifest entry and re-running the
  tool; the game only needs the sprite id wired to
  `sprite_art::Load_Sprite_Frame`.

- `return` — COLSUM.LBX entry 1, the colony RETURN button, 85x26,
  palette 1 -> menu palette. Frame 0 is the button at rest (grey
  bevel, dark "RETURN"); frame 1 is its pressed state (the text turns
  orange). No blanking: the baked label is the label the settings
  screen wants. The button's right edge is flat black as authored
  (it blends into the colony background there); the derived frame
  keeps it, and the settings screen's black background behind the
  button renders it the same way.

- `selector-frame` — COLSUM.LBX entry 4, the colony sort button
  ("Name"), 50x24, palette 1 -> menu palette, frame 0. The baked
  label is blanked (rows 4-19, columns 4 to the right edge, filled
  with the interior index 57) so the settings screen can render its
  own labels with the game font. Two states: `normal` (black
  interior) and `active` (interior overridden to the menu blue 138,
  the Auto Build blue the active tab already used). The sort button
  is asymmetric as authored — bevelled on the left and top, flat
  black on the right — so the stretched draw keeps a 4 px left slice
  and no right slice; the flat middle tiles invisibly to any tab
  width. The sort button's own frame 1 (the selected sort column)
  was not used for the active state: after blanking it differs from
  frame 0 only in bevel details, it carries no blue face, so the
  override is what produces the active tab.

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
