#!/usr/bin/env python3
"""Convert the authentic GPL Micropolis 16x16 XPM tile strip to Tandy 8x8x16.

Input is the 960-tile XPM strip in micropolis-activity/images/tiles.xpm.
Each 16x16 tile becomes an 8x8 tile, packed 2 pixels/byte (high nibble first),
32 bytes/tile. Output: 30,720-byte TILES.DAT loaded by MICROPOL.EXE.

This is a build-time conversion of original artwork, not a recreated tile set.
Source artwork © Electronic Arts; GPLv3-or-later with additional terms in COPYING.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

# IBM PC/Tandy-compatible 16-color palette (EGA-standard RGB intensities).
# Tandy's displayed palette depends on hardware/monitor and palette registers.
PALETTE = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]


def _parse_rgb(value: str) -> tuple[int, int, int]:
    if value.lower() == "none":
        return (0, 0, 0)
    if not value.startswith("#"):
        raise ValueError(f"Unsupported XPM palette entry: {value}")
    s = value[1:]
    if len(s) not in (3, 6, 9, 12):
        raise ValueError(f"Unsupported XPM hex color: {value}")
    digits = len(s) // 3
    peak = (1 << (digits * 4)) - 1
    return tuple(round(int(s[i:i+digits], 16) * 255 / peak)
                 for i in range(0, len(s), digits))


def _nearest(rgb: tuple[int, int, int]) -> int:
    return min(range(16), key=lambda i: sum(
        (rgb[k] - PALETTE[i][k]) ** 2 for k in range(3)))


def convert(source: Path) -> bytes:
    quoted = re.findall(r'^\s*"([^"]*)"', source.read_text(), re.M)
    if not quoted:
        raise ValueError("No XPM data found")
    width, height, colors, cpp = map(int, quoted[0].split()[:4])
    if (width, height, cpp) != (16, 15360, 1) or height // 16 != 960:
        raise ValueError(f"Unexpected Micropolis XPM dimensions: {quoted[0]}")
    if len(quoted) != 1 + colors + height:
        raise ValueError("XPM does not contain exactly the declared color and pixel rows")

    lookup: dict[str, int] = {}
    for line in quoted[1:1 + colors]:
        token = line[0]
        match = re.search(r'\bc\s+([^\s]+)', line[1:])
        if not match:
            raise ValueError(f"Invalid XPM color definition: {line}")
        lookup[token] = _nearest(_parse_rgb(match.group(1)))

    rows = quoted[1 + colors:]
    packed = bytearray()
    for tile in range(960):
        for sy in range(0, 16, 2):
            top = rows[tile * 16 + sy]
            bottom = rows[tile * 16 + sy + 1]
            if len(top) != 16 or len(bottom) != 16:
                raise ValueError(f"Malformed pixel row at tile {tile}")
            for sx in range(0, 16, 4):
                # 2x2 majority pixel, simple nearest-neighbor tie-breaking.
                # Keep texture/roads legible at the 8x8 target resolution.
                def get(x: int) -> int:
                    samples = (lookup[top[x]], lookup[top[x + 1]],
                               lookup[bottom[x]], lookup[bottom[x + 1]])
                    return max(range(16), key=lambda c: (
                        samples.count(c), -samples.index(c) if c in samples else -100))
                hi = get(sx)
                lo = get(sx + 2)
                packed.append((hi << 4) | lo)
    assert len(packed) == 960 * 32
    return bytes(packed)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    data = convert(args.source)
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(data)
    print(f"Converted 960 authentic Micropolis tiles to {len(data)} bytes at {args.destination}")


if __name__ == "__main__":
    main()
