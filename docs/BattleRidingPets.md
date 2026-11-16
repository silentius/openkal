# Battle riding animals

Battle Riding extends [common riding](RidingAnimals.md) with per-IID grade,
abilities, satiety, five equipment slots, mounted effects, skill restrictions,
combination, enchanting and upgrading. **E** opens the FreeType Riding panel.
Acquisition and deterministic Seal upgrades are described in
[Riding Cache and Stablemaster](BattleRidingCachePlan.md).

## Identity, registration and lifetime

A battle animal is a mount, distinct from a following pet. Item index identifies
its definition; IID identifies the owned instance. Registration, selection and
being mounted are separate states. Grade, abilities, satiety and gear belong to
the IID rather than the shared definition.

Initial common/battle lifetimes are seven days (`604800` seconds), beginning at
first successful registration and continuing offline or while unselected.
Expiration permanently removes the mount and its progression. Removing/expiring
a registered mount clears riding state and replicates the change to observers.
Stablemaster promotion starts the new Battle lifetime; Time Writs extend it under
the configured cap.

OpenKal uses its own client/server protocol and JSON runtime configuration.
External client packet compatibility and legacy text fallbacks are outside scope.

Reserved item ranges are 30000–30099 common mounts, 30100–30199 Battle mounts,
30200–30299 equipment and 30300–30399 Battle materials. The Cache/Seal/Writ
allocations are separately 41321–41323. Initial Battle animals are:

- 30100 Assault Captain's Battle Horse.
- 30101 Brave Captain's Battle Horse.
- 30102 Wild Battle Horse.

IDs fit the existing unsigned 16-bit item fields. Static indices, unique IIDs
and visual identities must remain distinct. Duplicate indices and missing item
references are invalid.

## Progression rules

Grades are Normal, Uncommon, Rare and Epic, using White, Green, Blue and Purple.
Initial ability slots are 1, 1, 2 and 2, with six unique abilities maximum. The
Golden Dragon Egg unlocks or rerolls abilities; the current assignment still
needs the random-roll/effect work below.

The older combination path has 100%, 70% and 40% chances for its three grade-ups.
The Stablemaster's deterministic Seal upgrades are a separate path. Mount grade
and equipment enchant/grade must not be confused.

Satiety starts at 100, loses one per successful skill hit and scales mounted
gear effects linearly. Zero satiety does not force a dismount. Gear has helmet,
armor, saddle, saddle-cloth and magnet slots. Enchants reach +10; grade-2-or-higher
gear may be destroyed on critical failure. A +10 item can attempt a 50% equipment
grade upgrade; failure consumes materials only.

Initial mounts use the common catalog's +75% movement speed. Mount, ability and
gear effects apply while riding, not merely while owning or registering the
mount. Dismount removes mount-only effects through combat recalculation. Common
animals dismount on combat; Battle animals consult grade-specific action/skill
restrictions. Death, expiry, invalid maps and item removal force dismount.

Collection unlocks are intended as permanent account history, independent of
timed mounts. Ownership tracking and bonuses remain unimplemented.

## Configuration and persistence

`config/riding/catalog.json` supplies common/Battle item and visual records;
`manifest.json` currently lists that catalog. Intended Battle rules are recorded
in `battle/grades.json`, `abilities.json`, `equipment.json` and `restrictions.json`.
Several rules still use matching C++ defaults rather than loading these files;
editing those Battle JSON files alone does not guarantee a gameplay change.
Cache and Stablemaster rules have their own active loaders.

Persistence records registration/expiry, per-IID grade/revision, satiety,
abilities and five equipment IIDs. Requests refer to owned IIDs and the server
validates outcomes/materials. Durable inventory and progression changes need
transactional retry safety; the current mutation paths are not fully hardened.

The native riding protocol synchronizes registration, expiration, local and
observer mounted state, Battle summaries and mutation requests. Existing common
riding transport, persistence and presentation are shared rather than forked.

## Presentation limits

The mount uses the existing actor/rig cache and configured saddle-bone rider
attachment. Available assets have no separate Battle Horse equipment meshes or
bone mapping. Gear appears in the Riding panel and affects server stats but is
not layered onto the horse; missing gear art never hides the base mount.

Mounted combat animation synchronization and remote equipment visuals still
need suitable assets and implementation. Riding work preserves terrain,
camera mirroring, global culling and unrelated player rendering.

## Remaining work and verification

- Load the remaining Battle JSON rules instead of matching C++ defaults.
- Replace deterministic placeholder abilities with validated unique server-side
  rolls, initialize grade-specific slots, apply effects and show six slot values.
- Make combine/feed/enchant/upgrade and Stablemaster mutations atomic across
  inventory and persistent progression, including retries and disconnects.
- Provide stable result codes and feedback for every rejected mutation.
- Finish fishing/drop restrictions, collection history and mounted combat visuals.
- Cover every panel action, grades/skill restrictions, PvP/towns, observer arrival,
  equipment changes, forced dismount and removal of mount-only effects.
- Verify expiration across logout/restart, deletion, catalog changes, satiety,
  gear destruction and concurrent mutation requests with synthetic fixtures.

The public [Battle Horse guide](https://gameagit.com/kalonline/WarHorse) is a
behavioral reference. OpenKal's rules and packets are independent; optional
model/configuration observations are data references, not implementation sources.
