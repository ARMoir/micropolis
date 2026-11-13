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

Then launch either the built-in test map or an authentic Micropolis city:

```dos
MICROPOL
MICROPOL HAPPISLE.CTY
```

The first command opens the built-in **powered starter neighborhood**
with an original 4x4 coal plant, connected power lines, roads, and empty
residential/commercial/industrial lots ready to grow. The second loads
the included **original Micropolis city map** and automatically focuses
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
Commercial and industrial 1x1 buildings reuse center tiles from the
original artwork; we may refine those sprites after a hardware test.
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
  tandy-dos/simulation.c tandy-dos/video_ops.c
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
