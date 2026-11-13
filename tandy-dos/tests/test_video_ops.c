/* SPDX-License-Identifier: GPL-3.0-or-later
 * Verify Tandy four-bank addresses and overlapping word scrolls against
 * byte-reference copies for all four camera directions, including
 * the first/last scanline and every bank boundary.
 */
#include "../video_ops.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VRAM_BYTES 32768U
#define VIEW_X 64U
#define VIEW_Y 16U
#define VIEW_W 32U
#define VIEW_H 21U

/* Match 16-bit accesses to aligned video memory in DOS. */
static union { unsigned short aligned[VRAM_BYTES/2]; unsigned char bytes[VRAM_BYTES]; } actual;
static unsigned char expected[VRAM_BYTES];

static void reset_buffers(void)
{
    unsigned int i;
    for (i=0U; i<VRAM_BYTES; ++i)
        actual.bytes[i] = expected[i] = (unsigned char)((i*17U + (i>>5)) & 255U);
}
static void reference_move(unsigned int dst, unsigned int src,
                           unsigned int byte_count, int backwards)
{
    unsigned int i;
    if (backwards) {
        for (i=byte_count; i>0U; --i)
            expected[dst+i-1U] = expected[src+i-1U];
    } else {
        for (i=0U; i<byte_count; ++i)
            expected[dst+i] = expected[src+i];
    }
}
static void compare(void)
{
    assert(memcmp(actual.bytes,expected,VRAM_BYTES)==0);
}
static void check_span(unsigned int dst, unsigned int src,
                       unsigned int bytes, int backwards)
{
    assert((dst&1U)==0U && (src&1U)==0U && (bytes&1U)==0U);
    assert(dst+bytes<=VRAM_BYTES && src+bytes<=VRAM_BYTES);
    tandy_move_words(actual.bytes,dst,src,bytes/2U,backwards);
    reference_move(dst,src,bytes,backwards);
}
int main(void)
{
    unsigned int y,x,off;
    tandy_init_scanlines();
    for(y=0U; y<TANDY_SCANLINES; ++y) {
        off = ((y&3U)<<13) + ((y>>2)*160U);
        assert(tandy_scanlines[y]==off);
        assert(off+159U<VRAM_BYTES);
    }
    assert(tandy_scanlines[0]==0U);
    assert(tandy_scanlines[1]==8192U);
    assert(tandy_scanlines[2]==16384U);
    assert(tandy_scanlines[3]==24576U);
    assert(tandy_scanlines[4]==160U);
    assert(tandy_scanlines[199]==24576U+49U*160U);

    /* Left shift uses forward copy through each bank. */
    reset_buffers();
    for(y=VIEW_Y; y<VIEW_Y+VIEW_H*8U; ++y) {
        off=tandy_scanlines[y]+VIEW_X/2U;
        check_span(off,off+4U,VIEW_W*4U-4U,0);
    }
    compare();
    /* Right shift uses backward copy, preserving overlapping source. */
    reset_buffers();
    for(y=VIEW_Y; y<VIEW_Y+VIEW_H*8U; ++y) {
        off=tandy_scanlines[y]+VIEW_X/2U;
        check_span(off+4U,off,VIEW_W*4U-4U,1);
    }
    compare();
    /* Downward scroll: screen pixels move UP by 8 lines. */
    reset_buffers();
    for(y=VIEW_Y; y<VIEW_Y+(VIEW_H-1U)*8U; ++y)
        check_span(tandy_scanlines[y]+VIEW_X/2U,
                   tandy_scanlines[y+8U]+VIEW_X/2U,VIEW_W*4U,0);
    compare();
    /* Upward scroll: screen pixels move DOWN by 8 lines. */
    reset_buffers();
    for(y=VIEW_Y+VIEW_H*8U; y>VIEW_Y+8U; --y)
        check_span(tandy_scanlines[y-1U]+VIEW_X/2U,
                   tandy_scanlines[y-9U]+VIEW_X/2U,VIEW_W*4U,0);
    compare();
    /* Every horizontal edge combination in the full 320-pixel mode. */
    reset_buffers();
    for(y=0U; y<TANDY_SCANLINES; ++y) {
        off=tandy_scanlines[y];
        check_span(off,off+4U,156U,0);
    }
    compare();
    reset_buffers();
    for(y=0U; y<TANDY_SCANLINES; ++y) {
        off=tandy_scanlines[y];
        check_span(off+4U,off,156U,1);
    }
    compare();
    /* Benchmark can clear its live city viewport without overwriting
     * any of the menu, toolbox, minimap or status rows. Verify every
     * byte, across all four graphics banks, against a reference fill.
     */
    reset_buffers();
    tandy_fill_region(actual.bytes,VIEW_X/2U,VIEW_Y,
                      VIEW_W*4U,VIEW_H*8U,0U);
    for(y=VIEW_Y; y<VIEW_Y+VIEW_H*8U; ++y) {
        off=tandy_scanlines[y]+VIEW_X/2U;
        memset(expected+off,0,VIEW_W*4U);
    }
    compare();
    /* Out-of-range regions must not overwrite adjacent graphics. */
    tandy_fill_region(actual.bytes,159U,0U,2U,1U,255U);
    tandy_fill_region(actual.bytes,0U,199U,160U,2U,255U);
    tandy_fill_region(actual.bytes,160U,0U,0U,1U,255U);
    compare();

    /* Zero words is a valid no-op. */
    tandy_move_words(actual.bytes,0U,4U,0U,0);
    compare();
    /* Source and destination equal: no change. */
    for(x=0U; x<100U; x+=2U)
        check_span(200U+x,200U+x,2U,0);
    compare();
    puts("Tandy video lookup and word-span scroll regression tests passed");
    return 0;
}
