/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../editor.h"
#include "../placement.h"
#include "../zoning.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int x, y;
    city_make_demo();
    zoning_clear();
    /* Precise budget cap and no map mutation before mouse-up. */
    city_set_funds(35L);
    for (x = 30U; x <= 40U; ++x)
        city_set(x, 90U, 0U);
    placement_begin(EDIT_ROAD);
    assert(placement_extend(30U, 90U, 34U, 90U) == 5U);
    assert(placement_count() == 3U);
    assert(placement_rejected() == 2U);
    assert(placement_cost() == 30L);
    assert(placement_remaining() == 5L);
    assert(placement_at(30U, 90U) == PREVIEW_FUNDED);
    assert(placement_at(33U, 90U) == PREVIEW_NO_FUNDS);
    assert(city_at(30U, 90U) == 0U);
    assert(city_get_funds() == 35L);
    assert(placement_extend(30U, 90U, 32U, 90U) == 0U);
    assert(placement_count() == 3U);
    placement_cancel();
    assert(city_at(30U, 90U) == 0U);
    assert(city_get_funds() == 35L);

    placement_begin(EDIT_ROAD);
    assert(placement_extend(30U, 90U, 34U, 90U) == 5U);
    assert(placement_commit() == 3U);
    assert(city_get_funds() == 5L);
    assert(!placement_active());
    for (x = 30U; x <= 32U; ++x)
        assert((city_at(x, 90U) & 1023U) >= 66U);
    assert(city_at(33U, 90U) == 0U);

    /* Existing roads are invalid rather than charged twice. */
    placement_begin(EDIT_ROAD);
    assert(placement_extend(30U, 90U, 31U, 90U) == 2U);
    assert(placement_count() == 0U);
    assert(placement_at(30U, 90U) == PREVIEW_BLOCKED);
    placement_cancel();

    /* Zones also preview, paint and persist cost only after release. */
    city_set_funds(36L);
    for (x = 50U; x <= 54U; ++x)
        city_set(x, 90U, 0U);
    placement_begin(EDIT_RESIDENTIAL);
    assert(placement_extend(50U, 90U, 54U, 90U) == 5U);
    assert(placement_count() == 3U);
    assert(placement_cost() == 36L);
    assert(zoning_at(50U, 90U) == ZONE_NONE);
    assert(placement_commit() == 3U);
    assert(zoning_at(52U, 90U) == ZONE_RES);
    assert(zoning_at(53U, 90U) == ZONE_NONE);
    assert(city_get_funds() == 0L);
    assert(placement_unit_cost(EDIT_POWER) == 5L);
    assert(placement_unit_cost(EDIT_ROAD) == 10L);
    assert(placement_unit_cost(EDIT_INDUSTRIAL) == 12L);

    city_set_funds(200L);
    for (y = 60U; y <= 63U; ++y)
        city_set(58U, y, 0U);
    placement_begin(EDIT_POWER);
    assert(placement_extend(58U, 60U, 58U, 63U) == 4U);
    assert(placement_cost() == 20L);
    assert(placement_commit() == 4U);
    assert(city_get_funds() == 180L);
    for (y = 60U; y <= 63U; ++y)
        assert((city_at(58U, y) & 1023U) >= 208U);

    /* Original city file preserves the live funds when saved/reopened. */
    assert(city_save_as("placement_test.cty"));
    city_set_funds(9876L);
    assert(city_load("placement_test.cty"));
    assert(city_get_funds() == 180L);
    assert(remove("placement_test.cty") == 0);
    puts("Preview/commit, cancellation, capped spending and CTY funds passed");
    return 0;
}
