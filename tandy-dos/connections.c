/* SPDX-License-Identifier: GPL-3.0-or-later
 * Road and power-line connection masks adapted from the ORIGINAL
 * Micropolis micropolis-activity/src/sim/w_con.c (_FixSingle tables).
 *
 * Original Micropolis Copyright (C) 1989-2007 Electronic Arts Inc.
 * GPLv3-or-later with the additional terms in COPYING.
 */
#include "city.h"
#include "connections.h"

#define ID_MASK 1023U
#define BULLBIT 4096U
#define BURNBIT 8192U
#define CONDBIT 16384U

/* Neighbor mask bit order: north=1, east=2, south=4, west=8.
 * These values are copied from the original _RoadTable and _WireTable.
 */
static const unsigned short road_table[16] = {
    66U, 67U, 66U, 68U,
    67U, 67U, 69U, 73U,
    66U, 71U, 66U, 72U,
    70U, 75U, 74U, 76U
};

static const unsigned short wire_table[16] = {
    210U, 211U, 210U, 212U,
    211U, 211U, 213U, 217U,
    210U, 215U, 210U, 216U,
    214U, 219U, 218U, 220U
};

/* Traffic-road variants 80..207 encode their base connection tile
 * in the low nibble, like NeutralizeRoad() in w_con.c.
 */
static unsigned int neutral_road(unsigned short tile)
{
    unsigned int id = tile & ID_MASK;
    if (id >= 64U && id <= 207U)
        id = 64U + (id & 15U);
    return id;
}

static int neighbor_road(unsigned int x, unsigned int y, int ns)
{
    unsigned int id;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    id = neutral_road(city_at(x, y));
    if (ns)
        return id >= 64U && id <= 78U && id != 64U && id != 77U;
    return id >= 64U && id <= 78U && id != 65U && id != 78U;
}

static int neighbor_wire(unsigned int x, unsigned int y, int ns)
{
    unsigned short tile;
    unsigned int id;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    tile = city_at(x, y);
    if (!(tile & CONDBIT))
        return 0;
    id = neutral_road(tile);
    if (ns)
        return id != 209U && id != 78U && id != 222U;
    return id != 208U && id != 77U && id != 221U;
}

static void fix_one(unsigned int x, unsigned int y)
{
    unsigned short tile;
    unsigned int id, mask = 0U;

    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return;
    tile = city_at(x, y);
    id = neutral_road(tile);

    if (id >= 66U && id <= 76U) {
        if (y > 0U && neighbor_road(x, y - 1U, 1)) mask |= 1U;
        if (x + 1U < CITY_WIDTH && neighbor_road(x + 1U, y, 0)) mask |= 2U;
        if (y + 1U < CITY_HEIGHT && neighbor_road(x, y + 1U, 1)) mask |= 4U;
        if (x > 0U && neighbor_road(x - 1U, y, 0)) mask |= 8U;
        city_set(x, y, road_table[mask] | BULLBIT | BURNBIT);
        return;
    }
    if (id >= 210U && id <= 220U) {
        if (y > 0U && neighbor_wire(x, y - 1U, 1)) mask |= 1U;
        if (x + 1U < CITY_WIDTH && neighbor_wire(x + 1U, y, 0)) mask |= 2U;
        if (y + 1U < CITY_HEIGHT && neighbor_wire(x, y + 1U, 1)) mask |= 4U;
        if (x > 0U && neighbor_wire(x - 1U, y, 0)) mask |= 8U;
        city_set(x, y, wire_table[mask] | BULLBIT | BURNBIT | CONDBIT);
    }
}

void connections_fix_cross(unsigned int x, unsigned int y)
{
    /* Read neighbors from the completed new layout, not stale tile IDs. */
    fix_one(x, y);
    if (x > 0U) fix_one(x - 1U, y);
    if (x + 1U < CITY_WIDTH) fix_one(x + 1U, y);
    if (y > 0U) fix_one(x, y - 1U);
    if (y + 1U < CITY_HEIGHT) fix_one(x, y + 1U);
    fix_one(x, y);
}
