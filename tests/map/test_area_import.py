# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def read_masks(path):
    data = path.read_bytes()
    magic, version, header_size = struct.unpack_from("<4sHH", data)
    assert (magic, version) == (b"AKAR", 2)
    attributes_size, masks_size = struct.unpack_from("<II", data, 28)
    masks_offset = header_size + attributes_size
    assert masks_offset + masks_size == len(data)
    runs = struct.iter_unpack("<IHH", data[masks_offset:])
    return [(blocked, movement) for count, blocked, movement in runs for _ in range(count)]


def main():
    importer = sys.argv[1]
    tiles = [0] * (256 * 256)
    for bit in range(16):
        tiles[9 * 256 + 12 + bit] = 1 << bit
    square = 34 * 256 + 12
    tiles[square] = 0xFFFF

    with tempfile.TemporaryDirectory(prefix="openkal-area-import-") as directory:
        source = Path(directory) / "n_031_031.ksm"
        output = Path(directory) / "n_031_031.akarea"
        source.write_bytes(struct.pack(f"<{1 + len(tiles)}I", 1, *tiles))
        command = [importer, str(source), str(output)]
        subprocess.run(command, check=True)
        assert read_masks(output) == [(mask, 0) for mask in tiles]

        subprocess.run(command + ["--player-collision"], check=True)
        assert read_masks(output) == [(mask, mask) for mask in tiles]

        subprocess.run(command + ["--player-collision", "--smooth"], check=True)
        tiles[square] = 0x6FF6
        assert read_masks(output) == [(mask, mask) for mask in tiles]


if __name__ == "__main__":
    main()
