/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy DOS power-network scanner.
 * This derives the conductivity/plant rules and plant capacities from
 * the original Micropolis s_power.c / s_zone.c implementation.
 *
 * Original Micropolis © 1989-2007 Electronic Arts Inc.; GPLv3-or-later
 * and additional terms in the repository COPYING file.
 *
 * This is an isolated foundation for porting the real simulation engine.
 * The full scheduled budget/growth/disaster simulator is NOT connected.
 */
#include "city.h"
#include "engine.h"

#define TILE_ID     1023U
#define PWRBIT     32768U
#define CONDBIT    16384U
#define COAL_PLANT   750U
#define NUKE_PLANT   816U
#define POWER_BYTES ((CITY_TILES + 7U) / 8U)

/* Original Micropolis has a stack-based scan. Use bounded BFS to make the
 * traversal deterministic for DOS, without recursion or Unix allocation.
 * The queue is 24KB, below DOS's 64KB per-object limit.
 */
static unsigned short work_queue[CITY_TILES];
static unsigned char discovered[POWER_BYTES];

static int seen(unsigned int index)
{
    return (discovered[index >> 3] & (1U << (index & 7U))) != 0;
}
static void mark_seen(unsigned int index)
{
    discovered[index >> 3] |= (unsigned char)(1U << (index & 7U));
}
static int is_plant(unsigned short tile)
{
    unsigned int id = tile & TILE_ID;
    return id == COAL_PLANT || id == NUKE_PLANT;
}
static int can_conduct(unsigned short tile)
{
    return (tile & CONDBIT) != 0U || is_plant(tile);
}

int engine_is_powered(unsigned int x, unsigned int y)
{
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    return (city_at(x, y) & PWRBIT) != 0U;
}

unsigned int engine_power_scan(void)
{
    unsigned int x, y, index, head = 0U, tail = 0U, i, powered = 0U;
    unsigned short tile;
    unsigned long capacity = 0UL;
    static const int dx[4] = {0, 1, 0, -1};
    static const int dy[4] = {-1, 0, 1, 0};

    /* Clear stale power flags without destroying tile ID or other flags.
     * Seeds are the real POWERPLANT and NUCLEAR zone-center IDs.
     */
    for (i = 0U; i < POWER_BYTES; ++i)
        discovered[i] = 0U;
    for (x = 0U; x < CITY_WIDTH; ++x) {
        for (y = 0U; y < CITY_HEIGHT; ++y) {
            tile = city_at(x, y);
            if ((tile & PWRBIT) != 0U)
                city_set(x, y, (unsigned short)(tile & ~PWRBIT));
            if (is_plant(tile)) {
                index = x * CITY_HEIGHT + y;
                mark_seen(index);
                work_queue[tail++] = (unsigned short)index;
                if ((tile & TILE_ID) == COAL_PLANT)
                    capacity += 700UL;
                else
                    capacity += 2000UL;
            }
        }
    }

    while (head < tail && (unsigned long)powered < capacity) {
        int nx, ny;
        index = work_queue[head++];
        x = index / CITY_HEIGHT;
        y = index % CITY_HEIGHT;
        tile = city_at(x, y);
        city_set(x, y, (unsigned short)(tile | PWRBIT));
        ++powered;

        for (i = 0U; i < 4U; ++i) {
            nx = (int)x + dx[i];
            ny = (int)y + dy[i];
            if (nx < 0 || ny < 0 || nx >= CITY_WIDTH || ny >= CITY_HEIGHT)
                continue;
            index = (unsigned int)nx * CITY_HEIGHT + (unsigned int)ny;
            if (seen(index))
                continue;
            if (can_conduct(city_at((unsigned int)nx, (unsigned int)ny))) {
                mark_seen(index);
                if (tail < CITY_TILES)
                    work_queue[tail++] = (unsigned short)index;
            }
        }
    }
    return powered;
}
