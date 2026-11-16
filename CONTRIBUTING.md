# Contributing

- Do not consult or copy proprietary source code when contributing.
- Base compatibility work on public documentation, independently written
  specifications, and observable protocol or file-format behavior.
- Never add HackShield / HsCrypt (not even stubs).
- Keep code, identifiers, documentation, and comments in English.
- Game code must not `#include` GL/Godot/Win32 headers — only `openkal::engine` and libs.
- Run `ctest --test-dir build --output-on-failure` before sending changes that touch libs/.
- Raise unresolved behavior or provenance questions explicitly instead of guessing.

## Contributions and external user data

Pull-request review must check the origin and redistribution permissions of
contributed code, data and assets before merging them into this repository.
Project releases contain the reviewed project files and dependencies selected
for those releases.

The resource packer intentionally supports external user-supplied resources.
Local inputs and packages created by users are outside the project distribution;
their selection, use and redistribution are the users' responsibility. Supporting
these inputs does not add them to OpenKal or require the packer to determine
their legal status. Project review and release checks cover the repository and
artifacts published by OpenKal.

Keep `SourceFiles.txt` up to date when adding or removing project files. Run
`python3 tools/package_source.py --check-tracked` and
`python3 tools/check_image_inventory.py` before submitting release-content changes.
Record new image sources/exports and hashes in `docs/provenance/Images.json`.
Report sensitive issues through [the security policy](SECURITY.md).

## Formatting

Style is defined in `.clang-format` (LLVM defaults for now). Pre-commit runs `clang-format -i` on C/C++ files; if anything changed, re-stage and commit again.

```bash
pre-commit install
pre-commit run --all-files
```
