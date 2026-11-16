# Riding Cache and Stablemaster

Riding Cache opening and Stablemaster progression are implemented. They extend
[common riding](RidingAnimals.md) and [battle riding](BattleRidingPets.md).
The active rules are in `config/riding/cache.json` and `progression.json`;
changing source JSON requires rebuilding the archive and restarting the server.

## Cache rewards

- **41321 Riding Cache:** consumes one cache and awards both a guaranteed Seal
  and one additional configured reward.
- **41322 Riding Seal:** stackable acquisition/progression currency.
- **41323 Stable Time Writ:** extends an eligible registered timed mount.

`openkal.riding-cache/v1` configures the cache/seal IDs, guaranteed `sealCount`
(currently 1) and weighted reward rows. The additional reward is selected on the
server with a roll from 1 through 1000. Current rewards include the common mounts,
three Normal-grade Battle Horses, Feed, equipment enchant/upgrade stones and
Time Writs. Exact probabilities and counts belong in the JSON reward table.

Opening requires a living player and an owned, unequipped cache. The server
stages cache consumption and both outputs before publishing the bag change.
Insufficient space awards nothing and leaves the cache intact. A cache's consumed
cell can contribute output space. The client receives a separate
`RidingCacheResult` identifying both rewards.

## Stablemaster services

Stablemaster Edmund uses NPC index **1234**. Requests identify the actual NPC
entity and the selected owned mount IID. The server checks the NPC's identity,
map and conversation distance; merely opening a client window grants no access.

The `openkal.riding-progression/v1` defaults are:

- Exchange **2 Seals** for Horse **30003**.
- Promote registered Horse 30003 for **6 Seals** into Battle Horse 30100,
  30101 or 30102, weighted **33.3%, 33.3%, 33.4%**. Promotion retains the IID,
  binds it to the account, starts the result's configured lifetime and initializes
  Battle Riding state at Normal grade. It dismounts before changing the mount.
- Upgrade a registered Battle mount deterministically for **5, 10, 20 Seals**,
  moving Normal → Uncommon → Rare → Epic. The base mount survives. This service
  is separate from the older chance-based Battle Riding combination operation.
- Convert an unwanted unregistered, unequipped and unbound mount into **2 Seals**.
  The registered mount and account/character-bound instances are rejected.
- Consume a Time Writ to add **7 days** to a registered mount, provided it is
  still active and the full extension fits within **30 days remaining**.
  The extension preserves progression. Expired mounts cannot be revived.

Only configured promotion sources are eligible; other common animals do not
implicitly acquire combat capability. The current progression catalog supports
Horse 30003 rather than every visual species. Future species mappings must
preserve the chosen species and define their combat budget explicitly.

## Remaining work

Cache and Stablemaster inventory changes are staged in memory, but persistence
still uses individual row writes. Database failure, disconnect and retry safety
need a single transaction spanning currency/items, mount expiration and per-IID
progression. The existing operation-ID suppression is not a substitute for
persistent idempotency. Grade upgrades currently clear equipped mount-gear
references; preservation/return behavior needs explicit coverage before treating
that path as production-safe.

Balance cache acquisition against intended first-mount playtime, promotion and
grade costs. Direct mounts remain jackpots while guaranteed Seals provide
progress. Drop sources and species expansion need explicit configuration and
playtesting. Permanent account collection history is separate from timed mount
ownership and never grants mount-only effects while dismounted.

Future species can distribute the same combat budget differently: balanced
horses, offensive raptors, mobile wolves/tigers, durable bears and defensive
turtles. Normal turtle appearances are travel mounts unless an eligible Battle
mapping exists. No turtle-to-horse substitution is implicit.

The `riding_cache_catalog` test covers reward-table validation. Progression
verification also needs end-to-end costs, both cache outputs, full bags, owned-IID
checks, expiry/cap boundaries, replay and persistence rollback. Synthetic tests
must remain independent of external icons or mount assets.

## Resources and references

The Cache, Seal and Writ use project runtime templates (`ridingCache`,
`ridingSeal`, `stableTimeWrit`). Existing mount/equipment icons and rigs remain
optional compatible data; validate them against the active client data root.

Public behavior references: [Riding](https://gameagit.com/kalonline/riding),
[Battle Horse](https://gameagit.com/kalonline/WarHorse) and
[Hero Emblem fragments](https://gameagit.com/kalonline/Hero_Emblem).
OpenKal costs, allocations and progression rules are maintained independently.
