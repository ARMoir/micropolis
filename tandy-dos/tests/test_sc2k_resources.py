"""Validate checked-in library bytes, XPM pixels, catalog and runtime subset."""
import hashlib
import json
from pathlib import Path
import re
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from convert_sc2k_lots import HEADER
from convert_sc2k_buildings import unpack

root=Path(__file__).resolve().parents[2]/'resources/sc2k-buildings'
catalog=json.loads((root/'catalog.json').read_text())
records=catalog['records']
assert [r['id'] for r in records]==list(range(0x70,0x100))
for size in (8,16):
    stride=size*size//2
    data=(root/f'SC2K{size}.DAT').read_bytes()
    assert len(data)==144*stride
    xpm=re.findall(r'^"([^"\n]*)"',(root/f'sc2k-{size}.xpm').read_text(),re.M)
    assert xpm[0]==f'{size} {size*144} 16 1'
    assert len(xpm)==17+size*144
    for i,r in enumerate(records):
        assert r[f'offset_{size}']==i*stride
        pix=unpack(data[i*stride:(i+1)*stride])
        rows=xpm[17+i*size:17+(i+1)*size]
        assert all(len(row)==size for row in rows)
        assert [int(p,16) for row in rows for p in row]==pix
        assert any(p!=2 for p in pix),r['hex']
        assert all(pix[y*size+x]==2 for y in range(size) for x in range(size)
                   if x in (0,size-1) or y in (0,size-1))
pack=(root/'SC2K.DAT').read_bytes()
data=(root/'SC2K8.DAT').read_bytes()
assert pack==HEADER+data[(0x7c-0x70)*32:(0x88-0x70)*32]
for line in (root/'SHA256SUMS').read_text().splitlines():
    sha,name=line.split('  ')
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==sha,name
print('144 IDs: DAT/XPM pixel parity, offsets, borders, nonempty sprites, runtime subset and hashes passed')

from convert_sc2k_footprints import split_tiles

def assemble(tiles, span, size):
    return b''.join(b''.join(tiles[ty*span+tx][y*size//2:(y+1)*size//2]
                           for tx in range(span))
                    for ty in range(span) for y in range(size))

# Coordinate-varying synthetic canvas proves ordering and seam continuity.
for span in (1,2,3,4):
    for size in (8,16):
        width=span*size
        canvas=bytes(((x+y*3)&15)<<4 | ((x+1+y*3)&15)
                     for y in range(width) for x in range(0,width,2))
        assert assemble(split_tiles(canvas,span,size),span,size)==canvas

multi=json.loads((root/'footprints.json').read_text())
assert [r['id'] for r in multi['records']]==list(range(0x70,0x100))
assert multi['tile_count']==797
assert {s:sum(r['footprint_width']==s for r in multi['records'])
        for s in range(1,5)}=={1:47,2:47,3:34,4:16}
for size in (8,16):
    stride=size*size//2
    data=(root/f'SC2KM{size}.DAT').read_bytes()
    assert len(data)==797*stride
    xpm=re.findall(r'^"([^"\n]*)"',(root/f'sc2k-multi-{size}.xpm').read_text(),re.M)
    assert xpm[0]==f'{size} {size*797} 16 1'
    assert len(xpm)==17+size*797
    assert [int(p,16) for row in xpm[17:] for p in row]==unpack(data)
    cursor=0
    for r in multi['records']:
        span=r['footprint_width']
        assert r['footprint_height']==span and r['tile_count']==span*span
        assert r['tile_start']==cursor and r[f'offset_{size}']==cursor*stride
        assert r[f'byte_count_{size}']==span*span*stride
        tiles=[data[(cursor+i)*stride:(cursor+i+1)*stride] for i in range(span*span)]
        pix=unpack(assemble(tiles,span,size)); width=span*size
        assert any(p!=2 for p in pix)
        assert all(pix[y*width+x]==2 for y in range(width) for x in range(width)
                   if x in (0,width-1) or y in (0,width-1))
        if span==1:
            old=(root/f'SC2K{size}.DAT').read_bytes()
            assert tiles[0]==old[(r['id']-0x70)*stride:(r['id']-0x70+1)*stride]
        cursor+=span*span
    assert cursor==797
for line in (root/'SHA256SUMS-FOOTPRINTS').read_text().splitlines():
    sha,name=line.split('  ')
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==sha,name
print('797 tiles: split/reassembly, footprint coverage, offsets, XPM parity, 1x1 compatibility and hashes passed')
