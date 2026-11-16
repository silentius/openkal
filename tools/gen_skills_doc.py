#!/usr/bin/env python3
"""Generate a skill inventory from a user-selected JSON directory."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


def load_rows(skills_root: Path) -> list[tuple[str, int, Path, dict[str, object]]]:
    """Load skill definitions without consulting external source trees."""
    rows: list[tuple[str, int, Path, dict[str, object]]] = []
    for path in skills_root.rglob("*.json"):
        document = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(document, dict):
            continue
        index = document.get("index")
        if not isinstance(index, int) or isinstance(index, bool):
            continue
        role = path.parent.name
        if role in ("actions", "monster"):
            continue
        rows.append((role, index, path, document))
    rows.sort(key=lambda row: (row[0], row[1], row[2].name))
    return rows


def render(rows: list[tuple[str, int, Path, dict[str, object]]], root: Path) -> str:
    """Render a provenance-neutral inventory of the selected local data."""
    counts: defaultdict[str, int] = defaultdict(int)
    for role, _, _, _ in rows:
        counts[role] += 1

    lines = [
        "# Local skill catalog",
        "",
        "Generated from a user-selected JSON directory.",
        "The catalog records configured values only; it does not claim provenance or",
        "behavioral equivalence with any third-party implementation.",
        "",
        f"Definitions: **{len(rows)}**.",
    ]
    if counts:
        lines.extend([
            "",
            "By role: " + "; ".join(
                f"{role}: {count}" for role, count in sorted(counts.items())
            ) + ".",
        ])

    previous_role: str | None = None
    for role, index, path, document in rows:
        if role != previous_role:
            lines.extend(["", f"## {role.title()}"])
            previous_role = role
        name = str(document.get("name", "(unnamed)")).strip()
        relative = path.relative_to(root).as_posix()
        lines.extend([
            "",
            f"### {role} {index}: {name}",
            "",
            f"- Definition: `{relative}`",
            f"- Class: `{document.get('class')}`; key: `{document.get('key', '')}`; "
            f"rank cap: `{document.get('maxLevel', 1)}`.",
            f"- Type: `{json.dumps(document.get('type', []), ensure_ascii=False)}`; "
            f"grade: `{json.dumps(document.get('grade', []), ensure_ascii=False)}`; "
            f"learn level: `{document.get('levelLimit', 0)}`.",
            f"- Base MP: `{document.get('mp', 0)}`; "
            f"delay: `{json.dumps(document.get('delay', []), ensure_ascii=False)}`; "
            f"duration: `{document.get('lastTime', 0)}`.",
        ])
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("skills_root", type=Path,
                        help="Directory containing locally supplied skill JSON files")
    parser.add_argument("--output", type=Path,
                        help="Write Markdown to this file instead of standard output")
    args = parser.parse_args()

    root = args.skills_root.resolve()
    if not root.is_dir():
        raise SystemExit(f"skill directory does not exist: {root}")
    markdown = render(load_rows(root), root)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(markdown, encoding="utf-8")
    else:
        print(markdown, end="")


if __name__ == "__main__":
    main()
