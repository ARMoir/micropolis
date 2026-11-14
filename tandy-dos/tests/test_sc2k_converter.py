"""Synthetic format tests: no commercial game assets needed in CI."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from convert_sc2k_lots import HEADER, convert, extract, read_palette, reduce_tile


class ConverterTests(unittest.TestCase):
    def bmp(self):
        data = bytearray(54+1024)
        data[:2] = b'BM'
        struct.pack_into('<I', data, 14, 40)
        struct.pack_into('<H', data, 28, 8)
        data[58:62] = bytes([255,255,255,0])
        return bytes(data)

    def sprites(self):
        # 2x1 sprite: skip one transparent pixel, then one white pixel.
        row = bytes([6,1,1,3,1,4,1,0,0,2])
        header = struct.pack('>H',12)
        for i in range(12):
            header += struct.pack('>HIHH',1124+i,122+i*len(row),1,2)
        return header+row*12

    def test_pixels_padding_and_transparency(self):
        self.assertEqual(extract(self.sprites(),0x7c),[[None,1]])
        self.assertEqual(read_palette(self.bmp())[1],(255,255,255))

    def test_pack_and_safe_border(self):
        pack = convert(self.sprites(),self.bmp())
        self.assertEqual(len(pack),392)
        self.assertEqual(pack[:8],HEADER)
        for i in range(12):
            tile = pack[8+i*32:8+(i+1)*32]
            pixels = [p for b in tile for p in (b>>4,b&15)]
            self.assertIn(15,pixels)
            self.assertTrue(all(pixels[y*8+x]==2 for y in range(8)
                                for x in range(8) if x in (0,7) or y in (0,7)))

    def test_invalid_files(self):
        for data in (b'',self.sprites()[:12],self.sprites()[:-2]):
            with self.assertRaises(ValueError):
                convert(data,self.bmp())
        with self.assertRaises(ValueError): read_palette(b'bad')
        with self.assertRaises(ValueError): extract(self.sprites(),0x88)
        data=bytearray(self.sprites()); data[127]=99
        with self.assertRaises(ValueError): extract(data,0x7c)

    def test_duplicate_placeholder_before_full_sprite(self):
        blank=bytes([0,1,0,2])
        full=bytes([6,1,1,3,1,4,1,0,0,2])
        data=(struct.pack('>H',2)+struct.pack('>HIHH',1124,22,1,1)+
              struct.pack('>HIHH',1124,26,1,2)+blank+full)
        self.assertEqual(extract(data,0x7c),[[None,1]])


if __name__ == '__main__': unittest.main()
