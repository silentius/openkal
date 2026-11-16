# Runtime data

This directory contains resources created for and distributed by OpenKal.
Synthesized quest audio is included from [`sounds/`](sounds/README.md).
Third-party audio is not part of the source or binary distribution.

## Image artwork provenance

The repository images were created for OpenKal using AI. The
[image provenance inventory](../docs/ImageProvenance.md) records the SHA-256
hashes, source chats, selected generation outputs, editable sources, export
steps and verification limits for every tracked image. Recovered generation
prompts are retained alongside it.

### Project artwork records

The current image inventory covers `RuntimeData/` and `config/ui/`, including
the logo, item templates, UI textures and runtime sprites. It identifies retained
generated outputs, vectors, procedural generators and exported
derivatives. Evidence kinds and limitations are recorded per file; no blanket
authorship assertion replaces those records.

Some retained generation records identify visual references. Generated origin
does not establish the absence of reference images or license those references.
The inventory preserves the available evidence without adding external inputs.

### Retained files and processing

Many small image files are the retained outputs of that workflow. Complete
generation sessions, large source sheets and exact prompts were not retained
for every image. No missing per-image prompt, generation date or model
assignment is inferred here.

Project scripts crop, resize, recolor and export artwork into the formats used
by the client. Existing per-family README files, prompts, vector sources and
procedural generators document the steps that remain available. Edits using
other OpenKal images use the same project-created artwork as their inputs.

### Distribution

The retained project artwork is distributed under the repository's
[GPL-3.0-or-later license](../LICENSE). Per-file origin evidence and any recorded
reference inputs are documented in the inventory. Contributions undergo the
origin and redistribution review described in `CONTRIBUTING.md`.

This statement applies to images distributed in this source repository.
Optional user-supplied game data is separate and is not covered by this
provenance statement or distributed by OpenKal.

### Authoring sources

Sheets and prompts live in `templates/<family>/source/` beside their
exported runtime templates. These source directories are excluded from icon
conversion and `config.pak`. The talent atlas and its prompt are retained in
[`templates/talents/source/`](templates/talents/source/).

## Medicine and magic potion sizes

Normal Medicine uses definition indices 23000 (Small, 100 HP), 23001 (Medium,
300 HP), 23002 (Large, 500 HP) and 23003 (Huge, 800 HP). Definition filenames,
display names and the `medicineSmall` through `medicineHuge` image keys follow
the same sizes.

Magic Potions use 23004 (Small, 300 MP), 23005 (Medium, 500 MP), 23006 (Large,
800 MP) and 23007 (Huge, 1200 MP), with `magicPotionSmall` through
`magicPotionHuge` image keys. The Large and Huge definitions were created for
OpenKal on 2026-10-18 with the following recovery and economic values:
buy/sell 2000/200 and 4000/400 respectively. Their other properties follow the
existing Medium definition. These are project-owned additions, distributed
under the repository's GPL-3.0-or-later license; no external data pack was used.

All eight icon templates already belong to this repository. The runtime build
exports their matching black-keyed 32-by-32 BMPs into `HyperText` and includes
them in the configuration archive. Auto-HP Medicine is a separate four-item
family, documented in [`templates/AutoHpMedicine.md`](templates/AutoHpMedicine.md).

New consumables and general utility items receive consecutive IDs in
23000–23999, with the same numeric prefix in their JSON filenames. Medicine occupies 23000–23003,
Magic Potions 23004–23007 and Auto-HP Medicine 23008–23011 (Small through Huge
within each family). The Essence Box uses 23012, inventory unlocks for 1, 7 and
30 days use 23013–23015, and Exp Event Coin uses 23016. The next allocation
starts at 23031. IDs 23017–23030 are allocated to the utility items listed in
`AGENTS.md`. Collection equipment and materials use 24000–24062; the next
Collection allocation starts at 24063. See `docs/CollectionEquipment.md`.
Follow the allocation rule in `AGENTS.md` and update starter items, shops,
rewards and tests whenever
an existing definition changes its ID. Existing saved inventories and shortcuts
using earlier IDs are not migrated by renaming the definitions.
