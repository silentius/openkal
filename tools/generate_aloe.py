#!/usr/bin/env python3
"""Generate OpenKal's aloe mesh and texture (GPL-3.0-or-later)."""

import math
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "RuntimeData/models/plants"


def make_mesh():
    """Build closed, ridged leaves; all dimensions are client world units."""
    vertices, indices = [], []
    for count, radius, height, width, droop, phase in (
        (8, 31, 32, 5.2, 19, 0.0),
        (5, 21, 40, 4.4, 12, 0.37),
        (3, 9, 44, 3.0, 3, 0.9),
    ):
        for leaf in range(count):
            angle = phase + leaf * math.tau / count
            variation = 1 + 0.09 * math.sin(leaf * 3.7 + phase)
            start = len(vertices)
            for ring in range(5):
                t = ring / 5
                r = 2 + radius * variation * t ** 0.85
                y = -2 + height * math.sin(t * math.pi / 2) - droop * t ** 3
                half_width = width * (0.18 + math.sin(t * math.pi) ** 0.8) * (1 - t * 0.45)
                thickness = half_width * 0.38
                for lateral, vertical, u in (
                    (-half_width, 0, 0), (0, thickness, 1 / 3),
                    (half_width, 0, 2 / 3), (0, -thickness, 1),
                ):
                    vertices.append((
                        math.cos(angle) * r - math.sin(angle) * lateral,
                        y + vertical,
                        math.sin(angle) * r + math.cos(angle) * lateral,
                        0, 1, 0, u, t,
                    ))
            for ring in range(4):
                for side in range(4):
                    a = start + ring * 4 + side
                    b = start + ring * 4 + (side + 1) % 4
                    indices.extend((a, b, b + 4, a, b + 4, a + 4))
            tip = len(vertices)
            vertices.append((math.cos(angle) * (2 + radius * variation),
                             -2 + height - droop, math.sin(angle) * (2 + radius * variation),
                             0, 1, 0, 1 / 3, 1))
            for side in range(4):
                indices.extend((start + 16 + side, start + 16 + (side + 1) % 4, tip))
            indices.extend((start, start + 2, start + 1, start, start + 3, start + 2))
    return vertices, indices


def write_gb(vertices, indices):
    """Use the GB v10 layout exercised by tests/fixture_util.h."""
    strings = b"aloe.dds\0\0aloe\0"
    empty = len(b"aloe.dds\0")
    header = struct.pack("<4BII", 10, 0, 0, 1, 0, empty)
    header += struct.pack("<12H", len(vertices), *([0] * 11))
    header += struct.pack("<4HIIHBBHH4I", len(indices), 0, 0, 0,
                          len(strings), 0, 0, 0, 0, 1, 0, 0, 0, 0, 0)
    material = struct.pack("<IHI f I", 0, 0, empty, 0, 0)
    mesh = struct.pack("<IiBBHHB", empty + 1, 0, 0, 0, len(vertices), len(indices), 0)
    payload = b"".join(struct.pack("<8f", *vertex) for vertex in vertices)
    payload += struct.pack(f"<{len(indices)}H", *indices)
    (OUTPUT / "aloe.gb").write_bytes(header + material + mesh + payload + strings)


def write_texture():
    """Paint opaque leaf faces with pale margins and subtle speckles."""
    size = 128
    header = bytearray(128)
    header[:4] = b"DDS "
    for offset, value in ((4, 124), (8, 0x100F), (12, size), (16, size),
                          (20, size * 4), (76, 32), (80, 0x41), (88, 32),
                          (92, 0xFF0000), (96, 0xFF00), (100, 0xFF),
                          (104, 0xFF000000), (108, 0x1000)):
        struct.pack_into("<I", header, offset, value)
    pixels = bytearray()
    for y in range(size):
        t = y / (size - 1)
        for x in range(size):
            u = x / (size - 1)
            top = u <= 2 / 3
            edge_distance = min(abs(u), abs(u - 2 / 3), abs(u - 1))
            margin = max(0, 1 - edge_distance * 55)
            ridge = max(0, 1 - abs(u - 1 / 3) * 5) if top else -0.25
            speckle = 14 if top and t < 0.8 and ((x * 37 + y * 17) % 131 < 3) else 0
            light = 0.72 + 0.28 * t + 0.19 * ridge
            r = int(72 * light + 57 * margin + speckle)
            g = int(132 * light + 37 * margin + speckle)
            b = int(68 * light + 12 * margin + speckle)
            pixels.extend((b, g, r, 255))
    (OUTPUT / "aloe.dds").write_bytes(header + pixels)


if __name__ == "__main__":
    OUTPUT.mkdir(parents=True, exist_ok=True)
    mesh_vertices, mesh_indices = make_mesh()
    write_gb(mesh_vertices, mesh_indices)
    write_texture()
    print(f"Aloe: {len(mesh_vertices)} vertices, {len(mesh_indices) // 3} triangles")
