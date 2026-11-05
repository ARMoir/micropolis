# Micropolis: Tandy 1000 EX native DOS port

**Status: original-art city editor / first engine-port milestone. Not yet a running city simulation.**

This is an experimental native 8086-compatible, **16-bit real-mode DOS**
adaptation of the GPL [Micropolis](https://github.com/SimHacker/micropolis)
project. It targets the NEC V20-equipped Tandy 1000 EX.

## What is now implemented

- Tandy video BIOS mode **9: 320x200, 16 colors**, direct 4-bank B800 framebuffer.
- Fast one-tile scrolling that shifts the existing VRAM and redraws only the exposed edge.
- Original Micropolis road/wire connection masks, with automatic corner, T-junction,
  four-way and straight-road orientation when placing or bulldozing nearby tiles.
- Read-only import of original 27,120-byte .CTY city files, preserving the
  original 3,120-byte histories/settings header on save.
- Real Micropolis artwork from `micropolis-activity/images/tiles.xpm`.
  All 960 original 16x16 tiles are scaled to 8x8 at build time and quantized
  to the 16-color IBM PC/Tandy palette. Vertical traffic-car sprites are
  optionally smoothed with the authentic clean road art (tile 67), since
  those cars collapse into visual noise at 8x8. Use the converter's
  `--preserve-traffic` flag to produce the unmodified 8x8 traffic variants.
  Runtime data: **30,720-byte TILES.DAT**.
- Visible cursor, road/power placement, 3x3 R/C/I zones and original 4x4 coal power plant
  placement, with original Micropolis tile IDs/flags, and a simple bulldozer.
- First isolated simulation subsystem: power-network reachability from coal/nuclear
  plant centers, using the original conductor flags and 700/2000 tile capacities.
  Triggered manually using P; no automatic simulation ticking yet.
- **Save as a new .CTY file**, never overwrite an existing file.
- **On-screen File / Tools / View / Help menus**, keyboard shortcuts, and
  clickable tool strip.
- **Optional DOS mouse support** using INT 33h, with its own small software
  arrow to work in Tandy graphics mode 9 even if the mouse driver's own
  cursor graphics do not support that video mode.
- Left-click to select a tool or build, right-click for Tools menu, and
  click-drag to paint roads, wires or bulldozer tiles. No mouse required.
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

### New menus and mouse support

The top row is a clickable menu bar: **FILE / TOOLS / VIEW / HELP**.
The second row is a clickable tool strip (0-7). Menus contain real
commands for saving a copy of the city, exit, changing tools, manual
power scan, center cursor, and help pages. They are available without
a mouse: press **F** (File), **T** (Tools), **V** (View), **H** (Help),
or **F10** (File). In an open menu use arrow keys, Enter and Esc.
Esc closes a menu or help window before it exits the program.

**A DOS mouse driver must be loaded before running MICROPOL.**
Micropolis checks for the Microsoft-compatible INT 33h API. For a
serial mouse, [CuteMouse](https://cutemouse.sourceforge.net/) is a
lightweight option. With CuteMouse on COM1, for example:

```dos
CTMOUSE /S1
MICROPOL HAPPISLE.CTY
```

Use `CTMOUSE /S2` if the mouse actually uses COM2. The COM number
depends on the hardware and how the serial interface is configured.
The program does **not** talk to serial registers directly; the
external TSR detects the device and handles mouse input. A generic
`MOUSE.COM` driver that implements INT 33h also works.

If you do not have a mouse attached, Micropolis still works using the
keyboard alone. To bypass mouse detection explicitly, run:

```dos
MICROPOL HAPPISLE.CTY /NOMOUSE
```

Mouse controls: left-click a **menu name** to open its dropdown,
left-click a **tool** in the second row to select it, left-click the
map to apply that tool, and right-click for the **Tools** dropdown.
Click and drag while using **Road**, **Power line**, or **Bulldoze** to
paint consecutive tiles. The map's yellow selection outline follows
the mouse, while a small black-and-white pointer shows the exact
pointer position. DOS keyboard shortcuts remain functional.

**Mouse movement and the cursor need testing on the real Tandy 1000
EX**, especially with a serial mouse driver. This hardware has unusual
video mode-9 memory and mouse-driver coordinate behavior; the
application maps conventional INT 33h 640-pixel virtual X coordinates
to its 320-pixel Tandy screen.

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
| F / T / V / H (or F10 for File) | Open File / Tools / View / Help menu |
| Arrow keys + Enter (menu open) | Navigate and choose menu items |
| Esc | Close menu/help window; otherwise exit and restore DOS video mode |

F2 finds the first available `CITY0001.CTY`, `CITY0002.CTY`,
and so on. It never overwrites a file that already exists. Copy the
result from your CF card for inspection, or load it using
`MICROPOL CITY0001.CTY`.

Construction is intentionally conservative: roads/wires require
empty or wooded non-water terrain, 3x3 zones require nine empty tiles,
and the bulldozer only clears one tile. Complex building-demolition
rules, cost calculations and full simulation tick scheduling are
**not yet integrated**. Road/wire adjacency tile selection is implemented,
but rail/bridge crossings and terrain correction need further work. A `BLOCKED` indicator means placement is
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
wcl -bcl=dos -0 -ml -q -fe=MICROPOL.EXE tandy-dos/tandy.c tandy-dos/city.c tandy-dos/editor.c tandy-dos/connections.c tandy-dos/engine.c tandy-dos/ui.c tandy-dos/mouse.c
```

Host-side regression tests for loading/saving cities and editing zones:

```sh
cc -std=c89 -Wall -Wextra -pedantic -o test-city tandy-dos/city.c tandy-dos/tests/test_city.c
./test-city
cc -std=c89 -Wall -Wextra -pedantic -o test-editor tandy-dos/city.c tandy-dos/editor.c tandy-dos/connections.c tandy-dos/tests/test_editor.c
./test-editor
cc -std=c89 -Wall -Wextra -pedantic -o test-engine tandy-dos/city.c tandy-dos/engine.c tandy-dos/tests/test_engine.c
./test-engine
cc -std=c89 -Wall -Wextra -pedantic -o test-ui tandy-dos/ui.c tandy-dos/tests/test_ui.c
./test-ui
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

## Road rendering fix

The first authentic-art hardware test revealed noisy vertical roads. Two
issues were addressed: previously, the DOS editor always placed road tile
`66` (horizontal), instead of selecting `67` for vertical streets and
the correct turn/junction shapes from `w_con.c`. The converter now also
replaces the tiny 8x8 traffic-vehicle detail on vertical road variants with
the original clean Micropolis vertical-road art, avoiding speckled lanes.
Other tiles keep their original converted appearance. This needs a fresh
real-hardware check.
