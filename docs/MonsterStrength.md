# Monster strength

`config/server/monster_strength.json` controls server-side monster stats. Both
MainSvr and the embedded single-player server read it at startup. Changes require
a server restart. A loose configuration root must contain this relative path;
archive-based installations must rebuild/deploy `config.pak` after editing the
source configuration. A missing optional file leaves monster stats unchanged;
an explicitly configured unreadable file or an invalid configuration fails startup
and logs the error.

The shipped template contains levels 1–255 with every supported option set to
zero. Additional positive integer level keys are supported. Missing level entries,
percentages, and stat offsets are neutral. The monster JSON files are
never rewritten. Player stats and PvP calculations do not use these modifiers.

For each supported authored numeric stat (or the monster loader's default when omitted):

1. Apply `globalPercent` and round to the nearest integer, with `.5` rounding up.
2. Apply that monster level's `percent` to the result and round again.
3. Add that level's signed integer `stats` offset for the individual stat.

Percentages are changes, so `10.0` adds 10%, `-10.0` subtracts 10%, and `0.0`
leaves the value unchanged. Values below `-100.0`, non-finite numbers, unknown
options, and non-integer offsets are rejected. Results saturate at the signed
32-bit maximum and cannot become negative.

This example applies global and level-15 adjustments (other level entries may be retained):

```json
{
  "schema": "openkal.monster_strength/v1",
  "globalPercent": 10.0,
  "levels": {
    "15": {
      "percent": 10.0,
      "stats": {
        "hth": -1,
        "dex": -1
      }
    }
  }
}
```

A level-15 monster with HTH 10 and DEX 20 becomes HTH 11 / DEX 22 after the
global step, HTH 12 / DEX 24 after the level step, and HTH 11 / DEX 23 after the
offsets. The two percentage steps are sequential, rather than added together.

Supported scalar offsets match the monster JSON `stats` keys: `str`, `hth`,
`int`, `wis`, `dex`, `hp`, `hit`, `dodge`, and `absorb`.
The global and level percentages apply only to these nine stats.

`sight`, `mspeed`, `range`, `attack`, `magic`, `exp`, `resist`, `aspeed`, and
`defense` cannot be configured here and are excluded from global and level percentage adjustments.
Their authored values are loaded unchanged. Supplying these keys in a level's
`stats` object is rejected.

Modifiers run before the existing monster combat derivation. Thus HTH still
contributes to HP, DEX to accuracy/evasion, and other primary stats to damage and
resistances. This existing derivation still applies; excluding authored `attack`,
`magic`, and `resist` from direct adjustments does not freeze the final combat
values derived from modified primary stats. The final derived HP or damage need
not change by exactly the configured percentage. The `hp` offset adjusts authored base HP, before HTH/level bonuses.

Monster identifiers, names, level, race, AI selectors, attack type, skill IDs and
ranks, drop/quest tables, and spawn definitions are preserved. Modifiers are used
once when loading definitions; initial spawns, later spawns, and respawns use the
same resulting definitions without accumulating modifiers.

The configuration and arithmetic are OpenKal behavior integrated with
the monster JSON loader.
