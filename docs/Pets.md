# Pets

OpenKal models pets as server-authoritative item instances with optional visual
presentation on the client. Pet definitions are JSON configuration; no legacy
text or binary configuration file is used as a runtime fallback.

## Runtime behavior

- Equipping a pet activates the definition associated with that item instance.
- Timed pets keep their expiration timestamp in persistence.
- Healing and automatic pickup are validated by the server.
- The client may select and approach a visible drop, but only the server can
  grant the item after ownership, distance, stacking, and capacity checks.
- Missing optional visual data disables that presentation without inventing a
  model or consulting another data format.

## Optional configuration

Users may place compatible definitions below
`OPENKAL_DATA_ROOT/config/pets/`. Such files remain outside the OpenKal source
distribution and must be obtained or authored lawfully. Tests use small
synthetic definitions created specifically for OpenKal.

A definition should use stable project-facing fields for the item index,
lifetime, healing interval and amount, pickup capability, and visual binding.
New fields require schema validation and tests for missing and invalid values.

## Verification

Project-owned tests should cover activation, replacement, persistence,
expiration, healing, pickup authorization, full inventories, stacking, and
concurrent requests. Rendering checks cover attachment, movement, detachment,
and missing assets independently of the gameplay tests.
