# NPC placement and overhead symbols

NPC definitions live in `config/npcs`. `xy` contains server X, Y and Z. The
debug HUD's `server` position can be copied directly; height is already in server
units. `map` selects the logical map. `dir` is a horizontal look-at point,
rather than the debug HUD's yaw angle. For yaw `a` in radians, a look-at point
is `(x - r * sin(a), y - r * cos(a))`, rounded to integer server pixels.
For the Trading Post Agent, radius 1,000 gives `dir: [232466, 292709]`, which
renders at 176.6 degrees to the debug HUD's one-decimal precision.

An optional `overhead_icon` names a bitmap in the existing UI texture namespace:

```json
{
  "index": 60003,
  "name": "Trading Post Agent",
  "map": 0,
  "xy": [232525, 291711, 45410],
  "dir": [232466, 292709],
  "overhead_icon": "dragon_menu/trading_post_dialog.bmp",
  "name_offset_y": 0,
  "name_offset_pixels": 10
}
```

The client centers the image above the NPC name using the existing projected
head/name anchor and depth handling. Its longest side is 32 pixels at native
UI scale, with a two-pixel gap; aspect ratio and image transparency are kept.
The icon also works for NPCs whose name is hidden. Omitting the field, an empty
string or an unavailable texture draws no symbol. Existing NPC behavior is
unchanged when the field is absent. This is display metadata, not permission
to use a service.

Optional `name_offset_y` overrides the rig name offset for this individual NPC,
in client world units above its head anchor. It moves both name and symbol.
The agent uses zero instead of the rig's five-unit gap; NPCs sharing its shape
keep their own placement. Omitting the field preserves the rig offset.
Optional `name_offset_pixels` then shifts both name and symbol upward in screen
space. It defaults to zero and scales with the UI. The agent uses 10 pixels at
native scale, keeping this adjustment independent of camera distance.

The Trading Post Agent additionally declares `trading_post_collector: true`.
Only that server-side role permits trading-post pickups, after map and distance
checks. Its dialog uses `trading_post_collection` to open the player's private
receipts. The balance image is the project-owned artwork already used
by the menu. Shape 75 refers to existing compatible appearance data; no external
mesh, texture or NPC configuration was imported for this agent.
