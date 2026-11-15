/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ground.h"
#include "../city.h"
#include "../zoning.h"
#include "../newer_lot.h"
#include <assert.h>
#include <stdio.h>
static void flat(void)
{
    unsigned int x,y;
    city_make_demo();
    newer_lot_clear();zoning_clear();
    for(x=0;x<CITY_WIDTH;++x)for(y=0;y<CITY_HEIGHT;++y)
        city_set(x,y,0U);
}
int main(void)
{
    unsigned int b,p,hi,lo,x,y;
    unsigned short before;
    ground_init();flat();
    /* Every non-background nibble is preserved, including bright green,
     * road gray, red roofs, blue water and all sixteen foreground colors. */
    for(p=0;p<2;++p)for(b=0;b<256;++b) {
        hi=b>>4;lo=b&15U;
        assert((ground_tint[p][b]>>4)==(hi==2U?(p?7U:6U):hi));
        assert((ground_tint[p][b]&15U)==(lo==2U?(p?6U:7U):lo));
    }
    assert(ground_tint[0][0x22]==0x67);
    assert(ground_tint[1][0x22]==0x76);
    assert(ground_is_clear(20,20));
    assert(!ground_keep_green(20,20));
    /* Center plus all eight neighbors, including diagonal-only contact. */
    for(y=19;y<=21;++y)for(x=19;x<=21;++x) {
        city_set(x,y,21U|8192U);
        before=city_at(x,y);
        assert(ground_keep_green(20,20));
        assert(city_at(x,y)==before);
        city_set(x,y,0U);
    }
    city_set(22,20,43U);
    assert(!ground_keep_green(20,20));
    assert(ground_keep_green(21,20));
    city_set(22,20,44U); /* rubble is not forest */
    assert(!ground_keep_green(21,20));
    city_set(0,0,43U);
    assert(ground_keep_green(0,0));
    assert(ground_keep_green(1,1));
    assert(!ground_keep_green(2,2));
    city_set(CITY_WIDTH-1,CITY_HEIGHT-1,21U);
    assert(ground_keep_green(CITY_WIDTH-2,CITY_HEIGHT-2));
    assert(!ground_keep_green(CITY_WIDTH,0));
    assert(!ground_is_clear(CITY_WIDTH,0));
    /* Old wooded terrain beneath a developed home must not keep the
     * whole neighborhood green after the visible forest is gone. */
    flat();city_set(20,20,21U);
    assert(zoning_set(20,20,ZONE_RES));
    assert(zoning_set_development(20,20,3));
    assert(!ground_keep_green(19,19));
    assert(!ground_is_clear(20,20));
    assert(zoning_set_development(20,20,0));
    assert(ground_keep_green(19,19));
    city_set(20,20,0);
    assert(!ground_keep_green(19,19));
    assert(ground_is_clear(20,20)); /* vacant zoning is clear ground */
    assert(zoning_set_development(20,20,3));
    assert(!ground_is_clear(20,20));
    puts("Tan ground, forest diagonals, edges, clearing and pixel ink passed");
    return 0;
}
