# Community EXP event noticeboard

The noticeboard is an procedural 3D asset. `ExpEventBoard` builds its
wooden frame, bronze hardware, stone feet, canopy and medallions in C++ and
creates its material textures mathematically. It does not load a legacy sign
model, texture or script. Both faces use FreeType text rendered into a 1024 x 640
texture, with normal world depth testing and perspective. The 1024 x 512 material
atlases add directional wood grain and knots, aged bronze, canopy tile joints and
rough stone. Beam UVs align the grain with the long axis. A separate cached slate
background adds subtle edge weathering beneath the live text; it is generated
once, not on every countdown update.

## In the game

The OpenKal repository owns the event configuration, coin item and icon,
manager NPC (`config/npcs/5037_exp_event_manager.json`), dialog
(`config/dialogs/000690.json`) and quest steps `9500/1`, `9501/1`, `9502/1`
under `config/quests/`. These project-owned event files remain in this
repository instead of the optional user-data directory.
The package builder combines them with the shared imported game data, including
monster drop tables. The manager still uses the existing external NPC model;
the procedural noticeboard is independent of legacy sign assets.

The client places the board beside the currently visible event manager received
from the server, using the configured `managerNpcIndex`. The authored model is
scaled uniformly by 0.1 for the game: about 27 world units high, compared with the
18-unit fallback humanoid NPC height. It stands 18 world units along the manager's
local side axis and uses the local NPC catalog's yaw when available. The standalone
viewer retains the authored model units. The ground height comes from the existing
floor query. It is an
additional prop; the existing map assets are not replaced or removed. Placement
and clearance beside the scenery still need a playtest in Temporary
Fort. The board currently has no click action or separate collision volume.

The English display shows contributed items, items still needed, the configured
goal and reward, and the donation item's name. When active, it shows the EXP
bonus, remaining time and progress towards the next upgrade or extension. `contributed` means
unspent progress towards the next 500-coin step, not lifetime donations. The current item is Exp Event
Coin, although other configured item names are supported.

Quest `9501/1` donates all coins in the bag. Quest `9502/1` donates exactly
100 coins, including across multiple stacks; fewer than 100 coins rejects that
option without taking any. Both options work while the event is active.

The first 500 coins start +10% EXP for 60 minutes. Each additional complete batch
of 500 during the event adds 10 percentage points, capped at 50%, and extends the
existing expiry by 15 minutes. At 50%, every further batch still adds 15 minutes.
Large donations apply every complete batch immediately. For example, 3,000 coins
from an empty event produce +50% EXP for 135 minutes. Partial progress is shared
across players and persists through expiry and restart. An expired event starts
again at +10%; its old bonus does not carry over.

The noticeboard shows the next bonus and funding progress while active. At the
bonus cap it labels the progress as an event extension. Funded legacy credit is
applied on restore or the next server tick without another donation. Active
bonuses and expiry timestamps are preserved across restart; applying existing
credit never resets the clock of an active event.

`S2C::ExpEventState` is an appended OpenKal protocol extension. The server sends a
snapshot on login, then broadcasts when the state changes (including countdown
seconds and expiry). No client query or NPC interaction is needed to refresh the
board. The client discards the snapshot on disconnect. Both the server and client
must include this extension; an older server does not provide board data.

Geometry and textures are created lazily near the manager. The surface is rebuilt
only when its snapshot or item name changes, before the world render pass. The
board owns and releases its GPU resources. Its UVs account locally for the
existing mirrored world camera; the renderer's global camera and culling rules
are unchanged.

## Standalone preview

With `OPENKAL_ENGINE=gl` and `OPENKAL_BUILD_TOOLS=ON`:

```sh
cmake --build build-ninja --target event_board_view openkal_client
./build-ninja/bin/tools/event_board_view
```

Drag to orbit; scroll to zoom. Press **1** for sample collection progress, **2**
for an active event, **3** for an empty round, or **Escape** to close. Preview
values are synthetic and never write to the game's save data.

```sh
./build-ninja/bin/tools/event_board_view --capture /tmp/board.ppm
./build-ninja/bin/tools/event_board_view --active --capture /tmp/board-active.ppm
./build-ninja/bin/tools/event_board_view --back --capture /tmp/board-back.ppm
```

The captures use the same model, materials, FreeType surface and GL renderer as
the client. The viewer requires a local graphics display.

## Verification

Build the client, preview and relevant tests, then run:

```sh
ctest --test-dir build-ninja -R '^(protocol|server_event|server|quest)$' --output-on-failure
```

Coverage includes 64-bit counters, malformed payload rejection, restored event
progress, the 499/500 threshold, bulk donations, the 50% cap, extensions, restart, expiry,
and actual socket delivery of both donation options with persisted inventory checks. The
`quest` test loads the combined archive and checks the manager's quest chain.
