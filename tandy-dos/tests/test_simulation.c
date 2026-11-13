/* SPDX-License-Identifier: GPL-3.0-or-later
 * Deterministic 16-bit simulation regression: real services, RCI growth,
 * city time, taxes, sidecar persistence, pause, and brownouts.
 */
#include "../city.h"
#include "../zoning.h"
#include "../simulation.h"
#include <assert.h>
#include <stdio.h>

static void run_months(unsigned int months)
{
    unsigned int i;
    long target = city_get_time() + (long)months * 4L;
    for (i = 0U; i < 9000U && city_get_time() < target; ++i)
        (void)simulation_advance(320U);
    assert(city_get_time() == target);
}

int main(int argc, char **argv)
{
    unsigned int x, y, level;
    long balance;
    const struct sim_statistics *st;
    city_make_demo();
    for (x=0U; x<CITY_WIDTH; ++x)
        for (y=0U; y<CITY_HEIGHT; ++y)
            city_set(x,y,0U);
    zoning_clear();
    city_set_time(0L);
    city_set_funds(5000L);
    city_set_tax_rate(7U);
    /* Coal source -> connected conductor -> powered 1x1 lot.
     * Road on a separate neighbor provides road access.
     */
    city_set(10U,10U,750U | 1024U | 16384U);
    for (x=11U; x<=15U; ++x)
        city_set(x,10U,208U | 16384U);
    city_set(16U,11U,66U | 4096U | 8192U);
    assert(zoning_set(15U,11U,ZONE_RES));
    assert(zoning_development_at(15U,11U)==0U);
    /* An authentic, original 3x3 building must stay untouched. */
    city_set(80U,80U,265U | 1024U | 16384U);
    simulation_init();
    assert(simulation_stats()->year == 1900U);
    assert(simulation_stats()->month == 1U);
    assert(simulation_speed() == 1U);
    simulation_set_speed(4U);
    assert(simulation_speed() == 4U);
    simulation_set_paused(1);
    assert(!simulation_advance(320U));
    assert(city_get_time()==0L);
    simulation_set_paused(0);
    run_months(32U);
    level = zoning_development_at(15U,11U);
    assert(level > 0U && level <= 3U);
    assert((city_at(15U,11U) & 1023U) == 0U);
    assert((city_at(80U,80U) & 1023U) == 265U);
    st = simulation_stats();
    assert(st->residents > 0UL);
    assert(st->road_tiles == 1U);
    assert(st->wire_tiles == 5U);
    assert(st->plants == 1U);
    assert(st->powered_tiles >= 6U);
    assert(st->month == 9U);
    assert(st->year == 1902U);
    assert(city_get_tax_rate() == 7U);
    /* Taxes and upkeep must now be applied automatically each year. */
    assert(st->last_maintenance > 0L);
    assert(st->last_income > 0L);
    balance = city_get_funds();
    assert(balance != 5000L);
    assert(city_save_as("sim_test.cty"));
    assert(zoning_save("sim_test.zon"));
    assert(zoning_set_development(15U,11U,0U));
    assert(city_load("sim_test.cty"));
    assert(zoning_load("sim_test.zon"));
    assert(zoning_development_at(15U,11U) == level);
    assert(city_get_funds() == balance);
    assert(city_get_time() == 128L);
    assert(remove("sim_test.cty") == 0);
    assert(remove("sim_test.zon") == 0);

    /* Stop feeding electricity; developed lot should eventually shrink. */
    city_set(10U,10U,0U);
    simulation_mark_power_dirty();
    run_months(100U);
    assert(zoning_development_at(15U,11U) == 0U);
    /* An untouched authentic vacant 3x3 RCI plot must be eligible for
     * complete nine-sprite development when ring-road and power exist.
     */
    for (x=0U; x<CITY_WIDTH; ++x)
        for (y=0U; y<CITY_HEIGHT; ++y)
            city_set(x,y,0U);
    zoning_clear();
    city_set_time(0L);
    city_set_funds(20000L);
    city_set(23U,29U,750U | 1024U | 16384U);
    for (x=24U; x<=28U; ++x)
        city_set(x,29U,208U | 16384U);
    for (y=0U; y<3U; ++y)
        for (x=0U; x<3U; ++x) {
            unsigned short value = (unsigned short)(240U + y*3U + x +
                                                     8192U + 16384U);
            if (x == 1U && y == 1U)
                value |= 1024U;
            city_set(29U+x,29U+y,value);
        }
    city_set(32U,30U,66U | 8192U | 4096U);
    simulation_init();
    run_months(24U);
    /* More than one density stage may develop within 24 months.
     * Confirm the entire nine-sprite group remains coherent.
     */
    {
        unsigned int center = city_at(30U,30U) & 1023U;
        assert(center >= 265U && center <= 400U);
        assert((center-265U)%9U == 0U);
        assert((city_at(29U,29U) & 1023U) == center-4U);
        assert((city_at(31U,31U) & 1023U) == center+4U);
        assert(simulation_stats()->growth_total != 0U);
    }
    assert((city_at(30U,30U) & 1024U) != 0U);
    assert((city_at(32U,30U) & 1023U) == 66U);
    /* Demo must be playable from a cold boot without loading or editing
     * an external city: generator supplies a coal plant + wires + roads.
     */
    city_make_demo();
    zoning_clear();
    simulation_seed_demo();
    assert((city_at(42U,26U) & 1023U) == 750U);
    assert((city_at(49U,30U) & 1023U) == 210U);
    assert(zoning_at(50U,31U) == ZONE_RES);
    assert(zoning_at(56U,31U) == ZONE_IND);
    assert(zoning_at(59U,31U) == ZONE_COM);
    assert(zoning_at(62U,36U) == ZONE_RES);
    simulation_init();
    run_months(36U);
    assert(zoning_development_at(50U,31U) > 0U ||
           zoning_development_at(51U,31U) > 0U ||
           zoning_development_at(52U,31U) > 0U);
    if (argc > 1) {
        assert(city_load(argv[1]));
        zoning_seed_from_city();
        simulation_init();
        run_months(12U);
        st = simulation_stats();
        printf("Original city regression: P=%lu Jobs=%lu "
               "growth=%u serviced=%u no-road=%u no-power=%u "
               "net-power=%u\\n",
               st->residents,
               st->commercial_jobs + st->industrial_jobs,
               st->growth_total, st->lots_ready,
               st->lots_no_road, st->lots_no_power,
               st->powered_tiles);
    }
    puts("Phased RCI growth (1x1 / 3x3), demo, clock, taxes and persistence passed");
    return 0;
}
