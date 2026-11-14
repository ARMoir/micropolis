#!/usr/bin/env python3
"""Independent checks for the Tandy edition of HAPPISLE.CTY."""
import sys
from collections import Counter
from pathlib import Path

original = Path(sys.argv[1]).read_bytes()
converted = Path(sys.argv[2]).read_bytes()
assert len(original) == len(converted) == 27120
assert original[:3120] == converted[:3120], "history, funds, time and taxes must not change"

changed = Counter()
old_roads = new_roads = 0
for offset in range(3120, 27120, 2):
    a = int.from_bytes(original[offset:offset + 2], "big")
    b = int.from_bytes(converted[offset:offset + 2], "big")
    old_id, new_id = a & 1023, b & 1023
    assert (a & ~1023) == (b & ~1023), "building and power flags changed"

    if 224 <= old_id <= 236:
        expected = old_id - 160
    elif old_id in (237, 238):
        expected = 76
    elif old_id == 221:
        expected = 77
    elif old_id == 222:
        expected = 78
    else:
        expected = old_id
    assert new_id == expected, f"wrong road conversion at offset {offset}"
    assert (new_id in range(224, 239)) is False, "railway remains"
    assert new_id not in (221, 222), "rail/power crossing remains"

    if new_id != old_id:
        changed[old_id] += 1
    if 64 <= old_id <= 207:
        old_roads += 1
    if 64 <= new_id <= 207:
        new_roads += 1

assert sum(changed[k] for k in range(224, 239)) == 935
assert changed[221] + changed[222] == 64
assert sum(changed.values()) == 999
assert old_roads == 230
assert new_roads == 1229
print("HAPPISLE.CTY: 999 rail-to-road conversions, including 64 conductive crossings; original metadata and flags preserved")
