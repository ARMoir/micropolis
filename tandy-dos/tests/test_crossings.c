/* SPDX-License-Identifier: GPL-3.0-or-later
 * Real original Micropolis road/rail/power intersection regression.
 * The six cross-tool cases use the exact w_con.c crossing sprites,
 * exercise the drag preview and verify that power travels THROUGH
 * rail and road crossings (not merely that the art looks correct).
 */
#include "../city.h"
#include "../editor.h"
#include "../infrastructure.h"
#include "../placement.h"
#include "../zoning.h"
#include "../engine.h"
#include <assert.h>
#include <stdio.h>

#define TILE(x,y) (city_at((x),(y)) & 1023U)
#define CONDBIT 16384U

static void clear_map(void)
{
    unsigned int x,y;
    city_make_demo();
    zoning_clear();
    city_set_funds(1000L);
    for (x=0U;x<CITY_WIDTH;++x)
        for (y=0U;y<CITY_HEIGHT;++y)
            city_set(x,y,0U);
}

static void test_cross(enum edit_tool tool, unsigned int input,
                       unsigned int output, int powered)
{
    clear_map();
    city_set(20U,20U,(unsigned short)(input|4096U|8192U|
                          ((input==210U||input==211U)?CONDBIT:0U)));
    assert(infrastructure_can_place(tool,20U,20U));
    assert(infrastructure_crossing_id(tool,input)==output);
    assert(edit_place(tool,20U,20U));
    assert(TILE(20U,20U)==output);
    assert((city_at(20U,20U)&(4096U|8192U))==(4096U|8192U));
    assert(((city_at(20U,20U)&CONDBIT)!=0U)==powered);
    assert(!infrastructure_can_place(tool,20U,20U));
    assert(!edit_place(tool,20U,20U)); /* no duplicate cost/work */
}

int main(void)
{
    unsigned int road,rail,x,y;
    /* Road OVER existing straight rail, both directions. */
    test_cross(EDIT_ROAD,226U,237U,0);
    test_cross(EDIT_ROAD,227U,238U,0);
    /* Road OVER existing straight power; wires remain conductive. */
    test_cross(EDIT_ROAD,210U,78U,1);
    test_cross(EDIT_ROAD,211U,77U,1);
    /* Power OVER existing road and existing rail. */
    test_cross(EDIT_POWER,66U,77U,1);
    test_cross(EDIT_POWER,67U,78U,1);
    test_cross(EDIT_POWER,226U,221U,1);
    test_cross(EDIT_POWER,227U,222U,1);
    /* Traffic-loaded straight road variants neutralize to the same
     * original road/power crossing art, not to railway sprites. */
    test_cross(EDIT_POWER,82U,77U,1);
    test_cross(EDIT_POWER,83U,78U,1);

    clear_map();
    /* Disallow rail corners, 3-way crossings, road junctions,
     * incompatible terrain, and overwriting ordinary buildings. */
    for(x=0U;x<10U;++x) {
        static const unsigned int blocked[]={
            0U,2U,68U,69U,76U,228U,229U,236U,237U,221U
        };
        city_set(20U+x,20U,(unsigned short)blocked[x]);
        assert(infrastructure_crossing_id(EDIT_POWER,blocked[x])==0U);
        assert(infrastructure_crossing_id(EDIT_ROAD,blocked[x])==0U);
    }
    assert(!infrastructure_can_place(EDIT_ROAD,21U,20U)); /* water */
    assert(!infrastructure_can_place(EDIT_ROAD,25U,20U)); /* corner */
    assert(!infrastructure_can_place(EDIT_POWER,26U,20U)); /* corner */
    assert(!infrastructure_can_place(EDIT_POWER,28U,20U)); /* 3-way */

    clear_map();
    /* N/S road continues through HRAILROAD=237 while E/W railway
     * stays intact; original neighbor repair must not end the road.
     */
    city_set(30U,30U,226U|4096U|8192U);
    assert(edit_place(EDIT_ROAD,30U,29U));
    assert(edit_place(EDIT_ROAD,30U,31U));
    assert(edit_place(EDIT_ROAD,30U,30U));
    assert(TILE(30U,30U)==237U);
    assert(TILE(30U,29U)==67U);
    assert(TILE(30U,31U)==67U);

    clear_map();
    /* E/W road continues through VRAILROAD=238. */
    city_set(30U,30U,227U|4096U|8192U);
    assert(edit_place(EDIT_ROAD,29U,30U));
    assert(edit_place(EDIT_ROAD,31U,30U));
    assert(edit_place(EDIT_ROAD,30U,30U));
    assert(TILE(30U,30U)==238U);
    assert(TILE(29U,30U)==66U);
    assert(TILE(31U,30U)==66U);

    clear_map();
    /* Dragging across rail previews only; release commits original
     * HRAILROAD art and charges exactly one $10 crossing.
     */
    city_set(40U,40U,226U|4096U|8192U);
    city_set_funds(10L);
    placement_begin(EDIT_ROAD);
    assert(placement_extend(40U,40U,40U,40U)==1U);
    assert(placement_at(40U,40U)==PREVIEW_FUNDED);
    assert(placement_cost()==10L);
    assert(TILE(40U,40U)==226U); /* unchanged until mouse-up */
    assert(placement_commit()==1U);
    assert(TILE(40U,40U)==237U);
    assert(city_get_funds()==0L);
    assert(city_save_as("crossing.tmp"));
    assert(city_load("crossing.tmp"));
    assert(TILE(40U,40U)==237U);
    assert(remove("crossing.tmp")==0);

    clear_map();
    city_set(40U,40U,66U|4096U|8192U);
    city_set_funds(5L);
    placement_begin(EDIT_POWER);
    assert(placement_extend(40U,40U,40U,40U)==1U);
    assert(placement_at(40U,40U)==PREVIEW_FUNDED);
    assert(TILE(40U,40U)==66U);
    placement_cancel(); /* cancelled preview must not alter road */
    assert(TILE(40U,40U)==66U);
    assert(city_get_funds()==5L);
    placement_begin(EDIT_POWER);
    assert(placement_extend(40U,40U,40U,40U)==1U);
    assert(placement_commit()==1U);
    assert(TILE(40U,40U)==77U);
    assert((city_at(40U,40U)&CONDBIT)!=0U);

    clear_map();
    /* Power truly flows through a road crossing AND a rail crossing.
     * Plant->vertical conductor->road/power cross->wire->rail/power
     * cross->wire: both crossing IDs remain conductive during BFS.
     */
    city_set(60U,20U,750U|1024U|CONDBIT);
    city_set(60U,21U,211U|CONDBIT);
    city_set(60U,22U,66U|4096U|8192U);
    city_set(60U,23U,211U|CONDBIT);
    city_set(60U,24U,226U|4096U|8192U);
    city_set(60U,25U,211U|CONDBIT);
    assert(edit_place(EDIT_POWER,60U,22U));
    assert(edit_place(EDIT_POWER,60U,24U));
    assert(TILE(60U,22U)==77U);
    assert(TILE(60U,24U)==221U);
    assert(engine_power_scan()>=6U);
    for(y=20U;y<=25U;++y)
        assert(engine_is_powered(60U,y));
    engine_shutdown();

    /* No road or rail identity changes in the original HAPPISLE map
     * are required by this edit path. The combined intersection IDs
     * survive a normal 27120-byte city save and load.
     */
    clear_map();
    road=rail=0U;
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y) {
            unsigned int id=TILE(x,y);
            if(id>=64U&&id<=207U)++road;
            if(id>=224U&&id<=238U)++rail;
        }
    assert(road==0U && rail==0U);
    puts("Original road/rail/power intersections, drag previews, adjacency, cost and live power passed");
    return 0;
}
