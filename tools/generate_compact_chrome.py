#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate compact bronze chrome and clock emblem; no external inputs."""

from pathlib import Path
import argparse

from PIL import Image, ImageDraw

from export_dragon_dialog_templates import RESAMPLING, write_bmp


def generate(destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    # An bevel matches the warm bronze palette of the owned Dragon HUD.
    image = Image.new("RGB", (32, 32), (24, 21, 16))
    draw = ImageDraw.Draw(image)
    for inset, color in enumerate(
        [(25, 20, 14), (114, 79, 39), (174, 133, 73), (79, 56, 31), (36, 30, 23)]
    ):
        draw.rectangle((inset, inset, 31 - inset, 31 - inset), outline=color)
    slices = {
        "top_left": (0, 0, 5, 5),
        "top": (8, 0, 24, 5),
        "top_right": (27, 0, 32, 5),
        "left": (0, 8, 5, 24),
        "right": (27, 8, 32, 24),
        "bottom_left": (0, 27, 5, 32),
        "bottom": (8, 27, 24, 32),
        "bottom_right": (27, 27, 32, 32),
    }
    for name, box in slices.items():
        image.crop(box).save(destination / f"compact_{name}.bmp")

    # Decorative clock emblem, drawn at 4x resolution for smooth small-size edges.
    scale = 4
    emblem = Image.new("RGBA", (28 * scale, 28 * scale), (0, 0, 0, 0))
    face = ImageDraw.Draw(emblem)

    def ellipse(box, fill, outline=None, width=1):
        face.ellipse(tuple(round(v * scale) for v in box), fill=fill,
                     outline=outline, width=width * scale)

    def line(points, fill, width=1):
        face.line([(round(x * scale), round(y * scale)) for x, y in points],
                  fill=fill, width=width * scale)

    ellipse((0.5, 0.5, 27, 27), (19, 16, 12, 255), (78, 52, 27, 255))
    ellipse((2, 2, 25, 25), (44, 32, 21, 255), (185, 141, 76, 255))
    ellipse((4, 4, 23, 23), (19, 21, 20, 255), (96, 73, 43, 255))
    for points in (((13.5, 5.5), (13.5, 7)), ((13.5, 20), (13.5, 21.5)),
                   ((5.5, 13.5), (7, 13.5)), ((20, 13.5), (21.5, 13.5))):
        line(points, (172, 142, 92, 255))
    line(((8, 9), (13.5, 13.5), (18, 8)), (221, 197, 145, 255))
    ellipse((12, 12, 15, 15), (208, 169, 98, 255))
    emblem = emblem.resize((28, 28), RESAMPLING)
    write_bmp(destination / "clock_emblem.bmp", emblem)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "destination",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parents[1]
        / "RuntimeData/templates/dragon_dialog/bmp",
    )
    generate(parser.parse_args().destination)
