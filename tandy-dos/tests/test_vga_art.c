/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../vga_art.h"
#include <assert.h>
#include <stdio.h>
static unsigned char tile[64];
static void read_tile(unsigned int index)
{
    FILE *f=fopen("VGATILES.DAT","rb");
    assert(f);
    assert(fseek(f,(long)index*64L,SEEK_SET)==0);
    assert(fread(tile,1U,64U,f)==64U);
    assert(fclose(f)==0);
}
static void check(unsigned int x,unsigned int y)
{
    unsigned int i,j;
    for(j=0U;j<8U;++j)for(i=0U;i<8U;++i)
        assert(vga_art_pixel(x+i,y+j,tile[j*8U+i]&15U)==tile[j*8U+i]);
}
int main(void)
{
    const unsigned char *pal;
    assert(vga_art_load());
    pal=vga_art_palette();
    assert(pal && pal[226U*3U]==45U);
    read_tile(265U);
    vga_art_begin(80,32,0,0);
    vga_art_stamp(80,32,0,265);
    check(80,32);
    assert(vga_art_pixel(80,32,(unsigned char)((tile[0]+1)&15))==
           ((tile[0]+1)&15)); /* cursor/overlay ink wins */
    assert(vga_art_pixel(0,0,15)==15); /* UI never shaded */
    assert(vga_art_active(0));
    assert(vga_art_pixel(80,32,tile[0]&15)==(tile[0]&15));
    assert(vga_art_active(1));
    check(80,32);
    vga_art_scroll(-8,0);check(72,32);
    vga_art_scroll(8,0);check(80,32);
    vga_art_scroll(0,-8);check(80,24);
    vga_art_scroll(0,8);check(80,32);
    /* Smooth tan only on soil, never arbitrary brown building pixels. */
    vga_art_begin(80,32,1,1);
    assert(vga_art_pixel(80,32,6)==226U);
    assert(vga_art_pixel(81,32,7)==226U);
    assert(vga_art_pixel(87,39,6)==226U); /* no 8-pixel texture */
    assert(vga_art_pixel(80,32,12)==12U); /* red roofs not recolored */
    assert(vga_art_pixel(80,32,15)==15);
    vga_art_scroll(8,0);
    assert(vga_art_pixel(88,32,6)==226U);
    vga_art_begin(88,32,0,1);
    assert(vga_art_pixel(88,32,2)==2);
    /* Source dirt is no longer red even on the green side of a forest. */
    read_tile(0U);
    assert(tile[0]==2U);
    vga_art_begin(80,32,0,1);
    vga_art_stamp(80,32,0,0);
    assert(vga_art_pixel(80,32,2)==2);
    vga_art_begin(80,32,1,1);
    vga_art_stamp(80,32,0,0);
    assert(vga_art_pixel(80,32,6)==226U);
    vga_art_free();
    assert(!vga_art_palette());
    assert(vga_art_pixel(88,32,6)==6);
    puts("VGA source detail, overlay preservation, soil and four-way scrolling passed");
    return 0;
}
