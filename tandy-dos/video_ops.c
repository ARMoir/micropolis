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

/* Restrict blanking to the city window: unlike a mode reset or full
 * renderer pass, this never writes any toolbar, HUD or minimap pixels.
 * The fill is outside timed sections; its cost cannot skew the ratio.
 */
void tandy_fill_region(volatile unsigned char TANDY_FAR *screen,
                       unsigned int start_byte, unsigned int top_line,
                       unsigned int width_bytes, unsigned int line_count,
                       unsigned char packed_color)
{
    unsigned int y, offset, x;
    if (start_byte >= TANDY_SCANLINE_BYTES ||
        top_line >= TANDY_SCANLINES ||
        width_bytes > TANDY_SCANLINE_BYTES - start_byte ||
        line_count > TANDY_SCANLINES - top_line)
        return;
    for (y = 0U; y < line_count; ++y) {
        offset = tandy_scanlines[top_line + y] + start_byte;
        for (x = 0U; x < width_bytes; ++x)
            screen[offset + x] = packed_color;
    }
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

/* AL=1Ah confirms support. BL=7/8 is the active VGA display, not BH. */
int video_has_vga(unsigned int signature, unsigned int active_display)
{
    return signature==0x1aU &&
           (active_display==7U || active_display==8U);
}

void vga_present_rows(const volatile unsigned char TANDY_FAR *packed,
                      volatile unsigned char TANDY_FAR *vga,
                      unsigned char *dirty)
{
    unsigned int y,x,src,dst;
    unsigned char pair;
    for(y=0U;y<200U;++y) {
        if(!dirty[y]) continue;
        src=tandy_scanlines[y];
        dst=y*320U;
        for(x=0U;x<160U;++x) {
            pair=packed[src+x];
            vga[dst++]=(unsigned char)(pair>>4);
            vga[dst++]=(unsigned char)(pair&15U);
        }
        dirty[y]=0U;
    }
}
