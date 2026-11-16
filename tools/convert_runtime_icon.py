#!/usr/bin/env python3
"""Convert an OpenKal PNG template to a 32x32, uncompressed 24-bit BMP."""

import argparse
from io import BytesIO
from pathlib import Path

from PIL import Image


def render_icon(source: Path) -> bytes:
    """Preserve aspect ratio and flatten keyed/alpha backgrounds to pure black."""
    with Image.open(source) as original:
        image = original.convert("RGBA")

    # Remove the client's color keys before filtering, avoiding a dark fringe.
    image.putdata([
        (r, g, b, 0) if (max(r, g, b) <= 4 or (r, g, b) == (15, 15, 15))
        else (r, g, b, a)
        for r, g, b, a in image.getdata()
    ])
    scale = min(32 / image.width, 32 / image.height)
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    resampling = getattr(Image, "Resampling", Image)
    image = image.resize(size, resampling.LANCZOS)

    # BMP color keys cannot represent partial alpha. Keep a crisp silhouette.
    mask = image.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0)
    output = Image.new("RGB", (32, 32), (0, 0, 0))
    output.paste(image.convert("RGB"), ((32 - size[0]) // 2, (32 - size[1]) // 2), mask)
    buffer = BytesIO()
    output.save(buffer, format="BMP", dpi=(72, 72))
    return buffer.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = render_icon(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(".bmp.tmp")
    temporary.write_bytes(data)
    temporary.replace(args.output)


if __name__ == "__main__":
    main()
