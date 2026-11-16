---
name: source-provenance
description: Check provenance and reuse permissions before adding external code, specifications, fixtures, or assets to OpenKal.
---

# Source provenance

Use this workflow when an implementation depends on material that was not
created in this repository.

## Allowed inputs

- Public documentation whose terms permit the intended use.
- Independently written interoperability specifications.
- Lawful black-box observations recorded as testable requirements.
- Code, fixtures, and assets with a compatible license and known author.
- New designs and synthetic fixtures created for OpenKal.

## Required record

For imported material, record its author, URL or origin, license, retrieval
date, and the OpenKal files that depend on it. Keep third-party notices and
license texts with redistributed material.

Do not add material when its origin or permission cannot be established. Never
commit personal filesystem paths, credentials, or restricted reference data.

Implementation notes should describe observable behavior and project decisions,
not reproduce external identifiers, comments, or source excerpts.
