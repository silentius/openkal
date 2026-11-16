#!/usr/bin/env python3
"""Crop the generated inventory clasp sheet into native RGBA button states."""

from PIL import Image

from export_dragon_dialog_templates import RESAMPLING, TEMPLATES, write_bmp


def main() -> None:
    source = Image.open(TEMPLATES / "source/collection_handle.png").convert("RGBA")
    size = (28, 112)
    for column, state in enumerate(("idle", "hover", "pressed")):
        cell = source.crop((round(column * source.width / 3), 0,
                            round((column + 1) * source.width / 3), source.height))
        bounds = cell.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if bounds is None:
            raise ValueError(f"Missing collection handle: {state}")
        sprite = cell.crop(bounds)
        sprite.thumbnail(size, RESAMPLING)
        canvas = Image.new("RGBA", size)
        canvas.alpha_composite(sprite, ((size[0] - sprite.width) // 2,
                                        (size[1] - sprite.height) // 2))
        write_bmp(TEMPLATES / f"bmp/collection_handle_{state}.bmp", canvas)


if __name__ == "__main__":
    main()
