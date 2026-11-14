# SimCity 2000 building resources

Converted building artwork in Tandy 8×8 and classic Micropolis 16×16 sizes.
All **144 building IDs, 0x70–0xFF**, are included: houses, apartments,
commercial and industrial buildings, construction/abandoned sites, power
plants, civic services, airport/seaport facilities, arcologies and the dome.
Terrain, roads, bridges and separate animation frames are outside this catalog.

## Downloads and formats

| File | Contents | Bytes |
|---|---|---:|
| [SC2K8.DAT](SC2K8.DAT) | 144 raw 8×8 tiles, 32 bytes each | 4,608 |
| [SC2K16.DAT](SC2K16.DAT) | 144 raw 16×16 tiles, 128 bytes each | 18,432 |
| [sc2k-16.xpm](sc2k-16.xpm) | Classic-size XPM vertical strip, 16×2304 pixels | text |
| [sc2k-8.xpm](sc2k-8.xpm) | Tandy-size XPM vertical strip, 8×1152 pixels | text |
| [SC2K.DAT](SC2K.DAT) | Current game's optional 12-building small-lot pack | 392 |
| [catalog.json](catalog.json) | Names, categories, source dimensions, offsets and palette | JSON |
| [CATALOG.md](CATALOG.md) | Human-readable index of every building | Markdown |

Use GitHub's **Raw / Download raw file** action for binary downloads.
The two complete DAT libraries have no header: pixels are row-major,
two 4-bit palette indices per byte, left pixel in the high nibble.
Record `i` represents SC2000 ID `0x70 + i`; byte offsets are `i*32` and
`i*128`. The palette is the 16-colour IBM PC/Tandy palette listed in
`catalog.json` and embedded in the XPMs. SHA256SUMS verifies generated files.

**The full libraries are not replacements for the original TILES.DAT.**
Their IDs and counts differ from Micropolis's 960-tile atlas. The 16×16 XPM
is a classic-size artwork library, not a plug-and-play SimCity Classic game
file: assigning gameplay IDs and composing larger footprints remains future
integration work. Each source building is fitted into one icon, including
buildings whose original footprint is 2×2, 3×3 or larger.

## Using the current integration

Copy `SC2K.DAT` beside the newly built `MICROPOL.EXE` and normal `TILES.DAT`.
Developed custom 1×1 commercial/industrial lots use eight commercial and
four industrial sprites. Density stages choose groups of sprites; world
coordinates choose repeatable variants. Original residential sprites,
authentic larger buildings, growth rules, jobs, power, and CTY/ZON saves
remain unchanged. Remove the optional pack to use the original fallback.
The Tandy CI release includes this optional pack beside the normal art.

## Reproducing the conversion

Use the Windows 95 Special Edition files from a local installation:
`WIN95/SC2K/DATA/LARGE.DAT` and `WIN95/SC2K/BITMAPS/PAL_MSTR.BMP`.
The disc's top-level DATA directory contains videos and is not the source.
No dependencies beyond Python's standard library are needed.

```sh
python3 tandy-dos/tools/convert_sc2k_buildings.py /path/to/WIN95/SC2K resources/sc2k-buildings
python3 tandy-dos/tests/test_sc2k_converter.py
python3 tandy-dos/tests/test_sc2k_resources.py
```

1. Read the big-endian sprite index: count, ID, absolute offset, height, width.
   The building sprite ID is the SC2000 map ID plus 1000. For duplicate
   IDs choose the largest image (ties keep index order). Some tiny
   placeholders precede their complete sprite: the B7 office tower has
   a 96×5 placeholder followed by the real 96×117 image.
2. Decode rows and runs: row markers, transparent skips, palette-index
   pixel runs, odd-run padding and sprite terminators. Reject invalid
   offsets, dimensions and runs before reading outside the sprite.
3. Read the BMP's BGR palette and map colours to the nearest Tandy RGB value.
4. Preserve aspect ratio and fit inside 6×6 or 14×14 pixels. Reserve a
   one-pixel border, bottom-align, and vote among opaque target-palette
   colours within each source area. Require at least 25% opaque coverage.
5. Fill transparency with green ground (palette index 2), pack the pixels,
   and emit raw DAT files, XPM strips, contact sheets, catalog and hashes.

The process is deterministic. Source-file hashes are recorded in the catalog;
source game archives and local filesystem paths are not included.

## Results and limitations

The initial experiment extracted 12 small C/I buildings at their original
size and compared 16×16 reductions. The Tandy renderer actually uses 8×8
tiles, so its final pack was reduced directly from the original sprites.
Low shops and warehouses retain useful silhouettes; towers and large
facilities lose detail. These are direct reductions, not hand-redrawn
top-down art: the isometric perspective and source lot outlines remain.

The DOS executable compiled with Open Watcom (`-0 -ml -bcl=dos`). Loader,
converter and existing host regression checks passed. The showcase rendered
in DOSBox 0.74-3 and pause input worked. Full gameplay and real Tandy timing
are not verified. Conversion of the remaining categories makes their art
available; it does not add those buildings to the simulation.

### Classic 16×16 contact sheet

IDs increase left-to-right and then top-to-bottom, starting at 0x70.

![All 144 classic-size buildings](preview-16.png)

### Tandy 8×8 contact sheet

![All 144 Tandy-size buildings](preview-8.png)

## Provenance

Artwork is derived from Maxis/Electronic Arts SimCity 2000 supplied for this
conversion. It is distinct from the original Micropolis GPL artwork; this
conversion does not assign a new license to the source images.
Converter code follows the repository's GPL-3.0-or-later licensing.
Format and building-index references:
[sprite format](https://github.com/OpenCity2k/SC2k-docs/blob/master/sprite%20data%20spec.md)
and [building IDs](https://github.com/OpenCity2k/SC2k-docs/blob/master/sc2%20file%20spec.md).
