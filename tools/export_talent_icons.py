#!/usr/bin/env python3
"""Split the initial, project-owned talent atlas into runtime PNG templates."""

from pathlib import Path

from PIL import Image


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    destination = root / "RuntimeData/templates/talents"
    names = (
        "physical", "magical", "explosive", "defense",
        "auto_hp", "auto_mp", "auto_pick", "speed",
        "beacon", "physical_path", "magical_path", "locked",
    )
    destination.mkdir(parents=True, exist_ok=True)
    with Image.open(destination / "source/Atlas.png") as atlas:
        for index, name in enumerate(names):
            column, row = index % 4, index // 4
            bounds = (
                round(column * atlas.width / 4),
                round(row * atlas.height / 3),
                round((column + 1) * atlas.width / 4),
                round((row + 1) * atlas.height / 3),
            )
            atlas.crop(bounds).save(destination / f"{name}.png")


if __name__ == "__main__":
    main()
