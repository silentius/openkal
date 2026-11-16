---
name: ui-json-schema
description: Maintain the openkal.ui/v1 JSON schema, templates, variable catalog, bindings, and UI editor when adding or changing data-driven UI.
---

# Universal UI JSON schema

Use `docs/ui/TEMPLATE.md` as the canonical `openkal.ui/v1` structure and
`docs/ui/VARIABLES.md` as the vocabulary of bindings, actions, tags, and tokens.
`config/ui/_template.json` is an annotated skeleton, not a live layout.
`config/ui/dungeon_minimaps.json` is map data and does not use this schema.

## Required workflow

- Reuse the universal schema instead of introducing dialog-specific nested
  structures or ad-hoc HUD keys.
- When adding a dialog, widget type, chrome preset, binding path, action, accept
  tag, or `${group.key}` token, update `docs/ui/VARIABLES.md`. Update
  `docs/ui/TEMPLATE.md` when the schema or vocabulary changes.
- Prefer bound labels such as `${stats.hth_label}` over hard-coded UI copy in C++.
- Represent framed panels with `chrome.kind: nine_slice` and a preset instead of
  manually stacking bitmap rails.
- Keep `tools/uiEditor` compatible with every schema feature it is expected to
  edit. Consult `docs/ui/EDITOR.md` for editor behavior.
- Follow the FreeType text requirement in `AGENTS.md` for new or changed UI text.

Validate changed JSON and exercise the affected layout in the client or UI
editor before finishing.
