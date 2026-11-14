#!/usr/bin/env python3
"""Create footprint-sized artwork and split it into 8/16px map tiles.

The raw libraries are indexed by footprints.json, not by Micropolis IDs.
Original SC2000 widths (32/64/96/128) define proposed 1/2/3/4-square lots.
"""
from collections import Counter
import hashlib
import json
from pathlib import Path
from convert_sc2k_lots import extract, read_palette, reduce_tile
from convert_sc2k_buildings import unpack, write_png, write_xpm
from convert_tiles import PALETTE


def split_tiles(packed, span, tile_size):
    width=span*tile_size
    if len(packed)!=width*width//2:
        raise ValueError('Canvas dimensions do not match the footprint')
    tiles=[]
    for ty in range(span):
        for tx in range(span):
            tile=b''.join(packed[(ty*tile_size+y)*width//2+tx*tile_size//2:
                                (ty*tile_size+y)*width//2+(tx+1)*tile_size//2]
                          for y in range(tile_size))
            tiles.append(tile)
    return tiles


def export_footprints(large, bmp, destination):
    destination.mkdir(parents=True,exist_ok=True)
    palette=read_palette(bmp)
    records=json.loads(Path(__file__).with_name('sc2k_buildings.json').read_text())
    all_tiles={8:[],16:[]}
    canvases={8:[],16:[]}
    for record in records:
        source=extract(large,record['id'])
        source_width=len(source[0])
        if source_width not in (32,64,96,128):
            raise ValueError(f"Unrecognized footprint width for {record['hex']}")
        span=source_width//32
        record.update(footprint_width=span,footprint_height=span,
                      tile_start=len(all_tiles[8]),tile_count=span*span,
                      source_width=source_width,source_height=len(source))
        for size in (8,16):
            record[f'offset_{size}']=len(all_tiles[size])*size*size//2
            record[f'byte_count_{size}']=span*span*size*size//2
            canvas=reduce_tile(source,palette,span*size)
            canvases[size].append(canvas)
            all_tiles[size].extend(split_tiles(canvas,span,size))
    outputs=[]
    for size in (8,16):
        dat=f'SC2KM{size}.DAT'
        xpm=f'sc2k-multi-{size}.xpm'
        preview=f'preview-multi-{size}.png'
        outputs += [dat,xpm,preview]
        (destination/dat).write_bytes(b''.join(all_tiles[size]))
        write_xpm(destination/xpm,all_tiles[size],size)
        # Reassembled buildings at their actual relative sizes, on fixed
        # 4x4-lot cells. No borders or gaps are introduced inside buildings.
        cell=4*size+4
        side=12*cell
        rgb=[24,29,35]*(side*side)
        for i,(record,canvas) in enumerate(zip(records,canvases[size])):
            width=record['footprint_width']*size
            ox=(i%12)*cell+2
            oy=(i//12)*cell+cell-2-width
            for p,color in enumerate(unpack(canvas)):
                x,y=ox+p%width,oy+p//width
                rgb[(y*side+x)*3:(y*side+x)*3+3]=PALETTE[color]
        write_png(destination/preview,side,side,rgb)
    catalog=dict(format='SC2K footprint libraries v1',
                 layout='building ID order, then tile row-major, then pixel row-major; 4bpp high nibble first',
                 footprint_policy='proposed square footprint = original sprite width / 32; not gameplay registration',
                 tile_count=len(all_tiles[8]),palette=PALETTE,records=records)
    (destination/'footprints.json').write_text(json.dumps(catalog,indent=2)+'\n',newline='\n')
    rows=['# Multi-tile building catalog','',
          'Proposed target footprints; no gameplay tile IDs have been assigned.','',
          '| ID | Building | Footprint | First tile | Tile count | 8px offset | 16px offset |',
          '|---|---|---|---:|---:|---:|---:|']
    for r in records:
        rows.append(f"| {r['hex']} | {r['name']} | {r['footprint_width']}x{r['footprint_height']} | {r['tile_start']} | {r['tile_count']} | {r['offset_8']} | {r['offset_16']} |")
    (destination/'FOOTPRINTS.md').write_text('\n'.join(rows)+'\n',encoding='utf-8',newline='\n')
    outputs+=['footprints.json','FOOTPRINTS.md']
    (destination/'SHA256SUMS-FOOTPRINTS').write_text(''.join(
        hashlib.sha256((destination/p).read_bytes()).hexdigest()+'  '+p+'\n' for p in outputs),newline='\n')
    print(f"Footprints: {dict(Counter(r['footprint_width'] for r in records))}; {len(all_tiles[8])} tiles per library")
    return records
