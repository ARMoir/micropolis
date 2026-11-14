#!/usr/bin/env python3
"""Export all 144 SC2000 building types as Tandy and classic tile libraries.

Raw DAT files are row-major, high nibble first, 4 bits per pixel.
Record i represents SC2000 tile ID 0x70+i, not a Micropolis map tile ID.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from convert_sc2k_lots import convert, extract, read_palette, reduce_tile
from convert_tiles import PALETTE


def unpack(data):
    return [p for b in data for p in (b >> 4,b & 15)]


def write_png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    rows=b''.join(b'\0'+bytes(rgb[y*width*3:(y+1)*width*3]) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+
                     chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))


def write_xpm(path, tiles, size):
    symbols='0123456789ABCDEF'
    lines=['/* XPM */','/* Converted SC2000 artwork; see README.md for provenance. */',
           'static char *sc2k_buildings[] = {',f'"{size} {size*len(tiles)} 16 1",']
    lines += [f'"{symbols[i]} c #{r:02X}{g:02X}{b:02X}",' for i,(r,g,b) in enumerate(PALETTE)]
    rows=[]
    for tile in tiles:
        pix=unpack(tile)
        rows += ['"'+''.join(symbols[p] for p in pix[y*size:(y+1)*size])+'"' for y in range(size)]
    path.write_text('\n'.join(lines)+ '\n'+',\n'.join(rows)+'\n};\n', newline='\n')


def export(large, bmp, destination):
    destination.mkdir(parents=True,exist_ok=True)
    palette=read_palette(bmp)
    records=json.loads(Path(__file__).with_name('sc2k_buildings.json').read_text())
    if [r['id'] for r in records] != list(range(0x70,0x100)):
        raise ValueError('Catalog must contain every building ID exactly once')
    tiles={8:[],16:[]}
    for i,record in enumerate(records):
        source=extract(large,record['id'])
        record.update(index=i,source_sprite_id=1000+record['id'],
                      source_width=len(source[0]),source_height=len(source),
                      offset_8=i*32,offset_16=i*128)
        for size in tiles:
            tiles[size].append(reduce_tile(source,palette,size))
    for size, images in tiles.items():
        (destination/f'SC2K{size}.DAT').write_bytes(b''.join(images))
        write_xpm(destination/f'sc2k-{size}.xpm',images,size)
        # Contact sheet: 12x12, in ascending source-ID order; 1px gutters.
        side=12*(size+2)
        rgb=[24,29,35]*(side*side)
        for i,data in enumerate(images):
            for p,color in enumerate(unpack(data)):
                x=(i%12)*(size+2)+1+p%size
                y=(i//12)*(size+2)+1+p//size
                rgb[(y*side+x)*3:(y*side+x)*3+3]=PALETTE[color]
        write_png(destination/f'preview-{size}.png',side,side,rgb)
    # Exact existing runtime pack, never substitute a library for this file.
    (destination/'SC2K.DAT').write_bytes(convert(large,bmp))
    catalog=dict(format='SC2K building tile libraries v1',
                 source_sha256=dict(large_dat=hashlib.sha256(large).hexdigest(),
                                    pal_mstr_bmp=hashlib.sha256(bmp).hexdigest()),
                 palette=PALETTE,records=records)
    (destination/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n', newline='\n')
    rows=['# Building catalog','',
          'Offsets are bytes into the raw DAT libraries. Source IDs are not Micropolis tile IDs.','',
          '| SC2000 ID | Index | Category | Building | Source px | 8px offset | 16px offset |',
          '|---|---:|---|---|---|---:|---:|']
    for r in records:
        rows.append(f"| {r['hex']} | {r['index']} | {r['category']} | {r['name']} | {r['source_width']}×{r['source_height']} | {r['offset_8']} | {r['offset_16']} |")
    (destination/'CATALOG.md').write_text('\n'.join(rows)+'\n',encoding='utf-8', newline='\n')
    outputs=['SC2K8.DAT','SC2K16.DAT','SC2K.DAT','sc2k-8.xpm','sc2k-16.xpm',
             'preview-8.png','preview-16.png','catalog.json','CATALOG.md']
    (destination/'SHA256SUMS').write_text(''.join(hashlib.sha256((destination/p).read_bytes()).hexdigest()+'  '+p+'\n' for p in outputs), newline='\n')
    return records


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path,help='Windows 95 SC2K directory')
    parser.add_argument('destination',type=Path)
    args=parser.parse_args()
    records=export((args.game/'DATA/LARGE.DAT').read_bytes(),
                   (args.game/'BITMAPS/PAL_MSTR.BMP').read_bytes(),args.destination)
    print(f'Exported {len(records)} buildings at 8x8 and 16x16, plus the 12-building runtime pack')


if __name__=='__main__': main()
