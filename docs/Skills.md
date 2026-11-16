# Skill configuration

OpenKal does not distribute a third-party skill catalog. Runtime skill data is
optional user-supplied configuration and belongs below `config/skills/` in the
directory selected through `OPENKAL_DATA_ROOT`.

The server and client consume JSON definitions. Each definition identifies its
own class and numeric index and may provide rank limits, resource costs, timing,
tooltip parameters, motion identifiers, and effect identifiers. Unsupported or
missing definitions remain unavailable instead of falling back to another file
format or a built-in private table.

To inspect a local data set without copying it into this repository, generate a
standalone inventory:

```bash
python3 tools/gen_skills_doc.py \
  "$HOME/.local/share/openkal-data/config/skills" \
  --output /tmp/openkal-local-skills.md
```

The generator reads only the selected JSON directory. It contains no embedded
third-party class mappings, filenames, identifiers, or source references.

Behavior implemented in C++ must be documented from project-owned tests and
observable requirements. A configured effect or motion name by itself is not
evidence that rendering, timing, sound, or combat behavior is correct.

OpenKal also owns five passive Talent Tree skill definitions at indices 240–244
and the Beacon self buff at index 205, independent of the optional class catalog.
These indices are reserved, learned through committed talent nodes, and
synchronized through SkillInfo. Like Archer Chance Capture, Beacon can be
equipped and activated from the skill bar without a target or selecting a combat
action. Its timed icon appears in the buff / Skill Info column while active. See
[Talent Tree](TalentTree.md) for costs, cumulative effects, timing and persistence.

## Client presentation

[Skill effect assets](SkillEffects.md) lists per-skill preparation/fire mappings,
FX and particle entry names, model/texture files, target attachment, lifetime and
current presentation limits. It documents the OpenKal implementation and keeps
runtime data-pack filenames separate from named container entries.

## Behavior verification

Describe skill behavior as observable inputs, targeting, resource cost, timing,
cancellation and resulting state. Cover those rules with minimal project-owned
synthetic JSON and tests. Verify client motion/effects/sound separately from
server combat. Externally supplied specifications retain their provenance;
unknown behavior remains explicit rather than an undocumented fallback.
