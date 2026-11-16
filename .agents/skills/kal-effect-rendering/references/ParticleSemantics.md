# OpenKal particle semantics

This document defines the behavior implemented and tested by OpenKal as an
independent project specification.

## Billboard dimensions

OpenKal treats `size` as billboard height and `size * aspect` as billboard
width. Its unit quad spans `-0.5` to `0.5` on both axes. The default aspect is
`(1, 1)`; animated aspect values may intentionally stretch a particle.

## Runtime fields

- `texture` and `animTexture` preserve quoted path separators. They are data
  paths, not C string literals.
- `shader bumpenvmap` and `heathaze` select the post-processing path and are not
  also drawn as ordinary quads.
- `aspect` interpolates width over particle lifetime.
- `cycles` uses zero for indefinite repetition and a positive value for a finite
  stage.
- `bunching` controls whether particles start together or are distributed over
  stage time. Zero remains a meaningful value.
- `blendFunc` distinguishes additive, additive-blend, and alpha-blend modes.
- `size`, `speed`, `gravity`, and `rotation` apply their envelopes over particle
  lifetime.
- `direction` and `distribution` control direction and the emitter volume.
- `time` and `count` independently control stage duration and density.

## Finite stages

Once a positive cycle limit is exhausted, the stage stops generating and
drawing new particles. A zero-cycle stage continues until its containing effect
is removed. Regression tests should cover both finite and indefinite stages.
