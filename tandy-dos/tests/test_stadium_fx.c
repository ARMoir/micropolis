/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test original 4x4 stadium construction and 932..947 football art,
 * with city tiles unchanged at all camera rotations and schedules.
 */
#include "../city.h"
#include "../stadium_fx.h"
#include "../view_rotation.h"
#include "../badge_flash.h"
#include <assert.h>
#include <stdio.h>

static void stadium_place(unsigned int cx,unsigned int cy,
                          unsigned int base,int powered)
{
    unsigned int x,y,i=0U;
    for(y=cy-1U;y<=cy+2U;++y)
        for(x=cx-1U;x<=cx+2U;++x) {
            unsigned short art=(unsigned short)(base+i++);
            if(x==cx && y==cy)
                art=(unsigned short)(art|1024U|
                                     (powered?32768U:0U));
            city_set(x,y,art);
        }
}

static unsigned int active_clock(unsigned int cx,unsigned int cy)
{
    unsigned int t;
    for(t=0U;t<STADIUM_FX_PERIOD;++t)
        if(stadium_fx_frame(t,cx,cy,1)!=0U)return t;
    assert(!"Could not find active stadium game window");
    return 0U;
}

static unsigned int inactive_clock(unsigned int cx,unsigned int cy)
{
    unsigned int t;
    for(t=0U;t<STADIUM_FX_PERIOD;++t)
        if(stadium_fx_frame(t,cx,cy,1)==0U)return t;
    assert(!"Could not find idle stadium window");
    return 0U;
}

static void validate_full_set(unsigned int cx,unsigned int cy,
                               unsigned int base)
{
    unsigned int tick_on=active_clock(cx,cy);
    unsigned int tick_off=inactive_clock(cx,cy);
    unsigned int frame=stadium_fx_frame(tick_on,cx,cy,1);
    unsigned int x,y,t,part,screen_part,expected,got,ix,iy;
    unsigned int before[16],i=0U;
    assert(frame>=1U&&frame<=8U);
    for(y=cy-1U;y<=cy+2U;++y)
        for(x=cx-1U;x<=cx+2U;++x)
            before[i++]=city_at(x,y);
    for(t=0U;t<4U;++t)
        for(iy=0U;iy<4U;++iy)
            for(ix=0U;ix<4U;++ix) {
                x=cx-1U+ix;y=cy-1U+iy;
                part=iy*4U+ix;
                screen_part=view_rotation_upright_part(t,4U,part);
                expected=779U+screen_part;
                got=stadium_fx_art(x,y,t,tick_off);
                assert(got==expected);

                expected=795U+screen_part;
                if(screen_part==6U) expected=931U+frame;
                if(screen_part==10U)expected=939U+frame;
                got=stadium_fx_art(x,y,t,tick_on);
                assert(got==expected);
                assert(got<960U);
            }
    i=0U;
    for(y=cy-1U;y<=cy+2U;++y)
        for(x=cx-1U;x<=cx+2U;++x)
            assert(city_at(x,y)==before[i++]);
    assert((city_at(cx,cy)&1023U)==base+5U);
}

int main(void)
{
    unsigned int x,y,frame,t,cx=50U,cy=50U,tick;
    city_make_demo();
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y) city_set(x,y,0U);

    /* Slow phase ~1.32s each and 2.64s for a full blink. */
    assert(badge_flash_color(0U)==BADGE_FLASH_BRIGHT);
    assert(badge_flash_color(23U)==BADGE_FLASH_BRIGHT);
    assert(badge_flash_color(24U)==BADGE_FLASH_DIM);
    assert(badge_flash_color(47U)==BADGE_FLASH_DIM);
    assert(badge_flash_color(48U)==BADGE_FLASH_BRIGHT);
    assert(badge_flash_color(65535U)==badge_flash_color(65535U));

    assert(!stadium_fx_center(10U,10U,0,0));
    assert(stadium_fx_art(10U,10U,0U,0U)==STADIUM_FX_NONE);
    assert(!stadium_fx_center(CITY_WIDTH,1U,0,0));

    stadium_place(cx,cy,779U,1);
    assert(stadium_fx_center(cx+1U,cy,0,0));
    assert(stadium_fx_center(cx+1U,cy+1U,0,0));
    validate_full_set(cx,cy,779U);
    /* Simulated "full stadium" .CTY sources sometimes contain
     * special animated playfield tiles. Both must still be recognized.
     */
    stadium_place(cx,cy,795U,1);
    city_set(cx+1U,cy,932U|2048U);
    city_set(cx+1U,cy+1U,940U|2048U);
    assert(stadium_fx_center(cx+1U,cy,&x,&y));
    assert(x==cx && y==cy);
    assert(stadium_fx_center(cx+1U,cy+1U,&x,&y));
    assert(x==cx && y==cy);
    tick=active_clock(cx,cy);
    frame=stadium_fx_frame(tick,cx,cy,1);
    assert(stadium_fx_art(cx+1U,cy,0U,tick)==931U+frame);
    assert(stadium_fx_art(cx+1U,cy+1U,0U,tick)==939U+frame);
    /* The old map remains unchanged even while the original game
     * graphics animate on screen.
     */
    assert((city_at(cx+1U,cy)&1023U)==932U);
    assert((city_at(cx+1U,cy+1U)&1023U)==940U);

    /* The authentic football effect requires electricity. */
    stadium_place(cx,cy,779U,0);
    for(t=0U;t<STADIUM_FX_PERIOD;t+=11U) {
        assert(stadium_fx_frame(t,cx,cy,0)==0U);
        assert(stadium_fx_art(cx+1U,cy,0U,t)==785U);
    }
    /* Damage to the original ZoneBIT center must disable the
     * stadium effect, rather than animating unrelated city art.
     */
    city_set(cx,cy,784U|32768U);
    assert(!stadium_fx_center(cx,cy,0,0));
    assert(stadium_fx_art(cx-1U,cy-1U,0U,tick)==STADIUM_FX_NONE);
    puts("Slow no-power blink and original stadium 4x4/football art in every rotation passed");
    return 0;
}
