# Talent Tree

OpenKal's project-owned talent tree contains 37 nodes on five independent,
ordered paths. The center uses the same class emblem as the identity HUD.
Class Updates is reserved and unavailable until class-specific nodes are defined.
Open the tree through Skills → Talent Tree or Shift+K. It covers the viewport
and captures gameplay input. Apply commits the entire draft; Exit or Escape
discards uncommitted choices. Clicking a pending prerequisite removes its pending
descendants. Committed nodes cannot be removed through this UI.

## Progression and effects

Total points equal character level, including one point on level 1. Existing
characters immediately receive their level's budget. Talent points are separate
from PU and SU. Every node requires the preceding node in its own path. The
server validates known node bits, prerequisites, total cost, the client's previous
selection, and that no committed node is removed. A failed persistence operation
rolls back the selection before bonuses are published. A developer level rebuild
that makes the current build unaffordable clears the build and restores the new
level's budget; normal leveling never clears it.

All specified values add together, including penalties. Percentages are additive
percentage points within an effect; Mana percentage applies after flat Mana
bonuses. The complete paths provide:

- Physical (100 points): Min/Max Physical +80, Defense −50, Explosive Blow power +25%.
- Magical (100 points): Min/Max Magical +80, Mana −50%, Explosive Blow power +25%.
- Defensive (50 points): Defense +85, Mana +225, Max Physical −15,
  Auto HP +20 every 2 seconds.
- Quality of Life (40 points): Auto MP +20 every 2 seconds,
  Auto Pick up to 6 nearby ground items every 5 seconds.
- Upgrades (35 points): movement Speed +25%, Evasion −20, and Beacon.

Attack minima cannot exceed the resulting maxima; Defense and Evasion cannot
fall below zero, and maximum Mana remains at least one. Bonuses participate in
the authoritative combat snapshot and remain independent of equipment, temporary
buffs and the PvE-only Collection modifiers. Explosive Blow increases power,
not activation chance. Regeneration has its own two-second pulse, works during
combat and rest, clamps to maximum vitals and never revives a dead player.

Auto Pick uses the same durable inventory insertion, ownership reservation,
party ownership, map, distance and capacity checks as manual pickup. Its range
is the existing manual pickup radius: 64 server pixels (two tiles). A ground
stack counts as one item. Both pickup nodes are additive: two plus four permits
six ground stacks per pulse. No catch-up burst is performed after a delayed tick.

## Beacon

Beacon is a timed buff, unlocked by the last Upgrades node. Its cooldown is
120 seconds from activation and its active period lasts 30 seconds. The initial
radius is 20 tiles (640 server pixels), specified in the shared protocol header
so client descriptions and server AI agree. It repeatedly attracts eligible idle
combat monsters within the same map and radius while active. A monster engaged
with another player is never reassigned by Beacon. Spawn protection, safe village
areas, monster race rules, death and hiding still apply.

During activity, attracted monsters may pursue beyond their usual detection and
chase range up to the Beacon radius. Expiration restores the normal leash.
Teleporting, death, disconnecting or restarting the server ends the active period.
The cooldown deadline persists in SQLite as Unix milliseconds, so reconnecting
cannot bypass it. Unlocking Beacon adds a rank-one self-buff skill to the Talents
page. Like Archer Chance Capture, it can be equipped on the skill bar and
activated without a target or changing the combat action. Existing shortcuts
remain valid. It can also be activated with the tree's Activate Beacon button.
Its icon and remaining time appear in the existing buff / Skill Info column
while active. Skill icons show the authoritative remaining 120-second cooldown,
including after reconnect. Beacon has no separate sound or particle effect.

## Persistence and wire interface

SQLite migration 26 adds `characters.talent_mask` and
`characters.beacon_ready_at`, both defaulting to zero. It is idempotent even
when an old backup retains a newer schema marker. The character save writes the
mask and cooldown in the same statement as the rest of the record.

`TalentTree` is appended before the existing C2S and S2C `End` sentinels. Existing
packet ordinals stay unchanged. Requests are Snapshot, Apply (expected and desired
64-bit masks), and Beacon. Responses carry status, authoritative mask, level and
remaining cooldown/activity times. Truncated, unknown-command and trailing-byte
payloads are rejected. The client stages choices until the server responds.

Skill indices 240–244 are reserved for five passive branch ranks in path order.
These definitions are owned by OpenKal and remain available without optional
class-skill data. They appear on the Talents page of Skills. The server includes
their committed ranks in ordinary SkillInfo snapshots. Index 205 is reserved for
the equippable Beacon self buff and appears at rank one once unlocked. SU
learning and optional skill catalogs cannot override these indices. The passive tooltips show the cumulative
values for the committed branch rank. Beacon activity is synchronized separately
through TalentTree snapshots, including cancellation and expiration, without
consuming a legacy buff-state bit.

## Artwork and UI

`config/ui/talent_tree.json` uses only standard `openkal.ui/v1` text, fill, button
and icon-slot widgets. Its 1600×900 design scales to fit the viewport and shades
any remaining margins. Hovering a node opens an opaque overlay beside it with
individual bonuses, red penalties, point cost, prerequisite and unlock status.
The overlay wraps long ability descriptions, switches sides near screen edges,
and disappears when the pointer leaves the node. The footer contains controls
and general request feedback; node details are not shown there.
All text uses FreeType. The heading is rasterized in bold at its actual screen
pixel size and drawn at integer coordinates, rather than enlarging a 14px atlas.
The overlay uses native-size font atlases as well. The runtime node icons are
black-and-white until committed; a pending selection has a gold border and
retains its grayscale icon. A committed node has a green border and the initial
colored image. The source atlas, generation prompt and provenance are retained
under `RuntimeData/templates/talents/source/` and
`RuntimeData/templates/talents/README.md`.

## Verification

`talent_tree` verifies costs, prerequisites, cumulative modifiers, wire parsing,
AI targeting/leash behavior, migrations, persistence and scaled draft controls.
`talent_tree_server` uses isolated project-owned fixtures and an actual Unix
connection to verify invalid/stale applies, authoritative stats, passive SkillInfo,
Auto Pick, timed regeneration and reconnect-safe Beacon cooldowns.

The GL `openkal_test_talent_tree` executable accepts a capture-file prefix to
render screenshots at 1600×900, 1280×720 and 1024×768, including physical,
right-edge, bottom-edge and Beacon overlays. Class emblem artwork comes
from the same optional runtime assets as the identity HUD; an asset-free preview
still labels the class in the center.
