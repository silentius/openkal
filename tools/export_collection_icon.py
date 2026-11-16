#!/usr/bin/env python3
"""Export the generated collection emblem in the menu's native RGBA format."""

from PIL import Image

from export_dragon_dialog_templates import RESAMPLING, ROOT, write_bmp


def main() -> None:
    templates = ROOT / "RuntimeData/templates/dragon_menu"
    source = Image.open(templates / "source/collection.png").convert("RGBA")
    bounds = source.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    if bounds is None:
        raise ValueError("Missing collection emblem")
    for name, size, inset_size in (("collection", 24, 22), ("collection_dialog", 64, 59)):
        sprite = source.crop(bounds)
        sprite.thumbnail((inset_size, inset_size), RESAMPLING)
        canvas = Image.new("RGBA", (size, size))
        canvas.alpha_composite(sprite, ((size - sprite.width) // 2, (size - sprite.height) // 2))
        write_bmp(templates / f"bmp/{name}.bmp", canvas)


if __name__ == "__main__":
    main()
