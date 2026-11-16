#!/usr/bin/env python3
"""Rasterize OpenKal's brass balance emblem for the menu and dialog."""
from PIL import Image

from export_dragon_dialog_templates import ROOT, RESAMPLING, write_bmp


def main() -> None:
    templates = ROOT / "RuntimeData/templates/dragon_menu"
    with Image.open(templates / "source/trading_post.png") as source:
        image = source.convert("RGBA")
    bounds = image.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0).getbbox()
    if bounds is None:
        raise ValueError("Missing trading-post emblem")
    for name, size, inset in (("trading_post", 24, 22), ("trading_post_dialog", 64, 59)):
        sprite = image.crop(bounds)
        sprite.thumbnail((inset, inset), RESAMPLING)
        canvas = Image.new("RGBA", (size, size))
        canvas.alpha_composite(sprite, ((size - sprite.width) // 2, (size - sprite.height) // 2))
        write_bmp(templates / f"bmp/{name}.bmp", canvas)


if __name__ == "__main__":
    main()
