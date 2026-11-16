# Castle War

Castle War runs on the authenticated dedicated server with guilds and
PostgreSQL enabled. Copy [castle_war.example.json](../server/castle_war.example.json)
beside the private `server.json`, set `"castle_war_file": "castle_war.json"` in
`server.json`, and name the copy `castle_war.json`. Configure the original
castle's layout using the deployed map, NPC, and monster catalogs, then set
`enabled` to `true` and restart MainSvr. The private event JSON is read from
disk outside the client configuration archive.

The guild catalog in Test Players is also available on servers without Castle
War. The **Start simulation** button requires an enabled layout on the connected
server. The shipped example has placeholder coordinates and definition IDs.

## Runtime castle layout

Positions use `[map, x, y, z]` in server coordinates. The inclusive
`bounds` rectangle is `[min_x, min_y, max_x, max_y]` on the attack camp map.
Place both camps, both gate destinations, and all four flags within it. The
exit must lie outside the rectangle or on another map. Configure the guard,
master, attack camp guide, and territory manager by NPC definition index.
The server uses those definitions' loaded positions and checks interaction
range on every action.

Each flag references a monster definition with the CastleStandard race. Slot
zero is the large flag and the other slots are the small flags. Enable any
territory merchant's purchase-price policy by listing its NPC index in
`tax.merchant_npc_indices`.

## Default schedule and play

The schedule uses the configured fixed UTC offset. The template assumes UTC+9
for the international guide's clock times. Weekly declarations open Friday at
01:00, preparation begins Saturday at 01:00, and admission opens Sunday from
00:50 to 01:00. Battle runs until Sunday 02:00 unless a faction wins early.
The template requires character level 70 and allows five partner guilds plus
the leading guild. Alliances can last 1 to 15 days.

Guild leaders use `/castle invite <leading-guild> <days>` to apply and
`/castle accept <applicant-guild>` to approve an application. Use
`/castle leave` to leave or dissolve an alliance. Alliance changes close at
the end of declaration. The leader of an attacking alliance or standalone guild
registers at the guard. The incumbent guild and its alliance register
automatically for defense. Participants enter through the Master. Select an
enemy and click or press Space for basic attacks. `/castle status` reports the
phase, score, castle owner, and personal Castle Honor.

Flags award 50 points for the large flag and 30 for each small flag.
Capture opens ten minutes into the battle. Controlling all four flags for
five continuous minutes wins early. Otherwise current flag control decides
the winner, with ties going to defense. Within the winning side, flag score,
enemy kills, then average participant level determine the castle lord.

## OpenKal defaults where the source leaves details open

- Auto-revival waits 30 seconds and protection lasts 10 seconds. Flags recover
  100 HP every five seconds and start with 10,000 HP for the large flag and
  5,000 HP for a small flag. The last damaging hit transfers ownership.
- An unoccupied castle starts with neutral flags. An incumbent starts with
  defensive control. The early-win timer starts on a capture. An exact
  alliance tie favors the incumbent, then the lower guild ID.
- The attacking player's buff uses the integer percentage of their kills
  divided by deaths, with no buff at zero or from 190 percent upward. Allied
  skill support, enemy kills, and flag captures contribute to the activity
  threshold. The current Collection system has no Yeoiju PvP bonuses to stack
  with the siege buff.
- The event uses a persistent Castle Honor balance with award weights of
  500 per capture, 15 per kill, and 10 per activity point, subject to the JSON
  caps. Honor-shop redemption and guild treasury payouts are outside the
  implemented economy.
- The common timing and balance settings are in the deployment JSON. The
  template includes an invitation expiry of one day and a five-second interval
  for persisting partial flag damage and scores.

The behavioral references are INIXSOFT's [Castle Siege guide](https://gameagit.com/kalonline/Castle),
[June 2026 flag recovery update](https://gameagit.com/updateView/3085/1), and
[October 2026 Castle Siege update](https://gameagit.com/updateView/3101/1),
consulted November 14, 2026. Implementation and dialogs are original OpenKal
work. The deployment layout uses separately supplied runtime assets.

## Follow-up work for the original castle

- [ ] Identify the visible flagpole in the deployed client and compare its
  base with the center of the intended platform. Verify how the map places
  the static geometry and how the event places the capturable standard.
  Check the standard's position, model, height, and interaction range in
  battle. Align the affected asset or spawn once the source of the offset
  is confirmed.
- [ ] Complete the private layout against the deployed catalogs. Verify the
  battle rectangle, both camps, east and west gate destinations, return exit,
  all four standards, and the NPC definition IDs. Walk the routes in the
  client and verify where each transfer lands.
- [ ] Verify the east and west gate workflows against the reference behavior
  and finish the missing interactions. Cover the camp guide transfers,
  physical gate access, phase and faction restrictions, and resets between
  rounds.
- [ ] Verify the Castle War behavior of the Black Crow statues and implement
  any missing siege interactions. Check their destinations and access rules
  by phase and faction, and verify that ordinary revival still works outside
  the siege.

## End-to-end test on the original castle

Run on an isolated PostgreSQL test world with the deployed original castle
assets and a completed private layout. Record the configuration, build and
observed results for each step.

- [ ] Play one round through ordinary registration and scheduling. Use a
  regular guild leader and partner guild, register the assault through the
  guard, and confirm that the incumbent's alliance defends. Exercise the
  alliance limit, registration cutoff, minimum level and closed admission.
- [ ] Play a separate round through the Test Players tool. Select a regular
  guild as the attack leader and add a managed guild as a partner. Verify
  automatic and manual admission with shortened timers, correct camp
  placement, and eligibility for characters in each alliance.
- [ ] Walk the castle routes with characters from both sides. Check both
  gates and the Black Crow interactions after their workflows are complete.
  Verify that the platform stairs remain traversable and that players cannot
  enter the solid platform after descending.
- [ ] Confirm that the four standards appear at the configured positions and
  can be captured after the unlock. Verify score changes, contested control,
  death and revival, and reconnect during the battle. Exercise both an
  early victory and a battle that runs to its deadline, including a tie.
- [ ] Verify the winning guild, Castle Honor and participant return. Restart
  the server to confirm regular siege results persist. Restore the regular
  schedule after a simulation and verify that the previous owner, Honor and
  schedule are restored, including a participant who reconnects afterward.
