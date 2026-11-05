/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../editor.h"
#include "../connections.h"
#include <assert.h>
#include <stdio.h>
#define ID(x,y) (city_at((x),(y)) & 1023U)

static void clear_city(void)
{
    unsigned int x, y;
    for (x = 0; x < CITY_WIDTH; ++x)
        for (y = 0; y < CITY_HEIGHT; ++y)
            city_set(x, y, 0U);
}

int main(void)
{
    city_make_demo();
    clear_city();
    /* Vertical streets must use 67, NOT the old hardcoded horizontal 66. */
    assert(edit_place(EDIT_ROAD, 10U, 10U));
    assert(edit_place(EDIT_ROAD, 10U, 11U));
    assert(ID(10U,10U) == 67U);
    assert(ID(10U,11U) == 67U);
    assert(edit_place(EDIT_ROAD, 11U, 11U));
    assert(ID(10U,11U) == 68U || ID(10U,11U) == 73U);
    /* Add south and west and verify the original connection table. */
    assert(edit_place(EDIT_ROAD, 10U, 12U));
    assert(ID(10U,11U) == 73U); /* north+east+south=7 */
    assert(edit_place(EDIT_ROAD, 9U, 11U));
    assert(ID(10U,11U) == 76U); /* four-way=15 */
    assert(edit_place(EDIT_BULLDOZE, 9U, 11U));
    assert(ID(10U,11U) == 73U);
    clear_city();
    assert(edit_place(EDIT_ROAD, 20U, 20U));
    assert(edit_place(EDIT_ROAD, 21U, 20U));
    assert(ID(20U,20U) == 66U);
    assert(ID(21U,20U) == 66U);
    assert(edit_place(EDIT_POWER, 30U,30U));
    assert(edit_place(EDIT_POWER, 30U,31U));
    assert(ID(30U,30U) == 211U);
    assert(ID(30U,31U) == 211U);
    assert(edit_place(EDIT_POWER, 31U,31U));
    assert(ID(30U,31U) == 212U || ID(30U,31U) == 217U);
    puts("Road and wire adjacency regression tests passed");
    return 0;
}
