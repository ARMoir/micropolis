#!/usr/bin/env python3
"""Verify precise palette-only dense-building change in Tandy TILES.DAT."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tandy-dos" / "tools"))
from convert_tiles import convert, _dense_urban_tile, _urban_green_color


def pixel(data: bytes, tile: int, index: int) -> int:
    packed = data[tile * 32 + index // 2]
    return packed >> 4 if index % 2 == 0 else packed & 15


def main() -> None:
    source = Path(sys.argv[1])
    packaged = Path(sys.argv[2]).read_bytes()
    baseline = convert(source, urban_concrete=False)
    updated = convert(source)
    assert len(baseline) == len(updated) == len(packaged) == 30720
    assert updated == packaged, "packaged TILES.DAT differs from converter"
    # Original XPM used a reddish-pink symbol for the soil background.
    # It now uses key 2 for conditional forest green/solid-brown rendering.
    assert pixel(updated, 0, 0) == 2
    assert pixel(updated, 0, 7) == 2
    # Other intentional red inks, including building colors, still exist.
    assert any(pixel(updated, t, i) == 4
               for t in range(240, 900) for i in range(64))

    for tile in (0, 2, 22, 66, 79, 226, 240, 261, 270,
                 405, 409, 418, 423, 427, 431, 432, 441, 612, 745):
        assert not _dense_urban_tile(tile), tile
    for tile in (279, 288, 387, 450, 458, 594, 611):
        assert _dense_urban_tile(tile), tile
    assert _urban_green_color(2, ("#", "#", "$", "$"), True) == 7
    assert _urban_green_color(2, ("$", "$", "#", "$"), True) == 8
    assert _urban_green_color(2, ("#", "#", "#", "#"), False) == 2
    assert _urban_green_color(12, ("#", "#", "#", "#"), True) == 12

    changed = {"residential": 0, "commercial": 0}
    new_colors = {7: 0, 8: 0}
    unchanged_tiles = 0
    for tile in range(960):
        old_tile = baseline[tile * 32:(tile + 1) * 32]
        new_tile = updated[tile * 32:(tile + 1) * 32]
        if not _dense_urban_tile(tile):
            assert old_tile == new_tile, f"Unrelated tile {tile} changed"
            unchanged_tiles += 1
        for index in range(64):
            old_color = pixel(baseline, tile, index)
            new_color = pixel(updated, tile, index)
            if old_color != new_color:
                assert _dense_urban_tile(tile), (tile, index)
                assert old_color == 2, (tile, index, old_color, new_color)
                assert new_color in (7, 8), (tile, index, new_color)
                name = "residential" if tile <= 404 else "commercial"
                changed[name] += 1
                new_colors[new_color] += 1
    assert changed["residential"] > 50, changed
    assert changed["commercial"] > 50, changed
    assert new_colors[7] > 0 and new_colors[8] > 0, new_colors

    print("Dense urban pavement recolor:", changed, new_colors)
    print("All other pixels and", unchanged_tiles,
          "non-dense tiles match the original conversion exactly.")


if __name__ == "__main__":
    main()
