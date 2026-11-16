#!/usr/bin/env python3
"""Generate OpenKal lotus and ginseng models (GPL-3.0-or-later).

All geometry and textures are mathematical designs created for OpenKal.
The GB v10 writer follows the project's owned generate_aloe.py fixture layout.
Inventory icons are separate authoring assets and are never overwritten here.
"""

import math
from pathlib import Path
import struct


OUTPUT = Path(__file__).resolve().parents[1] / "RuntimeData/models/plants"


class Mesh:
    """One closed, textured mesh assembled from low-poly plant shapes."""

    def __init__(self):
        self.vertices = []
        self.indices = []

    def vertex(self, position, uv):
        index = len(self.vertices)
        self.vertices.append((*position, 0, 1, 0, *uv))
        return index

    def triangle(self, a, b, c):
        self.indices.extend((a, b, c))

    def ellipsoid(self, center, radii, color):
        """Closed ellipsoid with distinct poles, avoiding collapsed triangles."""
        cx, cy, cz = center
        rx, ry, rz = radii
        bottom = self.vertex((cx, cy - ry, cz), (color, 0))
        rings = []
        for latitude in range(1, 5):
            t = latitude / 5
            phi = math.pi * t
            ring = []
            for side in range(12):
                angle = side * math.tau / 12
                ring.append(self.vertex((cx + rx * math.sin(phi) * math.cos(angle),
                                         cy - ry * math.cos(phi),
                                         cz + rz * math.sin(phi) * math.sin(angle)),
                                        (color + 0.08 * math.cos(angle), t)))
            rings.append(ring)
        top = self.vertex((cx, cy + ry, cz), (color, 1))
        for side in range(12):
            nxt = (side + 1) % 12
            self.triangle(bottom, rings[0][side], rings[0][nxt])
            for first, second in zip(rings, rings[1:]):
                self.triangle(first[side], second[side], second[nxt])
                self.triangle(first[side], second[nxt], first[nxt])
            self.triangle(rings[-1][side], top, rings[-1][nxt])

    def leaf(self, base, angle, length, width, rise, color):
        """Closed pointed leaf or petal with a raised central ridge."""
        bx, by, bz = base
        rings = []
        for step in range(5):
            t = step / 5
            half = width * (0.15 + math.sin(math.pi * t)) * (1 - 0.35 * t)
            ring = []
            for lateral, vertical in ((-half, 0), (0, half * 0.24),
                                      (half, 0), (0, -half * 0.16)):
                ring.append(self.vertex((bx + math.cos(angle) * length * t
                                         - math.sin(angle) * lateral,
                                         by + rise * t + vertical,
                                         bz + math.sin(angle) * length * t
                                         + math.cos(angle) * lateral),
                                        (color + lateral / width * 0.05, t)))
            rings.append(ring)
        tip = self.vertex((bx + math.cos(angle) * length, by + rise,
                           bz + math.sin(angle) * length), (color, 1))
        for side in range(4):
            nxt = (side + 1) % 4
            for first, second in zip(rings, rings[1:]):
                self.triangle(first[side], first[nxt], second[nxt])
                self.triangle(first[side], second[nxt], second[side])
            self.triangle(rings[-1][side], rings[-1][nxt], tip)
        self.triangle(rings[0][0], rings[0][2], rings[0][1])
        self.triangle(rings[0][0], rings[0][3], rings[0][2])


def lotus():
    mesh = Mesh()
    for x, z, height, radius in ((-13, 0, 14, 13), (12, 7, 21, 12), (0, -12, 25, 11)):
        mesh.ellipsoid((x, height / 2 - 2, z), (0.9, height / 2 + 2, 0.9), 0.22)
        mesh.ellipsoid((x, height, z), (radius, 1.4, radius), 0.22)
    mesh.ellipsoid((0, 15, 1), (1.2, 18, 1.2), 0.22)
    for count, radius, width, rise, phase in ((9, 12, 3.4, 1, 0), (6, 8, 2.9, 6, 0.3)):
        for petal in range(count):
            mesh.leaf((0, 32, 1), phase + petal * math.tau / count,
                      radius, width, rise, 0.75)
    mesh.ellipsoid((0, 34, 1), (2.5, 1.5, 2.5), 0.5)
    return mesh


def ginseng():
    mesh = Mesh()
    mesh.ellipsoid((0, -1, 0), (5, 7, 4), 0.75)
    for x, z in ((-3, 1), (3, -1)):
        mesh.ellipsoid((x, -8, z), (1.8, 6, 1.8), 0.75)
    mesh.ellipsoid((0, 14, 0), (1.2, 17, 1.2), 0.22)
    for x, z, height, phase in ((-8, 0, 22, 0), (8, 4, 28, 0.4), (0, -6, 36, 0.1)):
        mesh.ellipsoid((x, height / 2, z), (0.65, height / 2, 0.65), 0.22)
        for leaf in range(5):
            mesh.leaf((x, height, z), phase + leaf * math.tau / 5, 12, 3.3, -4, 0.22)
    return mesh


def write_model(name, mesh):
    texture_name = (name + ".dds").encode()
    empty = len(texture_name) + 1
    strings = texture_name + b"\0\0" + name.encode() + b"\0"
    header = struct.pack("<4BII", 10, 0, 0, 1, 0, empty)
    header += struct.pack("<12H", len(mesh.vertices), *([0] * 11))
    header += struct.pack("<4HIIHBBHH4I", len(mesh.indices), 0, 0, 0,
                          len(strings), 0, 0, 0, 0, 1, 0, 0, 0, 0, 0)
    material = struct.pack("<IHI f I", 0, 0, empty, 0, 0)
    mesh_header = struct.pack("<IiBBHHB", empty + 1, 0, 0, 0,
                              len(mesh.vertices), len(mesh.indices), 0)
    payload = b"".join(struct.pack("<8f", *v) for v in mesh.vertices)
    payload += struct.pack(f"<{len(mesh.indices)}H", *mesh.indices)
    (OUTPUT / (name + ".gb")).write_bytes(header + material + mesh_header + payload + strings)


def write_texture(name):
    size = 128
    header = bytearray(128)
    header[:4] = b"DDS "
    for offset, value in ((4, 124), (8, 0x100F), (12, size), (16, size),
                          (20, size * 4), (76, 32), (80, 0x41), (88, 32),
                          (92, 0xFF0000), (96, 0xFF00), (100, 0xFF),
                          (104, 0xFF000000), (108, 0x1000)):
        struct.pack_into("<I", header, offset, value)
    palette = ((51, 134, 113), (218, 182, 65), (80, 126, 218)) if name == "blue_lotus" else (
        (63, 128, 49), (96, 145, 52), (188, 142, 80))
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            color = palette[0 if x < 55 else 1 if x < 76 else 2]
            light = 0.72 + 0.25 * y / 127 + 0.09 * math.sin(x * 0.35)
            r, g, b = (int(component * light) for component in color)
            pixels.extend((b, g, r, 255))
    (OUTPUT / (name + ".dds")).write_bytes(header + pixels)


if __name__ == "__main__":
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for model_name, build in (("blue_lotus", lotus), ("ginseng", ginseng)):
        plant = build()
        write_model(model_name, plant)
        write_texture(model_name)
        print(f"{model_name}: {len(plant.vertices)} vertices, {len(plant.indices) // 3} triangles")
