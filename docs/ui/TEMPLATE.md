# Universal UI JSON template (`openkal.ui/v1`)

Schema for every framed dialog and HUD panel. Live documents live under
`config/ui/*.json` and are drawn through `UiDocument` + `UiView`. This file is
the vocabulary; [`VARIABLES.md`](VARIABLES.md) is the `${group.key}` catalog;
[`EDITOR.md`](EDITOR.md) is the Qt designer (`tools/uiEditor`).

Annotated skeleton: [`config/ui/_template.json`](../../config/ui/_template.json)
(not loaded by the client).

`config/ui/dungeon_minimaps.json` is map metadata, not a dialog. It stays
outside this schema.

The character-creation detail panel uses `config/ui/character_create.json`,
the `dragon_dialog` chrome preset, and the `creation.*` bindings and actions listed
in [`VARIABLES.md`](VARIABLES.md). Class selection and the 3D preview remain
hosted by the boot shell; name, appearance and starting-stat controls use the
standard input, text and button widgets.

The entrance uses `config/ui/startup.json` for its Dragon start panel. The
character overview uses `config/ui/character_select.json`. Its bottom-center
previous / enter / next controls use the unframed `config/ui/character_navigation.json`.
The framed panels use the same Dragon chrome as the in-game menu, settings,
creation and deletion panels.
The shell keeps the initial 3D staging and camera transitions. Selection
hitboxes come from the document, including its outer frame.

`dragon_hud.json` uses `skillbar.auto_hp` for the automatic medicine icon and
`item.auto_hp` as its accepted item tag. Standard `fill` widgets with
`visible_when` flags `skillbar.auto_hp_25`, `_50`, `_75` and `_100` draw the yellow
HP-orb threshold. `skillbar.auto_hp_status` supplies its FreeType label; no
additional widget type or schema extension is required.

The character-selection shell also uses `config/ui/character_delete.json` for
its modal delete confirmation and recoverable-character list. The list bind is
`character_delete.rows`; selection, restore, and close remain boot-shell actions.

The unframed single-player event HUD uses `config/ui/battle_feed.json`, with
standard `fill`, `list`, `text` and `input` widgets and `chrome.kind: pieces`.
`ChatWidget` supplies timed message fading, the soft fill mask, history and
command-input visibility; `GameHud` supplies responsive docking. Its bindings
are documented under `chat` in [`VARIABLES.md`](VARIABLES.md). The soft fill starts
20 native pixels left of the logical feed so text has a fully shaded left inset.

`chat.json` uses standard `tabs` bound to `chat.tab` for Normal, Party, Guild,
Alliance, Squad, and Shout below the input row. The host uses the same tab geometry
for rendering and hit testing, with independent channel filters and a global
battle feed. Personal-message rows retain partner metadata for click-to-reply.
The O menu uses a standard button bound to `player_options.send_pm` to open the
same chat draft. The chat window's native width remains its minimum; the host
expands it to halve the positive gap to the skill HUD and stretches the standard
tab strip accordingly. The compose label uses `chat.mode_color`.
No new widget type is needed. See the `chat` binding catalog in
[`VARIABLES.md`](VARIABLES.md).

`settings.json` uses standard `tabs` bound to `settings.tab` and `visible_when`
page flags for Display, World, Graphics, Audio, and Battle messages. Filter
buttons edit the character's message preferences as part of the Save/Cancel draft;
no additional widget type or schema extension is required.

---

The Dragon system inbox (`mail.json`) uses only standard buttons, text, fills,
images and an `icon_slot`. Paperclip images use `mail.row.N.has_attachment` to
distinguish messages with attachments. Its `mail.*` catalog supports explicit reading, paging,
sorting, body scrolling and confirmed attachment/deletion actions. See
[`VARIABLES.md`](VARIABLES.md) for the bindings and behavior mapping.

The Mix Master (`mix_master_window.json`) uses Dragon chrome, framed icon slots,
a mode caption and recipe readiness feedback. The existing pentagram remains an
inner image. All controls use standard widgets and the `mix.*` catalog; no schema
extension is needed.

The NPC material lists use `mixing_item_list.json` with Dragon chrome and standard
text, fill and button widgets. The `mix_list.*` catalog supplies nine recipe rows
per page, mode selection and paging. Recipe requirements come from the active
mixing catalog; the layout contains no gameplay rules.

The compact `clock.json` HUD window uses the same three-pixel `border` widget
and textures as chat, `chrome.kind: pieces`, standard text/button widgets and a
`window.close` button. Its host
supports dragging the integrated time header and per-character position storage.
The 198 x 118 outer area retains a five-pixel content inset and uses a small clock emblem instead of a separate title
strip, with a prominent server time, a thin divider and a compact online row. The selector
`clock_event_options.json` uses standard buttons, fill, border and text widgets.
The Misc settings checkbox uses `settings.misc.clock`; all clock bindings and
server notice configuration are documented under `clock` in `VARIABLES.md`.
No widget type or schema extension is needed. The UI editor supplies synthetic
clock and event-choice values through the shared preview catalog.

## Document shape

The fullscreen `powerbook.json` uses standard fills, text, buttons, icon slots,
and a rectangular `minimap` canvas bound to `powerbook.map`. Its host supplies
sixteen monster rows and a six-column, ten-row item-slot grid, independent
wheel/drag scrolling, item hover tooltips, selection, and spawn paging. Slots
reuse compact inventory frames and the inventory item tooltip builder. The
fullscreen Dragon dialog has a 1600×900 outer frame and an inset 1520×760 client
box at (40,84). Its nine-slice preset, stone backgrounds, bronze heading strips,
menu emblems and stateful buttons use the shared Dragon artwork. Three bronze
gradient headings sit below the corner ornament, flush with the inside edges
of their block frames. Their image source excludes the baked top highlight so
the two-pixel border alone draws the upper edge; standard `border` widgets
outline the full blocks with the existing bronze heading texture. The unlabeled
close X uses the shared footer control. The host
centers the map on the selected spawn, preserves the map
aspect ratio, and clips translucent yellow spawn rectangles to the canvas. The
editor supplies synthetic row values and a highlighted sample map area. No new
widget type or canvas kind is required. See `powerbook` in `VARIABLES.md`.

The quest journal adds `quest.snitch` to open `quest_snitch.json`, a compact
Dragon dialog built from standard text, button, fill and rectangular `minimap`
canvas widgets. `snitch.map` reuses the Powerbook location renderer for NPC
points and monster areas; `snitch.*` bindings and paging actions are documented
in `VARIABLES.md`. No widget type or schema extension is needed.

One JSON file = one window or HUD panel. Coordinates are **native pixels**
relative to the **client box** top-left `(0,0)` — the same origin today’s
layouts use. Chrome padding sits *around* that box. The client applies UI
scale at draw time.

```json
{
  "schema": "openkal.ui/v1",
  "id": "character_info",
  "kind": "window",
  "window": { "w": 245, "h": 417, "draggable": true, "title_drag_h": 44 },
  "theme": {
    "font_pixel_size": 10,
    "colors": {
      "label": "#E8DCC0",
      "value": "#FFFAEB",
      "pending": "#C8E090",
      "title": "#F2F0C0"
    }
  },
  "chrome": {
    "kind": "nine_slice",
    "preset": "info_character",
    "edges": "tile"
  },
  "widgets": []
}
```

| Key | Required | Meaning |
|---|---|---|
| `schema` | yes | Must be `openkal.ui/v1`. |
| `id` | yes | Layout name without `.json` (`character_info`, `status`, `dragon_hud`, …). |
| `kind` | yes | `window` (framed popup) or `hud` (status, dragon_hud, chat, minimap, menu). |
| `window` | yes | Client box. `w` / `h` required. `draggable` (default true). `title_drag_h` measured from the outer frame top. |
| `theme` | no | Default FreeType size and colors. Widgets may override. Theme colors are also exposed as `${theme.colors.*}`. |
| `chrome` | yes | Frame or background. Default kind is `nine_slice`. |
| `widgets` | yes | Draw/hit list. Array order = draw order (first = back). The frame is rendered above client widgets; the text widget with id `title` is the chrome caption and is rendered last. |
| `_doc` | no | Human note. Ignored by the renderer. Allowed on any object. |

Do not invent per-window nested schemas (`primaries.stats[]`, `wear_left.slots[]`,
…). Express those as widgets with `${…}` text or `bind` paths.

---

## Interpolation

Any string field may contain `${group.key}` tokens. The runtime catalog is
[`VARIABLES.md`](VARIABLES.md). Missing keys render as empty (not the raw
`${…}` text).

Legacy NPC content in `config/dialogs/*.json` also accepts `${player.name}` in
`caption`, `lines[]` and link `text`; see the NPC section of `VARIABLES.md`.
Dialog link types `guild_create` and `guild` open the guild creation page and
browser respectively. These reuse the universal `guild.json` layout and its
server-driven requirements, fees and status bindings.

```json
{ "type": "text", "x": 2, "y": 129, "text": "${stats.hth_label}", "color": "${theme.colors.label}" }
{ "type": "text", "x": 35, "y": 129, "text": "${stats.hth}", "color": "${theme.colors.value}" }
```

Labels are variables too, so layout JSON does not own copy.

The menu's standard FreeType text widgets also draw the Trading Post `!` badge,
using `hud.menu.trading_post.badge` and visibility
`hud.menu.trading_post.pending`. It shares the Messages counter's black glyph
and white outline; no widget or schema extension is required.
NPC dialog link type `trading_post_collection` opens the collection tab through
the current collection agent. Its `trading_post.act` button offers Collect All
across all pages without a selection. A separate `trading_post.collect_selected`
button uses `${trading_post.collect_selected}` and `trading_post.collection`
visibility for individual pickup. Both require NPC context; opening that tab
from the menu does not authorize pickup. Custom layouts may also use the
equivalent `trading_post.collect_all` action and label.

**Format suffixes** (optional, after `:`):

| Suffix | Example | Result |
|---|---|---|
| *(none)* | `${stats.hth}` | `42` |
| `comma` | `${inventory.money:comma}` | `1,234,567` |
| `geons` | `${inventory.money:geons}` | `1,234,567`, colored by the Geon tier |
| `+` | `${transform.str:+}` | `+3` or empty when 0 |
| `pct` | `${exp.pct:pct}` | `37%` |

Concatenation is allowed: `"${vital.hp}/${vital.max_hp}"`.

`:geons` formats the unsigned decimal amounts in a currency value or currency
label with grouping and individual colors. Existing comma separators are
accepted. For example, `${trading_post.range0:geons}` colors both range endpoints
independently while preserving the surrounding text. Use this suffix only for
currency fields. The shared client and editor render these spans in text,
inputs and button labels; alignment and wrapping use the plain displayed text.
Disabled buttons and pending text retain their state colors.
Amounts below 100,000 are white (`#FFFFFF`); below 1,000,000 green (`#00FF00`);
below 10,000,000 yellow (`#FFFF00`); below 100,000,000 orange (`#FF8040`);
below 1,000,000,000 blue (`#0080FF`); all larger amounts purple (`#AA55FF`).
The same thresholds apply to inventory tooltips and 64-bit stack totals.

**v1-optional color expression** (first renderer may skip this and use a
`pending` flag on the text widget instead):

```json
"color": "${stats.hth_pending ? theme.colors.pending : theme.colors.value}"
```

`visible_when` is a boolean catalog path or comparison, not free-form script:

```json
"visible_when": "slot.count > 1"
"visible_when": "transform.equipped"
```

Unknown `visible_when` expressions hide the widget.

`pending_when` is the same condition language. When true, text uses
`theme.colors.pending` (Character Info staged PU).

```json
"pending_when": "stats.hth_pending"
```

---

## Auto chrome

Info / Inventory / Mix were first laid out by stacking dozens of
`info-menu-left.bmp` copies. Skill / Transform already skip that:
`WidgetTools::draw_window_frame` builds a 9-slice from `window.w/h`.

**Default is automatic.** Changing `window.w/h` retiles the frame. Extra art
(inner panel, pentagram) is an `image` widget, not a chrome piece list.

```json
"chrome": {
  "kind": "nine_slice",
  "preset": "info_character",
  "edges": "tile",
  "pad_left": 26.5,
  "pad_top": 45.1,
  "pad_right": 19.1,
  "pad_bottom": 36.1
}
```

### `chrome.kind`

| Kind | Use |
|---|---|
| `nine_slice` | **Default.** Tile or stretch preset BMPs around the client box. |
| `bitmap` | HUD panels with a single background (`main-character.bmp`, `main-target-mop.bmp`, …). `file` plus optional `fallback_w` / `fallback_h`. |
| `pieces` | Escape hatch: explicit `{file,x,y,w,h}` array (draw order = array order). Do not use this to stack rails for a normal dialog. |

### `chrome.edges` (`nine_slice` only)

| Value | Behaviour | Matches |
|---|---|---|
| `tile` | Repeat rails at native BMP size. | Character Info / Inventory look. |
| `stretch` | Stretch edges to fill the outer rect. | Skill / Transform. |
| `repeat` | Repeat native-size rail textures around the outer rect; crop the last partial repeat. Corners remain fixed. | Dragon dialogs. |

### `chrome.close` (`nine_slice` only)

`true` (the default) enables the invisible close hitbox and, for `info_menu`, uses
`info-menu-top right.bmp`, which contains the visible X. `false` disables the
hitbox and uses `info-menu-top right - not close.bmp`. The X is part of the
frame art; do not add a second glyph over it.

### Presets

Legacy presets differ in corner artwork and use separate inner `image` widgets.
`dragon_dialog` supplies all eight frame textures plus a shared opaque interior.
Its footer automatically includes a right-aligned `OpenKal` inscription in the
HUD's two-layer engraved FreeType style over its textured bronze inlay;
no per-dialog widget is needed.

| Preset | Top-left | Typical inner `image` | Used by |
|---|---|---|---|
| `info_menu` | `info-menu-top left.bmp` | none | Skill learn |
| `info_character` | `info-top left(character).bmp` | `info-character-image(addmi).bmp` | Legacy Character Info |
| `dragon_dialog` | `dragon_dialog/frame_top_left.bmp` | automatic `dragon_dialog/panel_background.bmp` | Character Info, Inventory, Riding, Shinsu, Quest Journal |
| `dragon_compact` | `dragon_dialog/compact_top_left.bmp` | explicit stone background/bronze heading images | Custom compact windows |
| `info_inventory` | `info-top left(inven).bmp` | `info-inventory-image.bmp` | Inventory |
| `info_mix` | `info-top left(DragonBall).bmp` | `inchant(~ing)-image.bmp` | Legacy Mix Master |

`dragon_compact` uses five-pixel bronze bevels, native-size repeating
rails, and five-pixel padding on all sides. It defaults to `close: false`;
use a regular `window.close` button in compact layouts. The preset is available
in the UI editor and uses the same layout/render path as other nine-slice frames.

The Dragon preset uses native-size repeating rails, fixed-size asymmetric corners, and native
padding left/right `40`, top `84`, bottom `56`. Width and height can change
independently; do not scale the corner artwork to change the dialog size.
`chrome.close` defaults to `false`. Its lower-right close control is a regular
`button` with `action: window.close`, `x_from_right: 16`, `y_from_bottom: 0`,
and `w/h: 56`. It stays on the footer when the client dimensions change.
Reserve `dialog_icon` for the independent menu emblem; it and `title` render
above the chrome. Dragon `window.close` buttons also render above the frame.
Reserve `frame_handle` for a button mounted on a frame rail; it also renders
above the chrome in the client and UI editor. Inventory mounts its Collection drawer toggle on that rail.
The opaque title and dragon mouth backing come from the shared top-left asset.
The Character Info content needs at least a 420 x 560 client area.

Shared legacy 9-slice files unless a preset overrides them:

| Role | File |
|---|---|
| Top center | `info-menu-top center.bmp` |
| Top right | `info-menu-top right.bmp` |
| Left rail | `info-menu-left.bmp` |
| Right rail | `info-menu-right.bmp` |
| Bottom left | `info-menu-bottom left.bmp` (`info-bottom left.bmp` / `info-bottom right_2.bmp` on character/inventory/mix when matching renewal art) |
| Bottom center | `info-menu-bottom center.bmp` / `info-bottom center.bmp` |
| Bottom right | `info-menu-bottom right.bmp` / `info-bottom right.bmp` |
| Close glyph | `info-exit small.bmp` (non-`info_menu` frames only; `info_menu` has a baked X) |

A preset may override any of those keys with an explicit `files` object:

```json
"chrome": {
  "kind": "nine_slice",
  "preset": "info_menu",
  "edges": "stretch",
  "files": { "upleft": "info-menu-top left.bmp" }
}
```

Close hitbox defaults to the top-right X baked into the corner. Set
`"close": false` in `chrome` for a non-closable `info_menu`. Override a
non-default hitbox with a `button` whose `action` is `window.close`, or with:

```json
"close": { "ox": -25, "oy": -32, "w": 22, "h": 22 }
```

Offsets are relative to the client top-right (`client_x + w + ox`, `client_y + oy`).

---

## Widget vocabulary

Every widget has `type`. Common optional fields:

| Field | Meaning |
|---|---|
| `id` | Stable name for hit-testing, binds, and the editor tree. |
| `x`, `y` | Client-box native pixels. May be negative (wear columns outside the border). |
| `w`, `h` | Hit/draw size. `0` on buttons means native BMP size. |
| `visible` | Constant bool (default true). |
| `visible_when` | Catalog condition (see Interpolation). |
| `color` | `#RRGGBB` or `${theme.colors.*}`. |

Stat rows are **explicit `text` widgets**, not a C++ loop. That is what makes
Info rearrangeable. Repeating icons stay compact via `icon_grid`.

### `image`

BMP blit. Use for inner panels and decorations, not for the 9-slice rails.

```json
{ "type": "image", "id": "inner", "file": "info-character-image(addmi).bmp", "x": 0, "y": 0, "w": 245, "h": 416 }
```

Optional `alpha` (0..1, default 1).

Images may select an asset-pixel `source: {x, y, w, h}` rectangle and set
`repeat: true` to tile that region at native pixel size over the widget's
explicit `w` / `h`. Partial tiles are clipped at the right and bottom edges.
Without `repeat`, the selected region stretches to the widget bounds; without
`source`, the entire image is used. Client and Qt editor both support these
fields, and editor saves preserve them.

```json
{ "type": "image", "id": "texture", "file": "info-inventory-image.bmp",
  "x": 0, "y": 0, "w": 552, "h": 433,
  "source": { "x": 5, "y": 378, "w": 232, "h": 36 }, "repeat": true }
```


### `text`

FreeType. `text` may contain `${…}`.

```json
{
  "type": "text",
  "id": "hth_value",
  "x": 35, "y": 129,
  "text": "${stats.hth}",
  "color": "${theme.colors.value}",
  "pixel_size": 10,
  "bold": false,
  "align": "left"
}
```

`align`: `left` (default), `center`, `right`. `pixel_size` is FreeType em size;
HUD panels that still use bitmap glyphs today should move to FreeType when
migrated. `pending: true` tints with `theme.colors.pending` when the bound
preview differs from the committed value (alternative to the color expression).

### `button`

Hitbox plus optional BMP states and a label.

```json
{
  "type": "button",
  "id": "ok",
  "x": 90, "y": 201, "w": 0, "h": 0,
  "label": "${ui.ok}",
  "label_ox": 0, "label_oy": 3,
  "file_default": "I-mini-default.bmp",
  "file_over": "I-mini-over.bmp",
  "file_on": "I-mini-on.bmp",
  "file_off": "I-mini-off.bmp",
  "action": "stats.apply"
}
```

Button `align` defaults to `left`, preserving top-left label placement.
`center` centers the label line box horizontally and vertically within the
button; `right` right-aligns it and centers it vertically. `label_ox` /
`label_oy` add native-pixel offsets after alignment (leave them at zero for
automatic centering). `w` / `h` of `0` use the BMP’s native size.

The shared `UiView` renderer uses `file_over` for hover, `file_on` for pressed,
and `file_off` for disabled buttons when the host supplies their state. Disabled
artwork retains its own opacity. Hosts can override `widget_enabled` for a
binding-specific state; the default delegates to `button_enabled(action)`.
Character Info disables minus when no points are staged and plus when the next
point is unaffordable or the staged limit is reached. Click handlers enforce
the same limits. Character Info supplies pointer state through
`UiViewHost`; the editor previews idle artwork. Dragon text, square, and close
controls live under `dragon_dialog/{button,icon_button,close}_*.bmp`.

The Dragon menu host uses `file_over` for hover and `file_on` for a pressed or
open-window state. Its buttons use `text` for a FreeType tooltip with
`${hud.menu.*.tooltip}` tokens and leave `label` empty. These fields are
preserved by the editor; its canvas previews the idle artwork.

`action` values (extend [`VARIABLES.md`](VARIABLES.md) when adding new ones):

| Action | Meaning |
|---|---|
| `window.close` | Hide the dialog. |
| `window.noop` | Consume the click (stub footer buttons). |
| `stats.spend` | Stage +1 PU on the bound primary (`bind`: `stats.str`, …). |
| `stats.unspend` | Stage −1 PU. |
| `stats.apply` | Commit staged PU (OK). |
| `mix.start` | Start mixing. |
| `hud.menu.sinsu` / `riding` / `character` / `inventory` / `skill` / `quest` / `party` / `community` / `map` / `messages` / `system` | Menu strip. |
| `inventory.destroy` | Trash hitbox. |
| `inventory.expand` | Use the shortest available unlock writ for the next 36 General cells; hover lists all expansion lifetimes. |
| `inventory.collection_absorb` | Absorb into the explicitly selected bag Taegeuk; server validates dependencies. |
| `inventory.filter.*` | Toggle a General category (`consumables`, `weapons`, `armors`, `enchantment`, `skins_pets`, `other`); selected categories combine, no selection shows all. |
| `inventory.sort` | Sort General across writable blocks or each Other section independently, preserving instance data and locked blocks. |
| `minimap.zoom_in` / `minimap.zoom_out` | Minimap buttons. |
| `npc.open_dialog` | Follow a numeric dialog link. |
| `npc.quest_link` | Quest hyperlink. |
| `npc.save_revival:<point>` | Save the selected Black Crow statue as the server revival point. |
| `npc.village_revival` | Request revival at the saved statue. |
| `npc.blacksmith` | Open the blacksmith window. |
| `npc.previous` / `npc.next` | Navigate bounded NPC conversation content pages. |
| `npc.mixing` | Open Mix Master. |
| `npc.shop` | Request and open the current NPC shop. |
| `npc.speed_medicine` | Open the Chemist of Speed-up Medicine recipe selector. |
| `speed_medicine.*.decrease` / `increase` | Adjust one of the two exposed recipe quantities. |
| `speed_medicine.make` | Submit the selected recipe quantities. |
| `crafting.make.0` / `.1` / `.2` / `.3` | Make one item from the selected consumable family recipe row. |
| `blacksmith.revise` | Stone of Revision. |
| `blacksmith.explosive_blow` | Explosive Blow. |
| `shop.buy` / `shop.sell` | Trade one non-stackable item immediately, or open the quantity prompt for a stackable offer / bag item. |
| `shop.offers.previous` / `shop.offers.next` | Page the server offer grid. |
| `shop.quantity.decrease` / `increase` / `max` | Change the transaction quantity; purchases are capped at 10,000. |
| `shop.quantity.add10` / `add100` / `add1000` | Replace the untouched initial buy quantity, then add 10 / 100 / 1000 per click. |
| `shop.quantity.confirm` / `cancel` | Submit or dismiss the transaction prompt. |
| `game_menu.cancel` / `options` / `select_character` | Close the ESC menu, reserve Options, or return to character selection. |
| `game_menu.close` / `close_confirm` / `close_cancel` | Open, accept, or dismiss client-close confirmation. |
| `settings.*` | Edit, save, or cancel the active profile's client-settings draft, including independent monster-body, remote-player-body, and player-nameplate limits. |

Plus/minus on a primary row: two `button`s with `bind` `stats.hth` and
`action` `stats.spend` / `stats.unspend`.

### `progress`

Fill bar. `bind` is a 0..1 ratio.

```json
{
  "type": "progress",
  "id": "hp",
  "x": 75, "y": 37, "w": 116, "h": 8,
  "bind": "vital.hp_ratio",
  "file": "main-prograss-red"
}
```

`file` is a progress prefix or a single BMP, matching today’s HUD keys
(`main-prograss-red`, `main-prograss-target mopHP`, `main-prograss-exp1`).

For a single RGBA texture, `fill_mode: "crop"` clips matching texture UVs and
quad dimensions without stretching the remaining artwork. `direction` is
`left_to_right` (default) or `bottom_to_top`. Ratios are clamped to `[0,1]`;
non-finite values draw empty. The existing `lcr` default is unchanged.
The Dragon HUD uses bottom-to-top HP/MP globes and horizontal Rage/EXP fills.
The Qt editor previews both directions and preserves them on save.

```json
{ "type": "progress", "id": "hp_fill", "file": "dragon_hud/orb_hp_full.bmp",
  "x": 38, "y": 53, "w": 84, "h": 84, "bind": "vital.hp_ratio",
  "fill_mode": "crop", "direction": "bottom_to_top" }
```

A `progress` without `file` draws a solid rectangle using its interpolated
`color` and `alpha` (default color remains `#CC3333`). The client and Qt editor
both support this form. Textured HP bars can instead use `file` with `fill_mode: crop`.

### `icon_slot`

One bound icon. See [Icon fields](#icon-fields).

### `icon_grid`

Repeating slots. See [Icon fields](#icon-fields).

### `tabs`

Mutually exclusive buttons. `bind` holds the selected id.
`align: center` centers each tab caption horizontally and vertically; the default
keeps the existing four-pixel caption inset.

```json
{
  "type": "tabs",
  "id": "bag_tabs",
  "bind": "inventory.tab",
  "y": 44, "h": 28,
  "align": "center",
  "color_on": "#FFFAEB",
  "color_off": "#C7BFA6",
  "items": [
    { "id": "normal", "x": 148, "w": 65, "label": "${ui.tab_normal}", "file_on": "dragon_dialog/long_button_pressed.bmp", "file_off": "dragon_dialog/long_button_idle.bmp" },
    { "id": "quest", "x": 222, "w": 65, "label": "${ui.tab_task}", "file_on": "dragon_dialog/long_button_pressed.bmp", "file_off": "dragon_dialog/long_button_idle.bmp" }
  ]
}
```

Skill learn uses the same type with `bind`: `skill.tab` and items
`common`, `job1`, `job2`, `job3`, `awaken`, `action`.

Guild navigation uses `bind: guild.tab` with category IDs `0`–`8` and no button
textures. Standard fills supply a dark strip, bronze baseline, and the active
tab's warm background and gold underline. `guild.member` / `guild.non_member`
select seven- and eight-category layouts, each with matching active fills.
Guild tables also use standard text, fill and button widgets: the header and
aligned cell groups are drawn above striped rows; transparent full-row buttons
retain selection independently of cell text. No table widget type is required.
`guild_ranks.json` reuses these same standard widgets for a modal rank list,
permission matrix and optional title/permission editor. The host dims and blocks
the parent while this document is open; no schema extension is required.

### `fill` / `border`

Colored rect (chat/mix dark panel) and 4-edge BMPs.

```json
{ "type": "fill", "x": 0, "y": 0, "w": 400, "h": 180, "color": "#000000", "alpha": 0.45 }
{
  "type": "border",
  "thickness": 3,
  "top": "chatting-box-top.bmp",
  "bottom": "chatting-box-bottom.bmp",
  "left": "chatting-box-left.bmp",
  "right": "chatting-box-right.bmp"
}
```

### `input` / `list`

All inputs vertically center the FreeType line box within their declared `h`,
using line height rather than em size. `align: left`, `center` or `right` sets
horizontal alignment within `w`. Text widgets retain anchor-based alignment.

An input with `multiline: true` instead wraps UTF-8 text to `w`, preserves LF
paragraph breaks and blank lines, and aligns its first line to the top-left.
`line_height` is native-pixel row spacing, at least the font's line height.
Only complete lines fitting inside `h` are drawn. Optional `scroll_bind` holds
the number of wrapped lines to skip. The UI editor exposes Multiline input and
previews the same layout; these fields survive JSON roundtrips. The host owns
cursor movement, editing, read-only state and scrolling.

Guild Announcement uses a 876 x 320 multiline input inside a recessed panel.
It opens as published text for every member; explicit Edit/Save/Cancel actions
provide an authorized draft with cursor editing and mouse-wheel scrolling.
`guild.pagination` hides the footer arrows and page indicator on this tab. Ordinary inputs
retain their existing single-line behavior.

Chat compose line and message rows. `bind` is `chat.input` / `chat.lines`.

```json
{ "type": "input", "id": "chat_input", "x": 72, "y_from_bottom": 18, "h": 18, "bind": "chat.input" }
{ "type": "list", "id": "chat_log", "x": 8, "y": 8, "line_height": 14, "bind": "chat.lines" }
```

Ordinary widget rectangles and text can use `x_from_right` and `y_from_bottom`
instead of `x` and `y`. Tab strips retain their explicit per-tab coordinates.
Nonnegative values position the widget origin that many native pixels before
the client right or bottom edge. They are not margins measured from the widget's
far edge: `y_from_bottom: 0` starts a footer widget at the client bottom.
Missing or negative values use ordinary coordinates. The editor preserves these
anchors while moving widgets or editing their displayed X/Y coordinates.

### `canvas`

Escape hatch for draw paths that are not generic quads.

```json
{ "type": "canvas", "kind": "minimap", "x": 19, "y": 42, "w": 171, "h": 152 }
{ "type": "canvas", "kind": "level_orb", "cx": 58, "cy": 52, "inner": 18, "text": "${player.level}" }
```

`kind`: `minimap` | `level_orb` | `quest_journal`. Do not add new `canvas.kind` values without
updating this file.

The `quest_journal` canvas hosts wrapped quest rows, selected details, and live
item progress. Its left 40% is the selectable list; the right pane reserves a
footer for item requirements. The Qt editor preserves and edits its bounds;
content verification uses the client renderer.

A minimap canvas accepts `shape: "circle"`; the default is `"rect"`. A circle
uses the smaller of the scaled width and height, centered in the canvas bounds.
The client clips tiles, the background and player markers together before
rendering the frame and labels. The Qt editor previews the circular surface
and preserves the shape on save. Transparent corners pass pointer input through;
the map/rim, named `area_plate` / `coordinates_plate` images and zoom controls
are interactive. Dragging uses the existing per-character HUD position store.

```json
{ "type": "canvas", "id": "map", "kind": "minimap", "shape": "circle",
  "x": 36, "y": 55, "w": 188, "h": 188 }
```

---

## Icon fields

`bind` is a catalog path. The engine resolves the current item, skill, or buff
and draws its icon. Inside a slot, extra keys exist: `${slot.icon}`,
`${slot.count}`, `${slot.equipped}`, `${slot.empty}`, `${slot.level}`.

### `icon_slot`

Single cell: wear, mix diamond, class portrait, egg, destroy, active skill.

```json
{
  "type": "icon_slot",
  "id": "wear.helmet",
  "x": -47, "y": 54, "w": 32, "h": 32,
  "bind": "inventory.wear.helmet",
  "frame": { "file": "slot.bmp", "w": 42, "h": 42 },
  "empty": { "file": "slot back-helmet.bmp" },
  "accept": ["item.helmet"],
  "tooltip": true,
  "drag": true,
  "badge": { "text": "${slot.count}", "visible_when": "slot.count > 1" }
}
```

An `empty.file` image is drawn only for an empty slot and only when the texture
is available. This allows compatible-client slot backgrounds to remain optional
runtime assets without displaying a missing-texture rectangle.

Wear bind ids (initial `WearSlotID` / current `inventory.json`):

`helmet`, `upper`, `gauntlet`, `pants`, `boots`, `weapon`, `shield`, `ring`,
`necklace`, `accessory`, `face`, `sinsu`, `pet`, `ride`.

Mix: `mix.center`, `mix.north`, `mix.east`, `mix.south`, `mix.west`.

The inventory's Collection drawer uses ordinary `icon_slot` widgets in
`inventory_collection.json`, with `inventory.collection.0` through
`inventory.collection.25` bindings and the `item.collection` accept tag.
It uses the universal schema and fits beside the compact inventory.
See `VARIABLES.md` for mapping.

NPC shop: `shop.inventory` is a compacted read-only view of eligible General
and Other consumable stacks (`sell > 0`, not equipped), with 36 entries per register page. CTRL marks
multiple stacks for a full-stack sale. `shop.offers` is the server-authoritative
`buy > 0` list with 35 entries per page, matching the native 5-by-7 shop bitmap.

### `icon_grid`

Repeating cells: bag 6×6, skill learn, hotbar 1×10, apply-item buffs.
An optional `frame: { file, w, h }` centers a separate frame around each cell,
just as for `icon_slot`. Icon size remains controlled by `cell`; `pitch` should
leave room for the full frame. Framed cells use that full rectangle for pointer
hits. Inventory uses 32-pixel icons, 42-pixel frames, a 44-pixel column pitch and a
46-pixel row pitch.

```json
{
  "type": "icon_grid",
  "id": "bag.0",
  "x": 157, "y": 115,
  "cols": 6, "rows": 6,
  "cell": { "w": 32, "h": 32 },
  "pitch": { "x": 44, "y": 46 },
  "frame": { "file": "dragon_hud/slot_normal.bmp", "w": 42, "h": 42 },
  "bind": "inventory.general.0",
  "visible_when": "inventory.normal_active",
  "select_color": "#F0D020",
  "tooltip": true,
  "drag": true,
  "badge": { "text": "${slot.count}" }
}
```

`page` and `filter` are host-specific metadata. Inventory now uses explicit
`inventory.general.0` through `.5`, `inventory.quest` and
`inventory.task` grid binds in the active view (`normal` or `quest`). General uses
`general_viewport` below its filter icons; Other uses `bag_viewport`. The General
host maps filtered display cells back to underlying bag slots for both icons and
hit tests. The six ordinary buttons use `inventory.filter.*` actions, with
selected indicators bound through `inventory.filter.*_visible` (see VARIABLES). Dragon HUD hotbars: two `cols: 10`, `rows: 1` grids with
`skillbar.hotbar` (1–0) and `skillbar.hotbar_shift` (Shift+1–Shift+0).
Skill learn uses two grids (`skill.active`, `skill.passive`) with
`${slot.level}` under each icon.

### Vertical viewport bindings

Optional `viewport: "widget_id"` and `scroll_bind: "catalog.path"` fields apply
vertical scrolling to a widget. The viewport is an ordinary widget with explicit
`x/y/w/h`; its Y bounds clip the content. Coordinates on content widgets remain
absolute client coordinates, and the bound offset is measured in native pixels.
The host owns offset clamping and scroll input. This row-based feature draws only
complete icon-grid rows (including their frames) and complete non-grid widgets.
Use explicit `h` on headings and fills. Keep content within the viewport's X
bounds. It is intended for regular vertical lists, not arbitrary pixel clipping.

```json
{ "type": "fill", "id": "bag_viewport", "x": 148, "y": 64, "w": 268, "h": 322, "color": "#181713" }
```

Each scrolled grid, heading or row fill references that viewport and the same
scroll binding. Runtime drawing and hit testing preserve input-cell
indices after clipping. The editor previews and saves these fields and scrolls
the viewport under the pointer with the mouse wheel; Ctrl+wheel still zooms.

### `accept` tags

Drop rules stay in JSON, not in per-window C++.

| Tag | Accepts |
|---|---|
| `item.any` | Any bag stack. |
| `item.helmet` / `item.upper` / `item.gauntlet` / `item.pants` / `item.boots` / `item.weapon` / `item.shield` / `item.ring` / `item.necklace` / `item.accessory` / `item.sinsu` / `item.pet` / `item.flag` | Matching wear class. |
| `item.skin.helmet` / `item.skin.upper` / `item.skin.gauntlet` / `item.skin.pants` / `item.skin.boots` / `item.skin.weapon` / `item.skin.shield` / `item.skin.face` | Matching cosmetic slot; whole-body skins use `item.skin.upper`. |
| `skill.active` / `skill.passive` | Skill icons of that group. |
| `mix.weapon` / `mix.armor` / `mix.stone` | Mix diamond cells. |
| `buff` | Apply-item / shopbuff icons. |
| `riding.helmet` / `riding.armor` / `riding.saddle` / `riding.cloth` / `riding.magnet` | Matching Battle Riding equipment and mount grade; both helmet variants use `riding.helmet`. |

---

## Mapping from today’s files

| Live file | `kind` | Chrome | Widgets |
|---|---|---|---|
| `character_info.json` | `window` | `nine_slice` `dragon_dialog` | FreeType identity/primaries/combat/resists, stateful +/−/Apply buttons, anchored footer close, `icon_grid` effects, menu emblem |
| `inventory.json` | `window` | `nine_slice` `dragon_dialog` | Equipment/General/Other tabs (General on opening); six simultaneous 6×6 General grids, permanent Quest/Consumable grids, integrated wear/Collection, Expand tooltip, sort, destroy and anchored close |
| `skill_window.json` | `window` | `nine_slice` `dragon_dialog` | page `button`s, conditional `icon_grid`s, section `text`, anchored close |
| `riding.json` | `window` | `nine_slice` `info_menu` | Horse-panel image, mount and equipment `icon_slot`s, framed detail text, satiety `progress`, management buttons |
| `transform_window.json` | `window` | `nine_slice` `info_menu` | egg `icon_slot`, bonus `text`, two `icon_grid`s (stages / animals) |
| `mix_master_window.json` | `window` | `nine_slice` `info_mix` | pentagram `image`, five `icon_slot`s, mix `button` |
| `status.json` | `hud` | `pieces` (empty) | Dragon artwork, round class `icon_slot`, persistent FreeType level and hover-only name/frame (`hud.class_hovered`) |
| `dragon_hud.json` | `hud` | `pieces` (empty) | native artwork, HP/MP and Rage/EXP cropped `progress`, two hotbar grids, active skill and reserved auto-HP slot |
| `menu_bar.json` | `hud` | `pieces` (empty) | compact Dragon artwork, stateful `button`s with `hud.menu.*`, FreeType tooltips |
| `chat.json` | `hud` | `fill` + `border` | `list`, `input`, mode `text` |
| `minimap.json` | `hud` | `pieces` (empty) | circular `minimap` canvas, Dragon frame/plaques, FreeType area/coordinate labels, zoom buttons |
| `npc_talk.json` | `window` | `nine_slice` `dragon_dialog` | caption, quest emblem, conditional footer close; wrapped body and responses in C++ |
| `quest_journal.json` | `window` | `nine_slice` `dragon_dialog` | wrapped `quest_journal` canvas, section headings, quest emblem, anchored footer close |
| `blacksmith.json` | `window` | `nine_slice` `dragon_dialog` | framed `blacksmith.weapon` slot, bound upgrade details, Dragon action buttons and footer close |
| `shop.json` | `window` | `nine_slice` `dragon_dialog` | two filtered `icon_grid`s, selection/price/money `text`, buy/sell and quantity `button`s |
| `storage.json` | `window` | `nine_slice` `info_menu` | character and account `icon_grid`s, six-register `tabs`, deposit/withdraw `button`s |
| `speed_medicine.json` | `window` | `nine_slice` `dragon_dialog` | two recipe counters, five requirement rows with owned/required columns, Make `button` |
| `consumable_crafting.json` | `window` | `nine_slice` `dragon_dialog` | up to four recipe rows with materials, owned counts and Make `button`; future-family empty state |
| `game_menu.json` | `window` | `nine_slice` `info_menu` | modal ESC actions and close confirmation |
| `settings.json` | `window` | `nine_slice` `info_menu` | profile-backed display, streaming, effect, monster/player rendering, nameplate limits, and audio controls |

NPC body/link and quest-row wrapping stays in C++ (`TextWrap`). `DialogDef.width/height`
still overrides the JSON-defined NPC-talk dimensions at runtime.

---

## Adding a new UI

1. Prefer this schema. Do not add a one-off nested layout object.
2. Add every new `${group.key}`, `bind` path, `action`, `accept` tag, chrome
   preset, or widget `type` to [`VARIABLES.md`](VARIABLES.md) (and this file if
   the vocabulary changes).
3. Design in the Qt `uiEditor` ([`EDITOR.md`](EDITOR.md)). Do not stack 9-slice
   rails by hand.

`talent_tree.json` is a fullscreen 1600×900 design built entirely from standard
`fill`, `text`, `button` and `icon_slot` widgets. The host scales it uniformly to
fit the viewport, shades the margins, and captures input while open. Its
`talent.*` bindings supply the node state colors, connecting threads, cost labels,
class emblem, effect descriptions and Apply/Exit/Beacon actions. No schema
extension or custom canvas widget is required. The existing UI editor can edit
all controls and connectors and uses the shared dummy catalog for sample labels
and selection colors.

The talent tree's node descriptions are runtime hover overlays, like skill/item
tooltips. The existing tooltip chrome frames an opaque card; the host wraps the
`talent.detail*` bindings at native font size and places it beside the hovered
icon. It does not participate in document hit testing or cover the hovered node.
The footer contains only Apply/Exit controls, a separator and request status.
The heading is drawn separately from the content pass with a bold atlas baked
at its actual on-screen size and integer-aligned placement.

Multiplayer mode selection and account forms use this same schema in
`play_mode.json` and `multiplayer_login.json`. They use standard `input`, `text`,
`fill` and `button` widgets with the Dragon nine-slice preset. The `online.*`
account bindings contain no editable server address or port; the endpoint comes
from packaged `server/client.json`. Opening multiplayer starts the connection, and
`online.connect_visible` exposes the retry action while disconnected. The account
bindings include a show/hide password action, live password/confirmation
feedback and selection background colors. Clipboard editing uses Ctrl+A/C/X/V;
these features use the existing schema and are included in editor previews. The
visibility catalog reflects server capabilities; it does not authorize account
creation. The server checks its registration policy for every request. See
`VARIABLES.md` for the complete mode/account vocabulary and editor previews.


Party layouts use only standard v1 text, image, border, fill, button and progress
widgets. `party.json` has a 150 x 22 title region; its host extends `panel_fill`
and `frame` to enclose one 150 x 28 `party_member.json` row per actual member plus
three bottom pixels. The border widget uses the exact existing chat textures.
The title and cropped HP gradient are project-owned native HUD textures. These
passive roster documents have no buttons or MP bindings.
The runtime saves the roster anchor in the per-character UI settings entry
`characters.<id>.panels.party` as logical `{x, y}` coordinates after dragging.
This position is independent of membership and remains intact when viewport
clamping temporarily moves the growing list. It is not a new layout-schema key.
`party_invitation.json` uses shared Dragon nine-slice chrome, a separate inviter
name/body, a divider and stateful accept/decline buttons. Its content starts below
the native frame ornament. `party_actions.json` uses the same frame for Leave,
leader-only Invite, membership count and footer close. The host extends its
172-pixel client height by one 28-pixel `party_action_member.json` row per other
member when the local player is leader; these rows begin at client Y 118. Each
row contains a name and Remove button. The whole dialog fits the viewport,
including its native chrome padding.

Membership controls roster visibility; there is no P/Escape/menu visibility toggle
or selected-player overlay. `hud.menu.party` deliberately invites a selected world
player outside a party, and opens management during membership. Escape closes
management while preserving the roster. The editor previews every document with
shared `party.*` values; no custom widget or canvas kind is added.

The multiplayer guild window (`guild.json`) uses `dragon_dialog` chrome and
the menu bar's `dragon_menu/community.bmp` as its foreground `dialog_icon`.
It uses standard text, input, fill, image and button widgets and follows the
global UI scale, constrained by the available screen size. `GuildWindow` supplies
font-measured single-line labels and two centered result lines inside its footer.
The Search/Create form centers its name field, requirements and action buttons.
It also supplies paged rows, server-authoritative permissions and prices, founding consent, and
rank-option drafts. The Daily reward action on Membership follows the server's
`own.daily_reward_available` flag and stays disabled while a claim is pending.
The `guild.*` bindings/actions are listed in `VARIABLES.md`;
`ui_fill_dummy_preview` supplies synthetic ranking data to the editor. No guild
business rules are stored in the UI JSON.

The selected-player context (`player_options.json`) is a universal Dragon window.
Its host repeats `player_option_row.json`, a frameless HUD strip with a bound button,
for the server's available action list. The host sizes and scales the window to
the row count and screen, using FreeType for all text. No new widget kind is
needed; both documents roundtrip through the editor. Bindings and actions use
`player_options.*` from `VARIABLES.md`.

The Skin Artisan (`skin_shop.json`) uses existing canvas widgets bound to
`skin_shop.mirror` and `skin_shop.catalog`, plus standard buttons, labels and an
image for the ticket medallion. The host renders a detached character preview and
grouped model thumbnails inside the document's canvas bounds. It supplies the
`skin_shop.*` vocabulary documented in `VARIABLES.md`; shared dummy values keep
labels editable without optional models. There is no new widget kind or custom
schema branch.


The `trade`, `trade_invitation` and `trade_quantity` dialogs use standard widgets
with `dragon_dialog` chrome. The trade inventory uses a six-column `icon_grid`
inside a vertical viewport; the two offer grids each use six columns and two
rows. Their `trade.*` bindings, actions and tokens are listed in
[VARIABLES.md](VARIABLES.md#trade-trade_invitation-and-trade_quantity). The host
owns item movement, quantity editing, acceptance eligibility and the traffic lock.


The Trading Post page counter is a centered FreeType text widget bound to
`${trading_post.page}`, displaying the current page and total pages as `1/2`.
The Trading Post (`trading_post.json`) uses standard buttons as dropdown
selectors and repeats `trading_post_option.json` for their choices. Six one-row,
ten-column `icon_grid` widgets pair with bound price-range headings. The
`trading_post_quantity.json` modal reuses the NPC shop's quantity controls. All
three documents use the existing schema; no new widget type or canvas kind is
required. The host owns filter dependencies, paging, numeric editing and the
single-pending-request state. See `VARIABLES.md` for the complete vocabulary.
