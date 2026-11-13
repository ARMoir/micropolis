/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native Tandy mode-9 VRAM addressing and overlapping 16-bit copies.
 * No change to tile format, palette, renderer or screen layout.
 */
#ifndef TANDY_VIDEO_OPS_H
#define TANDY_VIDEO_OPS_H

#ifdef __WATCOMC__
#define TANDY_FAR far
#else
#define TANDY_FAR
#endif

#define TANDY_SCANLINES 200U
#define TANDY_BANK_SIZE 0x2000U
#define TANDY_SCANLINE_BYTES 160U

/* 400-byte table, initialized once before the first graphics draw. */
extern unsigned short tandy_scanlines[TANDY_SCANLINES];

void tandy_init_scanlines(void);

/* Copy a span of 16-bit words within the B800 VRAM address space.
 * Forward for destination <= source; backward for destination > source.
 * All callers use even offsets, and the copy is overlap-safe.
 * Preserve 8086 compatibility and avoid touching video page registers.
 */
/* Fill only a specified horizontal VRAM window and scanline range.
 * Used to make live-VRAM benchmarks visually quiet and keep HUD,
 * toolbar and minimap bytes untouched. Bounds are checked.
 */
void tandy_fill_region(volatile unsigned char TANDY_FAR *screen,
                       unsigned int start_byte, unsigned int top_line,
                       unsigned int width_bytes, unsigned int line_count,
                       unsigned char packed_color);

void tandy_move_words(volatile unsigned char TANDY_FAR *screen,
                      unsigned int dst_offset, unsigned int src_offset,
                      unsigned int words, int backwards);
#endif
