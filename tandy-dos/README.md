## Enhanced VGA: source-derived 256-color artwork

Automatic VGA and `/VGA` now load `VGATILES.DAT`, `VGANEWER.DAT` and
`VGA.PAL`. Keep all three files beside MICROPOL.EXE. The build generates
VGA-only shading from the original full-color 16x16 Micropolis tiles and
the higher-detail 16x16 NEWER library, preserving fine color mixtures
lost in the 16-color conversion. The resolution remains 320x200.
VGA ground uses smooth tan shades instead of the Tandy checker.

`/VGA16` selects the previous compatibility output for comparison.
Missing assets or insufficient conventional memory fall back to that
output. The exit summary identifies which renderer actually ran.
Enhanced VGA needs about 173 KB more conventional memory for its two
art packs and 64 KB detail buffer. Tandy allocates none of these buffers
and continues to use its existing artwork and palette.

The low nibble of every VGA art pixel retains its exact legacy color
key. Cursor, zone borders and construction ink override differing keys;
driveways and forest-edge ground rules remain shared. Menus and inspector
pages temporarily use compatibility colors so no hidden art leaks through
opaque text backgrounds. Scrolling moves the detail buffer with the map.
City formats, building selection and rectangular NEWER rules are unchanged.

## Light-brown ground and forest-edge grass

Clear ground now uses a fine brown/light-gray checker for a lighter tan
on the shared 16-color Tandy/VGA palette. Tiles touching forest keep green
ground; touching includes all eight neighbors, and forest itself stays
green. Developed lots covering old forest do not count as visible trees.

For other city artwork, dark-green background pixels (palette index 2)
receive the same tan treatment away from forest. Other palette colors,
including bright green, remain unchanged. NEWER driveway details are
composited first; zoning outlines and construction previews are drawn
after the soil pass. DAT assets and saved city data are untouched.
Clearing forest repaints neighboring tiles, including diagonal neighbors.

## Rectangular NEWER artwork is reserved for 3x2 lots

NEWERM8 sprites whose entire top tile row is blank green are used only
as 3x2 artwork, never as 3x3 facades. Imported-city 3x3 replacements
select full-height artwork. Older .NBL square records using rectangular
art receive a valid full-height replacement when artwork is loaded,
preserving their footprint, zoning and underlying .CTY data. Existing
3x2 records remain 3x2. The DAT files and save format are unchanged.

## VGA and Tandy automatic video selection

The same MICROPOL.EXE now selects VGA mode 13h (320x200) when the BIOS
reports an active VGA display, otherwise it tries native Tandy mode 9.
Use `MICROPOL /VGA` or `MICROPOL /TANDY` to override selection.
Options can be combined with a city filename and `/NOMOUSE`.
A forced VGA mode requires a VGA BIOS; unsupported modes report an error.

Both outputs use the existing 16 colors, tile packs, UI, simulation and
city saves. VGA allocates a separate 32 KB packed screen buffer and
expands changed rows to VGA memory. Tandy still draws directly into
B800 VRAM and does not allocate this buffer. VGA text uses the BIOS
8x8 font drawn into the packed buffer, so later screen updates preserve it.

`/TIMEDEMO` includes VGA presentation on every pan. `/BENCH` remains a
Tandy-only live-VRAM benchmark and is rejected in VGA mode rather than
reporting RAM-copy timings as video performance. VGA is a compatibility
output path, not a higher-resolution or 256-color art upgrade.
CI checks pixel expansion and runs automatic/forced VGA and Tandy timedemos
in DOSBox. Physical VGA and Tandy hardware testing is still recommended.

# Micropolis: Tandy 1000 EX native DOS port

**Status: experimental playable city simulation with phased RCI development and annual taxation. Incomplete compared with original Micropolis.**

This is an experimental native 8086-compatible, **16-bit real-mode DOS**
adaptation of the GPL [Micropolis](https://github.com/SimHacker/micropolis)
project. It targets the NEC V20-equipped Tandy 1000 EX.

## What is now implemented

- Tandy video BIOS mode **9: 320x200, 16 colors**, direct 4-bank B800 framebuffer.
- Fast one-tile scrolling that shifts existing VRAM and redraws only the exposed edge.
  Now uses a precomputed four-bank scanline table and overlapping 16-bit word
  copies, without changing pixels, the original renderer, or the game screen.
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
- **Running phased simulation**, including time, automatic road/power checks,
  1x1 RCI lot growth in three densities, original vacant and occupied 3x3-zone density growth
  using all nine authentic original building sprites, a population/jobs census,
  and yearly taxes and power/road maintenance. DOS BIOS clock scheduling
  processes only 80..320 map cells per interval so the mouse, keyboard and
  fast-scrolling renderer remain responsive on a V20.
- Power-network reachability from coal/nuclear plant centers, using the
  original conductor flags and 700/2000 tile capacities. The network is
  recomputed after infrastructure edits or initial load, rather than
  walking the entire map on every graphics frame.
- **Save as a matching .CTY / .ZON pair**, never overwrite any existing
  city or sidecar. The original .CTY stores real city funds, clock, taxes
  and native building tiles, and the companion **6,010-byte ZON v3**
  stores per-tile custom zoning and its 2-bit development density. Existing
  ZON v2 files are still accepted (initial density zero), and an imported
  original city without a sidecar seeds zoning from 3x3 zone centers.
- **Compact classic city-builder layout:** cyan menu bar (row 0), city
  statistics just below it (row 1), a clean 64-pixel two-column toolbox
  with recognizable 16x16 pixel icons and a larger minimap
  directly underneath, 256x168 city window to the right (32x21 tiles),
  and two separate debug/construction rows at the bottom. All fit
  native 320x200x16 Tandy video.
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

Then launch either the built-in test map or the road-converted Happisle city:

```dos
MICROPOL
MICROPOL HAPPISLE.CTY
```

The first command opens the built-in **powered starter neighborhood**
with an original 4x4 coal plant, connected power lines, roads, and empty
residential/commercial/industrial lots ready to grow. The second loads
the included **road-converted Happisle city map** and automatically focuses
on the most developed residential neighborhood, rather than its mostly
empty northwest ocean. Its buildings can develop from vacant and
already occupied original 3x3 plots. The input city file is never
overwritten unless you explicitly save a new copy.

### New menus and mouse support

The top row is a clickable menu bar: **SYSTEM / TOOLS / VIEW / HELP**.
The second row displays city date, population, jobs and treasury funds.
The 64-pixel left sidebar contains eight compact clickable icon buttons
arranged in two columns and four rows, with the enlarged minimap below.
The visual grouping is DOZ/ROAD, POWER/RES, COM/IND, INSPECT/COAL;
number keys **0..7** still select their original tools. The selected
icon is outlined in yellow, and the bottom status names the selected
function rather than squeezing labels into the icon buttons.
The city uses **256x168 pixels (32x21 tiles)** on the right. The two
bottom 8-pixel rows are reserved for world/mouse coordinates, selected
tool, simulation diagnostics, actions and construction-preview costs. Menus contain real
commands for saving a copy of the city, exit, changing tools, manual
power scan, clockwise/counterclockwise map rotation, center cursor,
and help pages. They are available without
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
| G | Pause / resume the live simulation |
| Tab | Jump the viewport and selection cursor to the latest residential, commercial or industrial development (if any) |
| + / - | Increase / decrease simulation speed (1x to 4x) |
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

**Important:** The first live growth rules are now functional, but they
are a deliberately simplified adaptation of the original Micropolis
engine, not a complete port. One-square empty zones grow through three
stages using the existing original 8x8 tiles, while authentic vacant
3x3 plots and previously developed classic buildings can advance to
higher authentic density stages using complete original 3x3 artwork.
Imported input files are not overwritten.
Commercial and industrial 1x1 buildings use the complete optional
`NEWER1.DAT` small-building pack when installed, or a complete
8x8 fallback building silhouette when it is absent. They never use
cropped center tiles from original 3x3 commercial/industrial buildings.
See the optional-pack instructions below.
The original traffic, pollution, land-value, policing, disasters,
historical census and detailed city economics remain to be ported.
Keep your original saves.

## Cross-compiler / build

The GitHub Actions workflow compiles with Open Watcom 2.0
(`-0 -ml -bcl=dos`) into a 16-bit MS-DOS MZ executable, not DOS/4GW.

To build manually with Open Watcom for DOS:

```sh
python3 tandy-dos/tools/convert_tiles.py micropolis-activity/images/tiles.xpm TILES.DAT
wcl -bcl=dos -0 -ml -q -fe=MICROPOL.EXE \
  tandy-dos/tandy.c tandy-dos/city.c tandy-dos/editor.c \
  tandy-dos/connections.c tandy-dos/engine.c tandy-dos/ui.c \
  tandy-dos/mouse.c tandy-dos/zoning.c tandy-dos/placement.c \
  tandy-dos/simulation.c tandy-dos/video_ops.c tandy-dos/menu_font.c \
  tandy-dos/view_rotation.c tandy-dos/tool_art.c tandy-dos/infrastructure.c \
  tandy-dos/small_lot_art.c
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
cc -std=c89 -Wall -Wextra -pedantic -o test-video-ops tandy-dos/video_ops.c tandy-dos/tests/test_video_ops.c
./test-video-ops
cc -std=c89 -Wall -Wextra -pedantic -o test-zoning tandy-dos/city.c tandy-dos/zoning.c tandy-dos/tests/test_zoning.c
./test-zoning
```

## Remaining simulation-port work

### Optional NewerSource commercial/industrial small-lot art

Build the optional pack from a local Windows 95 Special Edition installation:

```sh
python3 tandy-dos/tools/convert_sc2k_lots.py /path/to/WIN95/SC2K NEWER1.DAT
```

Place `NEWER1.DAT` beside `MICROPOL.EXE` and `TILES.DAT` in the working
directory. The 392-byte file contains eight commercial and four industrial
sprites extracted from `DATA/LARGE.DAT` using `BITMAPS/PAL_MSTR.BMP`.
Conversion fits each sprite inside a 6x6 area of its 8x8 Tandy tile,
reserving the outer pixels for the existing connected-zone borders.
This is a direct palette/size conversion; the source perspective is retained.

Only developed custom 1x1 commercial and industrial lots use the pack.
Three existing density stages choose progressively larger buildings;
world coordinates supply stable variety without extra save data or RNG use.
Residential art, vacant lots, authentic 3x3 buildings, CTY/ZON formats,
growth rules, jobs and power rules are unchanged. Map rotation keeps the
sprites upright. Removing the pack restores the original fallback art.
Missing, truncated, oversized or unsupported pack files are ignored safely.

CI tests the converter with synthetic data and the loader with synthetic
tiles. The optional runtime pack and complete 144-building libraries in
8×8 and classic 16×16 formats are available in
[resources/sc2k-buildings](../resources/sc2k-buildings/README.md), with the
full conversion process, catalog and previews. The runtime pack ships
alongside the standard art in the Tandy release.

Host checks:

```sh
python3 tandy-dos/tests/test_sc2k_converter.py
cc -std=c89 -Wall -Wextra -pedantic -o test-small-lot-art \
  tandy-dos/small_lot_art.c tandy-dos/tests/test_small_lot_art.c
./test-small-lot-art
```

### Compact 3x2 NEWER buildings

When `NEWERM8.DAT` loads, the Tandy engine examines the packed Tandy
palette pixels in the three upper 8x8 sprites of each 3x3 NEWER design.
Only a top row made entirely of palette color 2 (green ground) qualifies.
A qualifying NEWER structure can develop as a genuine **3-wide x 2-high**
lot, using the six original bottom sprites with no rescaling or changes to
the DAT file. Other NEWER designs remain 3x3, and choosing an original
TILES style continues to reserve a complete 3x3 building.

The six-tile footprint is used for build validation, ownership, outer
zone lines, demolition, save/load, and the tile inspector. The spare
source-ID range 0xC6..0xD9 distinguishes new cropped records from older
3x3 records in the existing five-byte NBL format: no old cities or
sidecars require conversion. Frontage decals remain disabled on cropped
sites until a rectangle-aware road-facing decal is available.

### Simulation work still outstanding

The original implementation under `micropolis-activity/src/sim/s_*.c`
contains detailed population, growth, traffic, pollution, police/fire,
disasters and budget rules that have **not been fully ported**. The
first DOS-friendly adaptation covers phased scans, a running city
clock, three-level custom small-lot growth, basic demand, an annual
tax/maintenance cycle, original 3x3 development and the real
network power scan. Complex effects and the full original economy
are still future work. Native V20 hardware timings remain unmeasured;
the phased scheduler may need tuning after testing.

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
- Narrow 64-pixel left sidebar with eight compact beveled icons: the
  original recognizable road tile, retained custom bulldozer and magnifier,
  and five new full 16x16 native-color pictograms for power, residential,
  commercial, industrial and coal. No tiny overlapping labels.
- Right-side city at 32 tiles wide by 21 tiles tall, panning within the
  original 120x100 world, in 16-color native Tandy mode 9.
- Enlarged 56x44-pixel minimap below the buttons, with a bright camera
  outline. Panning changes only the outline pixels rather than
  redrawing the entire minimap on every scroll.
- Top statistics row displays month/year, population, jobs and the live
  treasury balance. It is independent of the menu row directly above.
- Two debug rows at the bottom show the active tool, world-map cursor
  coordinates, mouse screen coordinates, cumulative growth, serviced lots,
  simulation speed and action or pending construction cost.

The fast-scroll routine now shifts only the 256-pixel-wide right-side
city area rather than the controls. This is 32 tiles wide, compared
with the previous 30, while scrolling remains limited to the city.

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

The current phased simulator **does grow individual 1x1 lots**, through
three occupancy stages, using the existing original tile artwork.
A complete implementation of the original city evaluation and services
is still to come.

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
basic tax income and RCI growth are now running, but the complete
original city-economy model has not been ported.
Bulldozing and coal plant placement retain their earlier simplified
editor behavior and have no transactional preview yet.

Use F2 to save a new CTY/ZON pair, and always keep the companion ZON
file for irregular zoning. Neither preview nor cancel rewrites saves.

## Live simulation milestone

**Controls:** G pauses and resumes; + / - adjust simulation speed
between 1x and 4x. The top city-statistics row shows the month/year,
population, total jobs and treasury funds. The bottom diagnostic row
shows cumulative growth (G), serviced buildable lots (R), speed,
pause state and the most recent action. A second bottom row shows
the current tool, world-map cursor and mouse screen position (or
M:OFF if there is no DOS mouse driver). With G=0 and R=0 over time,
check road and electrical access before expecting development.

Automatic city updates use small timed batches (up to 320 map squares
per update), keyed to the DOS BIOS time-of-day timer. The simulation
advances even when you use keyboard-only /NOMOUSE mode; it does not
block waiting for keyboard input. To avoid corrupting menu and preview
graphics, updates temporarily stop while a menu/dialog or an active
construction preview is on screen.

Newly zoned 1x1 lots develop only when they have road access and
adjacent **powered** infrastructure. Residential builds first; jobs
depend on housing demand. Loss of road access or electricity gradually
reduces development. Vacant and existing original 3x3 plots can progress through native
nine-tile building density stages when power, road and demand permit;
original sprite layouts remain coherent. The underlying map tiles of
custom 1x1 zones do not change, and their growth levels are saved in
the new backward-compatible ZON v3 sidecar.

Each full map scan advances four classic Micropolis CityTime units,
equivalent to one game month. At the end of each simulated year,
a simple RCI-based tax income and road/wire/plant maintenance cost
are applied to saved funds. The tax rate is read from the original
Micropolis CTY header. This is an **experimental economy**, not the
complete original Micropolis budget system.

To test growth immediately without finding a suitable location in an
existing save, run `MICROPOL` without arguments: a powered sample
neighborhood appears in view, with roads, a coal plant, and empty RCI
zones. Use G and +/- to explore simulation behavior. On the real
Tandy EX, please evaluate smoothness during city development and
whether the selected original 1x1 commercial/industrial artwork
looks coherent.

To meet the 16-bit linker’s **64KB DGROUP** limit, the power scanner's
24KB BFS work queue is now allocated once from the large-model
far heap instead of permanent near/static memory. The simulation
itself uses no large extra map arrays other than the 3KB
per-tile development bitmap.

## November 2026 real-Tandy UI and growth corrections

Real-hardware feedback showed that the toolbar was being repainted
too often, its text could slide across the city, and imported cities
appeared to have no new construction even while the clock advanced.

- Replaced BIOS teletype text output (which could scroll the whole
  graphics framebuffer at the bottom row) with explicitly positioned,
  non-scrolling BIOS character writes.
- City stats and both diagnostic rows are updated **only at changed glyph cells**.
  Dragging no longer redraws the entire icon toolbox; selecting a
  tool redraws just the former and new tool buttons. Coal and bulldoze
  button highlighting now matches their actual tool IDs.
- Sidebar icons use more distinct recesses, selection borders, and
  text backgrounds.
- Original **already developed** residential/commercial/industrial
  zones can now grow to higher original nine-tile density levels;
  we no longer restrict development to empty 3x3 plots.
- Bottom diagnostic status reports cumulative growth **G** and powered,
  road-served buildable opportunities **R**. If R=0, there is no
  eligible development to grow; if G=0 over time, inspect road and
  power availability.
- A new CI regression runs the actual HAPPISLE.CTY file for one
  simulated year and asserts that existing buildings develop. It
  prints growth, ready lots, and road/power bottlenecks.

The revised graphics and speed have been compiled and tested in CI,
but only the real Tandy 1000 EX can validate the screen and input
feel. As before, keep existing CTY/ZON save pairs together and save
to new filenames for safety.

## Compact 320x200 interface update

The current screen has two reserved top rows (SYSTEM / TOOLS / VIEW /
HELP, then date / POP / JOB / cash), and two bottom rows for input and
simulation diagnostics. The city occupies x=64..319, y=16..183, so it
now shows **32 columns by 21 rows** of actual Micropolis terrain.
The left toolbar occupies only x=0..63 and uses a compact 2x4 grid of
30x24-pixel buttons; the minimap is directly below it at x=4..59,
y=136..179. All eight original tool IDs, keyboard shortcuts, mouse
hitboxes and 16-color artwork are retained.

Mouse diagnostics `M:xxx,yyy` refer to screen pixel coordinates, and
`MAP:xxx,yyy` indicates the selected world tile. The bottom-most row
shows either `G` (growth), `R` (serviced buildable lots), simulation
speed and the last action, or live cost / affordable construction
quantities while dragging. Original date, population, jobs and saved
funds appear only at the top and are never mixed with input debugging.

For performance, the minimap camera rectangle is now updated by
restoring only its previous border and drawing the new border on each
scroll. The whole minimap is only reconstructed when the underlying
city terrain or zoning changes, keeping the narrow toolbar stable and
reducing VRAM writes on the NEC V20.

## Low-level Tandy/V20 renderer optimizations (no graphics rewrite)

The original 320x200, 16-color mode-9 renderer, converted TILES.DAT,
menu layout, 8x8 tile graphics, mouse and zoning overlays are **unchanged**.
The following hot paths were optimized based on techniques seen in
jhhoward/WolfensteinCGA's Tandy video implementation:

- **400-byte lookup table:** each y=0..199 maps directly to its four-bank
  VRAM offset. This replaces shifts, multiplication and additions
  when addressing every pixel and tile row. Table initialized once
  on startup.
- **Word-sized blits:** the original packed 8x8 tile data is now copied
  as two 16-bit writes per line instead of four 8-bit writes; solid
  background tiles also use two aligned 16-bit writes per row.
- **Overlap-aware word scrolling:** the four existing one-tile scrolling
  branches are unchanged visually, but internal VRAM span copies use
  16-bit words instead of individual bytes. Horizontal left/right
  copies still use opposite overlap-safe directions, and vertical
  copies still keep the exact original scanline ordering.
- **No page flipping:** Tandy 0x3DF page switching was deliberately
  avoided, since it risks graphics tearing/mouse-cursor compatibility
  on actual EX hardware and is unnecessary for the current incremental
  tile renderer.

These changes are built with Open Watcom's **8086 target (-0)**.
The host regression test `test-video-ops` compares the entire
32KB four-bank VRAM image against byte-reference copies for all four
scroll directions, including scanline-bank transitions.

### Actual Tandy timing test

For a repeatable hardware measurement, run:

```dos
MICROPOL /BENCH
```

You can also test an existing city without modifying it:

```dos
MICROPOL HAPPISLE.CTY /BENCH
```

The benchmark enters Tandy video mode 9 but uses an isolated test
screen rather than repeatedly rebuilding the in-game HUD. It measures
**two city-tile-only renders** (the original 8x8 tiles and overlays,
without the toolbar, minimap or status strips), then blanks **only the
32x21 city viewport** in real B800 video RAM. The old byte-copy and
optimized word-copy tests each perform **48 passes** of real Tandy
VRAM reads/writes over that uniformly black viewport. This still
benchmarks the actual video-memory throughput, but there is no
smearing from repeated shifts of visible city graphics, and no tool
redrawing between trials. A small benchmark heading remains at the top
of the screen during the test.

It restores the original DOS video mode and prints BIOS clock ticks
(~55 ms per tick), approximate milliseconds and the **byte/word copy
speed ratio**. Larger ratios mean faster word copying; 100% means equal
speed. The first output line is now **City tile draw x2**, not the old
Full redraw x2, and is **not directly comparable** with earlier builds
because it deliberately excludes all toolbar/menu work. Copy timings
still use the same 48-pass viewport test. The benchmark does not save
the city, change the simulation or modify the tile files. It does not
require a mouse driver.

Switches now work case-insensitively like normal DOS usage:
`/bench`, `/BENCH`, `/Bench`, `/nomouse`, and `/NOMOUSE`.
The hosted video-ops test verifies that the benchmark's viewport clear
leaves every other byte of the four-bank Tandy framebuffer intact.

The benchmark isolates VRAM copy throughput, not end-to-end game
frame rate. A build passing GitHub Actions proves 16-bit compilation
and pixel-equivalence in tests, **not** speed on real V20 hardware.
Please report the three times from your Tandy so the next iteration can
decide if a tuned 8086 `REP MOVSW` copy is justified.

## V20 stutter and visible house-growth fix

When original 3x3 residential, commercial and industrial buildings
advance to new density graphics, **their electrical connectivity does
not change**. Earlier builds incorrectly re-ran the entire 120x100
power-network flood fill after *every* such growth event, producing
short but noticeable freezes on the NEC V20. The live simulation now
only invalidates that expensive scan after infrastructure changes
(roads, power lines, coal plants, bulldozing), and still performs an
initial scan on city load. The simulation's map cursor also advances
without the former division/modulo operation on every tile.

Opening an original city now uses a one-time, low-memory neighborhood
density scan to select a useful initial camera position. For the
included HAPPISLE city, the regression-tested initial viewport
contains 20 residential centers. The former northwest camera was
showing ocean, so growth could happen without being visible.

Press **Tab** during play to jump directly to the most recently
grown building or custom 1x1 zone, highlighted by the yellow cursor.
Tab shows `NO GROWTH` if no development has occurred yet. The camera
does not move automatically during normal play.

CI includes a no-repeat-power-scan regression over many months of
actual development, a test verifying an infrastructure change forces
one appropriate rescan, and a saved-city test confirming the initial
camera shows actual homes. These are software checks; report any
remaining periodic stalls or visual glitches from the actual Tandy.

## Classic SimCity-inspired icon palette (not an exact reproduction)

The two historical SimCity toolbar examples inform the **raised, beveled
buttons**, strong pressed/selected state, and grouping by function.
The Tandy 1000 EX still uses a **64-pixel sidebar** and a four-row,
two-column grid of 30x24-pixel controls; a larger toolbar would steal
visible city tiles and slow mouse/UI handling on the 7 MHz V20.

Button positions, from left to right and top to bottom:

| Row | Left | Right |
| --- | --- | --- |
| 1 | Bulldoze | Road |
| 2 | Power line | Residential |
| 3 | Commercial | Industrial |
| 4 | Inspect | Coal plant |

Residential, commercial and industrial buttons have **green, blue and
yellow** lower accent bars, respectively. The selected tool has a
bright yellow pressed border. The road icon retains the original
Micropolis TILES.DAT artwork, and the bulldozer and inspector retain
their working custom pixel art. Power, residential, commercial,
industrial and coal now use **dedicated full 16x16 pictograms** rather
than enlarged 8x8 fragments of one building/utility map tile. This
fixes the five icons that looked nonsensical on the real Tandy.
No extra disk assets, graphics-mode changes, or renderer rewrites.

The original numeric shortcuts and menus remain stable: 0=Inspect,
1=Road, 2=Power, 3=Residential, 4=Commercial, 5=Industrial,
6=Bulldoze, 7=Coal. Mouse clicks are translated from the visual order
to those stable tool keys, with host regression tests for all eight
buttons. The bottom diagnostic line identifies the selected tool.

The TOOLS and MAP sidebar captions now use the same tiny transparent
bitmap font as the fixed top menu, avoiding the black rectangles caused
by DOS graphics-mode BIOS text rendering. Tool changes still repaint
**only the previously selected and newly selected buttons**. The
minimap remains below all tools, and the main city window remains
32x21 original 8x8 tiles.

## Four-way SimCity 2000-style map view rotation (no sprite rotation)

Use **R** for a clockwise 90-degree turn and **Shift+R** for a
counterclockwise 90-degree turn, or open the **VIEW** menu and select
"Rotate CW" / "Rotate CCW". Repeating a direction gives 0°, 90°, 180°
and 270° orientations and returns to normal after four turns.

This is a **view-only camera change**. The actual original 120x100
Micropolis world, zoning, budgets, simulation, road/power networks and
CTY/ZON saves never rotate or change just because the view is turned.
At 90° or 270°, the virtual display coordinate space becomes 100x120
tiles; the visible Tandy city viewport stays **32x21** tiles. The
selection cursor remains on the same world square when rotating,
with the camera refocused on it. WASD/arrows pan in screen directions,
while mouse clicks, drag-to-build previews, zoning removal, keyboard
construction, and Tab-to-last-house still target the correct world
coordinates. The minimap turns with the city and shows the correct
rotated viewport rectangle.

No individual graphics pixels are rotated. Original 3x3 residential,
commercial and industrial buildings, plus 4x4 coal/nuclear plants, have
their component tiles reselected from the existing TILES.DAT artwork
to keep their *whole building appearance upright* when their positions
turn. Roads, power lines **and railways** switch among their
respective original directional graphics so bends and straight segments
still join visually. Crossing tiles for road/power, rail/power and
road/rail switch orientations together. Road IDs 64..207 and railway
IDs 224..238 must never be interchanged; the initial rotation
implementation omitted rail tiles entirely (it stopped at tile 223),
making roads rotate while train tracks stayed pointing the old way.
Road-bridge variants 79/95 also have their own horizontal/vertical
pair rather than being treated as ordinary street segments.
Other specialized multi-tile landmarks may require additional sprite
layout rules if they look incorrect at 90°/270°.

Rotation intentionally costs one full screen redraw; all later one-tile
panning continues to use the same accelerated 16-bit VRAM shifts.
The saved files retain normal north-up coordinates, and restarting the
program resets view rotation to 0°.

CI exhaustively checks world/display round trips for all **12,000**
tiles at all four orientations; bounds and cursor focus; connected zone
border directions; 3x3/4x4 upright tile arrangements; and correct
original road, rail and power sprite selection through four turns.
The transport-sprite regression additionally checks **all 960 tile
IDs** at 0/90/180/270 degrees: roads stay road graphics, railways stay
rail graphics, power stays power, and four turns restore every ID.

## Tandy hardware toolbar icon correction

The first compact toolbar looked correct for Road, Bulldozer and Inspect,
but the other five tools were simply using one arbitrary **8x8 map tile**
(each enlarged to 16x16). A map tile is only a piece of a larger factory,
house or utility, so those images did not read as functional tool icons.

The five new 16x16 toolbar-only designs are stored as compact C data in
`tandy-dos/tool_art.c` and use the same native Tandy palette as the game:

- **Power:** large yellow lightning bolt with blue-white sparks
- **Residential:** white house, red roof and green lawn
- **Commercial:** blue glass office/shop with windows and a door
- **Industrial:** gray factory with two smokestacks
- **Coal plant:** red-brown power station, tall stacks and yellow power motif

Road keeps its original artwork; the existing good Bulldozer and
magnifying-glass graphics are untouched. All eight buttons retain their
existing positions, selection highlights, numeric shortcuts, drag
behavior, and mouse hitboxes. Tool icons never alter original city
terrain, simulated buildings, or CTY/ZON data.

The `test-tool-art` host regression verifies each of the five new
icons contains real, distinguishable 16-color pixels, stays within
16x16 bounds, and that every displayed icon still maps to its intended
mouse and keyboard tool ID. Native 8086 DOS compilation is verified
separately by the Open Watcom build.

## Happisle railway access and imported-city growth fix

The **upstream original** `micropolis-activity/cities/happisle.cty`
is a genuine railway-heavy Micropolis city: it has 935 railway tiles,
64 rail/power crossings, and 230 roads. The Tandy **release copy**
`HAPPISLE.CTY` now deliberately converts all 999 rail-related tiles
to their appropriate road equivalents at packaging time, as requested.
The upstream original stays unchanged for historical preservation and
for the imported-city simulation regression tests.

The first simplified 1x1 and classic 3x3 RCI growth rules required a
road beside each developable lot. This prevented residential growth
on large areas of Happisle, where the original transportation network
uses rails. Both **road or railway adjacency** now count as local
transport access; electricity is still required. Empty residential
lots have faster initial move-in chances, while later densities remain
slower. This is a simplified DOS simulation rule, not an exact
reproduction of original Micropolis commute and traffic modeling.

The magnifying glass (**Inspect**, numeric shortcut 0) now reports
`ROAD TILE`, `RAIL TILE` or `POWER LINE` when clicked on actual
infrastructure. For a user-painted 1x1 zone on empty or wooded ground,
it instead reports `NEED ROAD/RAIL`, `NEED POWER` or `LOT SERVICED`
in the bottom diagnostic row. A serviced zone may still need several
simulated months before a building appears. The full-city electricity
scan is **not** triggered by inspecting a tile.

A dedicated GitHub Actions regression loads the actual unmodified
Happisle sample, checks its **935 original rails and 230 roads**,
and tests development of a wooded residential lot at world tile
(25,34), which has **rail access but no adjacent roads**. It verifies
the new residence develops, the original terrain artwork remains
unchanged, and every original rail/road tile stays in its own family.
The new transport checks add only a handful of local tile lookups,
suitable for the NEC V20 at 7 MHz.

## Original road/rail/power intersection construction

The existing **Road** and **Power** tools can now cross compatible
straight infrastructure instead of rejecting the square. These are the
actual original Micropolis intersection sprites from `w_con.c`, not
custom road/rail pixels or a replacement for Happisle's transport:

| Operation | Original tile | Completed crossing |
| --- | --- | --- |
| Road across horizontal railway | 226 | 237 |
| Road across vertical railway | 227 | 238 |
| Road across horizontal power | 210 | 78 |
| Road across vertical power | 211 | 77 |
| Power across horizontal road | 66 (and straight traffic variants) | 77 |
| Power across vertical road | 67 (and straight traffic variants) | 78 |
| Power across horizontal railway | 226 | 221 |
| Power across vertical railway | 227 | 222 |

Both networks remain intact: road neighbors connect through 237/238,
and crossings 77/78/221/222 retain the original power-conducting flag
used by the BFS electricity scan. A road still costs $10 per square;
power still costs $5. Drag previews display affordable crossings
without modifying anything until mouse-up; cancellation leaves original
roads and tracks unchanged. Completed crossings survive the standard
27,120-byte .CTY save/load round trip.

Rail corners, junctions, three-way combinations and unsupported bridges
are blocked rather than silently destroying infrastructure. There is
no rail-construction tool yet; this feature lets the existing road and
power tools cross **original imported railways**.

The `test-crossings` GitHub Actions regression covers all eight
crossings, traffic variants, road adjacency, charged drag commit and
cancel, save/reload, and live power transmission through successive
road/power and rail/power crossings.

## Rendering ideas from WolfensteinCGA v1.0

The original [WolfensteinCGA v1.0](https://github.com/jhhoward/WolfensteinCGA/releases/tag/v1.0)
was an 8088-capable CGA port with an *experimental 160x200 Tandy mode*,
preconverted VGA assets (`cgaify.exe`), packed graphics, lookup
tables, drawing-specific assembler loops, a CGA back buffer and a
repeatable gameplay `timedemo`. Micropolis keeps its sharper
**320x200x16 native Tandy mode 9** and does not adopt Wolfenstein's
raycasting-specific generated scale functions.

Already applied in this port: offline packed TILES.DAT conversion,
precomputed interlaced video-scanline addresses, 16-bit word tile
stores and overlapping VRAM scrolls, sparse HUD/toolbox redraw and
explicit tile-art regressions. Full-frame RAM backbuffer copying, CGA
mode switching and raster dithering are **not** enabled by default
because they consume memory/bandwidth or impair sharp original tiles
on a 7 MHz V20. The original source textures are converted at build
time, never at Tandy runtime.

The one additional safe improvement from v1.0 is a reproducible
**end-to-end camera timedemo**. Run either of these at DOS:

```dos
MICROPOL /TIMEDEMO
MICROPOL HAPPISLE.CTY /TIMEDEMO
```

This deliberately displays real city scrolling through a fixed 80-step
route (12 right, 8 down, 12 left, 8 up, repeated twice). It benchmarks
the exact existing VRAM scroll and edge redraw path with and without
normal status/minimap updates, returning to DOS afterward and printing
BIOS clock ticks, approximate milliseconds, and pan updates per second.
The two trials each start from an identical centered camera. This
reveals whether apparent pauses originate in VRAM rendering or the
HUD. It never runs the simulation, saves files, changes zoning or
requires a mouse. Its display changes **only during the benchmark**;
ordinary gameplay is unchanged.

This does not replace the existing `/BENCH`, which deliberately uses
an isolated blank viewport to compare raw VRAM byte-vs-word copy speed.
The new `/TIMEDEMO` intentionally renders real tiles and real camera
motion, like Wolfenstein's gameplay benchmark. Neither benchmark
result claims a performance increase until tested on real hardware.

## Menu and bulldozer latency fixes (Tandy V20)

On the actual 7 MHz Tandy, interacting with **SYSTEM, TOOLS, VIEW,
HELP** previously triggered `render_full()` to repaint the whole
32x21 city, all eight icon buttons, and the minimap. It also repainted
*every* row of a dropdown on every highlighted-item change.

Now each popup remembers the packed B800 video-memory pixels directly
under its 128-pixel-wide rectangle. Opening and changing dropdowns
touches **only the popup**, switching/closing restores only its saved
pixels, and navigating between entries redraws **the two affected
rows**. The SYSTEM dropdown begins at x=64, beyond the sidebar; no
dropdown covers the left toolbar. Menus keep the exact same controls,
actions and keyboard shortcuts. The overlay backup is at most
9 rows × 8 scanlines × 64 bytes = **4,608 bytes**, and it does not
require a second frame buffer or graphics-mode switch. Help/about
screens and intentional 90-degree camera rotations may still redraw
their larger content.

Bulldozer now redraws **3x3 nearby tiles** rather than a 5x5 region
after each map edit (coal placement retains its required 5x5 area).
A one-square edit updates only the corresponding sampled minimap
pixel(s), preserving the bright camera border even after rotating the
view, rather than repainting all 56x44 minimap pixels. The
`test-view-rotation` regression checks the precise minimap projection
for all 12,000 world squares at all four view orientations.

Removing a tile marks the power network dirty **only when the removed
tile actually had CONDBIT set**; deleting grass, ordinary roads, or
railways doesn't need a 12,000-cell electrical rescan. A mouse-held
bulldozer stroke additionally defers that scan until release so a
continuous demolition doesn't repeatedly stop on the V20. Wire,
power-crossing, conductive-building, and power-plant removal still
invalidate the network correctly. The next simulation update performs
one required scan, not one scan per bulldozed square.

**Hardware validation:** GitHub Actions proves Open Watcom 8086
compilation and host regression correctness; actual wall-clock menu
speed and bulldozer responsiveness still need to be tried on the
Tandy itself. Use `MICROPOL /TIMEDEMO` to separately measure actual
viewport scrolling and city+HUD performance, without changing saves.

## Bundled Happisle: railways converted to roads

The current Tandy release deliberately includes a **road-only Happisle**
scenario, generated with `tandy-dos/tools/convert_happisle_rails.py`.
It does **not** overwrite the original upstream Micropolis sample.
The same 27,120-byte CTY format, map, buildings, terrain, date, tax,
funds and tile flags are retained. Only these transportation sprites
are substituted:

- 224..236 (rail bridges, straight tracks, corners and junctions)
  become the corresponding road sprites 64..76.
- 237/238 (road/rail crossings) become the four-way road crossing 76.
- 221/222 (railway/power crossings) become road/power crossings 77/78,
  preserving their conductivity flags.

Exactly **935 railway tiles and 64 conductive rail/power crossings**
are converted, leaving **1,229 road-family tiles** and no railway
tiles in the release copy. The original Micropolis source city still
has the full railway network. Automated release validation compares
each of all 12,000 map tile words and ensures non-transport sprites,
flags, city histories and other metadata remain identical.

Copy the newly released `HAPPISLE.CTY` to your DOS CompactFlash card
alongside `MICROPOL.EXE` and `TILES.DAT`. Existing previously
saved `CITYnnnn.CTY` files are not modified automatically.

## Six-tile transport access and static no-power badges

The Tandy 1x1 and original 3x3 zone service checks now accept either a
road or a railway within **six Manhattan-distance tile steps** (formerly
three). Roads and railways remain visually and structurally distinct.
A local short-circuiting lookup is used rather than a city-wide traffic
recalculation; the area is a diamond, not a six-tile square.

A small **solid red lightning bolt** is shown on unpowered zone tiles:
one badge on each original multi-tile zone center, or on each custom 1x1
zoned lot. Vacant parcels also warn, so missing electricity is visible
before move-in. A powered zone has no badge. Electricity can still be
inherited through directly adjacent occupied 1x1 buildings, as before.

There is no blinking, periodic full-screen refresh, or extra electrical
flood fill. The 32x21 viewport maintains an 84-byte badge shadow. After
a change to the power graph or the adjacent-lot power relay, the display
checks up to 24 visible squares per normal simulation interval and
repaints **only badges that change**. Normal one-tile scrolling shifts the
shadow alongside existing VRAM pixels, without redrawing the whole city.
Tests cover six-versus-seven-step transport reach for roads and rails,
unpowered empty lots, original 3x3 centers and power restoration.

## Non-blinking mouse pointer and city selection

The city's **solid yellow 8x8 tile cursor** is now the only pointer
shown while the mouse is over the 32x21 city viewport. The software
arrow is suppressed there. Outside the map it remains available over
the toolbar, top menus, footer and help/menu overlays, so pixel-accurate
clickable controls remain easy to use.

Previously the simulation tick unconditionally hid and redrew the
software arrow even when nothing had changed, causing persistent
flashing on the NEC V20. It now leaves the arrow untouched on idle
simulation steps, interrupting it only for actual changes to city
sprites, status or power badges. Mouse movement also updates the
city-selection square only when the active tile changes.

A host C89 regression verifies boundary transitions and the menu/help
overlay exception. Open Watcom still compiles the native DOS build;
visible mouse behavior must be checked on real hardware.

## Click-to-jump minimap and center-pivot view rotation

Left-clicking inside the 56x44 MAP overview now centers the main city
view on that pixel's world tile. It works at every 90-degree orientation,
clamps the camera at the world edges, and moves the yellow selection
square to the picked location. Only the city viewport and camera
outline are redrawn, never the toolbar.

The r / Shift+R rotation now pivots on the world tile beneath the
**center of the visible city**, rather than the selected yellow square.
Rotation redraws the city viewport and rotated minimap, leaving the
sidebar buttons and top bar untouched. Camera clamping still applies
near the world boundary. Host regressions cover all 56x44 minimap
pixels in all four orientations and camera-center stability.

## Original road bridge construction

The road brush can now bridge the original Micropolis water tiles 2
(RIVER), 3 (REDGE) and 4 (CHANNEL), using the original 8x8-scaled
HBRIDGE/VBRIDGE graphics (IDs 64/65). Begin at a shoreline road (or
an already connected bridge) and drag across the river. Isolated
open-water placement remains blocked. The preview reserves the source
SimCity costs of **$50 per water bridge tile**, compared with **$10 per
ordinary land road tile**; insufficient funds are shown as rejected
preview tiles and never charged.

Final placement lays the dry shoreline road first, then walks outward
through any approved bridge spans, so left-to-right and right-to-left
mouse drags both work. Bridge road connections are repaired at both
ends, and bulldozing restores river water instead of dirt. No additional
rendering or simulation scans run continuously. C89 host regressions
cover horizontal/vertical bridge art, preview costs, money limits,
reverse-direction drags and CTY save/load persistence.

## Press-and-hold grouped toolbar tools

A short click on the top-row Road button repeats the last Road/Rail
choice. A short click on the bottom-right Coal button repeats the most
recent civic choice. Holding either button for roughly **0.44 seconds**
(eight BIOS clock ticks) opens a compact flyout to the right of the
sidebar: **Road / Rail** or **Coal / Nuclear / Police / Fire**. Move the
mouse into the flyout and release on the chosen option. Releasing on the
original button leaves the flyout open for a second click; Esc or
right-click closes it. Keys 1..4, arrows and Enter work while open.

The flyout shares the existing 4,608-byte menu pixel-underlay buffer,
redrawing only its affected option rows, not the city/toolbar. The
normal 0..7 keyboard shortcuts and Tools menu remain backward-compatible.
The selected Road/Rail and Coal/Nuclear/Police/Fire variants share their
original physical button and show the correct source artwork or grouped
icon, with a small disclosure corner.

Rails use original Micropolis connection sprites and crossings. Their
land cost is **$20/tile**, bridge cost **$100/tile**, with the same
preview/commit and save behavior as roads. Nuclear plants are original
4x4 buildings (center tile **816**) and function as power sources in
the existing electricity scan; coal center tile **750** remains supported.
Police and Fire stations place authentic 3x3 original art (center tiles
**774** and **765**), but the stripped-down DOS simulation does **not**
yet simulate police coverage/crime or fire-protection effects. These
are buildable buildings, not full municipal service implementations.
The original game calls the fossil-fuel plant **Coal**, not Oil.

## Dense-city building contrast without changing Micropolis artwork

The Tandy build's high-density residential and commercial sites now
use light- and dark-gray **paving** instead of green that was nearly
indistinguishable from the surrounding lawn. This remaps only pixels
which were already green in the original 8x8 converted tile, within
the upper density tiers of the original RZB/CZB building families.
Source **bright green** ('#') becomes light concrete **7**, and source
**dark green** ('$') becomes shaded concrete **8**. The standard
conversion had collapsed both source greens to color **2**, losing
the original light/shadow differentiation.

The original 16x16 XPM artwork, 8x8 silhouettes, windows, facades,
roofs, black outlines and all other colors remain exactly the same.
Low-density homes and offices, natural grass, forest, parks, water,
roads, factories, hospitals, churches, power plants, and non-building
tiles are **unchanged**. The patch does not change global color
registers or add any screen-refresh work on the 7 MHz V20.

The color remap is performed **offline**, while creating the standard
30,720-byte `TILES.DAT`, so replace **TILES.DAT** on the Tandy to
apply it. Replacing the DOS executable alone will not change art.

For before/after comparison, the old building colors can be generated
with the same source art and converter:

```sh
python3 tandy-dos/tools/convert_tiles.py \
  micropolis-activity/images/tiles.xpm TILES-OLD.DAT \
  --original-building-colors
```

A deterministic test compares all 61,440 packed pixels against the
original conversion; only the green pixels in dense residential or
commercial artwork may differ. The converted file stays 30,720 bytes,
without increasing RAM consumption or CPU work during gameplay.

## Fast electrical extension and vacant-lot power relay

Vacant independently zoned 1x1 residential, commercial and industrial
lots now relay electricity to adjoining zoned lots **before any houses
or other buildings grow**. This changes only transient power flags; the
original .CTY tile IDs, zone assignments, source sprites and saved
sidecar data remain unchanged. Unzoned empty ground, trees, and ordinary
roads cannot relay electricity.

Building positive power-line connections does not cause a global
120x100-tile rebuild. The active power network retains its source
capacity and powered-node count, and reuses the existing far-memory BFS
queue to energize **only newly reachable** wires and vacant zoned lots.
Completely disconnected new wires require no propagation. Attaching a
previously unpowered branch energizes it immediately, within the
original coal/nuclear capacity constraints. No extra VRAM refresh or
polling is required.

Removing a powered wire, a power station or a zoned relay can fragment
the grid and still schedules one exact full reconstruction, after the
mouse button is released. Initial load similarly scans once.

The **no-power warning is now bright yellow (Tandy index 14)** rather
than the old red (12), with a thin dark backing. The small indicator
remains static, and disappears on restored service without blinking.

Automated tests check vacant-chain power inheritance, wire connection
and disconnection, immediate change notifications, and that additions
do not increment the full-scan counter. Actual timing should still be
assessed on 7 MHz V20 hardware.

## Civic woodland placement and upright facility rotation

Coal and Nuclear 4x4 plants and Police and Fire 3x3 buildings now
accept ordinary clearable woodland tiles (original IDs 21..43) as
well as empty ground. Happisle has many wooded locations and the
previous zero-only validator incorrectly made these tools appear
broken. The entire footprint must still be available: water, roads,
rail, power wires, existing buildings and even vacant individually
zoned lots cannot be overwritten. The world boundary is enforced.

Fire/Police source sprite groups were missing from the camera's
upright-multi-tile rendering map, causing their artwork to jumble
when viewing a city at 90/180/270 degrees. The source-art lookup is
now host-tested for all four orientations and also covers plants,
RCI buildings, hospitals, churches and stadiums. Camera turning
never alters any original sprite bytes or .CTY map tiles.

Civic placement also refreshes only the 3x3 or 4x4 minimap footprint
instead of redrawing the complete 56x44 overview. The existing
simplified DOS simulator does not yet implement Police and Fire
coverage effects; this update fixes placement and appearance.

## Long-press tool flyout now clears immediately

The road/rail and coal/nuclear/police/fire long-press flyouts previously
saved the correct 128-pixel-wide underlay, but the common restore loop
always wrote it back starting at screen y=16, the **main menu** origin.
The two tool flyouts actually draw at y=24 and y=96, respectively.
Consequently their old menu pixels remained painted on the map until
an unrelated simulation tile update overwrote them.

Underlay capture and restore now share the identical popup rectangle:
both its x-column and *screen y* are recorded at capture time. A tool
choice, Escape, right-click or outside click therefore restores only
the 128x16 Road/Rail or 128x32 civic-popup pixels immediately. No
full viewport refresh or simulation-tick delay is needed. These changes
also preserve the existing 4,608-byte scratch buffer and pointer
hide/show rules, without adding work to ordinary gameplay.

## NewerSource complete residential, commercial and industrial lots

Original Micropolis `TILES.DAT`, original built-in 3x3 buildings, and
the underlying .CTY city tile IDs are preserved unchanged. Residential
custom 1x1 plots may use the original detached-house art. Developed
commercial and industrial 1x1 plots **never** show a cropped center
piece from an original 3x3 building. They use the full 1x1 source
sprite or a simple fallback silhouette when optional assets are absent.
Vacant 1x1 lots show normal ground.

The legacy 12-sprite small-lot pack is **byte-for-byte unchanged**.
Its new public filename `NEWER1.DAT` is copied directly from legacy
`SC2K.DAT`; the older filename continues to load as a fallback. The
new `NEWERM8.DAT` contains the full 797-tile 8x8 footprint library.
Both are bundled beside the original `TILES.DAT`.

During phased growth, four mature adjacent 1x1 same-type RCI plots can
consolidate into a complete **2x2 apartment, office/shop or factory**.
With a sufficiently mature and unobstructed adjoining neighborhood, a
2x2 building can later expand into a complete **3x3** building. Each
zone type uses its own source building family and complete subtile
graphics. Unavailable space leaves existing independent 1x1 buildings
alone. Source 4x4 resources are power/civic facilities and arcologies,
not ordinary RCI density tiers; those assets are preserved separately
for future work.

Multi-lot records use a compact owner lookup and sparse entries rather
than putting new IDs into the original .CTY map. Each saved city now has
a matching `CITYnnnn.NBL` footprint sidecar in addition to its original
`CITYnnnn.CTY` and `CITYnnnn.ZON`. .NBL records have a versioned
header, checksum and load-time validation for bounds, building identity
and overlap. The existing .ZON format remains v3, and older cities with
no .NBL still open normally.

**Bulldozing any tile** of a new 2x2 or 3x3 building removes its entire
structure but leaves all its underlying 1x1 zone assignments intact,
now vacant (development density zero). Explicit zone erasure also
dismantles its containing multi-lot before unzoning that single cell.
Roads, rails, wires, existing source buildings, mixed zone types and
water cannot be consumed by automatic expansion.

All 8x8 sprite pieces are precomputed at build time; camera rotation
selects the correct piece of the upright whole building. There is no
dynamic sprite scaling or per-frame full-city lookup. Source provenance
remains in the archived resource documentation; public runtime asset
names use `NewerSource` and do not imply ownership of original art.

### Release asset cleanup

The normal DOS release and ZIP now include `NEWER1.DAT` and
`NEWERM8.DAT`, **not** the redundant legacy `SC2K.DAT` filename.
The checked-in historical resource file remains the byte-for-byte
source of `NEWER1.DAT`. A locally supplied `SC2K.DAT` is still
accepted by older installations via the backward-compatible loader;
it is no longer generated at build root or uploaded as a release asset.

## Industrial and developed-area highlighting cleanup

The Tandy 16-color city renderer now draws bright residential,
commercial and industrial zoning outlines **only on vacant** 1x1
zoned land. In earlier builds, the yellow industrial overlay followed
individual zoning edges *across the pixel art of developed buildings*,
producing disconnected L-shaped roof highlights, extra bright borders
and visually broken original industrial blocks.

Original Micropolis 3x3 developed sprites, small occupied 1x1 lots
and new complete 2x2/3x3 NewerSource buildings are no longer painted
over by the zoning perimeter. Their source pixels and color palette
are unchanged. The minimap and Inspector still distinguish R/C/I types,
while unbuilt parcels retain the colored zoning boundary during
construction. No extra RAM, redraw passes or global map work is needed.
A C89 regression tests every edge-mask combination for vacant lots,
developed 1x1 stages, original RCI/utility art, roads, and water.

## Persistent vacant-zone hatch and individual lot outlines

Vacant individually zoned 1x1 residential, commercial and industrial
lots now **retain the same alternating 9-pixel stipple used by the
placement preview after you release the mouse**. Every individual
tile is surrounded by all four color-coded border sides, including
internal borders between adjacent lots of the same type. The R/C/I
colors remain green, blue, and bright yellow respectively.

When a lot develops (density stage 1-3), both the hatch and outline
disappear from that tile, leaving its original Micropolis or complete
NewerSource artwork unobstructed. Original 3x3 prefabs, complete
new 2x2/3x3 structures, roads, rails, power lines and water receive
no vacant overlay. The saved .CTY/.ZON/.NBL files and TILES.DAT
are unchanged by this purely cosmetic rendering policy.

The renderer writes only the affected packed 4-bit Tandy VRAM nibbles
rather than calling the pixel offset routine for every border point,
so there is no new simulation scan, memory allocation, or full-screen
refresh. Host regression exercises every tile ID and density stage,
plus all nine hatching positions and full per-cell outlines.

## Adaptive RCI borders that follow building growth

The parcel outline now evolves with actual residential, commercial,
and industrial **building footprint size**, keeping each zone's
existing green/blue/yellow color:

- **Vacant 1x1:** All four colored outline edges, plus the persistent
  nine-pixel placement-style hatch.
- **Developed 1x1 (density 1-3):** All four outline edges, but NO hatch.
- **2x2 composite building:** One continuous exterior 2x2 outline;
  zero internal dividing lines, preserving the four original source
  art pieces.
- **3x3 composite building:** The outline grows to the full 3x3
  perimeter; there are no lines through its center or roofs.
- **Original Micropolis 3x3 RCI buildings:** Also one exterior
  perimeter, only when the intact matching ZONEBIT center exists;
  partial/debris sprites cannot accidentally claim a building border.
- **Demolition or rezoning:** Removing a composite building resets its
  1x1 plots to vacant with individual outlines and hatches.

The exact sizes and origins come directly from the sparse .NBL
owner records. Perimeter edges are transformed with the same map
orientation (0/90/180/270 degrees) as the displayed sprites, so
the border always matches the building after a camera rotation.
Only changed tiles redraw during phased growth and demolition.

Tandy mode 9 uses packed VRAM bytes/nibbles; there is no new
framebuffer, save-format change, or all-city scan. A dedicated
host regression checks every zone type through 1x1, 2x2 and 3x3
growth, classic 3x3 source artwork, every rotation, and full
demolition back to hatched vacant lots. The original TILES.DAT,
NEWER1.DAT and NEWERM8.DAT are not modified.

## Gentle no-power warning blink and occasional stadium football games

Unpowered-zone lightning bolts now pulse **bright yellow (EGA 14) to
dim gray (EGA 8)** slowly, holding each phase for 24 BIOS timer ticks
(~1.32 seconds). The original black symbol backing remains fixed so
that badges are legible on grass, industrial roofs and gray paving.
Only the original seven lightning pixels in the **existing 84-byte
visible badge cache** change on each phase; no 12,000-tile power
rescan, whole city redraw or new framebuffer. The pointer is hidden
and restored only once per phase even if multiple warnings are visible.

Authentic Micropolis **stadium games** return as a display-only event.
A powered 4x4 stadium (original source center tile 784 or 800) plays
occasionally, about 12 seconds per minute, staggered between stadiums.
During a game the view substitutes the original fully attended stadium
tiles **795..810** and real animated football-field frames
**932..939 / 940..947** in their original positions. The old stadium
tiles 779..794 return between games. Timing uses BIOS ticks, with
one football frame about every 0.44 seconds for 8 frames cycling.
The source artwork is **not edited, replaced or approximated**.

The animation respects 0/90/180/270-degree view orientations and uses
the same already-tested original 4x4 upright-art mapping. The
underlying .CTY city tiles and all flags are never overwritten.
The display cache looks for visible original stadiums only when the
camera moves/rotates; each game frame draws at most the affected 4x4
sprite footprint, not the whole viewport. When there are no stadiums
or a stadium lacks power, this does essentially no rendering work.

All effects pause while menus, long-press flyouts or construction
drags are active, retaining mouse stability on the 7MHz V20.
The host regression checks timing/phase changes, both original stadium
states, correct original football sprites in four rotations,
no-power behavior and that world tiles remain bit-for-bit intact.

## Subtle NewerSource paved access details (display-only)

Developed NEWER commercial and industrial 1x1 lots now get
subtle, 1-pixel gray concrete/driveway accents on exposed **grass**
beside the original complete shop/warehouse art. NewerSource 2x2
and 3x3 buildings receive **one coordinated frontage** across the
whole footprint rather than an unrelated mini-road per subtile.
Residential NewerSource 2x2/3x3 buildings get only a tiny walkway;
original Micropolis residential 1x1 houses and all other original
Micropolis source art are unchanged.

The access side follows a real adjacent **road or road/rail
crossing**, choosing the best-connected side of the building,
with south/east/west/north used only to break ties. Bare railroad,
water or power conductors do not create fake driveways. When the
map rotates, the access pad turns with the road-facing side,
while the existing building sprite continues to face upright in the
display. Vacant lots keep their standard zoning hatch; no new
marks appear until development.

Converted 8x8 NEWER art is opaque. Drawing an underlay *before*
the sprite would hide it completely. The renderer instead draws
the original sprite unmodified, then replaces **only color 2
(grass)** in a small candidate access pattern with dark gray 8 or
light gray 7; it never overpaints roof, glass, walls, windows,
ornamental art or the surrounding zone outline. There are no
changes to NEWER1.DAT, NEWERM8.DAT, TILES.DAT, .CTY, .ZON, or
.NBL files. If the optional commercial/industrial 1x1 art is
missing, the same rule applies only to exposed green pixels of
the existing fallback silhouette.

The 8086 implementation has no allocation, animation, extra
viewport redraw, or city-wide scan. A handful of local road
checks and direct packed Tandy VRAM nibble writes occur only
when the affected building tile is already being drawn. Adjacent
road construction already redraws its immediate neighbor, which
makes connecting access lines appear after placing a new road.
Dedicated host tests verify all road directions, priority and
road/rail exclusions, source-pixel preservation, all four map
rotations, complete 1x1/2x2/3x3 frontage geometry, and that
actual converted source sprites contain visible grassy pixels
under the proposed access marks.

## Reliable Save/Resume for XT-IDE CompactFlash (Build after #392)

Earlier builds used File > Save/F2 to create a fresh
`CITY0001.CTY`, then `CITY0002.CTY` on every press, without
showing the filename. This made saving *appear* broken, particularly
when launching `MICROPOL.EXE HAPPISLE.CTY` after saving: the game
simply loaded the **original unmodified Happisle** again.

The new system uses **one save slot per playthrough**. First F2 makes
an unused `CITYnnnn.CTY` file with matching `CITYnnnn.ZON`
and `CITYnnnn.NBL`. Every subsequent F2 updates that same slot.
The status clearly reports `SAVED CITYnnnn` rather than a generic
`SAVED`, and while writing displays `SAVING...`. Separate
`CTY DISK ERROR`, `ZON DISK ERROR`, `NBL DISK ERROR` and
`SAVE COMMIT ERR` identify media/space problems.

Writes first create temporary DOS 8.3 files (`.TCT`, `.TZN`,
`.TNB`) and close **all three successfully** before replacing any
live save. Previous saves are retained as `.BCT`, `.BZN`,
and `.BNB` backup sidecars. Failed temporary writes leave the
previous save intact; failed renames attempt to restore backups.
`LASTCITY.CFG` holds a small checked last-slot pointer only after
a successful commit. Keep `.CTY`, `.ZON`, and `.NBL` together
if copying a city to another disk.

**Restart/resume:** Running `MICROPOL.EXE` **without a city
argument** automatically reloads the most recent committed save,
if `LASTCITY.CFG` exists. Running `MICROPOL.EXE HAPPISLE.CTY`
always deliberately starts that original city instead. In either
case, File > **Load last (F3)** reopens the most recent saved city
during play. Unsaved changes are replaced when F3 loads a saved
city. Passing a local filename `CITYnnnn.CTY` also reopens that
specific saved slot for repeated F2 updates.

The .CTY writer still produces exactly 27,120 big-endian bytes,
but now writes 120 buffered columns of 200 bytes instead of calling
fputc 24,000 times. This improves save responsiveness on the
7MHz V20/XT-IDE CF while adding only a 200-byte stack buffer.
The existing TILES.DAT, NEWER1.DAT, NEWERM8.DAT, source art,
and saved-file formats are unchanged. A native-independent C89
regression validates initial and repeated F2 saves, all three
sidecars, exact .CTY content, complete NBL ownership reload,
previous-version backups and corruption detection.

## Road-colored inner lot boundaries

The bright green/blue/yellow grid previously outlined **each 1x1
parcel with its zone color**, even when several independently zoned
lots formed a single continuous neighborhood. The Tandy view now
treats a boundary shared by two **adjacent, same-kind R/C/I lots**
as a narrow **asphalt-gray line (Tandy palette 8)**. This makes the
grid read more like small local connecting streets. The genuine
**outside perimeter of the zone group remains green/blue/yellow**.

The rule is applied to BOTH facing sides of a shared boundary, so
paving does not turn yellow/gray on alternate sides. At corners
where a local gray street meets the district exterior, the zone's
outer border color wins. The placement-style interior hatch is
retained on vacant lots; developed 1x1 lots keep their artwork.

A single 2x2/3x3 NewerSource building or intact original Micropolis
3x3 prefab still gets **no internal grid lines at all**. Its own
outer outline is road-gray only where another independent lot or
separate building of the **same zone type** touches that perimeter.
A different R/C/I zone, unzoned land or a real road retains the
normal colored edge. These decorative local streets are NOT
functional road tiles; they never change transport, access, power,
construction costs or saved city files.

All color classification is calculated from nearby tile/footprint
metadata only for tiles already being drawn. Every 90-degree map
rotation transforms the gray and outer-colored edges together.
Painting or removing a zone refreshes its immediate neighbor tiles
so road-gray inner edges appear or disappear without waiting for
the simulation to repaint a tile. Empty-zone removal also keeps
the existing two-tile expanded refresh for 3x3 building demolition.
This change is display-only and needs no new graphics buffers.

## Detailed Inspect tool: four pages per city tile

With the magnifying-glass tool selected (keyboard **0**, or the toolbar
button), **click any world tile** or move the yellow cursor with
**I/J/K/L** and press **Space**. A large, readable 38-column inspector
panel replaces the old one-word bottom status message. It captures all
facts at selection time rather than repeatedly scanning the city.

- **Identity (1/4):** Exact world X/Y, source tile ID in decimal/hex,
  actual tile type (terrain, water, woods, fire, roads/rail/bridges,
  power, original zones, hospitals, airports, power plants, stadiums,
  NewerSource structures, source animations), zoning kind,
  original versus NewerSource art, complete building footprint,
  top-left anchor, and tile's section within that footprint.
- **Services (2/4):** Power-network scan state, actual electricity
  flag, conductive flag, six-tile road/rail-access check, nearby
  electricity, no-power warning status, cardinally adjacent roads,
  rails and power lines. The inspector explicitly notes that the
  simplified DOS model does not yet calculate crime or pollution.
- **Development (3/4):** Actual zoning, custom-lot 0–3 growth stage
  (or original building instead), vacant versus developed status,
  exact 1x1/2x2/3x3 NewerSource footprint or valid original prefab,
  anchor/section, source design ID, current service availability, and
  *clearly labeled* simplified 1x1 residents/jobs estimates.
- **Raw data (4/4):** Original 16-bit .CTY word, tile ID, exact
  PWR/COND/BURN/BULL/ANIM/ZONE flag states, and raw IDs of all
  four neighbors. Neighbor ID 1024 means outside the 120x100 map.

A source-accurate group detector recognizes original 3x3 RCI/civic
and hospital/church buildings, 4x4 ports/power plants/stadiums and 6x6
airports when their actual original ZONEBIT center exists. It never
claims an isolated fragment is a complete building. NewerSource
footprints use the recorded .NBL coordinates and design IDs. Every
one of the 1,024 possible original tile-ID values has a safe
classification, including reserved/unknown graphic IDs.

**Navigation:** Right/Down, Tab, Enter, Space or left click advance
to the next inspector page. Left/Up go backward; Esc, right-click or
another key closes. This is a **read-only pause while inspecting**,
not a continuous expensive background scan. The original HUD,
toolbox, minimap, camera, road access, growth, .CTY/.ZON/.NBL files
and artwork remain unchanged. Unit tests cover all 1024 possible IDs,
four pages of bounded 40-column text, complete original and NewerSource
building footprints, stage changes, tile flags, neighboring terrain,
map boundaries and non-mutating reads.

## Flat gray picture-based tile inspector

The Inspect magnifier now opens a **simple flat gray bordered dialog**,
in the style of the classic later city-simulator property windows.
The top left identifies the selected tile/building and the right side
shows a genuine preview from the loaded original `TILES.DAT` or
optional `NEWER1.DAT`/`NEWERM8.DAT`. Complete original and
NewerSource buildings show their entire 1x1, 2x2, 3x3, 4x4 or 6x6
footprint, rather than a disconnected single tile. The preview uses
nearest-neighbor scaling (6x for 1x1, 4x for 2x2, 2x for 3x3/4x4,
1x for 6x6); all original pixels/colors are preserved.

There are **no bevels, shadows, graphic assets or extra framebuffers**.
The normal 320x200x16 Tandy display routines fill a flat gray panel;
BIOS text uses black on gray. The 4 inspector pages retain the
identity, services, development and raw tile/flag information in
compact 24-character rows next to the picture. The dialog has
mouse-clickable **PREV**, **NEXT**, and **DONE** buttons.
Click DONE (bottom-right) or press Esc/right-click to close.
Left/right arrows or Tab continue to change pages. Other clicks
on the panel still advance one page, retaining the prior behavior.

The one-time preview is drawn only on first opening the inspector.
Moving between pages updates only ten text rows, not the preview,
whole viewport, simulation, or mouse cursor. All geometry and
compact label lengths are verified by the host-side C89 inspector
regression, including every original tile ID and whole-building
source-art tile ranges. Inspect remains read-only and does not change
any existing source data or save format.

## Inspect dialog text legibility (Tandy graphics mode)

The original flat-gray inspector used BIOS character attribute
`0x70` (a **text-mode** black-on-light-gray attribute). Tandy
graphics mode 9 does **not** honor this as a background/foreground
pair: it selects black or inverted glyph pixels, which appeared as
unreadable overlapping black blocks in DOSBox.

The inspector now uses the same proven `bios_text()` ink color
**15 (white)** as the date/population/status HUD, with the text cells
explicitly filled black **before** printing. Its title, ten detailed
data rows, PREV/NEXT/DONE buttons, and footer are all clear
white-on-black within the original flat-gray frame. The accurate
building-art preview and all data pages are retained. No extra font
ROM, buffer, graphics mode or per-frame work has been added. A
regression asserts the graphics-mode color constants.

## Vacant parcels keep zoning-colored outlines (no gray local roads)

The visual local-street treatment now applies **only between two
developed lots or intact buildings of the same R/C/I type**. Adjacent
vacant parcels retain the existing green/blue/yellow outlines around
each 1x1 lot, plus their nine-pixel placement-style hatching.
At a vacant-to-developed boundary both sides stay zone-colored,
even if both lots are zoned identically. Only after BOTH sides
develop does the shared line become asphalt gray (palette 8).

Completed original Micropolis 3x3 and NEWER 2x2/3x3 buildings count as
developed even though they may not have an independent 1x1 growth stage.
Their external edges may turn gray beside another developed,
same-type structure, but no colored/gray lines cross their internal
footprint. Bulldozing reverts all owned 1x1 lots to stage zero,
returning their zone-colored outlines/hatch.

Single-lot development/decline events now redraw that tile and
immediate neighbors, so both sides change color at the same moment
instead of waiting for an unrelated refresh. A zone paint stroke
continues to refresh its neighboring boundaries. Source art and
.CTY/.ZON/.NBL files are unchanged, and the site roads remain
entirely decorative (not part of the transport network).

## Inspector footer spacing refinement

The flat inspector buttons used to extend through screen row 159,
with their text on y=152..159 and the keyboard footer starting at
y=160. On DOSBox and the Tandy CRT this made the instructions run
directly into the PREV/NEXT/DONE controls. The buttons now occupy
y=140..155 with labels centered on y=144..151. The single-row
white-on-black footer stays at y=160..167 with 4 clear gray pixels
between controls and footer, and the footer has a modest additional
left margin. This also provides 4 pixels between the preview panel
and buttons. Mouse hit boxes follow the new geometry.

The inspector picture, four pages of text, palette, original sprite
files, city data and DOS memory allocation remain unchanged.
Host-side geometry and mouse-hitbox assertions prevent future
overlap regressions.

## Mixed original + NewerSource RCI artwork, including imported cities

Complete original Micropolis **TILES.DAT** and complete NewerSource
**NEWER1.DAT/NEWERM8.DAT** stay unmodified and fully usable.

All three R/C/I zone types share a genuine 50/50 source-art choice
**where both sources contain a complete building of the correct
footprint**. The actual available sprites matter:

- **1x1 residential:** about half original complete detached-house
  tiles; half complete NewerSource homes `0x70..0x7B` from NEWERM8.
  A mixed-coordinate 50/50 selection is *stable through save/load*
  and camera rotation, with different source families for each
  density stage. Never use an incomplete original 3x3 center sprite
  as a 1x1 commercial or industrial building.
- **1x1 commercial/industrial:** always full-size NEWER1 shop/
  warehouse artwork. The original TILES.DAT contains no complete
  independent 1x1 C/I building.
- **2x2 residential/commercial/industrial:** still NEWERM8 complete
  source buildings. TILES.DAT does not include complete original
  2x2 RCI structures. No cropping original 3x3 art to fake one.
- **3x3 residential/commercial/industrial:** every eligible upgrade
  chooses **50/50** between an authentic full original Micropolis
  3x3 TILES building and a full NEWERM8 3x3 structure. An extra
  `source` marker is encoded in unused bits of the existing 5-byte
  .NBL structure-design ID; there is NO saved-file format change.
  The original 3x3 remains upright and coherent in all rotations.

**Imported original cities:** Original 3x3 prefabs are already seeded
as independently zoned R/C/I lots in .ZON. They remain visually and
physically **unchanged when imported**. At a later real, serviced
development event, an original prefab gets the same 50/50 opportunity:
either mature using its normal next original TILES 3x3 sprite, or
transition *visually* to a complete NewerSource 3x3 building. A full
density original may be visually renovated when demand eventually
permits. For a NEWER retrofit, the entire original .CTY footprint and
flags remain underneath, untouched. The additional .NBL owner record
is saved beside .CTY and .ZON. If a game with no optional NEWERM8 art
is loaded, imported prefabs continue to evolve with original graphics.

Bulldozing any subtile of a retrofitted old 3x3 removes the complete
new facade **and its hidden original 3x3 background**, restoring
nine individually vacant, still-zoned 1x1 lots. On NBL load, both
ordinary mature bare-ground 2x2/3x3 records and intact original 3x3
retrofits are validated for correct terrain/zone/footprint/overlap.
An incomplete source city block cannot be converted or bulldozed
as a whole building. All source IDs and graphics remain auditable in
the four-page inspector, including whether a structure is displayed
with original or NEWER art.

The Tandy DOS runtime does not need extra heap arrays, city-wide
redraws, new assets, larger .CTY/.ZON files or a new .NBL version.
The same sparse building index, real source sprites and on-demand
tile drawing are used. Dedicated host tests verify both source
alternatives, actual original source tile IDs in every orientation,
50/50 residential variety, backward-compatible NBL round trips,
non-destructive imported prefab facades, and safe demolition.
