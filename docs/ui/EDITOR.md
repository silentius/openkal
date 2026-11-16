# Qt UI editor (`tools/uiEditor`)

Binary: `ui_editor`. Schema: [`TEMPLATE.md`](TEMPLATE.md). Catalog:
[`VARIABLES.md`](VARIABLES.md). Enable with `-DOPENKAL_BUILD_UI_EDITOR=ON`.

The old GLFW BMP placer (`ui_layout_edit`) and the Skill/Inventory engine
sandboxes are gone. Layout work belongs here; pixel-perfect checks stay in
`character_info_ui_view` / the client.

## Binary and stack

- Path: `tools/uiEditor/` (same CMake/Qt pattern as `tools/lootEditor`).
- Target name: `ui_editor`.
- Links: `Qt6::Widgets`, `Qt6::Gui`, `openkal::platform`. Reuse BMP load +
  magenta key from the loot editor; do not link the game engine for v1.
- Data root: `OPENKAL_DATA` / profile `data_root` / `--data`, then `data/UI`
  and `data/HyperText`. Native HUD textures are loaded first from the configured
`RuntimeData/UI` directory; build `openkal_runtime_icons` before previewing.
- Preview is layout-accurate, not shader-accurate.

## Main window

Four docks around a canvas:

1. **File / document** — Open/save `config/ui/<id>.json` with
   `"schema": "openkal.ui/v1"`. New dialog: pick `kind` (`window` | `hud`),
   chrome preset, and `window.w/h`. For the built-in `info_menu` frame, it also
   exposes the `chrome.close` toggle that selects the visible-X or no-close
   top-right corner. The dock also edits `chrome.edges`
   (`tile` / `stretch` / `repeat`) and `pad_left` / `pad_top` / `pad_right` /
   `pad_bottom` (0.1 native pixels). Changing the preset reapplies the
   preset’s default pads.
   An **Inventory preview** selector switches General and Other (default: General) for
   `inventory.json`, updating the visibility bindings without modifying JSON or
   marking the document dirty. Every tab can be inspected and edited in place.
2. **Widget tree** — `chrome` then `widgets[]` in draw order. The frame is
   rendered over client widgets; the text widget with id `title` is the chrome
   caption and is rendered last. Selecting
   `chrome` outlines the outer frame and exposes the close hitbox
   (`close.ox/oy/w/h`) in the inspector for non-`info_menu` frames. The
   built-in `info_menu` close region is fixed and controlled by
   `chrome.close`. Reorder = z-order. Hide/lock toggles.
3. **Palette** — `text`, `button`, `icon_slot`, `icon_grid`, `tabs`,
   `progress`, `image`, `fill`, `border`, `input`, `list`, `canvas`. Drag onto
   the canvas or double-click to add at `(0,0)`.
4. **Inspector** — fields for the selection: `id`, `x/y/w/h` (integer pixels),
   `align` (`left` / `center` / `right`) for `text` and `input` — `x` is the
   left, center, or right edge of the glyphs, same as the client. For buttons,
   `center` / `right` align the label within the button and vertically center
   its line box; label offsets apply after alignment.
   `label_ox` / `label_oy` (button caption offset from the widget top-left),
   `text` / `bind`, colors, `action`, `accept`, `visible_when`.
   Minimap canvases expose `shape` (`rect` / `circle`) and preview the clipped
   surface with a player marker. Progress widgets expose `fill_mode` (`lcr` / `crop`) and `direction`
   (`left_to_right` / `bottom_to_top`); cropped RGBA fills preview at dummy ratios. String fields
   that accept interpolation offer a catalog combo sourced from `VARIABLES.md`
   (parse the tables, or a later generated `variables.json`; do not hardcode a
   second list). Chrome selection shows close-hitbox offsets instead of widget
   geometry.

Canvas:

- Checker / dim desktop behind the dialog.
- The client box stays fixed on the canvas. `pad_*` move the chrome around it
  (same as in the game). Tiled Info chrome uses `pad_left` / `pad_top` as the
  **top-left bitmap** position; the top-center and top-right strips sit on the
  client, not on those pads.
- Gold outer rect. Drag the top-left chrome bitmap (highlighted when `chrome`
  is selected) to change `pad_left` + `pad_top`. Drag the four outer edges
  (or the gold handles) for a single pad. Pads always snap to 0.1 px; the
  32 px widget grid does not apply. With chrome selected, arrow keys nudge
  `pad_top` / `pad_left` (Shift = 0.1).
- Client box outline; drag the inner right/bottom edges to change
  `window.w/h` and **retile** the 9-slice.
- Widgets are selectable, draggable, snap-optional (1 px default, 32 px grid
  toggle).
- Dummy catalog so `${stats.hth}` and bag icons paint without a server:
  `stats.hth = 42`, `player.name = Preview`, a few HyperText item BMPs in the
  bag grid.

```
chrome.preset + window size → 9-slice tiler → Qt preview canvas
widget palette              → canvas
VARIABLES.md catalog        → inspector → openkal.ui/v1 JSON
canvas                      → JSON
```

## Auto-frame

On New Window / preset change / client resize:

1. Read `chrome.kind` (default `nine_slice`).
2. Resolve preset files ([`TEMPLATE.md`](TEMPLATE.md) presets).
3. Tile (`edges: tile`) or stretch (`edges: stretch`) around the client box
   using `pad_*`. Pads are editable in the Document dock and by dragging the
   gold outer edges; they are written back to JSON on save. The built-in
   `info_menu` preview selects its top-right bitmap from `chrome.close` and
   never draws an additional exit glyph.
4. For non-`info_menu` frames, draw the configured close glyph at the default
   top-right hitbox unless a `button` with action `window.close` overrides it.
   With chrome selected, the inspector edits `close.ox/oy/w/h` for those
   frames only. `info_menu` instead uses its baked X and fixed invisible hitbox.

The editor never writes a `chrome.pieces` array of repeated left/right rails.
`image` widgets cover inner panels (`info-character-image(addmi).bmp`,
`info-inventory-image.bmp`, `inchant(~ing)-image.bmp`). `chrome.kind: pieces`
is an advanced override, hidden behind an “Edit raw pieces” action.

HUD documents (`kind: hud`) skip 9-slice and pad editing, and show
`chrome.file` as a single bitmap (`main-character.bmp`, …).

## Save / load

- Load `openkal.ui/v1` fully.
- Load today’s live layouts as **read-only chrome preview** is out of scope
  for v1 (those files are a different schema). Optional later: import
  `chrome.pieces` from `character_info.json` as `image` widgets, then switch
  the document to `nine_slice`.
- Preserve `_doc` keys.
- Pretty-print JSON (2-space), English identifiers only.
- Do not write `_template.json` from the editor; that file is the hand-edited
  skeleton.

## Dummy preview values

Minimum set so Stats and Inventory examples in `_template.json` render:

| Path | Dummy |
|---|---|
| `ui.title` | `Info` |
| `player.name` | `Preview` |
| `player.level` | `50` |
| `stats.hth` / `_label` | `42` / `HTH` |
| `stats.pu_remaining` | `5` |
| `vital.hp_text` | `800/800` |
| `combat.attack` | `40–55` |
| `inventory.count_text` | `3/72` |
| `inventory.money` | `12345` |
| `inventory.general.0` … `.5` | six vertically stacked 36-cell blocks on General |
| `inventory.quest`, `inventory.task` | Quest Items above protected Task Items, 36 cells each on Other |
| `inventory.normal_active`, `inventory.quest_active` | controlled by Inventory preview |
| `inventory.wear.helmet` | empty (show `empty.file`) |

A later “Preview as class” toggle (Knight/Mage/Archer/Thief) can swap
`player.class_icon` without a live `NetClient`.

## Implementation notes (when coding)

- Mirror `lootEditor` CMake: `find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)`,
  `AUTOMOC`, `OPENKAL_REPO_ROOT`.
- GPL-3.0 headers, Doxygen, `m_` members, no `using namespace std`.
- Parse `VARIABLES.md` tables with a small line scanner (`| \`group.key\` |`)
  so the picker cannot drift from the catalog. If that proves brittle, add
  `docs/ui/variables.json` generated from the Markdown — still one source of
  truth, documented in `VARIABLES.md`.
- Wire `add_subdirectory(tools/uiEditor)` next to `lootEditor` (optional Qt).

### Dragon dialog editing

Select `dragon_dialog` in the chrome preset list for the shared opaque bronze and
stone frame. It defaults to native-size repeating rails and 40/84/40/56 padding; the dragon and
corners keep their native size when width or height changes. `dialog_icon`,
`title`, and Dragon close controls preview above the frame. The canvas includes
the same opaque panel background as the client. Dragon footers automatically
preview the right-aligned, engraved `OpenKal` inscription and the HUD's textured
bronze inlay from the shared frame.

Widgets with `x_from_right` or `y_from_bottom` stay anchored on resize. Inspector
X/Y fields show resolved client coordinates; editing or dragging them updates
the existing anchor offsets, which survive JSON save/reload. Button previews
show idle artwork; hover/pressed/disabled textures are preserved for runtime.
Character Info's minimum content size is 420 x 560 native pixels.

Inventory uses a 440 x 508 client area. Icon-grid previews repeat every cell and
honor its separate centered `frame` dimensions; wear-slot frames follow the same
rule. Tab captions preview their state color and optional centered alignment.
The equipment columns stay visible beside both tabs. Inventory viewports preview
complete rows, and wheel scrolling moves through the stacked 36-cell blocks.
The inspector edits `viewport` and `scroll_bind`; both survive save/reload.
The sort/destroy controls and geometric trash/lock pictograms are regular v1
buttons and fills, so they remain editable without dialog-specific editor code.

Startup, character selection/creation/deletion, and the game menu have populated
dummy text. The deletion preview starts in confirmation mode; changing the
`character_delete.*` flags selects the recovery view. These layouts use standard
Dragon chrome and widgets without editor-specific rendering.


Party documents use shared preview values for names/HP and separate invitation
controls. `party_member.json` edits one reusable 150 x 28 row with a cropped HP
gradient and recessed track. The 150 x 22 title region uses a bronze texture and
the same border widget as chat; runtime extends its `panel_fill` and `frame` to
the full member-list height. There are no empty slots, member controls or mana
bars. `party_invitation.json` previews the separate invitation prompt with Dragon
chrome, a highlighted inviter and stateful buttons. `party_actions.json` previews
the management frame with leader-only Invite, membership count, Leave and footer
close. `party_action_member.json` edits one reusable name/Remove row. Preview
`party.is_leader` as false to inspect ordinary-member controls. These all use
existing image, progress, text, fill, border and button widget types.

The General inventory preview includes six native filter-symbol buttons above
`general_viewport`. Their selected underlines are ordinary bound fills; the
`inventory.filter.<category>_visible` dummy variables can preview selection.
The Other sections retain their separate `bag_viewport`. Runtime category
projection belongs to the inventory host and does not rewrite the UI document.

The guild layout (`guild.json`) previews an owned synthetic ranking in a Dragon
frame. Switch the `guild.page.N` visibility variables to inspect individual
management pages. Rows and offline colors use `guild.row.N.*`. Standard widgets and all bindings/actions survive
editor roundtrips; owner guild rules live separately in server JSON.
For navigation styling, select `guild.member` or `guild.non_member`, set
`guild.tab` to the selected category ID and its matching
`guild.tab.N.member_active` or `.non_member_active` flag to true. Other active
flags stay false. Members omit category `1` (Search/Create).
Both navigation layouts omit the retired category `8` (Rank options).
The default table preview shows Ranking with `guild.table.simple`; switch to
`guild.table.members` and `guild.row.N.members_visible` for the six-column roster,
then edit `guild.column.C` and `guild.row.N.cell.C` sample values. Set the other
row group false. `guild.row.N.selected` previews a full selected row, and
`guild.table.empty` previews the empty-state caption. Prose pages use
`guild.row.N.body_visible` with table flags false. These are ordinary text,
fill, button and tabs widgets, so the editor mirrors the client layout.

Open `guild_ranks.json` for the shared rank dialog. Synthetic `guild_ranks.*`
values populate all five titles, permission columns, selection and availability.
Set `.edit` true and `.assign` false to preview the title field and permission
checkbox buttons instead of assignment confirmation. `.row.R.selected` switches
the highlight; `.input`, `.selected`, `.option.B` and `.status` supply editor
previews. The runtime host enforces permissions and request state.

`player_options.json` previews the selected-player context frame, name and status.
Open `player_option_row.json` to edit the reusable action button. Runtime repeats
that panel only for server-permitted actions; synthetic `player_options.*`
variables supply preview copy. Both use ordinary universal widgets.

The inspector's Multiline input checkbox enables top-aligned wrapping in an
input's declared width and height. `line_height` and `scroll_bind` are preserved;
the preview honors explicit LF/blank lines and clips to complete visible rows.
For Guild Announcement set `guild.page.6`, use `guild.announcement_text` and
`.announcement_scroll` for paragraphs and paging, and toggle
`.announcement_can_edit` / `.announcement_editing` for reader/editor controls.
Membership (category 7) owns Daily reward, Leave guild and leadership Transfer.
Set `guild.pagination` outside Announcement to preview its navigation controls;
the page indicator is centered between the arrows. Rank modes share the same
permission-symbol artwork.

The `dragon_compact` preset supplies five-pixel bronze bevels for small HUD
windows. `clock.json` uses the same thin border widget and textures as chat,
with a five-pixel content inset. It previews server time, online duration, an event selector
and a zero countdown. `clock_event_options.json` previews synthetic choice rows.
Both use standard v1 widgets and survive editor roundtrips.
