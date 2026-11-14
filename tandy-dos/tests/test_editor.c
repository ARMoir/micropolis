/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../editor.h"
#include "../zoning.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int x,y;
    /* Clear artificial demo map to deterministic flat terrain. */
    city_make_demo();
    zoning_clear();
    for (x=0; x<CITY_WIDTH; ++x)
        for (y=0; y<CITY_HEIGHT; ++y)
            city_set(x,y,0U);
    assert(edit_place(EDIT_RESIDENTIAL, 15U, 15U));
    assert((city_at(15,15) & 1023U) == 244U);
    assert((city_at(15,15) & 1024U) != 0U);
    assert((city_at(14,14) & 1023U) == 240U);
    assert(!edit_place(EDIT_COMMERCIAL, 15U, 15U)); /* collision */
    assert(edit_place(EDIT_COMMERCIAL, 30U, 30U));
    assert((city_at(30,30) & 1023U) == 427U);
    assert(edit_place(EDIT_INDUSTRIAL, 60U, 60U));
    assert((city_at(60,60) & 1023U) == 616U);
    assert(!edit_place(EDIT_INDUSTRIAL, 0U, 0U)); /* bounds */
    assert(edit_place(EDIT_COAL, 80U, 20U));
    assert((city_at(80U,20U) & 1023U) == 750U);
    assert((city_at(80U,20U) & 1024U) != 0U);
    assert(!edit_place(EDIT_COAL, 80U, 20U));
    /* New grouped variants retain original Micropolis art and size. */
    assert(edit_place(EDIT_NUCLEAR,90U,20U));
    assert((city_at(90U,20U)&1023U)==816U);
    assert((city_at(90U,20U)&1024U)!=0U);
    assert(!edit_place(EDIT_COAL,90U,20U));
    assert(edit_place(EDIT_POLICE,45U,55U));
    assert((city_at(45U,55U)&1023U)==774U);
    assert((city_at(45U,55U)&1024U)!=0U);
    assert(edit_place(EDIT_FIRE,54U,55U));
    assert((city_at(54U,55U)&1023U)==765U);
    assert(!edit_place(EDIT_FIRE,54U,55U));
    assert(edit_place(EDIT_RAIL,9U,9U));
    assert((city_at(9U,9U)&1023U)==226U);
    assert(edit_place(EDIT_RAIL,10U,9U));
    assert((city_at(9U,9U)&1023U)==226U);
    assert((city_at(10U,9U)&1023U)==226U);
    /* Civic buildings work over woods, but never erase zoning,
     * a road, water, or an existing prefab just because one tile
     * of the footprint was empty.
     */
    {
        static const enum edit_tool tools[4]={
            EDIT_COAL,EDIT_NUCLEAR,EDIT_POLICE,EDIT_FIRE
        };
        unsigned int k,i,j,cx,cy,span;
        for(k=0U;k<4U;++k) {
            cx=70U+k*10U;cy=60U;span=k<2U?4U:3U;
            for(j=0U;j<span;++j)
                for(i=0U;i<span;++i)
                    city_set(cx-1U+i,cy-1U+j,
                             (unsigned short)(21U+(i+j*span)%23U));
            assert(edit_place(tools[k],cx,cy));
            assert((city_at(cx,cy)&1024U)!=0U);
            assert((city_at(cx,cy)&1023U)==
                   (k==0U?750U:k==1U?816U:k==2U?774U:765U));
            assert(!edit_place(tools[k],cx,cy));
        }
        assert(zoning_set(71U,80U,ZONE_RES));
        assert(!edit_place(EDIT_FIRE,71U,80U));
        assert(zoning_at(71U,80U)==ZONE_RES);
        assert(zoning_set(71U,80U,ZONE_NONE));
        city_set(71U,80U,2U);
        assert(!edit_place(EDIT_FIRE,71U,80U));
        city_set(71U,80U,66U);
        assert(!edit_place(EDIT_FIRE,71U,80U));
        city_set(71U,80U,0U);
        assert(edit_place(EDIT_FIRE,71U,80U));
        assert(!edit_place(EDIT_COAL,0U,90U));
        assert(!edit_place(EDIT_NUCLEAR,119U,90U));
    }
    assert(edit_place(EDIT_ROAD, 3U, 3U));
    assert((city_at(3,3) & 1023U) == 66U);
    /* Original Micropolis supports POWER crossing an existing
     * horizontal road. This used to be rejected by our editor.
     */
    assert(edit_place(EDIT_POWER, 3U, 3U));
    assert((city_at(3U,3U) & 1023U) == 77U);
    assert((city_at(3U,3U) & 16384U) != 0U);
    assert(!edit_place(EDIT_POWER, 3U, 3U)); /* no triple crossing */
    assert(edit_place(EDIT_BULLDOZE, 3U, 3U));
    assert(city_at(3,3) == 0U);
    assert(city_save_as("editor.tmp"));
    assert(city_load("editor.tmp"));
    assert((city_at(15,15) & 1023U) == 244U);
    assert(remove("editor.tmp") == 0);
    puts("Micropolis edit / zoning / CTY round-trip tests passed");
    return 0;
}
