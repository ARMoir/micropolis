/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../engine.h"
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
    puts("Micropolis power propagation regression test passed");
    return 0;
}
