# Synthetic area-map fixture

`n_000_000.akarea` was created from an empty `AreaCell` for OpenKal on
2026-10-29 by OpenKal contributors with Codex. All coordinates, zone placements
and collision patterns were designed for these tests. No game map, external
asset pack or previously imported fixture is read or used by the generator.
The fixture and its generator are distributed under GPL-3.0-or-later; see
the repository's [LICENSE](../../../LICENSE).

The file uses format version 2 and embedded cell coordinates `(0, 0)`. It
contains a full 256-by-256 attribute grid and two independent four-by-four
subcell mask layers per tile: Blocked and NotMoveable.

## Test patterns

- Tiles `(1, 1)` through `(6, 1)` each hold one zone flag: Portal, Village,
  Safety, AttackCastle, DefenceCastle and Object, in that order. Tile `(7, 1)`
  combines Portal, Village and Safety.
- Tile `(2, 4)` has only a full Blocked mask; `(3, 4)` only a full
  NotMoveable mask. Tile `(5, 4)` fills both layers.
- Tile `(4, 4)` has a Blocked diagonal (`0x8421`) and an independent
  NotMoveable anti-diagonal (`0x1248`).
- Tiles `(16, 8)` through `(31, 8)` isolate each of the sixteen Blocked bits.
  The corresponding tiles in row 9 isolate each NotMoveable bit.
- The rectangle `64 <= x < 96, 64 <= y < 68` is fully Blocked. The adjacent
  rectangle `96 <= x < 128` over the same rows is fully NotMoveable. These
  rectangles also exercise repeated runs in the serialized layers.
- The final tile `(255, 255)` combines Portal and Object, with distinct
  partial masks `0x8001` and `0x0180` in the two collision layers.
- All other data starts at zero. Setting a nonzero NotMoveable mask also
  sets the tile-level NotMoveable attribute through the existing API.

Each collision layer has 130 full tiles, 18 partial tiles and 2102 set
subcells. The layers overlap in sixteen subcells. Exactly 148 tiles have
the NotMoveable attribute.

## Regeneration and verification

The generator is `tests/map/GenerateAreaFixture.cpp`. It writes exclusively
through the existing `AreaMapCodec::save` implementation:

```sh
cmake -S . -B build-null -DOPENKAL_ENGINE=null
cmake --build build-null --target openkal_generate_area_fixture openkal_test_area_map_fixture
build-null/tests/openkal_generate_area_fixture tests/map/fixtures/n_000_000.akarea
ctest --test-dir build-null -R '^area_map_fixture$' --output-on-failure
```

`area_map_fixture` runs without `OPENKAL_DATA_ROOT`. It checks layer
independence, every subcell bit, zone flags, full-cell populations and a
byte-identical write/read roundtrip. It is separate from the optional
catalog-based `map` integration test. Normal builds read the committed
fixture; regeneration is an explicit authoring step.
