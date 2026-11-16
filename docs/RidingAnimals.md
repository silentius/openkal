# Common riding animals

## Implemented scope

OpenKal has a native common-riding system for its own client and server. A
riding item is registered by equipping it from the inventory. The registered
animal can then be mounted or dismounted with `R`; `E` opens the Riding window.

The server owns registration, expiration, mounted state, and movement speed.
Common riding uses an exclusive persisted wear slot (`5`) and an independent
per-item expiration timestamp. The initial lifetime begins at the first
successful registration and is seven days (`604800` seconds), including time
spent offline.

The first catalog contains these conflict-free OpenKal item IDs:

- `30000` White Tiger
- `30001` Fierce Tiger
- `30002` Black Horse
- `30003` Horse
- `30004` Dire Wolf
- `30005` Wolf
- `30006` Dire Bear
- `30007` Bear
- `30008` White Horse
- `30009` Raptor
- `30010` Skeleton Raptor
- `30011` Battle Raptor
- `30012` Battle Skeleton Raptor

Despite their names, the two raptor items are ordinary common mounts. They do
not opt into the Battle Riding system described in
[`BattleRidingPets.md`](BattleRidingPets.md).

Every initial common mount grants `+75%` movement speed while mounted. Combat,
pre-skill/skill use, resting, teleporting, transforming, death, expiration, and
removing the registered item force a dismount. Registering a mount alone grants
no movement bonus.

## Data and assets

`config/riding/catalog.json` is the strict server and client catalog. Each
record maps an item IID-independent definition to a Riding visual key, skeleton
key, clothes part count, saddle bone, fallback rider height, duration, and
movement-speed percentage.

The renderer resolves the Ride asset family below the configured
data root:

```text
Ride/Clothes/A{visualKey:03}_b{part}.gb
Ride/Motion/Ta{bone:03}_Bone.gb
Ride/Motion/Ta{bone:03}_0_A_01.gb
Ride/Motion/Ta{bone:03}_2_A_01.gb
Model/Motion/T{class}_0_A_04.gb
```

The character uses the class-specific mounted idle pose while riding. The
rider is vertically attached to the configured saddle bone. The `riderOffsetY`
value is used only when that bone cannot be evaluated, so asset packs can be
corrected through configuration without changing gameplay code.

The implementation uses OpenKal-native `C2S_RIDING` and `S2C_RIDING` messages.
It deliberately does not emulate an official-client packet layout.

## Manual smoke test

1. Start the client with a data pack containing the `Ride` assets.
2. Give the character a mount, for example `/getitemid 30000 1`.
3. Right-click the item. Its seven-day timer starts and the item becomes the
   registered mount.
4. Press `E`; verify name, registration status, and remaining lifetime.
5. Press `R`; verify the animal and rider appear, riding animation follows
   movement, and movement speed increases.
6. Press `R` again; verify both the visual and speed bonus disappear.
7. Mount again, then attack or use a skill; verify an automatic dismount.
8. Restart the client; verify the same mount remains registered with a reduced
   remaining lifetime and starts unmounted.

## References

- [GameAgit Pet guide](https://gameagit.com/kalonline/Pet)
- [GameAgit Battle Horse guide](https://gameagit.com/kalonline/WarHorse)

Optional mount asset coordinates are runtime-data observations; no external
implementation or asset payload is included.
