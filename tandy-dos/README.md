# Micropolis: Tandy 1000 EX native DOS port

**Status: original-art city editor / first engine-port milestone. Not yet a running city simulation.**

This is an experimental native 8086-compatible, **16-bit real-mode DOS**
adaptation of the GPL [Micropolis](https://github.com/SimHacker/micropolis)
project. It targets the NEC V20-equipped Tandy 1000 EX.

## What is now implemented

- Tandy video BIOS mode **9: 320x200, 16 colors**, direct 4-bank B800 framebuffer.
- Fast one-tile scrolling that shifts the existing VRAM and redraws only the exposed edge.
- Read-only import of original 27,120-byte .CTY city files, preserving the
  original 3,120-byte histories/settings header on save.
- Real Micropolis artwork from `micropolis-activity/images/tiles.xpm`.
  All 960 original 16x16 tiles are scaled to 8x8 at build time and quantized
  to the 16-color IBM PC/Tandy palette. Runtime data: **30,720-byte TILES.DAT**.
- Visible cursor, road/power placement, 3x3 R/C/I zones and original 4x4 coal power plant
  placement, with original Micropolis tile IDs/flags, and a simple bulldozer.
- First isolated simulation subsystem: power-network reachability from coal/nuclear
  plant centers, using the original conductor flags and 700/2000 tile capacities.
  Triggered manually using P; no automatic simulation ticking yet.
- **Save as a new .CTY file**, never overwrite an existing file.
- Original demo-city generator and fallback colored glyph art if TILES.DAT
  isn't present.

## Download and test on the EX

Go to the [Tandy DOS workflow](../../actions/workflows/tandy-dos.yml) and
download the latest successful `micropolis-tandy-ex-dos` artifact.

Copy these to one directory on the DOS CompactFlash partition:

```
MICROPOL.EXE
TILES.DAT
HAPPISLE.CTY
```

Then launch either the built-in test map or an authentic Micropolis city:

```dos
MICROPOL
MICROPOL HAPPISLE.CTY
```

The first command displays the built-in generated terrain/buildings.
The second loads the included **original Micropolis city map**. The map
file stays unchanged.

### Keyboard controls

| Key | Action |
| --- | --- |
| Arrow keys or W / A / S / D | Pan the city (fast VRAM shift) |
| I / J / K / L | Move the yellow selection cursor |
| 0 | Inspect (no placement) |
| 1 | Road |
| 2 | Power line |
| 3 | Residential zone |
| 4 | Commercial zone |
| 5 | Industrial zone |
| 6 | Bulldoze (one tile) |
| 7 | Construct 4x4 coal power plant (16 empty tiles required) |
| P | Recalculate powered tiles and show the count in the bottom status bar |
| Space | Apply the selected tool at the cursor |
| F2 | Save a **new** city file in the current directory |
| Esc | Exit and restore DOS video mode |

F2 finds the first available `CITY0001.CTY`, `CITY0002.CTY`,
and so on. It never overwrites a file that already exists. Copy the
result from your CF card for inspection, or load it using
`MICROPOL CITY0001.CTY`.

Construction is intentionally conservative: roads/wires require
empty or wooded non-water terrain, 3x3 zones require nine empty tiles,
and the bulldozer only clears one tile. Complex building-demolition
rules, cost calculations, road connection tile selection and full
simulation tick scheduling are **not yet integrated**. A `BLOCKED` indicator means placement is
disallowed.

**Important:** Although edited maps are written in the original .CTY
file layout, we have not yet integrated the original simulation rules
or validated edited maps as playable cities in another Micropolis port.
Keep your original save files. The generated demo has empty
budget/history metadata and is for renderer/editor tests only.

## Cross-compiler / build

The GitHub Actions workflow compiles with Open Watcom 2.0
(`-0 -ml -bcl=dos`) into a 16-bit MS-DOS MZ executable, not DOS/4GW.

To build manually with Open Watcom for DOS:

```sh
python3 tandy-dos/tools/convert_tiles.py micropolis-activity/images/tiles.xpm TILES.DAT
wcl -bcl=dos -0 -ml -q -fe=MICROPOL.EXE tandy-dos/tandy.c tandy-dos/city.c tandy-dos/editor.c tandy-dos/engine.c
```

Host-side regression tests for loading/saving cities and editing zones:

```sh
cc -std=c89 -Wall -Wextra -pedantic -o test-city tandy-dos/city.c tandy-dos/tests/test_city.c
./test-city
cc -std=c89 -Wall -Wextra -pedantic -o test-editor tandy-dos/city.c tandy-dos/editor.c tandy-dos/tests/test_editor.c
./test-editor
cc -std=c89 -Wall -Wextra -pedantic -o test-engine tandy-dos/city.c tandy-dos/engine.c tandy-dos/tests/test_engine.c
./test-engine
```

## Remaining simulation-port work

The original simulation implementation under
`micropolis-activity/src/sim/s_*.c` still depends on X11/Tcl/Tk,
Unix interfaces, and allocations that are unsuitable for segmented DOS
without adaptation. The **first power-network scan** is now isolated in `engine.c`.
Next steps are to integrate the real city tick/update schedule, zoning
growth and population/economy loops, then budgets, disasters and save-game verification. Game-state
updates will need profiling on the V20, independently of the already
fast map redraw.

Optional serial mouse, original 16x16 zoom view and optimized tile
lookup are also planned. **Do not mistake this editor build for a
finished, playable Micropolis simulation.**

## License

Micropolis and this adaptation remain GPLv3-or-later with original
additional terms and notices in [COPYING](../COPYING).
Original Micropolis and its artwork: Copyright (C) 1989-2007
Electronic Arts Inc. No SimCity trademark rights or affiliation are
claimed. This is a modified implementation, not the original game.
