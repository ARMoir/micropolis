/* SPDX-License-Identifier: GPL-3.0-or-later
 * VGA-only 256-color source shading. Each detail pixel stores its
 * legacy reference color in the low nibble, allowing UI ink to override it.
 * All large allocations are separate, below the 16-bit 64KB object limit.
 */
#include "vga_art.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char TANDY_FAR *detail,*original,*newer;
static unsigned char soil[40*25],palette[768];
static int ready,active;
static int read_pack(const char *name,unsigned char TANDY_FAR *out,
                      unsigned int size)
{
    FILE *f=fopen(name,"rb");
    int ok;
    if(!f)return 0;
    ok=fread(out,1U,size,f)==size && fgetc(f)==EOF && !ferror(f);
    if(fclose(f)!=0)ok=0;
    return ok;
}
void vga_art_free(void)
{
    free(detail);free(original);free(newer);
    detail=original=newer=0;
    ready=active=0;
}
int vga_art_load(void)
{
    unsigned int i;
    vga_art_free();
    detail=(unsigned char TANDY_FAR *)malloc(64000U);
    original=(unsigned char TANDY_FAR *)malloc(61440U);
    newer=(unsigned char TANDY_FAR *)malloc(51008U);
    if(!detail || !original || !newer ||
       !read_pack("VGATILES.DAT",original,61440U) ||
       !read_pack("VGANEWER.DAT",newer,51008U) ||
       !read_pack("VGA.PAL",palette,768U)) {
        vga_art_free();return 0;
    }
    for(i=0U;i<768U;++i)if(palette[i]>63U) {
        vga_art_free();return 0;
    }
    memset(detail,0,64000U);memset(soil,0,sizeof(soil));
    ready=active=1;
    return 1;
}
const unsigned char *vga_art_palette(void)
{
    return ready?palette:0;
}
void vga_art_begin(unsigned int x,unsigned int y,int dry,int clear)
{
    unsigned int row;
    if(!ready || x>312U || y>192U)return;
    soil[(y/8U)*40U+x/8U]=(unsigned char)((dry?1U:0U)|(clear?2U:0U));
    for(row=0U;row<8U;++row)memset(detail+(y+row)*320U+x,0,8U);
}
void vga_art_stamp(unsigned int x,unsigned int y,int pack,unsigned int tile)
{
    unsigned int row;
    const unsigned char TANDY_FAR *src;
    if(!ready || x>312U || y>192U || tile>=(pack?797U:960U))return;
    src=(pack?newer:original)+tile*64U;
    for(row=0U;row<8U;++row)
        memcpy(detail+(y+row)*320U+x,src+row*8U,8U);
}
int vga_art_active(int enabled)
{
    int next=ready && enabled,changed=next!=active;
    active=next;
    return changed;
}
unsigned char vga_art_pixel(unsigned int x,unsigned int y,unsigned char ink)
{
    unsigned char high,flags;
    if(!active || x<UI_MAP_X || y<UI_MAP_Y ||
       x>=UI_MAP_X+UI_MAP_COLS*8U || y>=UI_MAP_Y+UI_MAP_ROWS*8U)
        return ink;
    high=detail[y*320U+x];
    flags=soil[(y/8U)*40U+x/8U];
    if((flags&1U) && (ink==6U || ink==7U) &&
       ((flags&2U) || (high&15U)==2U)) {
        /* A single solid light brown, never red roofing or road paint. */
        return 226U;
    }
    return (high&15U)==ink?high:ink;
}
void vga_art_scroll(int dx,int dy)
{
    int x,y,start,end,step,sx,sy;
    if(!ready || (dx==0 && dy==0))return;
    /* Destination order follows memmove semantics, including down/right. */
    start=dy>0?(int)(UI_MAP_Y+UI_MAP_ROWS*8U)-1:(int)UI_MAP_Y;
    end=dy>0?(int)UI_MAP_Y-1:(int)(UI_MAP_Y+UI_MAP_ROWS*8U);
    step=dy>0?-1:1;
    for(y=start;y!=end;y+=step) {
        sy=y-dy;
        if(sy<(int)UI_MAP_Y || sy>=(int)(UI_MAP_Y+UI_MAP_ROWS*8U))continue;
        if(dx>0)memmove(detail+y*320U+UI_MAP_X+dx,
                       detail+sy*320U+UI_MAP_X,UI_MAP_COLS*8U-dx);
        else memmove(detail+y*320U+UI_MAP_X,
                     detail+sy*320U+UI_MAP_X-dx,UI_MAP_COLS*8U+dx);
    }
    start=dy>0?(int)(UI_MAP_Y/8U+UI_MAP_ROWS)-1:(int)(UI_MAP_Y/8U);
    end=dy>0?(int)(UI_MAP_Y/8U)-1:(int)(UI_MAP_Y/8U+UI_MAP_ROWS);
    for(y=start;y!=end;y+=step) {
        sy=y-dy/8;
        if(sy<(int)(UI_MAP_Y/8U) || sy>=(int)(UI_MAP_Y/8U+UI_MAP_ROWS))continue;
        for(x=0;x<(int)UI_MAP_COLS;++x) {
            int col=dx>0?(int)(UI_MAP_X/8U+UI_MAP_COLS)-1-x:
                         (int)(UI_MAP_X/8U)+x;
            sx=col-dx/8;
            if(sx>=(int)(UI_MAP_X/8U) && sx<(int)(UI_MAP_X/8U+UI_MAP_COLS))
                soil[y*40+col]=soil[sy*40+sx];
        }
    }
}

void vga_present_detail_rows(const volatile unsigned char TANDY_FAR *packed,
    volatile unsigned char TANDY_FAR *screen,unsigned char *dirty)
{
    unsigned int y,x,src,dst;
    unsigned char pair;
    for(y=0U;y<200U;++y) {
        if(!dirty[y])continue;
        src=tandy_scanlines[y];dst=y*320U;
        for(x=0U;x<320U;x+=2U) {
            pair=packed[src++];
            screen[dst++]=vga_art_pixel(x,y,(unsigned char)(pair>>4));
            screen[dst++]=vga_art_pixel(x+1U,y,(unsigned char)(pair&15U));
        }
        dirty[y]=0U;
    }
}
