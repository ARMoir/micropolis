/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../engine.h"
#include "../zoning.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int x, y;
    unsigned int result;
    city_make_demo();
    for (x=0; x<CITY_WIDTH; ++x)
        for (y=0; y<CITY_HEIGHT; ++y)
            city_set(x,y,0U);

    /* Coal plant, four adjacent wires and a connected residential zone. */
    city_set(10U, 10U, 750U | 1024U | 16384U);
    for (x=11U; x<15U; ++x)
        city_set(x,10U,208U | 16384U | 4096U | 8192U);
    city_set(15U, 10U, 244U | 1024U | 16384U);
    city_set(30U, 30U, 208U | 16384U); /* disconnected wire */
    city_set(16U, 10U, 66U | 4096U | 8192U); /* road, not conductive */
    city_set(17U, 10U, 208U | 16384U);
    result = engine_power_scan();
    assert(result == 6U);
    assert(engine_is_powered(10U, 10U));
    assert(engine_is_powered(15U, 10U));
    assert(!engine_is_powered(30U, 30U));
    assert(!engine_is_powered(16U, 10U));
    assert(!engine_is_powered(17U, 10U));

    /* Remove plant and re-run; old power status must be cleared. */
    city_set(10U,10U,0U);
    result = engine_power_scan();
    assert(result == 0U);
    assert(!engine_is_powered(15U, 10U));

    /* The key hardware report: VACANT, independently zoned 1x1
     * squares must pass electricity BEFORE even one house appears.
     * Do not give power to empty unzoned grass or woods.
     */
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y)
            city_set(x,y,0U);
    zoning_clear();
    city_set(10U,10U,750U|1024U|16384U);
    city_set(11U,10U,208U|16384U);
    assert(zoning_set(12U,10U,ZONE_RES));
    assert(zoning_set(13U,10U,ZONE_RES));
    for(x=15U;x<=22U;++x)
        assert(zoning_set(x,10U,ZONE_COM));
    assert(zoning_development_at(12U,10U)==0U);
    assert(zoning_development_at(22U,10U)==0U);
    assert(engine_power_scan()==4U); /* plant, wire, 2 vacant lots */
    assert(engine_is_powered(12U,10U) && engine_is_powered(13U,10U));
    assert(!engine_is_powered(15U,10U));
    assert(!engine_is_powered(14U,11U)); /* unzoned is inert */
    assert(engine_power_valid());

    /* One freshly placed wire connects a distant vacant neighborhood.
     * The 8 empty lots receive power immediately from the existing
     * cached graph, without re-running a 120x100 flood fill.
     */
    city_set(14U,10U,208U|16384U);
    assert(engine_power_extend_rect(14U,10U,14U,10U)==9U);
    assert(engine_powered_count()==13U);
    for(x=12U;x<=22U;++x) {
        assert(engine_is_powered(x,10U));
        if(x!=14U) assert(zoning_development_at(x,10U)==0U);
    }
    assert((city_at(18U,10U)&1023U)==0U);
    assert(!engine_is_powered(19U,11U)); /* adjacent but not zoned */
    assert(engine_power_extend_rect(14U,10U,14U,10U)==0U);

    /* Removing the link is fundamentally different from adding it:
     * a genuine full rebuild must discard stranded downstream power.
     */
    city_set(14U,10U,0U);
    assert(engine_power_scan()==4U);
    assert(!engine_is_powered(22U,10U));
    assert(engine_is_powered(12U,10U));
    engine_shutdown();
    puts("Vacant-lot electrical relay and no-rescan wire expansion passed");
    puts("Micropolis power propagation regression test passed");
    return 0;
}
