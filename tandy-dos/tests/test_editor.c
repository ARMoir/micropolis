/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../editor.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int x,y;
    /* Clear artificial demo map to deterministic flat terrain. */
    city_make_demo();
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
    assert(edit_place(EDIT_ROAD, 3U, 3U));
    assert((city_at(3,3) & 1023U) == 66U);
    assert(!edit_place(EDIT_POWER, 3U, 3U)); /* collision */
    assert(edit_place(EDIT_BULLDOZE, 3U, 3U));
    assert(city_at(3,3) == 0U);
    assert(city_save_as("editor.tmp"));
    assert(city_load("editor.tmp"));
    assert((city_at(15,15) & 1023U) == 244U);
    assert(remove("editor.tmp") == 0);
    puts("Micropolis edit / zoning / CTY round-trip tests passed");
    return 0;
}
