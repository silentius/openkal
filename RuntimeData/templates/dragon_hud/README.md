# Dragon HUD assets

The live assembly is `config/ui/dragon_hud.json` (`openkal.ui/v1`). It is the
single source of truth for positions, draw order, text and bindings. Edit it
with the UI editor or as JSON. This directory contains only the native runtime
BMP textures; generation sheets, duplicate exports and design previews were
removed after approval.

## Textures and assembly

`bmp/` contains 20 uncompressed 32-bit BI_RGB textures with straight alpha.
Preserve all four BGRA bytes when editing; do not flatten against black or
convert to 24-bit BMP. The build copies these textures unchanged into
`RuntimeData/UI/dragon_hud`, and the config package includes the same files.
The client and UI editor search that runtime UI directory first.

The HUD is 792 by 180 logical pixels and anchored at the bottom center of the
screen. It scales uniformly to fit narrower viewports. Its EXP frame ends at
the bottom edge. Keep the opaque center backing through both progress bars
to prevent the world from showing through seams.

Slot frames are 40 by 40 pixels with native 32-by-32 skill or item icons
inset by four pixels. The ten lower slots use 1–0; the ten upper slots use
Shift+1–Shift+0. The auxiliary column contains the active skill and a reserved,
disabled auto-HP slot. The top-left identity widget (`config/ui/status.json`)
reuses `bar_backing`, `orb_empty` and a half-size `dragon_left` around the
round class icon. Its 204-by-92 logical-pixel layout centers the
FreeType name inside matching patterned top/bottom rails and simple bronze
side rails. The name and its frame appear only while hovering the class icon;
the emblem and level remain visible, and the hidden name area passes clicks through.
A slightly enlarged crop of the dragon's small lower shield holds the centered
level number, with space for three digits. HP and MP appear in the bottom HUD.

All labels and numbers use FreeType. HP/MP captions start at y=135; ACTIVE at
y=90 and AUTO HP at y=144. The vertical OpenKal inscription uses two text
layers per letter to create an engraved appearance.

## Live state and interactions

`DragonHud` supplies HP/MP current and maximum values and HP/MP/EXP/Rage
ratios. Empty globes remain behind the colored 84-by-84 interiors. Generic
`progress` widgets use `fill_mode: crop` and `direction: bottom_to_top` for
HP/MP, preserving texture proportions as they empty. EXP and Rage crop from
the left. Ratios are clamped to [0,1], and zero maxima produce empty fills.

`SkillBar` handles both rows, cross-row drag/drop, active skills, item counts,
cooldowns and tooltips. Auto-HP rejects clicks and drops until that feature
is implemented. All twenty shortcuts persist through the protocol and server
store; old ten-slot saves migrate with an empty upper row. Update client and
server together. Shift+digit replaces the former city-teleport shortcut.

See `docs/ui/TEMPLATE.md`, `docs/ui/VARIABLES.md` and `docs/ui/EDITOR.md` for
schema and editing details. The `ui_document`, `runtime_icons`, `protocol`
and `server` tests cover layout, texture loading and shortcut persistence.
Verify visual edits in the client or UI editor at native and scaled sizes.

Artwork was generated with the built-in image generation tool from the
project-created OpenKal HUD concept. Compatible game skill icons were not
converted or replaced.
