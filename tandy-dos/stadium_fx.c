/* SPDX-License-Identifier: GPL-3.0-or-later
 * Stadium games based on original s_sim.c DoSPZone/DrawStadium and
 * original animtab.h (932..947): regular stadium base 779, full
 * stadium base 795, field animation tiles at parts 6 and 10.
 * No world mutation, no random writes, no save format changes.
 */
#include "stadium_fx.h"
#include "city.h"
#include "view_rotation.h"
#define TILE_ID 1023U
#define ZONEBIT 1024U
#define PWRBIT 32768U
#define STADIUM_BASE 779U
#define FULL_BASE 795U
#define STADIUM_CENTER 784U
#define FULL_CENTER 800U

/* A tile's stadium center must match the original 4x4 group and
 * have a ZONEBIT. Handles legacy save files containing game sprites
 * in two field positions as well as the two ordinary stadium groups.
 */
int stadium_fx_center(unsigned int x,unsigned int y,
                      unsigned int *cx,unsigned int *cy)
{
    unsigned int id,base,part,expected;
    int ax,ay;
    unsigned short center;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    id=city_at(x,y)&TILE_ID;
    if(id>=STADIUM_BASE && id<STADIUM_BASE+16U) {
        base=STADIUM_BASE;
        part=id-base;
        expected=STADIUM_CENTER;
    } else if(id>=FULL_BASE && id<FULL_BASE+16U) {
        base=FULL_BASE;
        part=id-base;
        expected=FULL_CENTER;
    } else if(id>=932U && id<=939U) {
        /* Field 1 is to the right of the center (part 6). */
        part=6U;expected=FULL_CENTER;
    } else if(id>=940U && id<=947U) {
        /* Field 2 is one row below field 1 (part 10). */
        part=10U;expected=FULL_CENTER;
    } else return 0;
    ax=(int)x+1-(int)(part%4U);
    ay=(int)y+1-(int)(part/4U);
    if(ax<1 || ay<1 || ax+2>=CITY_WIDTH || ay+2>=CITY_HEIGHT)
        return 0;
    center=city_at((unsigned int)ax,(unsigned int)ay);
    if((center&(TILE_ID|ZONEBIT))!=(expected|ZONEBIT))
        return 0;
    if(cx!=0)*cx=(unsigned int)ax;
    if(cy!=0)*cy=(unsigned int)ay;
    return 1;
}
/* Each center receives a short, staggered game once per minute.
 * One animation frame ~=0.44 sec; 0 means idle stadium.
 * Unsigned 16-bit BIOS tick wrap is supported by normal modulo.
 */
unsigned int stadium_fx_frame(unsigned int ticks,unsigned int cx,
                              unsigned int cy,int powered)
{
    unsigned int stagger,position;
    if(!powered) return 0U;
    stagger=(cx*7U+cy*13U)%191U;
    position=(unsigned int)((ticks+stagger)%STADIUM_FX_PERIOD);
    if(position>=STADIUM_FX_LENGTH) return 0U;
    return 1U+((ticks/STADIUM_FX_FRAME_TICKS)&7U);
}

unsigned int stadium_fx_art(unsigned int x,unsigned int y,
                            unsigned int turns,unsigned int ticks)
{
    unsigned int cx,cy,id,part,frame,screen_part,center_id;
    unsigned short center;
    if(!stadium_fx_center(x,y,&cx,&cy))
        return STADIUM_FX_NONE;
    id=city_at(x,y)&TILE_ID;
    if(id>=STADIUM_BASE && id<STADIUM_BASE+16U)
        part=id-STADIUM_BASE;
    else if(id>=FULL_BASE && id<FULL_BASE+16U)
        part=id-FULL_BASE;
    else part=id<=939U?6U:10U;
    screen_part=view_rotation_upright_part(turns,4U,part);
    center=city_at(cx,cy);
    center_id=center&TILE_ID;
    if(center_id!=STADIUM_CENTER && center_id!=FULL_CENTER)
        return STADIUM_FX_NONE;
    frame=stadium_fx_frame(ticks,cx,cy,(center&PWRBIT)!=0U);
    if(frame==0U)
        return STADIUM_BASE+screen_part;
    if(screen_part==6U) return 931U+frame; /* 932..939 */
    if(screen_part==10U)return 939U+frame; /* 940..947 */
    return FULL_BASE+screen_part;
}
