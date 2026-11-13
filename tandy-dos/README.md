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
- **Per-square freeform zoning** for residential (green), commercial (blue),
  and industrial (yellow): single 1x1 tiles or irregular painted areas.
  External borders join adjacent same-type zones instead of showing an
  artificial grid through every connected zoning area.
- Visible cursor, road/power placement, original 4x4 coal power plant
  placement, and a basic bulldozer. The old 3x3 RCI builder code remains
  available for original-format regression tests but is no longer used
  by the user-facing RCI construction tools.
- First isolated simulation subsystem: power-network reachability from coal/nuclear
  plant centers, using the original conductor flags and 700/2000 tile capacities.
  Triggered manually using P; no automatic simulation ticking yet.
- **Save as a matching .CTY / .ZON pair**, never overwrite any existing
  city or sidecar. The existing .CTY keeps the original map and sprites;
  the optional 3,010-byte .ZON file contains per-tile custom zoning.
  When a city file is loaded, its matching .ZON is loaded automatically
  if valid; otherwise original 3x3 zone centers seed the overlay.
- **Classic city-builder layout:** cyan top menu bar, beveled left-side
  two-column construction toolbox with miniature authentic Micropolis
  tile icons, city window to the right, a compact city overview, and
  two-line bottom status area. All fit native 320x200x16 Tandy video.
- **On-screen System / Tools / View / Help menus**, keyboard shortcuts,
  and clickable sidebar toolbox.
- **Optional DOS mouse support** using INT 33h, with its own small software
  arrow to work in Tandy graphics mode 9 even if the mouse driver's own
  cursor graphics do not support that video mode.
- Left-click to select a tool. Hold left mouse to preview R/C/I zones,
  roads or power lines as light-colored stippled ghosts with a running
  cost. Beyond-budget cells show red X marks. Release to purchase only
  affordable cells, or press Esc to cancel for free. Right-click/right-drag
  with an RCI tool erases zoning only. Keyboard Space buys one tile.
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

The top row is a clickable menu bar: **SYSTEM / TOOLS / VIEW / HELP**.
The left side contains eight clickable beveled tool buttons (0-7),
arranged in two columns and four rows. The main city viewport fills
the rightmost **240x176 pixels (30x22 tiles)**, with a compact world
overview map below the buttons and a 16-pixel status footer. Menus contain real
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

Mouse controls: left-click menus and sidebar tools as before. With
Residential, Commercial, Industrial, Road or Power selected, **left-drag
to preview a path** in ghost colors. The bottom status line displays
the projected cost (C), funds remaining (R), accepted tiles (+), and
rejected tiles (-). Crossed-out squares cannot be built or are too
expensive. **Nothing is changed or paid until you release left mouse**.
Press Esc while dragging to cancel at no cost. Right-drag with an RCI
tool erases zoning only. Keyboard Space buys one square using the same
fund checks. Bulldozing remains immediate, without preview.

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
| 3 | Residential zoning brush (green, 1x1 or larger) |
| 4 | Commercial zoning brush (blue, 1x1 or larger) |
| 5 | Industrial zoning brush (yellow, 1x1 or larger) |
| 6 | Bulldoze (one tile) |
| 7 | Construct 4x4 coal power plant (16 empty tiles required) |
| P | Recalculate powered tiles and show the count in the bottom status bar |
| Space | Purchase one zone, road or wire tile at cursor, budget permitting |
| F2 | Save a **new** city file in the current directory |
| F / T / V / H (or F10 for System) | Open System / Tools / View / Help menu |
| Arrow keys + Enter (menu open) | Navigate and choose menu items |
| Esc | Close menu/help window; otherwise exit and restore DOS video mode |

F2 finds the first **free pair** of `CITY0001.CTY` /
`CITY0001.ZON`, `CITY0002.CTY` / `CITY0002.ZON`, etc.
It never overwrites either file. **Keep both files together**, and load
as usual with `MICROPOL CITY0001.CTY`. The zoning file is loaded
automatically if present. A vanilla Micropolis program can still read
the .CTY, but it will not understand custom zones stored in .ZON.

Construction is intentionally conservative: roads/wires and custom zones
require empty/wooded non-water terrain, or a custom zone of the same
type may be drawn over its original occupied building sprites.
Custom zones can be 1x1 or any freehand shape, but roads, water and
unrelated structures are not silently overwritten. The bulldozer
clears one tile; zoning erase removes *only the zoning layer*. Complex building-demolition
rules, cost calculations and full simulation tick scheduling are
**not yet integrated**. Road/wire adjacency tile selection is implemented,
but rail/bridge crossings and terrain correction need further work. A `BLOCKED` indicator means placement is
disallowed.

**Important:** New single-square zone art uses existing authentic
Micropolis sprites: a small house, the commercial empty-zone center,
and the industrial empty-zone center. These are visual placeholders
on undeveloped dirt, not actual generated businesses or factories.
The new per-tile zone overlay is **not consumed by the original 3x3
simulation yet**. We still need to port the city-growth rules and
adapt them to support small buildings, including true 1x1 commercial
and industrial assets when suitable. Original city buildings and
sprite IDs remain untouched. Treat this build as a zoning-editor
prototype, not a completed city simulation. Keep your original saves.

## Cross-compiler / build

The GitHub Actions workflow compiles with Open Watcom 2.0
(`-0 -ml -bcl=dos`) into a 16-bit MS-DOS MZ executable, not DOS/4GW.

To build manually with Open Watcom for DOS:

```sh
python3 tandy-dos/tools/convert_tiles.py micropolis-activity/images/tiles.xpm TILES.DAT
wcl -bcl=dos -0 -ml -q -fe=MICROPOL.EXE tandy-dos/tandy.c tandy-dos/city.c tandy-dos/editor.c tandy-dos/connections.c tandy-dos/engine.c tandy-dos/ui.c tandy-dos/mouse.c tandy-dos/zoning.c
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
cc -std=c89 -Wall -Wextra -pedantic -o test-zoning tandy-dos/city.c tandy-dos/zoning.c tandy-dos/tests/test_zoning.c
./test-zoning
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

## Horizontal road centerline fix

The real Tandy graphics test showed that horizontal roads lost their white
dashed lane markings. This was a graphics-conversion issue, not a road
connection issue: in the original 16x16 tile artwork, the horizontal dashes
are only one pixel high (source row 7). Our 2x2 majority downsampling
discarded that detail. The converter now specifically preserves paired
original white road-marking pixels in output row 3 of the 8x8 tile, including
horizontal traffic and T-junction variants. The change affects only
`TILES.DAT`, not the city files, scroll logic, or road-placement rules.

**When upgrading, copy both the new TILES.DAT and MICROPOL.EXE** to the
same directory on your DOS CF card. The stripe fix needs the new
`TILES.DAT` even if the executable is unchanged.

## Classic DOS city layout

The earlier full-width map with a horizontal text tool strip has been
replaced with a classic late-1980s/early-1990s city-builder layout, using
the supplied original SimCity DOS screen as visual inspiration.

- Cyan SYSTEM/TOOLS/VIEW/HELP top-row menu, with working submenus.
- Gray left sidebar with eight large beveled icon buttons, each using
  authentic reduced Micropolis graphics and a short tool label.
- Right-side city at 30 tiles wide by 22 tiles tall, panning within the
  original 120x100 world. It remains a 16-color native Tandy mode-9
  framebuffer, not a VGA-only UI.
- Lower left mini-overview of the actual loaded map and bright viewport
  outline to show what area you are viewing.
- Two-line lower status strip with the selected tool and cursor map
  coordinates. The `FUNDS: --` line is intentionally a placeholder until
  the real Micropolis economic simulation is ported.

The fast-scroll routine now shifts only the 240-pixel-wide right-side
city area instead of sliding the controls and map together. This reduces
VRAM traffic per pan, but the new toolbox and minimap should be
performance-tested on the real V20.

Tool numbers, keyboard panning, map editing, save-as, power scan,
optional DOS mouse handling and `/NOMOUSE` are unchanged.

## Flexible single-tile zoning (new)

The original 3x3 stamp is no longer the primary RCI editor action.
Select RES, COM or IND, then left-drag to paint custom irregular areas,
even if the desired area is only one square. The colors are classic
light-green, light-blue and yellow; borders are drawn around connected
areas, not between neighboring same-type cells. Sprite rendering,
underlying city map data, and the faster right-window scroll are
unchanged. Brush strokes interpolate across gaps when the V20's COM
mouse sampling misses intermediate cells.

Right-drag while an RCI tool is selected **removes just zoning**.
It never bulldozes roads or buildings. Keyboard Space paints one tile
at the yellow cursor.

Edited zones are held in a static 3,000-byte 2-bit-per-tile grid and saved to
`CITYnnnn.ZON` sidecars, preserving the .CTY map. The parser validates
the exact size, magic/version, map dimensions, and the 16-bit checksum
before applying a sidecar. On original cities without a sidecar,
3x3 zone centers are used to reconstruct their existing colored zones.

The original Micropolis simulation engine does **not** yet grow
buildings within arbitrary per-square zoning. A future growth port
must choose appropriate original sprites by parcel size, including
one-square commercial and industrial artwork, while respecting
roads and power. This build proves editing, color borders, and
persistence without claiming automatic development.

## Transactional drag-to-build preview

The construction editor now supports a two-stage drag-and-release
workflow for Residential / Commercial / Industrial / Road / Power.
During the drag the map and balance stay unchanged. Tiles to be built
are rendered as light stippled ghosts over the original graphics,
while blocked or unaffordable squares show an X. The bottom bar reports
live construction cost and funds remaining. Repeat passes over an
already previewed cell do not add to the price.

Releasing the left button applies only the affordable cells and charges
once. Esc cancels before committing. Missing mouse samples on the
7 MHz NEC V20 are interpolated to avoid gaps in dragged roads/zones.

**Funds are read from the actual original Micropolis CTY save header**
(the MiscHis[50..51] 32-bit funds field), not merely a placeholder.
Balances are debited only upon committing construction and persist in
new CTY save files. The demo city starts with a test balance of $20,000.

Current editor costs: Road $10/tile, Power line $5/tile, RCI zoning
$12/tile. This rounds the classic $100 per nine-tile 3x3 zone to an
individual-tile price. The budget is currently construction-only;
tax income, city economy and full simulation growth are not ported.
Bulldozing and coal plant placement retain their earlier simplified
editor behavior and have no transactional preview yet.

Use F2 to save a new CTY/ZON pair, and always keep the companion ZON
file for irregular zoning. Neither preview nor cancel rewrites saves.
