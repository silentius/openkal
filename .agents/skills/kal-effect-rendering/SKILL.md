---
name: kal-effect-rendering
description: Verify OpenKal particle and sprite effects with owned tests, documented asset semantics, timing, and sound.
---

# Kal effect rendering

Use this workflow for particle, sprite, billboard, `CombatFx`, `SpriteFx`, skill
effects, hit effects, or related sound changes. Visual presence alone does not
prove that an effect satisfies its documented behavior.

## Required workflow

1. Resolve the exact effect name spawned by OpenKal. For encoded containers, use
   `tools/skill_effect_view` with an explicit user-supplied data root.
2. Record the script's particles, sound, duration, attachment point, and offset.
3. Read every referenced particle stage.
4. Inventory each stage field and verify that OpenKal both parses and applies it.
5. Derive implementation requirements only from public documentation,
   independently written specifications, lawful black-box observations, and
   project-owned tests whose provenance is recorded.
6. Verify the rendered result on screen: texture choice, proportions, blending,
   motion, density, lifetime, looping, and sound.

Read [references/ParticleSemantics.md](references/ParticleSemantics.md) when a
stage uses aspect envelopes, cycles, bunching, blending, animated textures,
post-processing, or particle kinematics.

## Completion evidence

Report the resolved effect name, referenced stages, parsed fields, runtime use,
texture and blend verification, loop behavior, sound origin, and visual test.
Record the provenance and license of every redistributable test asset.

## Inspecting user-supplied assets

Pass the data directory explicitly so the viewer and application inspect the
same user-supplied files:

```sh
build/bin/tools/skill_effect_view --data /absolute/path/to/data
```

An absent raw-text match in an encoded container is not evidence that an effect
is absent. Use the production decoder and add a project-owned regression fixture
for any decoder behavior changed during the investigation.
