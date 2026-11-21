# Player-versus-player combat

In multiplayer, select an eligible opponent and click or press Space to attack.
During Castle War and duels, the local player's side has blue nameplates and
the opposing side has red nameplates. The server sends each client the
allegiance and attack eligibility of visible players.

## Assassin Mask

Buy an Assassin Mask from the Mask Merchant (NPC 213) in Temporary Fort
for 100,000 Geon. The shop stocks separate masks for Knights, Archers,
Magicians and Thieves. From level 30, double-click the mask for your class in
the inventory or drag it into the face slot to equip it.
The mask replaces the public character name with `Assassin` in red and hides
their guild name and title. Nearby players see
`The assassin is coming up close` on screen and in chat when an Assassin first
comes within 512 server pixels. The mask stays equipped through logout and
prevents voluntary removal and teleportation. Death consumes the mask.

Assassins can fight any other player outside protected areas, including other
Assassins. Both players must be outside protection. Village and Safety tiles
on the outdoor map automatically protect players. For a deployment where these
flags do not cover the full starter route from Narootuh through Jook-Suh Cargo
Station and Geum-Oh Mine to the Pub of the Giant Bird, add inclusive
`[map_id, min_x, min_y, max_x, max_y]` rectangles in server pixels to
`assassin_safe_areas` in the private `server.json`. The four settlement
destinations are known but the route boundaries depend on the deployed maps.

A player defeated by an Assassin loses 5% of the EXP required for the current
level on village revival. A defeated Assassin loses 15%. Each loss is capped at
the EXP currently accumulated toward the next level. When another player
defeats an Assassin, there is a 50% chance one randomly selected eligible
inventory item or full stack drops where the Assassin fell. Equipped, bound
and quest items are ineligible. The ground item has no pickup priority and
goes to the first nearby player who successfully picks it up.

### Assassin List

Interact with a Town Board named Assassin List to read the 50 most recent
PvP defeats of masked Assassins, newest first. Each entry shows the defeated
Assassin's real character name and the name of the player who landed the
fatal hit. The list persists across server restarts.

## Duels

Select a nearby player, press O to open Player Options, and choose
**Challenge to Duel**. The recipient has 30 seconds to select the inviter and
choose **Accept Duel** or **Decline Duel** from the same menu.
The chat commands `/duel <character name>`, `/duel accept`, and `/duel decline`
offer the same actions. Either participant can end the duel with `/surrender`.
Duels can begin in towns and starter areas. The defeated player plays the death
animation through the collapse, then stands up at the fight location with full
HP and keeps their EXP and inventory.

The 50% drop chance and Assassin List rules come from the public guide,
consulted November 21, 2026. [INIXSOFT's PvP guide](https://gameagit.com/kalonline/Pvp)

The four class mask definitions and their shared icon are OpenKal assets.
The icon at `RuntimeData/templates/assassinMask.png` was generated with
OpenAI ImageGen on November 21, 2026. The prompt specified an original
front-facing dark burgundy leather mask on a transparent background, with
bronze rivets, ivory highlights and a readable silhouette at inventory size.
