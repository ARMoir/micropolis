#!/usr/bin/env python3
"""Generate source-derived VGA shading, with a legacy-color key per pixel.
Low nibble = exact Tandy reference color, high nibble = VGA-only shade.
This lets the existing cursor, borders, roads and menus composite safely.
"""
from collections import Counter
from pathlib import Path
import re
from convert_tiles import PALETTE, _parse_rgb

def unpack(data):
    return [v for b in data for v in (b >> 4, b & 15)]

def xpm(path):
    lines = re.findall(r'^\s*"([^"]*)"', path.read_text(), re.M)
    w,h,n,cpp = map(int, lines[0].split())
    assert (w,h,cpp) == (16,15360,1)
    colors = {s[0]: _parse_rgb(re.search(r'\bc\s+(\S+)',s[1:]).group(1))
              for s in lines[1:1+n]}
    rows = lines[1+n:]
    return [[colors[c] for c in row] for row in rows]

def desired_original(rows, low):
    out=[]
    for tile in range(960):
        src=67 if 80<=tile<=207 and tile&15==3 else tile
        for y in range(8):
            for x in range(8):
                key=low[tile*64+y*8+x]
                samples=[rows[src*16+y*2+dy][x*2+dx]
                         for dy in range(2) for dx in range(2)]
                mean=tuple(sum(c[k] for c in samples)//4 for k in range(3))
                # The original XPM dirt symbol now uses low key 2.
                # Avoid restoring its reddish source hue on forest edges:
                # dry soil is painted solid brown at presentation time.
                if key==2 and (204,127,102) in samples:
                    mean=PALETTE[2]
                # Keep intentional concrete and recovered white road paint.
                if (key in (7,8) and mean[1]>mean[0]*1.5) or (
                        64<=tile<=207 and key==15):
                    mean=PALETTE[key]
                out.append(mean)
    return out

def desired_newer(data):
    pixels=unpack(data)
    assert len(pixels)==797*256
    out=[]
    for tile in range(797):
        for y in range(8):
            for x in range(8):
                samples=[PALETTE[pixels[tile*256+(y*2+dy)*16+x*2+dx]]
                         for dy in range(2) for dx in range(2)]
                out.append(tuple(sum(c[k] for c in samples)//4 for k in range(3)))
    return out

def build(root=Path(".")):
    low1=unpack((root/"TILES.DAT").read_bytes())
    low2=unpack((root/"NEWERM8.DAT").read_bytes())
    small=(root/"NEWER1.DAT").read_bytes()
    assert small[:8]==b"S2LT\x01\x08\x0c\x20"
    low2[12*64:24*64]=unpack(small[8:])
    rgb1=desired_original(xpm(root/"micropolis-activity/images/tiles.xpm"),low1)
    rgb2=desired_newer((root/"resources/sc2k-buildings/SC2KM16.DAT").read_bytes())
    groups=[Counter() for _ in range(16)]
    for keys,rgbs in ((low1,rgb1),(low2,rgb2)):
        for key,rgb in zip(keys,rgbs):
            # Quantize directly to the VGA DAC's six-bit channels.
            groups[key][tuple(round(c*63/255) for c in rgb)]+=1
    palette=[tuple(round(c*63/255) for c in PALETTE[i&15]) for i in range(256)]
    for key,counts in enumerate(groups):
        # Palette 210/226/242 are reserved for subtle tan terrain.
        limit=12 if key==2 else 15
        shades=[rgb for rgb,_ in counts.most_common() if rgb!=palette[key]][:limit]
        for level,rgb in enumerate(shades,1):
            palette[level*16+key]=rgb
    palette[210]=(43,32,21)
    palette[226]=(45,34,23)
    palette[242]=(47,36,25)
    def encode(keys,rgbs):
        result=bytearray()
        for key,rgb in zip(keys,rgbs):
            target=tuple(round(c*63/255) for c in rgb)
            candidates=range(13 if key==2 else 16)
            level=min(candidates,key=lambda s:sum(
                (palette[s*16+key][k]-target[k])**2 for k in range(3)))
            result.append(level*16+key)
        assert all((a&15)==b for a,b in zip(result,keys))
        return result
    original=encode(low1,rgb1);newer=encode(low2,rgb2)
    assert len(original)==61440 and len(newer)==51008
    assert len(set(original+newer))>32
    (root/"VGATILES.DAT").write_bytes(original)
    (root/"VGANEWER.DAT").write_bytes(newer)
    (root/"VGA.PAL").write_bytes(bytes(v for rgb in palette for v in rgb))
    print("VGA source art:",len(set(original+newer)),
          "used palette entries; exact legacy pixel keys verified")
if __name__=="__main__":
    build()
