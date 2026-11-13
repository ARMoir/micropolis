/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../city.h"
#include "../zoning.h"
#include <stdio.h>
#include <assert.h>

static void flat_city(void)
{
    unsigned int x, y;
    city_make_demo();
    for (x = 0; x < CITY_WIDTH; ++x)
        for (y = 0; y < CITY_HEIGHT; ++y)
            city_set(x, y, 0U);
    zoning_clear();
}
int main(void)
{
    unsigned int i;
    FILE *f;
    flat_city();
    assert(zoning_set(4U, 6U, ZONE_RES) == 1);
    assert(zoning_at(4U, 6U) == ZONE_RES);
    assert(zoning_count(ZONE_RES) == 1U);
    assert(!zoning_set(4U, 6U, ZONE_RES));
    assert(zoning_paint_line(5U, 7U, 12U, 7U, ZONE_COM) == 8U);
    assert(zoning_count(ZONE_COM) == 8U);
    assert(zoning_paint_line(13U, 8U, 13U, 15U, ZONE_IND) == 8U);
    assert(zoning_count(ZONE_IND) == 8U);
    assert(zoning_paint_line(20U, 20U, 23U, 23U, ZONE_RES) == 4U);
    assert(zoning_at(22U, 22U) == ZONE_RES);
    assert(zoning_paint_line(12U, 7U, 12U, 7U, ZONE_NONE) == 1U);
    assert(zoning_at(12U, 7U) == ZONE_NONE);
    assert(!zoning_can_set(120U, 0U, ZONE_COM));
    assert(!zoning_can_set(0U, 100U, ZONE_RES));
    city_set(5U, 5U, 66U);
    assert(!zoning_set(5U, 5U, ZONE_COM));
    city_set(6U, 6U, 2U);
    assert(!zoning_set(6U, 6U, ZONE_IND));
    city_set(7U, 7U, 249U);
    assert(zoning_set(7U, 7U, ZONE_RES));
    assert(!zoning_set(7U, 7U, ZONE_COM));

    assert(zoning_save("testzone.zon"));
    zoning_clear();
    assert(zoning_load("testzone.zon"));
    assert(zoning_at(4U, 6U) == ZONE_RES);
    assert(zoning_at(7U, 7U) == ZONE_RES);
    assert(zoning_at(5U, 7U) == ZONE_COM);
    assert(zoning_at(13U, 10U) == ZONE_IND);
    assert(!zoning_load("missing.zon"));
    assert(zoning_at(4U, 6U) == ZONE_RES);
    /* Corrupt one byte: reject without losing existing zone state. */
    f = fopen("testzone.zon", "r+b");
    assert(f != NULL);
    assert(fseek(f, 8L + 4L * CITY_HEIGHT + 6L, SEEK_SET) == 0);
    assert(fputc(99, f) != EOF);
    assert(fclose(f) == 0);
    assert(!zoning_load("testzone.zon"));
    assert(zoning_at(4U, 6U) == ZONE_RES);
    assert(remove("testzone.zon") == 0);
    /* Original 3x3 centers can be recognized without changing .CTY. */
    flat_city();
    for (i = 0U; i < 9U; ++i)
        city_set(19U + i % 3U, 19U + i / 3U,
                 (unsigned short)(240U + i));
    city_set(20U, 20U, 244U | 1024U);
    zoning_seed_from_city();
    assert(zoning_count(ZONE_RES) == 9U);
    assert(zoning_at(19U, 19U) == ZONE_RES);
    assert(zoning_at(20U, 20U) == ZONE_RES);
    puts("Per-tile freeform zoning, persistence and safety tests passed");
    return 0;
}
