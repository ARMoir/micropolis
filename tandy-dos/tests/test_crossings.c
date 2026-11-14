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
    /* Rail is the reciprocal crossing tool, using source IDs. */
    test_cross(EDIT_RAIL,210U,222U,1);
    test_cross(EDIT_RAIL,211U,221U,1);
    test_cross(EDIT_RAIL,66U,238U,0);
    test_cross(EDIT_RAIL,67U,237U,0);
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
    /* River next to a roadway is NOW valid original bridge terrain.
     * Open water without a bank remains blocked (tested below).
     */
    assert(infrastructure_can_place(EDIT_ROAD,21U,20U));
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

    /* Authentic original H/V bridge sprites over river, river edge
     * and channel. An isolated water tile remains unbuildable.
     */
    clear_map();
    city_set(20U,20U,2U);
    assert(!infrastructure_can_place(EDIT_ROAD,20U,20U));
    assert(!edit_place(EDIT_ROAD,20U,20U));
    assert(!infrastructure_can_place(EDIT_POWER,20U,20U));
    city_set(19U,20U,66U|4096U|8192U);
    assert(infrastructure_can_place(EDIT_ROAD,20U,20U));
    assert(infrastructure_road_bridge_id(20U,20U)==64U);
    assert(edit_place(EDIT_ROAD,20U,20U));
    assert(TILE(20U,20U)==64U);
    assert((city_at(20U,20U)&4096U)!=0U);
    assert((city_at(20U,20U)&16384U)==0U);
    assert(edit_place(EDIT_BULLDOZE,20U,20U));
    assert(TILE(20U,20U)==2U);
    clear_map();
    city_set(20U,19U,67U|4096U|8192U);
    city_set(20U,20U,3U);
    city_set(20U,21U,4U);
    assert(infrastructure_road_bridge_id(20U,20U)==65U);
    assert(edit_place(EDIT_ROAD,20U,20U));
    assert(edit_place(EDIT_ROAD,20U,21U));
    assert(TILE(20U,20U)==65U && TILE(20U,21U)==65U);
    assert(TILE(20U,19U)==67U);
    /* Source BRWV (95) and 948..951 are vertical variants, while
     * BRWH (79) and 828..831 are horizontal bridge animations.
     * Old traffic normalization must not confuse their directions.
     */
    clear_map();
    city_set(20U,20U,2U);
    city_set(20U,19U,95U);
    assert(infrastructure_road_bridge_id(20U,20U)==65U);
    city_set(20U,19U,948U);
    assert(infrastructure_road_bridge_id(20U,20U)==65U);
    city_set(20U,19U,0U);
    city_set(19U,20U,79U);
    assert(infrastructure_road_bridge_id(20U,20U)==64U);
    city_set(19U,20U,828U);
    assert(infrastructure_road_bridge_id(20U,20U)==64U);
    assert(edit_place(EDIT_BULLDOZE,19U,20U));
    assert(TILE(19U,20U)==2U);
    city_set(19U,20U,951U);
    assert(edit_place(EDIT_BULLDOZE,19U,20U));
    assert(TILE(19U,20U)==2U);

    /* Drag over four river tiles: $50 per bridge plus $10 dry land.
     * The preview must accept water tiles following already funded
     * bridge previews, even when the city map hasn't changed yet.
     */
    clear_map();
    city_set(20U,20U,66U|4096U|8192U);
    for(x=21U;x<=24U;++x) city_set(x,20U,2U);
    city_set_funds(210L);
    placement_begin(EDIT_ROAD);
    assert(placement_extend(20U,20U,25U,20U)==6U);
    assert(placement_at(20U,20U)==PREVIEW_BLOCKED);
    for(x=21U;x<=25U;++x)
        assert(placement_at(x,20U)==PREVIEW_FUNDED);
    assert(placement_cost()==210L);
    assert(placement_commit()==5U);
    for(x=21U;x<=24U;++x) assert(TILE(x,20U)==64U);
    assert(TILE(25U,20U)==66U);
    assert(city_get_funds()==0L);
    assert(city_save_as("bridge.tmp"));
    assert(city_load("bridge.tmp"));
    assert(TILE(23U,20U)==64U);
    assert(remove("bridge.tmp")==0);

    /* Not enough money: the unpurchased bridge remains as water. */
    clear_map();
    city_set(20U,20U,66U|4096U|8192U);
    for(x=21U;x<=23U;++x) city_set(x,20U,2U);
    city_set_funds(100L);
    placement_begin(EDIT_ROAD);
    (void)placement_extend(20U,20U,23U,20U);
    assert(placement_at(21U,20U)==PREVIEW_FUNDED);
    assert(placement_at(22U,20U)==PREVIEW_FUNDED);
    assert(placement_at(23U,20U)==PREVIEW_NO_FUNDS);
    assert(placement_commit()==2U);
    assert(TILE(21U,20U)==64U && TILE(22U,20U)==64U);
    assert(TILE(23U,20U)==2U);
    assert(city_get_funds()==0L);

    /* Commit must also support drag right-to-left. The first water
     * square in x-major order is farthest from the right-hand shore.
     */
    clear_map();
    city_set(25U,20U,66U|4096U|8192U);
    for(x=21U;x<=24U;++x) city_set(x,20U,2U);
    city_set_funds(200L);
    placement_begin(EDIT_ROAD);
    (void)placement_extend(25U,20U,21U,20U);
    assert(placement_cost()==200L);
    assert(placement_commit()==4U);
    for(x=21U;x<=24U;++x) assert(TILE(x,20U)==64U);
    assert(city_get_funds()==0L);

    /* Railroad bridges share the original 2-bit drag transaction:
     * ground tracks cost $20 and river segments $100 per square.
     */
    clear_map();
    city_set(40U,40U,226U|4096U|8192U);
    for(x=41U;x<=43U;++x) city_set(x,40U,2U);
    city_set_funds(320L);
    placement_begin(EDIT_RAIL);
    assert(placement_extend(40U,40U,44U,40U)==5U);
    assert(placement_at(40U,40U)==PREVIEW_BLOCKED);
    assert(placement_cost()==320L);
    assert(placement_commit()==4U);
    for(x=41U;x<=43U;++x) assert(TILE(x,40U)==224U);
    assert(TILE(44U,40U)==226U);
    assert(city_get_funds()==0L);
    clear_map();
    city_set(40U,40U,227U|4096U|8192U);
    for(y=41U;y<=42U;++y) city_set(40U,y,3U);
    assert(infrastructure_rail_bridge_id(40U,41U)==225U);
    assert(edit_place(EDIT_RAIL,40U,41U));
    assert(edit_place(EDIT_RAIL,40U,42U));
    assert(TILE(40U,41U)==225U && TILE(40U,42U)==225U);

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
