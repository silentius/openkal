# Parties

Parties contain at most **12 players in total**, including the leader. The
150-pixel-wide roster appears automatically during membership and disappears
when the party dissolves or the player leaves. P, Escape and the menu button do
not hide it. Drag the list to move it. Actual members add 28-pixel names/HP rows;
there are no empty slots, mana bars, numeric HP labels or management controls.
The leader is marked with an asterisk. Mana is not included in party snapshots.

Dragging saves the list's position as soon as the mouse is released. The anchor
is stored per character in `ui_settings.json`, under
`characters.<id>.panels.party`, in logical HUD coordinates. Restarting, leaving
or reforming a party preserves it, and UI scaling is applied when restoring it.
Viewport clamping keeps a growing list visible without overwriting the preferred
anchor. Older settings without this entry retain the default right-side position.

The thin bronze title strip uses gold TrueType text. The enclosing border uses
the exact `chatting-box-{top,bottom,left,right}.bmp` textures and three-pixel
border widget used by chat. HP bars use a recessed bronze edge, a dark remaining
track and a cropped red vertical gradient with a fine upper highlight.

Select a player in the world, then deliberately click the **Party** menu button
to invite that player. Merely selecting a character never shows an Invite overlay.
During membership, the **Party** menu button opens a separate management dialog.
Every member can **Leave party** there. The leader can invite the selected player
and use **Remove** beside another member's name to kick that member. The own row
is excluded from removal. Escape or the footer close button closes management
while keeping the roster visible. Permissions and rows track each server snapshot.
Both dialogs use the shared Dragon frame, warm text and stateful bronze buttons.
The menu tooltip reads **Party actions / invite selected player**. `/party Name`
also works without a distance limit. Recipients have 30 seconds to accept or decline in a
separate prompt. Only the leader may invite players once a party exists; the
server also checks leader permissions for membership and distribution commands.
Invitations do not reserve slots: capacity is checked again on acceptance.
Replaced, expired and replayed answers cannot join a party.

`/secedeparty` leaves the party. If the leader leaves or disconnects, the earliest
remaining member becomes leader. A party with only one member dissolves. Parties
and invitations are session state; they are not restored after disconnecting.
Party chat uses the existing `#` channel.

## Rewards

The server retains hostility-based attribution between competing parties and solo
players. A contributor's EXP share is divided equally among living, online party
members on the monster's map, within **800 world pixels (25 tiles)** of the kill.
Integer remainders are retained; each recipient's level adjustment and existing
personal EXP modifiers are applied afterwards. No additional party EXP bonus is
introduced. Solo attribution remains unchanged.

Protected monster drops can be picked up by members of the owning party. Geon is
split equally among living members within the same 800-pixel radius of the drop,
regardless of item-distribution mode; integer remainders are retained. Personal
quest drops and player-dropped items are excluded from party distribution.

Each recipient sees a white **You acquired N Geon.** battle-feed line for their
own credited share, including non-pickers. Geon awards are positive money deltas;
initial login balances and other absolute balance corrections stay silent.
Actual pickups are visible immediately after entry, without the item-restoration
grace period. Solo pickups use the same visible-gain path.

After a successful non-Geon ground-item pickup, the other online party members
see a dark-green **Name acquired Item xN.** battle-feed line. The actual recipient
keeps the existing bright-green own-item line. These notices use the existing
ItemGet filter and identify the awarded member in both item modes. They reach
party members on other maps or outside sharing range as well; the information
alone does not grant a reward. Outsiders receive no notice. Failed or replayed
pickups, Geon, bag restoration and purchases do not generate these notices.
The independent `S2C::UpdateParty` item payload contains party/recipient IDs,
recipient name, item index and awarded count; it does not replace roster snapshots.

Item modes:

- **Random** (default): eligible nearby members receive whole item drops in
  shuffled rounds. Every member receives one drop before a new round starts,
  independently of who picks up the drops. With two equally eligible members,
  every two drops give one to each (50/50); with an odd number, their totals
  differ by at most one. This balances the number of drops, not their value or
  the number of units in a stack. Each new round shuffles the order again.
  If the assigned member's bag is full, the item remains on the ground and
  retains its recipient while that member remains eligible. Failed pickups do
  not advance the round. Membership, eligibility-set or distribution-mode
  changes restart the round; absent members do not accumulate future turns.
- **Free for All**: explicitly selecting this mode gives the item to its picker.

The server validates pickup range, map, membership and drop protection. Repeated
pickup requests cannot award the same ground item twice. Dead, teleporting,
out-of-range and other-map members do not receive shared rewards.

## Options

**Options → Misc → Ignore party invitations** blocks new requests on the server
and clears any pending invitation. Save applies and stores the preference in the
active client profile (`client.misc.ignore_party_requests`). Cancel discards the
draft. Changing the graphics preset preserves this preference.

## Compatibility and provenance

The public [official Party guide](https://kalonline.gameagit.com/kalonline/Party)
(INIXSOFT/GameAgit, consulted 2026-10-22) describes invitations by mouse or
`/party`, the P shortcut, leaving, proximity-based sharing, and Free for All and
Random item modes. It is used only as a behavioral reference; no external code,
assets or documentation excerpts are imported or redistributed. There is no
claimed source-code reuse license. Implementation and synthetic tests are
independently written for OpenKal.

The twelve-member capacity, sharing radius, equal EXP formula, remainder handling,
invitation timeout, shuffled item-sharing rounds and full-bag policy are OpenKal
rules. The equal-per-round algorithm supplies the default 50/50 split for two
eligible members; the cited guide does not specify this algorithm. The project-owned
packet payloads use reserved `C2S::AskParty`
and `S2C::PartyInfo` opcodes; they do not claim binary compatibility with an
external client/server implementation.

The user-supplied [LucYda screenshot](https://www.lucyda.de/wp-content/uploads/2012/02/kalonline-monster-luren.jpg)
(LucYda, viewed 2026-10-22) is a visual reference for the compact names/HP list.
No image pixels, artwork or other assets from it are included in OpenKal.

The three `party_*.svg` textures under
`RuntimeData/templates/dragon_hud/source` and corresponding BMP exports are
project-owned vector artwork by OpenKal contributors (2026-10-22,
GPL-3.0-or-later). They contain no copied external pixels. The chat frame is
reused by its existing runtime texture names without importing another asset pack.

## Validation

`party_service` checks membership, permissions, invitation expiry/replay, ignored
requests, concurrent accepts at capacity, disconnects, succession, malformed
packets, exact 50/50 sharing across 100 awards, twelve-member sharing rounds and
retry/eligibility handling. `party_server` uses three real clients, a synthetic map and guaranteed
loot to verify shared membership, EXP for a non-attacker, Geon conservation,
protected loot, exactly-once item notices (excluding Geon, self and outsiders),
leader kicks, explicit leaving and default 50/50 item distribution even when the
same member picks up both drops. It also checks silent initial money restoration,
personal Geon receipts for party shares and solo pickups, no receipt for distant
members, and exactly-once credits and receipts on repeated pickups.
`battle_feed_hud` exercises Misc
save/cancel behavior and universal-layout editor roundtrips. Its `--capture`
mode renders the empty, two-member and twelve-member lists, the separate
invitation prompt, leader/member management and all option tabs. Input checks
cover persistent membership visibility, list dragging, absence of selection
overlays, independent invitation input, leaving, targeted kicks and lost leader
permissions. Rows contain neither
buttons nor mana bindings.
