/* SPDX-License-Identifier: GPL-3.0-or-later
 * Tandy 320x200x16 has four 8KB interleaved banks:
 * B800:0000 (y%4=0), 2000 (y%4=1), 4000, 6000.
 * Each bank has 50 scanlines of 160 packed bytes. The lookup removes
 * repeated division/multiplication from every pixel/tile lookup.
 *
 * Word copies follow the same overlap direction as the original byte
 * scrolling loops. Portable C89 and 8086 -0 large-memory-model output.
 * No VGA dependence, self-modifying code or display-mode changes.
 */
#include "video_ops.h"

unsigned short tandy_scanlines[TANDY_SCANLINES];

void tandy_init_scanlines(void)
{
    unsigned int y;
    for (y = 0U; y < TANDY_SCANLINES; ++y)
        tandy_scanlines[y] =
            (unsigned short)(((y & 3U) * TANDY_BANK_SIZE) +
                             ((y >> 2) * TANDY_SCANLINE_BYTES));
}

void tandy_move_words(volatile unsigned char TANDY_FAR *screen,
                      unsigned int dst_offset, unsigned int src_offset,
                      unsigned int words, int backwards)
{
    volatile unsigned short TANDY_FAR *dest =
        (volatile unsigned short TANDY_FAR *)(screen + dst_offset);
    volatile unsigned short TANDY_FAR *source =
        (volatile unsigned short TANDY_FAR *)(screen + src_offset);
    if (backwards) {
        dest += words;
        source += words;
        while (words != 0U) {
            --dest;
            --source;
            *dest = *source;
            --words;
        }
    } else {
        while (words != 0U) {
            *dest++ = *source++;
            --words;
        }
    }
}
