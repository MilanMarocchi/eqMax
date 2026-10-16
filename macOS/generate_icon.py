#!/usr/bin/env python3
"""Rebuild eqMax's existing EQ-bar mark as a macOS icon; standard library only."""
import math
from pathlib import Path
import struct
import zlib


def rounded_distance(x, y, left, top, width, height, radius):
    dx = abs(x - left - width / 2) - width / 2 + radius
    dy = abs(y - top - height / 2) - height / 2 + radius
    return math.hypot(max(dx, 0), max(dy, 0)) + min(max(dx, dy), 0) - radius


def png(size):
    scale = 1024 / size
    data = bytearray()
    for row in range(size):
        data.append(0)
        y = (row + 0.5) * scale
        for col in range(size):
            x = (col + 0.5) * scale
            d = rounded_distance(x, y, 80, 80, 864, 864, 190)
            coverage = max(0, min(1, 0.5 - d / scale))
            base = 43 - 17 * y / 1024
            color = [base, base + 2, base + 8]
            edge = max(0, 1 - abs(d + 2) / 3)
            color = [c + edge * 22 for c in color]
            for bx, height in [(274, 250), (402, 440), (530, 335), (658, 185)]:
                bd = rounded_distance(x, y, bx, 736 - height, 88, height, 24)
                fill = max(0, min(1, 0.5 - bd / scale))
                accent = [79 + 22 * y / 1024, 178 - 26 * y / 1024, 255]
                color = [c * (1 - fill) + a * fill for c, a in zip(color, accent)]
            data.extend([*(round(max(0, min(255, c))) for c in color), round(255 * coverage)])
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(data, 9)) + chunk(b'IEND', b'')


if __name__ == '__main__':
    destination = Path(__file__).resolve().parent / 'AppIcon.icns'
    # ICNS is a length-prefixed container; modern macOS accepts PNG representations.
    representations = [(b'icp4', 16), (b'icp5', 32), (b'icp6', 64), (b'ic07', 128),
                       (b'ic08', 256), (b'ic09', 512), (b'ic10', 1024)]
    chunks = []
    for kind, size in representations:
        data = png(size)
        chunks.append(kind + struct.pack('>I', len(data) + 8) + data)
    payload = b''.join(chunks)
    destination.write_bytes(b'icns' + struct.pack('>I', len(payload) + 8) + payload)
    print(destination)
