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
