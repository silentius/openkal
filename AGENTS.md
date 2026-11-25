# OpenKal agent instructions

These instructions apply to the entire repository. Keep code, identifiers,
documentation, and comments in English, regardless of the language used in the
request. The project targets C++17; do not introduce C++20 or C++23 features
unless the project standard is explicitly being upgraded.

Task-specific workflows live in `.agents/skills`. Read the relevant `SKILL.md`
before working in one of those areas. Do not copy skill content into
assistant-specific directories.

When a requested path, filename or identifier differs from an existing name
(including capitalization or singular/plural), read
`.agents/skills/existing-project-conventions/SKILL.md` and clarify the discrepancy
before changing the structure or adding support for the alternative spelling.

1. ARCHITECTURE, MODULARITY & OOP:
- Follow RAII and SOLID where they improve the existing design. Introduce an
  interface only at a real substitution or test boundary; do not add abstraction
  solely to satisfy a pattern.
- Give polymorphic base classes a virtual destructor and mark derived overrides
  with `override`.
- Keep class state private. Use accessors where they preserve an invariant or
  form part of the public API; simple value types may remain aggregates.
- Use member initializer lists and make read-only member functions `const`.
- Prefer free functions for stateless operations. Use a `static` member only when
  the operation belongs to the class abstraction.

2. MODERN C++ & PERFORMANCE ATTRIBUTES:
- Never use owning raw `new` or `delete`. Use standard containers and smart
  pointers for ownership; non-owning raw pointers and references are allowed.
- Prefer `std::string_view` for non-owning string input and `std::string` for
  owned text. Use `enum class` for new enumerations.
- Add `[[nodiscard]]` when ignoring a result is likely to be a correctness error,
  especially for factories and result/status objects. Do not annotate trivial
  getters mechanically.

3. CODE STYLE & STRUCTURE:
- Never add `using namespace std;`.
- Always use braces for the bodies of `if`, `else`, `for` (including range-for),
  `while` and `do` statements, even for a single statement or an empty body.
  Keep ordinary `else if` chains; every branch body must use braces.
- Put declarations in `.h` files with `#pragma once` and implementations in
  `.cpp` files. Templates, small inline functions, and compile-time definitions
  may remain in headers when required.
- Produce complete, compilable changes without placeholder implementations.
- Format changed C++ with the repository's `.clang-format` and run relevant tests
  before finishing.
- In JSON files, write arrays containing only numbers entirely on one line,
  with a space after each comma, for example `"code": [4, 8, 0, 0]`,
  `"country": [0, 1, 2]` and `"cooltime": [1300, 33]`. Apply this to every
  numeric array when creating or editing JSON; preserve values and avoid
  reformatting unrelated content.

4. DOCUMENTATION:
- Document public classes, methods, and non-obvious members in headers with short
  Doxygen comments.
- Do not use `@brief`. Put the summary on the same line as `/**`.
- List every parameter and any non-void return value. Leave one empty line before
  the next documented declaration.
- Keep implementation comments short and explain only information the code does
  not make clear.
- Add the project's existing GPL-3.0-or-later header to new C++ headers and source
  files. Preserve the header when modifying an existing file.

5. NAMING:
- Prefix private member variables with `m_`.
- Use PascalCase for new class and C++ filenames.

6. UI TEXT:
- Always use TrueType (FreeType / FreetypeFont) for all new or updated UI text.
- Do not use bitmap glyph atlases (draw_text_glyphs) for new windows unless matching an existing bitmap-only HUD panel that is not being changed.
- For the universal UI JSON schema, dialogs, widgets, bindings, or the UI editor,
  read `.agents/skills/ui-json-schema/SKILL.md`.

7. IMPLEMENTATION PROVENANCE:
- Do not consult, copy, translate, or derive implementation details from
  proprietary source code or private asset packs.
- Implement compatibility behavior from public documentation, independently
  written specifications, and observable protocol or file-format behavior.
- Keep a short provenance note for imported data and third-party assets.
- Do not regenerate, bulk-format, or replace curated configuration files unless
  explicitly requested. Existing configuration contains manual corrections.

8. LANGUAGE:
- Always use English when adding or changing code, identifiers, documentation,
  and comments.


9. COMPATIBILITY BEHAVIOR:
- Reserve item definition IDs 23000–23999 for consumables, general utility
  items and event currency from `config/items/general/etc`. Allocate IDs
  sequentially: use 23000 when the range is empty, otherwise one above the
  highest allocated ID in this range. Check all item definitions before
  allocating; do not reuse retired IDs or fill gaps. Keep the JSON filename's
  numeric prefix equal to its `index` and update every affected reference.
- Current consumable allocations are 23000–23003 for normal Medicine,
  23004–23007 for Magic Potions and 23008–23011 for Auto-HP Medicine, each ordered
  Small, Medium, Large, Huge. IDs 23012–23016 are the Essence Box, inventory
  unlocks (1, 7 and 30 days) and Exp Event Coin. ID 23017 is the Polishing Stone.
  IDs 23018–23020 are Perforation Shot, Dragon Spirit Scroll and Qigong Talisman.
  ID 23021 is the Revision Hammer; IDs 23022–23026 are the Talismans of
  Enchantment I, II, III, IIII and V. ID 23027 is Aloe Vera, ID 23028 is
  Blue Lotus Leaf and ID 23029 is Ginseng Root.
  ID 23030 is the tradeable Skin Ticket.
  ID 23031 was the retired generic Assassin Mask prototype.
  ID 23032 is the Honor Coupon.
  The next available ID is 23033;
  update this note whenever allocating more items in this range.
- Reserve item definition IDs 24000–24999 for the other Collection items.
  IDs 24000–24002 are Yin-Yang Mirrors, 24003–24026 raw trigrams,
  24027–24050 enchanted trigrams, 24051–24053 Taegeuk, 24054–24056 Hanin's
  Mirrors, 24057–24059 essences and 24060–24062 Yeoiju/fortification.
  The next available ID is 24063. Allocate sequentially and preserve the
  grade/position order within each family. Update the shared ID constants in
  `CollectionEquipment.h`, recipes, rewards, shops and tests when changing IDs.
- Keep documented protocol behavior, timing, sounds, and effects stable while
  fixing clear defects.
- Convert external configuration to JSON only from redistributable inputs and
  only as part of an explicitly requested conversion.
- Use project naming conventions instead of legacy Windows-style prefixes such
  as `dwClickCounter`; prefer `clickCounter`.

10. VISUAL EFFECTS:
- Do not change terrain or map rendering, the camera X-mirror, global culling or
  `FrontFace`, or player mesh/draw behavior unless the task explicitly requests those rendering changes.
- An FX name, audible sound, or visible particle alone is not sufficient
  verification.

11. DELIVERY:
- After completing and verifying a requested change, leave it local and
  uncommitted by default so the user can test it before it is merged.
- Do not create a feature branch, commit, push, open a pull request, merge, or
  delete branches unless the user explicitly says `push` or gives an equivalent
  publication instruction.
- An explicit `push` authorizes the complete delivery workflow: create a
  `codex/` feature branch if needed, commit the relevant changes, push it, open
  a pull request against `main`, and enable squash auto-merge. Stop and report
  only when merge conflicts, unresolved review feedback, or failing required
  checks need user direction.
- When `origin` has multiple configured push URLs, push to every available
  destination. A mounted local backup remote is optional: if it is unavailable,
  complete the primary remote push and report the skipped backup without
  treating it as a delivery failure.
- Delete the remote and local feature branches only after a successful merge.

## Cursor Cloud specific instructions

The image already installs the README build dependencies, including FreeType,
OpenSSL, SQLite, GLFW, GLEW, Pillow, Xvfb, and Qt 6.9.3 under
`/opt/qt/6.9.3/gcc_64`. `cmake` on `PATH` exports `QTDIR` to that prefix so the
default area editor can `find_package(Qt6 6.9)`. `/usr/bin/c++` is `g++`.

Use the configure, build, and `ctest` commands in `README.md`. Project-owned
tests do not need proprietary game data. Catalog tests labeled
`optional-user-data` stay disabled until `OPENKAL_DATA_ROOT` is set.

The OpenGL client loads `build/config.pak`, creates its window, and exits when
`map/login_map/login_maxl.gb` is missing. That login model is not in this
repository. Standalone MainSvr loads configuration and then stops until a
monsters directory exists; those catalogs are user-supplied.
`build/bin/tools/area_info` can read `tests/map/fixtures/n_000_000.akarea`.
`area_editor` opens without `mapinfo.json`. A desktop session is on
`DISPLAY=:1`; use `xvfb-run` when that display is absent.
