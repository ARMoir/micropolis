#!/usr/bin/env python3
"""Convert the original Happisle railway network into ordinary roads.

The original Micropolis city file is untouched. This generates the
HAPPISLE.CTY included in the Tandy DOS build/release. Micropolis .CTY
files contain 3,120 bytes of history/settings followed by 12,000
big-endian 16-bit map tiles in x-major order.

Source tile definitions: micropolis-activity/src/sim/headers/sim.h
The replacement preserves ALL original tile flags and city metadata.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

CITY_SIZE = 27120
MAP_OFFSET = 3120
TILE_COUNT = 120 * 100
TILE_ID_MASK = 0x03FF

# The original connection tables use exactly the same 16-tile layout
# for standard rail and road junctions, shifted by 160 IDs:
# rail bridge H/V -> road bridge H/V;
# rail straight, corner, T, four-way -> matching road art.
def road_version(tile_id: int) -> int:
    if 224 <= tile_id <= 236:
        return tile_id - 160
    if tile_id in (237, 238):  # perpendicular road/rail intersections
        return 76             # now an ordinary 4-way road intersection
    if tile_id == 221:        # horizontal rail crossing vertical wire
        return 77             # horizontal road crossing vertical wire
    if tile_id == 222:        # vertical rail crossing horizontal wire
        return 78             # vertical road crossing horizontal wire
    return tile_id


def convert(source: bytes) -> tuple[bytes, Counter[int]]:
    if len(source) != CITY_SIZE:
        raise ValueError(f"Expected {CITY_SIZE} bytes, got {len(source)}")
    output = bytearray(source)
    changes: Counter[int] = Counter()
    for i in range(MAP_OFFSET, CITY_SIZE, 2):
        original = (source[i] << 8) | source[i + 1]
        old_id = original & TILE_ID_MASK
        new_id = road_version(old_id)
        if new_id == old_id:
            continue
        replacement = (original & ~TILE_ID_MASK) | new_id
        output[i] = replacement >> 8
        output[i + 1] = replacement & 255
        changes[old_id] += 1
    return bytes(output), changes


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original_cty", type=Path)
    parser.add_argument("output_cty", type=Path)
    args = parser.parse_args()
    if args.original_cty.resolve() == args.output_cty.resolve():
        parser.error("Refusing to overwrite the original Micropolis city")

    source = args.original_cty.read_bytes()
    result, changed = convert(source)
    n_rail = sum(changed[k] for k in range(224, 239))
    n_wire_crossing = changed[221] + changed[222]

    # The exact checked-in original Happisle city has 935 rail/rail-road
    # tiles and 64 rail/power crossings. Fail closed if that changes:
    # the original scenario should never be silently overwritten.
    if (n_rail, n_wire_crossing) != (935, 64):
        raise ValueError(
            f"Unexpected Happisle rail counts: {n_rail} tracks and "
            f"{n_wire_crossing} power crossings (expected 935 and 64)"
        )
    assert result[:MAP_OFFSET] == source[:MAP_OFFSET]
    args.output_cty.write_bytes(result)
    print(
        f"Converted {n_rail} rail tiles and {n_wire_crossing} "
        f"rail/power crossings into roads: {args.output_cty}"
    )


if __name__ == "__main__":
    main()
