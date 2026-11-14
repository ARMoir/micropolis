#!/usr/bin/env python3
"""Build optional SC2K.DAT from a local SC2000 Windows 95 installation.

Uses only the standard library. Extracts building IDs 7C..87 and converts
them directly to 8x8 Tandy tiles. Original assets are not part of this tool.
Format: github.com/OpenCity2k/SC2k-docs/blob/master/sprite%20data%20spec.md
"""
import argparse
from collections import Counter
from pathlib import Path
import struct
from convert_tiles import PALETTE, _nearest

HEADER = b'S2LT\x01\x08\x0c\x20'


def read_palette(data):
    if len(data) < 54 or data[:2] != b'BM':
        raise ValueError('Expected PAL_MSTR.BMP')
    dib = struct.unpack_from('<I', data, 14)[0]
    if dib < 40 or struct.unpack_from('<H', data, 28)[0] != 8:
        raise ValueError('Expected an 8-bit Windows bitmap palette')
    start = 14 + dib
    if start + 1024 > len(data):
        raise ValueError('Truncated palette')
    return [tuple(data[start+i*4:start+i*4+3][::-1]) for i in range(256)]


def extract(data, tile):
    if len(data) < 2:
        raise ValueError('Truncated sprite file')
    count = struct.unpack_from('>H', data)[0]
    end_header = 2 + count * 10
    if end_header > len(data):
        raise ValueError('Truncated sprite index')
    entries = [struct.unpack_from('>HIHH', data, 2+i*10) for i in range(count)]
    # Some IDs have tiny placeholder entries BEFORE the full sprite (e.g.
    # B7 office tower: 96x5 followed by 96x117). Prefer the largest image;
    # equal-size duplicates retain index order for deterministic output.
    entry = max((e for e in entries if e[0] == tile+1000),
                key=lambda e: e[2]*e[3], default=None)
    if entry is None:
        raise ValueError(f'Missing sprite {tile:02X}')
    _, pos, height, width = entry
    if not (0 < width <= 128 and 0 < height <= 256 and end_header <= pos < len(data)):
        raise ValueError('Invalid sprite dimensions/offset')
    limit = min((e[1] for e in entries if e[1] > pos), default=len(data))
    pixels = [[None]*width for _ in range(height)]
    y = 0
    while pos+2 <= limit:
        length, mode = data[pos:pos+2]
        pos += 2
        if mode == 2:
            return pixels
        if mode != 1 or y >= height or pos+length > limit:
            raise ValueError('Invalid sprite row')
        end, x = pos+length, 0
        while pos < end:
            if pos+2 > end:
                raise ValueError('Truncated pixel command')
            n, mode = data[pos:pos+2]
            pos += 2
            if mode == 0:
                continue
            if mode == 3:
                x += n
            elif mode == 4:
                if pos+n+(n&1) > end or x+n > width:
                    raise ValueError('Pixel run exceeds row')
                pixels[y][x:x+n] = data[pos:pos+n]
                x += n
                pos += n+(n&1)
            else:
                raise ValueError('Unknown pixel command')
            if x > width:
                raise ValueError('Pixel skip exceeds row')
        y += 1
    raise ValueError('Sprite has no end marker')


def reduce_tile(pixels, palette, size=8, inset=1):
    """Fit inside 6x6: reserve edges for runtime connected-zone borders.

    Vote in the target palette over each source area, ignoring transparent
    samples. A minimum coverage suppresses isolated subpixel speckles.
    Transparency becomes ordinary Micropolis green ground (index 2).
    """
    if size not in (8,16) or inset < 0 or 2*inset >= size:
        raise ValueError('Unsupported tile size/inset')
    h, w = len(pixels), len(pixels[0])
    extent = size - 2*inset
    scale = min(extent/w, extent/h)
    nw, nh = max(1, round(w*scale)), max(1, round(h*scale))
    lookup = [_nearest(rgb) for rgb in palette]
    result = [2]*(size*size)
    for y in range(nh):
        for x in range(nw):
            samples = [pixels[sy][sx]
                       for sy in range(y*h//nh, (y+1)*h//nh)
                       for sx in range(x*w//nw, (x+1)*w//nw)]
            opaque = [lookup[p] for p in samples if p is not None]
            if len(opaque)*4 >= len(samples) and opaque:
                color = Counter(opaque).most_common(1)[0][0]
                result[(size-inset-nh+y)*size+(size-nw)//2+x] = color
    return bytes((result[i]<<4)|result[i+1] for i in range(0,size*size,2))


def convert(large, palette_bmp):
    palette = read_palette(palette_bmp)
    return HEADER + b''.join(reduce_tile(extract(large, tile), palette)
                             for tile in range(0x7c,0x88))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='WIN95/SC2K directory')
    parser.add_argument('destination', type=Path, help='Output SC2K.DAT')
    args = parser.parse_args()
    data = convert((args.game/'DATA/LARGE.DAT').read_bytes(),
                   (args.game/'BITMAPS/PAL_MSTR.BMP').read_bytes())
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(data)
    print(f'Wrote {len(data)} bytes: 8 commercial + 4 industrial tiles')


if __name__ == '__main__':
    main()
