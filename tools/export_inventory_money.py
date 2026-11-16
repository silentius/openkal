#!/usr/bin/env python3
"""Export the text-free Geon panel as a native RGBA UI bitmap."""

from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from PIL import Image

from export_dragon_dialog_templates import RESAMPLING, TEMPLATES, write_bmp


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="openkal-inventory-money-") as directory:
        rendered = Path(directory) / "inventory_money.png"
        subprocess.run(
            ["convert", "-background", "none",
             str(TEMPLATES / "source/inventory_money.svg"), str(rendered)],
            check=True,
        )
        with Image.open(rendered) as source:
            image = source.convert("RGBA").resize((340, 48), RESAMPLING)
        write_bmp(TEMPLATES / "bmp/inventory_money.bmp", image)
        # Reuse the panel's coin artwork as a standalone currency icon.
        tree = ET.parse(TEMPLATES / "source/inventory_money.svg")
        root = tree.getroot()
        for child in list(root):
            if child.tag.rsplit("}", 1)[-1] not in {"defs", "g"}:
                root.remove(child)
        root.set("viewBox", "11 8 40 32")
        root.set("width", "80")
        root.set("height", "64")
        coins = Path(directory) / "geons.svg"
        tree.write(coins)
        subprocess.run(["convert", "-background", "none", str(coins), str(rendered)], check=True)
        with Image.open(rendered) as source:
            icon = source.convert("RGBA")
            icon.thumbnail((32, 32), RESAMPLING)
            image = Image.new("RGBA", (32, 32))
            image.alpha_composite(icon, ((32 - icon.width) // 2, (32 - icon.height) // 2))
        write_bmp(TEMPLATES / "bmp/geons.bmp", image)


if __name__ == "__main__":
    main()
