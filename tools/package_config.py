#!/usr/bin/env python3
"""Create a deterministic, uncompressed config.pak from OpenKal and data roots."""

import argparse
import json
from pathlib import Path
import zipfile

from convert_runtime_icon import render_icon


OWNED_QUEST_AUDIO = frozenset(("quest_accept.mp3", "quest_update.mp3", "quest_done.mp3"))


def validate_unique_item_indices(files: dict[str, Path | bytes]) -> None:
    """Reject duplicate item indices that would coexist in the packaged configuration."""
    item_sources: dict[int, tuple[str, Path]] = {}
    for relative, source in sorted(files.items()):
        path = Path(relative)
        if (len(path.parts) < 3 or path.parts[0] != "config" or path.parts[1] != "items" or
                path.suffix.lower() != ".json" or path.name == "manifest.json"):
            continue
        if not isinstance(source, Path):
            continue
        try:
            document = json.loads(source.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise SystemExit(f"cannot read packaged item definition {source}: {error}") from error
        if not isinstance(document, dict):
            continue
        index = document.get("index")
        if not isinstance(index, int) or isinstance(index, bool):
            continue
        previous = item_sources.get(index)
        if previous:
            previous_relative, previous_source = previous
            raise SystemExit(
                f"duplicate item index {index} in packaged configuration:\n"
                f"  {previous_relative} ({previous_source})\n"
                f"  {relative} ({source})")
        item_sources[index] = (relative, source)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path,
                        help="OpenKal source root containing owned config and RuntimeData")
    parser.add_argument("output", type=Path)
    parser.add_argument("--data-root", type=Path,
                        help="Generated-data repository; its files are overlaid first")
    parser.add_argument("--generated-runtime-root", type=Path,
                        help="Use BMPs already converted by the build instead of converting PNGs")
    parser.add_argument("--include-external-audio", action="store_true",
                        help="Include user-supplied RuntimeData/sounds from --data-root")
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    data_root = args.data_root.resolve() if args.data_root else None
    source_roots = ([data_root] if data_root else []) + [source_root]
    required_directories = [source_root / "config", source_root / "RuntimeData"]
    missing = [directory for directory in required_directories if not directory.is_dir()]
    if missing:
        raise SystemExit(f"packaged resource directory does not exist: {missing[0]}")
    if data_root and not data_root.is_dir():
        raise SystemExit(f"optional data root does not exist: {data_root}")
    if data_root and not any((data_root / name).is_dir() for name in ("config", "RuntimeData")):
        raise SystemExit(
            f"optional data root must contain config/ or RuntimeData/: {data_root}")
    if args.include_external_audio and not data_root:
        raise SystemExit("--include-external-audio requires --data-root")

    templates = source_root / "RuntimeData" / "templates"
    hud_templates = ("dragon_hud", "dragon_minimap", "dragon_menu", "dragon_dialog")
    files = {}
    for root in source_roots:
        root_templates = root / "RuntimeData" / "templates"
        for directory in (root / "config", root / "RuntimeData"):
            if not directory.is_dir():
                continue
            for path in directory.rglob("*"):
                audio_root = root / "RuntimeData" / "sounds"
                include_audio = root == data_root and args.include_external_audio
                owned_audio = (root == source_root and path.parent == audio_root and
                               path.name in OWNED_QUEST_AUDIO)
                if audio_root in path.parents and not (include_audio or owned_audio):
                    continue
                if path.is_file() and root_templates not in path.parents:
                    # Generated data is inserted first. The source repository
                    # deliberately wins when it supplies a local override.
                    files[path.relative_to(root).as_posix()] = path
    for template in sorted(templates.rglob("*")):
        if any((templates / hud) in template.parents for hud in hud_templates):
            continue
        if "source" in template.relative_to(templates).parts[:-1]:
            continue
        if not template.is_file() or template.suffix.lower() != ".png":
            continue
        relative = Path("HyperText") / template.relative_to(templates).with_suffix(".bmp")
        name = (Path("RuntimeData") / relative).as_posix()
        if args.generated_runtime_root:
            generated = args.generated_runtime_root / relative
            if not generated.is_file():
                raise SystemExit(f"generated runtime icon does not exist: {generated}")
            files[name] = generated
        else:
            files[name] = render_icon(template)
    for hud in hud_templates:
        for texture in sorted((templates / hud / "bmp").glob("*.bmp")):
            files[f"RuntimeData/UI/{hud}/{texture.name}"] = texture
    if not files:
        raise SystemExit("packaged resource directories are empty")
    validate_unique_item_indices(files)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(".pak.tmp")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED) as archive:
        for relative, source in sorted(files.items()):
            entry = zipfile.ZipInfo(relative, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_STORED
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, source if isinstance(source, bytes) else source.read_bytes())
    temporary.replace(args.output)


if __name__ == "__main__":
    main()
