/* SPDX-License-Identifier: GPL-3.0-or-later
 * Portable city-data regression test (run with host C compiler).
 */
#include "../city.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv)
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

    /* Opening an imported city should focus on inhabited neighborhoods,
     * never default to an empty ocean-only corner.
     */
    {
        unsigned int vx = 0U, vy = 0U;
        assert(!city_suggest_view(32U,21U,&vx,&vy));
        city_set(86U,74U,265U | 1024U);
        city_set(87U,75U,274U | 1024U);
        city_set(88U,76U,283U | 1024U);
        assert(city_suggest_view(32U,21U,&vx,&vy));
        assert(vx > 0U && vy > 0U);
        assert(86U >= vx && 86U < vx+32U);
        assert(74U >= vy && 74U < vy+21U);
        assert(!city_suggest_view(0U,21U,&vx,&vy));
    }
    if (argc > 1) {
        unsigned int vx,vy,ix,iy,centers=0U;
        assert(city_load(argv[1]));
        assert(city_suggest_view(32U,21U,&vx,&vy));
        for(ix=vx; ix<vx+32U; ++ix)
            for(iy=vy; iy<vy+21U; ++iy) {
                unsigned short t=city_at(ix,iy);
                unsigned int id=t&1023U;
                if ((t&1024U) && id>=265U && id<409U)
                    ++centers;
            }
        assert(centers > 0U);
        printf("Original city view starts at %u,%u with %u "
               "residential centers visible\n",vx,vy,centers);
    }
    puts("Micropolis CTY parser and neighborhood focus tests passed");
    return 0;
}
