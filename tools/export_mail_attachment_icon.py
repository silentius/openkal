#!/usr/bin/env python3
"""Export the OpenKal vector paperclip as a native RGBA UI bitmap."""

from pathlib import Path
import subprocess
import tempfile

from PIL import Image

from export_dragon_dialog_templates import RESAMPLING, TEMPLATES, write_bmp


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="openkal-paperclip-") as directory:
        rendered = Path(directory) / "paperclip.png"
        subprocess.run(
            ["convert", "-background", "none",
             str(TEMPLATES / "source/mail_attachment.svg"), str(rendered)],
            check=True,
        )
        with Image.open(rendered) as source:
            image = source.convert("RGBA").resize((16, 16), RESAMPLING)
        write_bmp(TEMPLATES / "bmp/mail_attachment.bmp", image)


if __name__ == "__main__":
    main()
