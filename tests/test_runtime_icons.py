"""Regression checks for icon conversion and the packaged resource overlay."""

from io import BytesIO
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

from PIL import Image

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
from convert_runtime_icon import render_icon


class RuntimeIconsTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_bmp_format_aspect_ratio_and_determinism(self):
        source = self.root / "wide.png"
        Image.new("RGB", (64, 32), (230, 160, 40)).save(source)
        data = render_icon(source)
        self.assertEqual(data, render_icon(source))
        self.assertEqual(data[:2], b"BM")
        self.assertEqual(len(data), 3126)
        self.assertEqual(struct.unpack_from("<IiiHHI", data, 14), (40, 32, 32, 1, 24, 0))
        image = Image.open(BytesIO(data))
        self.assertEqual(image.mode, "RGB")
        self.assertEqual(image.getpixel((16, 7)), (0, 0, 0))
        self.assertEqual(image.getpixel((16, 8)), (230, 160, 40))
        self.assertEqual(image.getpixel((16, 23)), (230, 160, 40))
        self.assertEqual(image.getpixel((16, 24)), (0, 0, 0))

    def test_color_keys_and_alpha(self):
        source = self.root / "keys.png"
        image = Image.new("RGBA", (32, 32), (255, 0, 255, 0))
        colors = [(0, 0, 0, 255), (4, 4, 4, 255), (15, 15, 15, 255),
                  (255, 0, 0, 127), (255, 0, 0, 128), (5, 5, 5, 255)]
        for x, color in enumerate(colors):
            image.putpixel((x, 16), color)
        image.save(source)
        converted = Image.open(BytesIO(render_icon(source)))
        self.assertEqual(converted.getpixel((31, 31)), (0, 0, 0))
        for x in range(4):
            self.assertEqual(converted.getpixel((x, 16)), (0, 0, 0))
        self.assertEqual(converted.getpixel((4, 16)), (255, 0, 0))
        self.assertEqual(converted.getpixel((5, 16)), (5, 5, 5))

    def test_package_generated_icons_override_legacy_and_exclude_templates(self):
        (self.root / "config").mkdir()
        (self.root / "config" / "example.json").write_text("{}")
        templates = self.root / "RuntimeData" / "templates"
        templates.mkdir(parents=True)
        source = templates / "expEvent.PNG"
        Image.new("RGB", (64, 64), (255, 180, 40)).save(source)
        menu = templates / "dragon_menu" / "bmp"
        menu.mkdir(parents=True)
        menu_icon = menu / "inventory.bmp"
        Image.new("RGBA", (24, 24), (175, 130, 60, 128)).save(menu_icon)
        # Authoring sheets must never become HyperText icons or ship as templates.
        Image.new("RGBA", (60, 60), (175, 130, 60, 128)).save(menu.parent / "sheet.png")
        dialog = templates / "dragon_dialog" / "bmp"
        dialog.mkdir(parents=True)
        dialog_frame = dialog / "frame_top_left.bmp"
        Image.new("RGBA", (128, 128), (175, 130, 60, 255)).save(dialog_frame)
        Image.new("RGBA", (64, 96)).save(dialog.parent / "frame.png")
        talent_sources = templates / "talents" / "source"
        talent_sources.mkdir(parents=True)
        Image.new("RGB", (64, 48), (80, 120, 160)).save(talent_sources / "Atlas.png")
        (talent_sources / "Prompt.txt").write_text("Synthetic authoring prompt")
        nested_sources = talent_sources / "drafts"
        nested_sources.mkdir()
        Image.new("RGB", (48, 48)).save(nested_sources / "reference.PNG")
        talent_icon = talent_sources.parent / "physical.png"
        Image.new("RGB", (32, 32), (180, 40, 60)).save(talent_icon)
        legacy = self.root / "RuntimeData" / "HyperText"
        legacy.mkdir()
        (legacy / "expEvent.bmp").write_bytes(b"stale legacy image")
        (legacy / "other.bmp").write_bytes(b"keep")
        sounds = self.root / "RuntimeData" / "sounds"
        sounds.mkdir()
        (sounds / "third_party.mp3").write_bytes(b"optional source-tree sound")
        for cue in ("quest_accept", "quest_update", "quest_done"):
            (sounds / f"{cue}.mp3").write_bytes(b"synthesized cue")
        areas = self.root / "RuntimeData" / "areas"
        areas.mkdir()
        (areas / "n_032_032.akarea").write_bytes(b"packaged area")

        generated = self.root / "generated"
        icon = generated / "HyperText" / "expEvent.bmp"
        subprocess.run([sys.executable, str(TOOLS / "convert_runtime_icon.py"),
                        str(source), str(icon)], check=True)
        generated_talent = generated / "HyperText" / "talents" / "physical.bmp"
        generated_talent.parent.mkdir(parents=True)
        generated_talent.write_bytes(render_icon(talent_icon))
        archive = self.root / "config.pak"
        command = [sys.executable, str(TOOLS / "package_config.py"),
                   str(self.root), str(archive)]
        subprocess.run(command, check=True)
        standalone = archive.read_bytes()
        build_command = command + ["--generated-runtime-root", str(generated)]
        subprocess.run(build_command, check=True)
        self.assertEqual(archive.read_bytes(), standalone)
        with zipfile.ZipFile(archive) as pak:
            names = pak.namelist()
            self.assertEqual(names, sorted(set(names)))
            self.assertFalse(any("templates/" in name for name in names))
            self.assertFalse(any("/source/" in name for name in names))
            self.assertEqual(pak.read("RuntimeData/HyperText/talents/physical.bmp"),
                             generated_talent.read_bytes())
            self.assertNotIn("RuntimeData/HyperText/dragon_menu/sheet.bmp", names)
            self.assertNotIn("RuntimeData/HyperText/dragon_dialog/frame.bmp", names)
            self.assertEqual(pak.read("RuntimeData/UI/dragon_dialog/frame_top_left.bmp"),
                             dialog_frame.read_bytes())
            self.assertEqual(pak.read("RuntimeData/UI/dragon_menu/inventory.bmp"),
                             menu_icon.read_bytes())
            self.assertEqual(pak.read("RuntimeData/HyperText/expEvent.bmp"), icon.read_bytes())
            self.assertEqual(pak.read("RuntimeData/HyperText/other.bmp"), b"keep")
            self.assertNotIn("RuntimeData/sounds/third_party.mp3", names)
            for cue in ("quest_accept", "quest_update", "quest_done"):
                self.assertEqual(pak.read(f"RuntimeData/sounds/{cue}.mp3"),
                                 b"synthesized cue")
            self.assertEqual(pak.read("RuntimeData/areas/n_032_032.akarea"), b"packaged area")
            self.assertTrue(all(entry.compress_type == zipfile.ZIP_STORED for entry in pak.infolist()))

        # Stale build outputs are not enough to keep a removed template in the archive.
        source.unlink()
        menu_icon.unlink()
        (legacy / "expEvent.bmp").unlink()
        subprocess.run(build_command, check=True)
        with zipfile.ZipFile(archive) as pak:
            self.assertNotIn("RuntimeData/HyperText/expEvent.bmp", pak.namelist())
            self.assertNotIn("RuntimeData/UI/dragon_menu/inventory.bmp", pak.namelist())

    def test_package_painted_enchantment_icons_remain_distinct(self):
        (self.root / "config").mkdir()
        templates = self.root / "RuntimeData" / "templates"
        templates.mkdir(parents=True)
        generated = self.root / "generated"
        (generated / "HyperText").mkdir(parents=True)
        names = []
        for i, rank in enumerate(("I", "II", "III", "IIII", "V")):
            stem = f"talismanOfEnchantment{rank}"
            source = templates / f"{stem}.png"
            Image.new("RGBA", (32, 32), (40 + i * 40, 80, 230, 255)).save(source)
            icon = generated / "HyperText" / f"{stem}.bmp"
            icon.write_bytes(render_icon(source))
            names.append(f"RuntimeData/HyperText/{stem}.bmp")
        archive = self.root / "config.pak"
        command = [sys.executable, str(TOOLS / "package_config.py"),
                   str(self.root), str(archive)]
        subprocess.run(command, check=True)
        standalone = archive.read_bytes()
        subprocess.run(command + ["--generated-runtime-root", str(generated)], check=True)
        self.assertEqual(archive.read_bytes(), standalone)
        with zipfile.ZipFile(archive) as pak:
            images = [pak.read(name) for name in names]
            self.assertEqual(len(set(images)), 5)
            for name, data in zip(names, images):
                self.assertEqual(data, (generated / "HyperText" / Path(name).name).read_bytes())
        (templates / "talismanOfEnchantmentII.png").unlink()
        subprocess.run(command + ["--generated-runtime-root", str(generated)], check=True)
        with zipfile.ZipFile(archive) as pak:
            self.assertNotIn(names[1], pak.namelist())
            for name in names[:1] + names[2:]:
                self.assertIn(name, pak.namelist())

    def test_package_overlays_generated_data_with_local_configuration(self):
        data_root = self.root / "user_data"
        (data_root / "config").mkdir(parents=True)
        (data_root / "config" / "generated.json").write_text('{"source":"data"}')
        (data_root / "config" / "shared.json").write_text('{"source":"data"}')
        (data_root / "config" / "items").mkdir()
        (data_root / "config" / "items" / "shared.json").write_text(
            '{"index":23000,"source":"data"}')
        areas = data_root / "RuntimeData" / "areas"
        areas.mkdir(parents=True)
        (areas / "n_032_032.akarea").write_bytes(b"generated area")
        sounds = data_root / "RuntimeData" / "sounds"
        sounds.mkdir()
        (sounds / "quest_done.mp3").write_bytes(b"user-supplied sound")

        (self.root / "config").mkdir()
        (self.root / "config" / "local.json").write_text('{"source":"local"}')
        (self.root / "config" / "shared.json").write_text('{"source":"local"}')
        (self.root / "config" / "items").mkdir()
        (self.root / "config" / "items" / "shared.json").write_text(
            '{"index":23000,"source":"local"}')
        (self.root / "RuntimeData" / "templates").mkdir(parents=True)

        archive = self.root / "config.pak"
        subprocess.run([sys.executable, str(TOOLS / "package_config.py"),
                        str(self.root), str(archive), "--data-root", str(data_root)], check=True)
        with zipfile.ZipFile(archive) as pak:
            self.assertEqual(pak.read("config/generated.json"), b'{"source":"data"}')
            self.assertEqual(pak.read("config/local.json"), b'{"source":"local"}')
            self.assertEqual(pak.read("config/shared.json"), b'{"source":"local"}')
            self.assertEqual(pak.read("config/items/shared.json"),
                             b'{"index":23000,"source":"local"}')
            self.assertEqual(pak.read("RuntimeData/areas/n_032_032.akarea"), b"generated area")
            self.assertNotIn("RuntimeData/sounds/quest_done.mp3", pak.namelist())

        subprocess.run([sys.executable, str(TOOLS / "package_config.py"),
                        str(self.root), str(archive), "--data-root", str(data_root),
                        "--include-external-audio"], check=True)
        with zipfile.ZipFile(archive) as pak:
            self.assertEqual(pak.read("RuntimeData/sounds/quest_done.mp3"),
                             b"user-supplied sound")

    def test_package_rejects_item_index_conflicts(self):
        data_root = self.root / "user_data"
        external_items = data_root / "config" / "items"
        external_items.mkdir(parents=True)
        (external_items / "external.json").write_text('{"index":23000}')
        areas = data_root / "RuntimeData" / "areas"
        areas.mkdir(parents=True)
        (areas / "n_032_032.akarea").write_bytes(b"packaged area")

        local_items = self.root / "config" / "items"
        local_items.mkdir(parents=True)
        (local_items / "local.json").write_text('{"index":23000}')
        (self.root / "RuntimeData" / "templates").mkdir(parents=True)

        archive = self.root / "config.pak"
        result = subprocess.run(
            [sys.executable, str(TOOLS / "package_config.py"), str(self.root), str(archive),
             "--data-root", str(data_root)],
            capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate item index 23000", result.stderr)
        self.assertIn("config/items/external.json", result.stderr)
        self.assertIn("config/items/local.json", result.stderr)


if __name__ == "__main__":
    unittest.main()
