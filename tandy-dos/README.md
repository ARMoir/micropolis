# Micropolis for the Tandy 1000 EX (16-bit DOS)

**Status: first hardware-test milestone; read-only map viewer. NOT yet a playable simulation.**

This work is an experimental native DOS port of the GPL-licensed
[Micropolis](https://github.com/SimHacker/micropolis) source, for the
Tandy 1000 EX with NEC V20 / 8088-compatible instructions and Tandy video.

## Hardware target

- Real-mode **16-bit MS-DOS**, 8086 instruction set (works with a NEC V20).
- Tandy extended BIOS **video mode 9: 320x200, 16 colors**.
- 640K conventional RAM recommended, keyboard.
- Original Tandy graphics memory at **B800:0000** in four interleaved 8K banks.
- No VGA, 386, protected-mode DOS extender, X11, Python, or SDL required.
- No mouse required. A selectable serial mouse backend is a future milestone.

## First build: test on the real Tandy

Download the `micropolis-tandy-ex-dos` artifact from the
[Tandy DOS build workflow](../../actions/workflows/tandy-dos.yml).
Extract `MICROPOL.EXE` to your CompactFlash DOS partition.

From the DOS prompt:

```dos
MICROPOL
```

This displays a generated city-style **demonstration map**. No city
simulation is running. Arrow keys or W/A/S/D scroll the viewport one
tile at a time. **Esc** returns to the prior DOS video mode.

To view an existing normal-sized original Micropolis city:

```dos
MICROPOL CITY.CTY
```

`CITY.CTY` must be an unmodified **27,120-byte** city file. The
viewer loads the map **read-only**. It does not save or edit files,
and it intentionally rejects other city sizes.

Important: original Micropolis tile IDs are displayed using a
**provisional color-coded glyph set**, not the original graphics yet.
The proper tile-art converter, city-editing tools, and simulation will
come in later milestones. Don't judge final appearance by this viewer.

If the EX does not enter graphics mode 9, the program reports the
problem and returns to DOS. Do not use this build on a standard CGA-only PC.

## Building

The GitHub Actions workflow provisions Open Watcom 2.0 and builds an
8086-compatible **16-bit DOS EXE**, not a 32-bit DOS4GW executable:

```sh
wcl -bt=dos -0 -ms -q -fe=MICROPOL.EXE tandy-dos/tandy.c tandy-dos/city.c
```

For a host-side `.CTY` parser regression test:

```sh
cc -std=c89 -Wall -Wextra -pedantic -o test-city tandy-dos/city.c tandy-dos/tests/test_city.c
./test-city
```

## Porting roadmap

1. [x] Keep original repository and licensing intact; isolate work under `tandy-dos/`.
2. [x] Real Tandy 16-color VRAM renderer and DOS keyboard/exit.
3. [x] Original Micropolis `.CTY` format loading (read-only).
4. [ ] Verify mode 9 and map appearance on a **real 1000 EX**.
5. [ ] Convert authentic tile graphics to the 16-color Tandy palette.
6. [ ] Remove X11/Tcl/Tk dependencies from the original C simulation engine,
       adapt its allocations and data types for 16-bit segmented DOS memory.
7. [ ] Implement simulation tick, zones, budget, edit tools, and save games.
8. [ ] Add optional serial mouse; preserve keyboard-only operation.
9. [ ] Benchmark and optimize simulation on the NEC V20.

**No claim of a working game or verified real-hardware executable is made
until CI and your Tandy have tested it.**

## License

Micropolis and this adaptation are distributed under the GPLv3-or-later,
subject to the original additional terms in [COPYING](../COPYING).
Original Micropolis: Copyright (C) 1989-2007 Electronic Arts Inc.
No affiliation with or trademark rights from Electronic Arts or Maxis
are implied. The modified DOS renderer and viewer are not the original game.
