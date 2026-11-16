# Quest audio

These three cues were synthesized for OpenKal on 2026-10-22 with the current
soft struck-key timbre, note patterns and timing for in-game evaluation:

- `quest_accept.mp3`: two ascending notes, A5 to E6; approximately 1.17 seconds.
- `quest_update.mp3`: one E6 note; approximately 1.70 seconds.
- `quest_done.mp3`: two descending notes, E6 to A5; approximately 1.47 seconds.

## Rights and provenance of the bundled files

The three MP3 files listed above are project-owned synthetic recordings created for
OpenKal with Codex on 2026-10-22. They were generated from mathematical
oscillators, envelopes and a synthetic room response, not downloaded or sampled
from a third-party recording. No external sound-asset author or licensor supplied
any audio incorporated into these files, and there is no third-party sample or
sound-library attribution associated with them.

These files are project-owned assets distributed under **GPL-3.0-or-later**, as
specified in the repository's [LICENSE](../../LICENSE).

## Synthesis and playback

The sounds use additive synthesis: sinusoidal bodies, short harmonic
and tine transients, and a procedurally generated room response. They were
created with Codex for this project, without recorded samples, sound libraries,
external instruments or audio model outputs. Supplied reference recordings
informed only broad timbre and envelope characteristics; no reference audio is
included in these files. These project-owned assets are distributed under the
repository's GPL-3.0-or-later license.

The files are stereo 48 kHz MP3 at 256 kbps, approximately -21 LUFS. Encoder
padding may slightly increase the reported container durations. They are
included in `config.pak` by default and take precedence over optional external
and client-data cues with the same stem. Other source-tree audio is excluded.

The acceptance cue plays after a server-confirmed transition from an inactive
quest to an active quest. Loading the initial quest snapshot is silent, and
acceptance takes precedence over the update cue in the same client frame.
