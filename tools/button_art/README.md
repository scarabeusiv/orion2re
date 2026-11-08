# orion2re-owned button art

The original menu buttons are baked art in `MAINMENU.LBX`: three opaque
frames each (normal / hover / disabled) with the label painted in. There
is no blank panel sprite and no seventh button, so new buttons cannot
reuse the original art. `../render_button_frames.py` renders
orion2re-owned frames from a bundled open font instead; its module
docstring documents the full pipeline (template, font, stamp, fit,
output format).

Contents:

- `template-quit-*.png` — the original Quit button frames
  (`MAINMENU.LBX` entries 18/19/20, 153x22 px, palette-indexed), the
  template every rendered button starts from. Quit is the template
  because a new button takes the row Quit used to occupy: the frames
  carry one pixel of neighbour bleed in their top/bottom rows, and that
  bleed is only correct for that row. Re-derive with
  `render_button_frames.py --extract-templates MAINMENU.LBX` using the
  original game data; the committed PNGs are the checked-in result.
- `buttons.json` — the manifest: one entry per button, with one label
  per language (`en`/`de`/`fr`/`es`/`it`). This is the source of truth
  for rendered labels. Adding a button means adding a manifest entry
  and re-running the tool; the game only needs the button id wired to
  `button_art::Load_Button_Frames`.
- The font lives with the game data, not here:
  `data/base/fonts/Archivo[wdth,wght].ttf` (Archivo variable font,
  SIL OFL 1.1, `data/base/fonts/OFL.txt`), bundled unchanged. The
  rendered frames ship in `data/base/buttons/<id>/<lang>/<state>.bin`
  and are part of `base.zip`; the runtime never reads the font.

Reproduce everything with `tools/render_button_frames.py`, and verify
with `tools/render_button_frames.py --check`.
