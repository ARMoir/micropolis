/* SPDX-License-Identifier: GPL-3.0-or-later
 * Portable city-data regression test (run with host C compiler).
 */
#include "../city.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char *name = "testcity.tmp";
    FILE *f;
    unsigned int x, y, n;
    unsigned short tile;

    f = fopen(name, "wb");
    assert(f != NULL);
    for (n = 0; n < 3120U; ++n)
        assert(fputc(0, f) != EOF);
    for (x = 0; x < CITY_WIDTH; ++x) {
        for (y = 0; y < CITY_HEIGHT; ++y) {
            tile = (unsigned short)(0x4000U | ((x * 100U + y) & 1023U));
            assert(fputc((tile >> 8) & 255U, f) != EOF);
            assert(fputc(tile & 255U, f) != EOF);
        }
    }
    assert(fclose(f) == 0);
    assert(city_load(name) == 1);
    assert(city_at(0, 0) == 0x4000U);
    assert(city_at(12, 34) == (unsigned short)(0x4000U | (1234U & 1023U)));
    assert(city_at(119, 99) == (unsigned short)(0x4000U | (11999U & 1023U)));
    assert(city_at(120, 0) == 0U);
    assert(remove(name) == 0);

    f = fopen(name, "wb");
    assert(f != NULL);
    assert(fputc(0, f) != EOF);
    assert(fclose(f) == 0);
    assert(city_load(name) == 0);
    assert(remove(name) == 0);

    city_make_demo();
    assert(city_at(0, 0) == 2U);
    assert(city_at(51, 30) == 66U);
    puts("Micropolis CTY parser test passed");
    return 0;
}
