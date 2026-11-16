#!/usr/bin/env python3
"""Extract the approved Dragon dialog artwork into native RGBA runtime BMPs.

This is a deterministic crop/scale/export step, not an artwork generator.
Keep the source sheets intact; regenerate them with the image-generation tool
when changing the actual design.
"""

import json
from pathlib import Path
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TEMPLATES = ROOT / "RuntimeData/templates/dragon_dialog"
FRAME_CROP = (32, 80, 976, 1424)
FRAME_SIZE = (472, 672)
RESAMPLING = getattr(Image, "Resampling", Image).LANCZOS


def write_bmp(path: Path, image: Image.Image) -> None:
    """Write straight BGRA, including alpha, without Pillow's BMP alpha loss."""
    image = image.convert("RGBA")
    # Generated solid surfaces use alpha 252-254. Make them fully opaque;
    # preserve the softer silhouette coverage and discard invisible RGB halos.
    pixels = bytearray(image.tobytes())
    for offset in range(0, len(pixels), 4):
        alpha = pixels[offset + 3]
        if alpha >= 240:
            pixels[offset + 3] = 255
        elif alpha <= 8:
            pixels[offset:offset + 4] = b"\0\0\0\0"
    image = Image.frombytes("RGBA", image.size, bytes(pixels))
    raw = image.transpose(getattr(Image, "Transpose", Image).FLIP_TOP_BOTTOM).tobytes("raw", "BGRA")
    header = struct.pack("<2sIHHI", b"BM", 54 + len(raw), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, image.width, image.height, 1, 32,
                       0, len(raw), 2835, 2835, 0, 0)
    path.write_bytes(header + info + raw)


def main() -> None:
    output = TEMPLATES / "bmp"
    output.mkdir(exist_ok=True)
    source = Image.open(TEMPLATES / "source/frame-quiet.png").convert("RGBA")
    frame = source.crop(FRAME_CROP).resize(FRAME_SIZE, RESAMPLING)
    # Asymmetric corners preserve the dragon's scale while plain rails repeat without ornamental rhythm.
    slices = {
        "frame_top_left": (0, 0, 128, 128),
        "frame_top": (192, 0, 256, 84),
        "frame_top_right": (408, 0, 472, 96),
        "frame_left": (0, 256, 40, 320),
        "frame_right": (432, 256, 472, 320),
        "frame_bottom_left": (0, 616, 96, 672),
        "frame_bottom": (192, 616, 256, 672),
        "frame_bottom_right": (416, 616, 472, 672),
        "panel_background": (192, 256, 320, 384),
    }
    manifest = {
        "frame_source": "source/frame-quiet.png",
        "frame_size": FRAME_SIZE,
        "frame_source_crop": FRAME_CROP,
        "frame_slices": slices,
        "button_sizes": {},
        "notes": "Native pixels. No baked title, dialog icon or text-button glyphs.",
    }
    # Only the upper-right corner uses the ornamental revision; preserve every other slice.
    corner_source = "source/frame-corner-ornament.png"
    corner = Image.open(TEMPLATES / corner_source).convert("RGBA")
    corner = corner.crop(FRAME_CROP).resize(FRAME_SIZE, RESAMPLING)
    manifest["frame_slice_sources"] = {"frame_top_right": corner_source}
    for name, rect in slices.items():
        sheet = corner if name == "frame_top_right" else frame
        write_bmp(output / (name + ".bmp"), sheet.crop(rect))
    sheet = Image.open(TEMPLATES / "source/buttons.png").convert("RGBA")
    assert sheet.size == (1536, 1024)
    for row, (family, size) in enumerate([
            ("button", (144, 48)), ("icon_button", (28, 28)), ("close", (36, 36))]):
        manifest["button_sizes"][family] = size
        for column, state in enumerate(("idle", "hover", "pressed", "disabled")):
            cell = sheet.crop((column * 384, round(row * 1024 / 3),
                               (column + 1) * 384, round((row + 1) * 1024 / 3)))
            silhouette = cell.getchannel("A").point(lambda a: 255 if a >= 128 else 0)
            bounds = silhouette.getbbox()
            if bounds is None:
                raise ValueError(f"Missing sprite: {family}/{state}")
            sprite = cell.crop(bounds).resize(size, RESAMPLING)
            write_bmp(output / f"{family}_{state}.bmp", sprite)
    long_source = "source/long_buttons.png"
    sheet = Image.open(TEMPLATES / long_source).convert("RGBA")
    controls = {
        "long_button_idle": (0, 0, 1536, 210),
        "long_button_hover": (0, 210, 1536, 390),
        "long_button_pressed": (0, 390, 1536, 565),
        "long_button_disabled": (0, 565, 1536, 755),
        "toggle_active": (380, 755, 760, 1024),
        "toggle_inactive": (760, 755, 1140, 1024),
    }
    for name, rect in controls.items():
        cell = sheet.crop(rect)
        bounds = cell.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if bounds is None:
            raise ValueError(f"Missing sprite: {name}")
        size = (276, 30) if name.startswith("long_button") else (20, 20)
        sprite = cell.crop(bounds)
        # Fit uniformly; the rails, rivets and circular settings must never stretch.
        sprite.thumbnail(size, RESAMPLING)
        canvas = Image.new("RGBA", size)
        canvas.alpha_composite(sprite, ((size[0] - sprite.width) // 2,
                                        (size[1] - sprite.height) // 2))
        write_bmp(output / f"{name}.bmp", canvas)
    manifest["long_controls_source"] = long_source
    manifest["long_controls_cells"] = controls
    manifest["button_sizes"]["long_button"] = (276, 30)
    manifest["button_sizes"]["toggle"] = (20, 20)
    (TEMPLATES / "slices.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
