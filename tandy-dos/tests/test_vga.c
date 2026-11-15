/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../video_ops.h"
static unsigned char packed[32768], screen[64002], dirty[200];
int main(void)
{
    unsigned int x,y,offset;
    assert(video_has_vga(0x1a,7));
    assert(video_has_vga(0x1a,8));
    assert(!video_has_vga(0,8));
    assert(!video_has_vga(0x1a,4));
    assert(!video_has_vga(0x1a,0));
    tandy_init_scanlines();
    memset(screen,0xcc,sizeof(screen));
    for(y=0;y<200;++y) {
        offset=tandy_scanlines[y];
        for(x=0;x<160;++x)
            packed[offset+x]=(unsigned char)((x+y)&255);
    }
    memset(dirty,1,sizeof(dirty));
    vga_present_rows(packed,screen+1,dirty);
    assert(screen[0]==0xcc && screen[64001]==0xcc);
    for(y=0;y<200;++y) {
        assert(!dirty[y]);
        for(x=0;x<160;++x) {
            unsigned int pair=(x+y)&255;
            assert(screen[1+y*320+x*2]==(pair>>4));
            assert(screen[2+y*320+x*2]==(pair&15));
        }
    }
    /* One row changes; clean rows must retain existing pixels. */
    memset(packed,0xff,sizeof(packed));
    dirty[199]=1;
    vga_present_rows(packed,screen+1,dirty);
    assert(screen[1]==0 && screen[2]==0);
    assert(screen[1+199*320]==15 && screen[64000]==15);
    assert(screen[64001]==0xcc && !dirty[199]);
    /* No dirty rows means no output writes. */
    screen[1]=99;
    vga_present_rows(packed,screen+1,dirty);
    assert(screen[1]==99);
    puts("VGA detection, all bank rows, nibble order and dirty rows passed");
    return 0;
}
