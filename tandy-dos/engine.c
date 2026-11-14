/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy DOS power-network scanner.
 * This derives the conductivity/plant rules and plant capacities from
 * the original Micropolis s_power.c / s_zone.c implementation.
 *
 * Original Micropolis © 1989-2007 Electronic Arts Inc.; GPLv3-or-later
 * and additional terms in the repository COPYING file.
 *
 * The phased simulation uses this power scanner on initial load and
 * when infrastructure changes; full disaster/traffic logic is not ported.
 */
#include "city.h"
#include "engine.h"
#include "zoning.h"
#include <stdlib.h>

#ifdef __WATCOMC__
#define ENGINE_FAR far
#else
#define ENGINE_FAR
#endif

#define TILE_ID     1023U
#define PWRBIT     32768U
#define CONDBIT    16384U
#define COAL_PLANT   750U
#define NUKE_PLANT   816U
#define POWER_BYTES ((CITY_TILES + 7U) / 8U)

/* A 24KB BFS queue would push Watcom's 64KB DGROUP over its hard limit
 * once small-lot development state is enabled. Allocate it from DOS's
 * far heap instead (large-model far data pointers). It is reused for
 * the lifetime of the application, so scans never malloc in the hot path.
 */
static unsigned short ENGINE_FAR *work_queue = NULL;
/* Cached generation capacity and live use let a NEW wire or vacant
 * zoned lot extend the network without a second 12,000-tile rebuild.
 * Removing a conductor still uses the exact original full scan.
 */
static unsigned long network_capacity=0UL;
static unsigned int network_powered=0U;
static int network_valid=0;

static int ensure_queue(void)
{
    if (work_queue != NULL)
        return 1;
    work_queue = (unsigned short ENGINE_FAR *)
        malloc((size_t)CITY_TILES * sizeof(unsigned short));
    return work_queue != NULL;
}

void engine_shutdown(void)
{
    if (work_queue != NULL) {
        free(work_queue);
        work_queue = NULL;
    }
    network_valid=0;
    network_capacity=0UL;
    network_powered=0U;
}

void engine_power_invalidate(void)
{
    network_valid=0;
}

int engine_power_valid(void)
{
    return network_valid;
}

unsigned int engine_powered_count(void)
{
    return network_powered;
}
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
static int can_conduct(unsigned int x,unsigned int y,
                       unsigned short tile)
{
    unsigned int id=tile&TILE_ID;
    if((tile&CONDBIT)!=0U || is_plant(tile)) return 1;
    /* Independently zoned 1x1 lots relay electricity whether vacant
     * or built. No CONDBIT is written, so original .CTY art and zone
     * information remain intact. Ordinary unzoned grass/trees do NOT
     * conduct. This also allows power from a powered lot into a newly
     * connected wire and vice versa.
     */
    return (id==0U || (id>=21U && id<=43U)) &&
           zoning_at(x,y)!=ZONE_NONE;
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

    if (!ensure_queue()) {
        network_valid=0;
        return 0U; /* Allocation failure: retain old tile flags. */
    }
    network_valid=0;

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
            if (can_conduct((unsigned int)nx,(unsigned int)ny,
                            city_at((unsigned int)nx,(unsigned int)ny))) {
                mark_seen(index);
                if (tail < CITY_TILES)
                    work_queue[tail++] = (unsigned short)index;
            }
        }
    }
    network_capacity=capacity;
    network_powered=powered;
    network_valid=1;
    return powered;
}

/* Add an attached branch of new conductor/zone tiles. A connected
 * frontier can traverse existing unpowered wires and EMPTY RCI lots,
 * but never unzoned terrain. BFS reuses the already allocated 24KB far
 * queue and does NOT clear or scan the whole world. Every visited tile
 * gets PWRBIT as it enters the queue, so there is no second bitset or
 * repeated queue entry. The same plant-capacity limit applies.
 */
static unsigned int expand_one(unsigned int x,unsigned int y)
{
    unsigned int i,head=0U,tail=0U,added=0U,index;
    unsigned short tile;
    static const int dx[4]={0,1,0,-1};
    static const int dy[4]={-1,0,1,0};
    if(network_powered >= network_capacity || !network_valid)
        return 0U;
    tile=city_at(x,y);
    if((tile&PWRBIT)!=0U || !can_conduct(x,y,tile))
        return 0U;
    /* Only a branch touching an ALREADY powered tile may energize. */
    if(!((x>0U && engine_is_powered(x-1U,y)) ||
         (x+1U<CITY_WIDTH && engine_is_powered(x+1U,y)) ||
         (y>0U && engine_is_powered(x,y-1U)) ||
         (y+1U<CITY_HEIGHT && engine_is_powered(x,y+1U))))
        return 0U;
    if(!ensure_queue()) return 0U;
    city_set(x,y,(unsigned short)(tile|PWRBIT));
    work_queue[tail++]=(unsigned short)(x*CITY_HEIGHT+y);
    ++network_powered;
    ++added;
    while(head<tail && network_powered<network_capacity) {
        int nx,ny;
        index=work_queue[head++];
        x=index/CITY_HEIGHT;
        y=index%CITY_HEIGHT;
        for(i=0U;i<4U && network_powered<network_capacity;++i) {
            nx=(int)x+dx[i];
            ny=(int)y+dy[i];
            if(nx<0 || nx>=CITY_WIDTH || ny<0 || ny>=CITY_HEIGHT)
                continue;
            tile=city_at((unsigned int)nx,(unsigned int)ny);
            if((tile&PWRBIT)!=0U ||
               !can_conduct((unsigned int)nx,(unsigned int)ny,tile))
                continue;
            city_set((unsigned int)nx,(unsigned int)ny,
                     (unsigned short)(tile|PWRBIT));
            work_queue[tail++]=(unsigned short)
                ((unsigned int)nx*CITY_HEIGHT+(unsigned int)ny);
            ++network_powered;
            ++added;
        }
    }
    return added;
}

unsigned int engine_power_extend_rect(unsigned int x0,unsigned int y0,
                                      unsigned int x1,unsigned int y1)
{
    unsigned int x,y,total=0U;
    if(!network_valid) return 0U;
    if(x0>x1 || y0>y1 || x1>=CITY_WIDTH || y1>=CITY_HEIGHT)
        return 0U;
    for(x=x0;x<=x1;++x)
        for(y=y0;y<=y1;++y)
            total+=expand_one(x,y);
    return total;
}

