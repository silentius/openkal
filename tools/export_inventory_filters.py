#!/usr/bin/env python3
"""Export OpenKal inventory-filter vectors as native RGBA UI icons."""

from pathlib import Path
import subprocess
import tempfile

from PIL import Image

from export_dragon_dialog_templates import RESAMPLING, TEMPLATES, write_bmp


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="openkal-inventory-filters-") as directory:
        for source in sorted((TEMPLATES / "source").glob("inventory_filter_*.svg")):
            rendered = Path(directory) / (source.stem + ".png")
            subprocess.run(
                ["convert", "-background", "none", str(source), str(rendered)],
                check=True,
            )
            with Image.open(rendered) as image:
                icon = image.convert("RGBA").resize((24, 24), RESAMPLING)
            write_bmp(TEMPLATES / "bmp" / (source.stem + ".bmp"), icon)


if __name__ == "__main__":
    main()
