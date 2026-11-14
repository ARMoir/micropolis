/* SPDX-License-Identifier: GPL-3.0-or-later
 * Phased, deterministic, bounded-cost RCI growth and yearly cash flow.
 *
 * Inspired by original Micropolis:
 *   s_sim.c: one scan divided into phases; CityTime steps; taxes at 48.
 *   s_zone.c: powered zones, road access, and R/C/I population/demand.
 *   s_power.c: original conductor flags and plant capacities.
 *
 * Not yet the exact original simulator: there is no traffic assignment,
 * land-value/pollution/crime model, school/service demand, disasters,
 * 3x3 building redevelopment, or complete original budget algorithm.
 * User-added per-tile zone stages persist in the extended ZON v3 sidecar;
 * original CTY sprites and building footprints are never overwritten.
 */
#include "city.h"
#include "engine.h"
#include "simulation.h"
#include "zoning.h"
#include <stddef.h>

#define TILE_MASK 1023U
#define ZONE_BIT 1024U
#define PWR_BIT 32768U
#define SIM_CHUNK_LIMIT 320U
/* Six Manhattan-distance steps to the nearest road OR rail.
 * Both small lots and original 3x3 zones use the same access test.
 */
#define TRANSPORT_RADIUS 6
#define LOT_POWER_BYTES ((CITY_TILES + 7U) / 8U)

static struct sim_statistics stats;
static unsigned int cursor = 0U;
/* Avoid 16-bit division/modulo on every scanned map cell. */
static unsigned int scan_x = 0U, scan_y = 0U;
static unsigned int power_scan_count = 0U;
static unsigned int last_change_valid = 0U;
static unsigned int last_home_valid = 0U, last_home_x = 0U, last_home_y = 0U;
static unsigned int last_x, last_y;
static unsigned int last_radius = 0U;
static unsigned int changed = 0U;
static unsigned int power_dirty = 1U;
static unsigned int paused = 0U;
static unsigned int speed = 1U;
static unsigned int growth_res, growth_com, growth_ind;
static unsigned int rng = 0x31a5U;
static unsigned long pop_res, pop_com, pop_ind;
static unsigned int lots_res, lots_com, lots_ind;
static unsigned int roads, wires, plants;
static unsigned int growth_this_month = 0U, ready_month = 0U;
static unsigned int no_road_month = 0U, no_power_month = 0U;
/* 1500 bytes: which occupied small lots are connected to a source?
 * A developed house/shop/factory can relay electricity to an ADJACENT
 * developed lot. Empty/undeveloped squares never carry power.
 * Unlike adding CONDBIT to city tiles, this never mutates the original
 * CTY file or triggers a city-wide BFS when a house grows.
 */
static unsigned char lot_power[LOT_POWER_BYTES];
/* Changes only when effective electrical service may change. The GUI
 * checks visible badges in small chunks, never on an animation timer.
 */
static unsigned int power_visual_revision = 0U;
static void clear_lot_power(void)
{
    unsigned int i;
    for(i=0U;i<LOT_POWER_BYTES;++i)
        lot_power[i]=0U;
    ++power_visual_revision;
}
static unsigned int lot_power_index(unsigned int x,unsigned int y)
{
    return x*CITY_HEIGHT+y;
}
static int lot_is_powered(unsigned int x,unsigned int y)
{
    unsigned int pos;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       zoning_at(x,y)==ZONE_NONE ||
       zoning_development_at(x,y)==0U)
        return 0;
    pos=lot_power_index(x,y);
    return (lot_power[pos>>3] & (1U<<(pos&7U)))!=0U;
}
static void set_lot_power(unsigned int x,unsigned int y,int enabled)
{
    unsigned int pos=lot_power_index(x,y);
    unsigned char mask=(unsigned char)(1U<<(pos&7U));
    unsigned char old=lot_power[pos>>3];
    if(enabled) lot_power[pos>>3] |= mask;
    else lot_power[pos>>3] &= (unsigned char)~mask;
    if(old!=lot_power[pos>>3]) ++power_visual_revision;
}


/* Avoid signed integer overflow on both 8086 and native test hosts. */
static unsigned int random16(void)
{
    rng = (rng * 25173U + 13849U) & 65535U;
    return rng;
}
static int road_tile(unsigned int id)
{
    return id >= 64U && id <= 207U;
}
static int rail_tile(unsigned int id)
{
    return id >= 224U && id <= 238U;
}
/* Both roads and railways provide transportation to zones. Happisle
 * has substantially more railway than road squares, unlike the built-in
 * demo. Restricting access to roads made new residences appear inert.
 * This affects simulated service access only, never original CTY tiles
 * or the distinct road/rail display artwork.
 */
static int transport_tile(unsigned int id)
{
    return road_tile(id) || rail_tile(id);
}
static int wire_tile(unsigned int id)
{
    return id >= 208U && id <= 223U;
}
/* A plot may be up to six Manhattan-distance steps from a road/rail.
 * The bounded local search short-circuits on the first match; edits
 * require no city-wide road/rail scan.
 */
static int transport_access(unsigned int x,unsigned int y)
{
    int d,dx,dy,nx,ny;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT)return 0;
    for(d=1;d<=TRANSPORT_RADIUS;++d)
        for(dx=-d;dx<=d;++dx) {
            dy=d-(dx<0?-dx:dx);
            nx=(int)x+dx;
            if(nx<0 || nx>=CITY_WIDTH)continue;
            ny=(int)y+dy;
            if(ny>=0 && ny<CITY_HEIGHT &&
               transport_tile(city_at((unsigned int)nx,
                                      (unsigned int)ny)&TILE_MASK))
                return 1;
            if(dy && (ny=(int)y-dy)>=0 && ny<CITY_HEIGHT &&
               transport_tile(city_at((unsigned int)nx,
                                      (unsigned int)ny)&TILE_MASK))
                return 1;
        }
    return 0;
}

/* A source powers its own square and its direct neighbors. Occupied
 * independently zoned squares can then pass that power to adjacent
 * OCCUPIED squares. This single-pass frontier progresses during the
 * normal 80..320-square simulation chunks; no extra full-map walk.
 * Cached relays reset immediately whenever conductors or zoning
 * change, so abandoned islands cannot continue supplying electricity.
 */
static int has_power(unsigned int x,unsigned int y)
{
    if (engine_is_powered(x,y) || lot_is_powered(x,y)) return 1;
    if (x>0U &&
        (engine_is_powered(x-1U,y) || lot_is_powered(x-1U,y)))
        return 1;
    if (x+1U<CITY_WIDTH &&
        (engine_is_powered(x+1U,y) || lot_is_powered(x+1U,y)))
        return 1;
    if (y>0U &&
        (engine_is_powered(x,y-1U) || lot_is_powered(x,y-1U)))
        return 1;
    if (y+1U<CITY_HEIGHT &&
        (engine_is_powered(x,y+1U) || lot_is_powered(x,y+1U)))
        return 1;
    return 0;
}
/* Determine whether a zone needs a static no-power badge. The original
 * 3x3 buildings have one ZONE_BIT center; custom 1x1 zoning may be
 * empty or developed and must also warn before an unpowered lot grows.
 * Ordinary roads, rails, terrain and unzoned lots never show badges.
 */
int simulation_needs_power_warning(unsigned int x, unsigned int y)
{
    unsigned short tile;
    unsigned int id;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    tile=city_at(x,y);
    id=tile&TILE_MASK;
    if(!(((tile&ZONE_BIT)!=0U && id>=240U) ||
         ((id==0U || (id>=21U && id<=43U)) &&
          zoning_at(x,y)!=ZONE_NONE))) return 0;
    return !has_power(x,y);
}
unsigned int simulation_power_visual_revision(void)
{
    return power_visual_revision;
}

/* O(1) local tile/neighborhood check, used only by the magnifying glass.
 * It doesn't trigger or wait for the full electricity BFS.
 */
unsigned int simulation_lot_service(unsigned int x, unsigned int y)
{
    unsigned int result = 0U;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0U;
    if (transport_access(x,y))
        result |= SIM_SERVICE_TRANSPORT;
    if (has_power(x,y))
        result |= SIM_SERVICE_POWER;
    return result;
}

static unsigned int zone_developed_population(unsigned int kind,
                                               unsigned int stage)
{
    if (kind == ZONE_RES) return stage * 4U;
    if (kind == ZONE_COM) return stage * 3U;
    if (kind == ZONE_IND) return stage * 4U;
    return 0U;
}

static void survey_original(unsigned short tile)
{
    unsigned int id = tile & TILE_MASK;
    /* Recognize existing authentic 3x3 zone CENTER, not all nine art
     * tiles, so imported cities are not counted nine times.
     */
    if (!(tile & ZONE_BIT))
        return;
    if (id >= 265U && id < 409U) {
        pop_res += 16UL + (unsigned long)(((id-265U)/9U)%4U)*8UL;
    } else if (id >= 436U && id < 612U) {
        pop_com += 1UL + (unsigned long)(((id-436U)/9U)%5U);
    } else if (id >= 625U && id < 693U) {
        pop_ind += 1UL + (unsigned long)(((id-625U)/9U)%4U);
    }
}

/* Only new, individually zoned lots with empty/wooded ground undergo
 * small-lot growth. Existing multi-tile original city buildings stay
 * untouched. A 1x1 lot may grow to density stage 1, 2, or 3.
 */
static int process_zone(unsigned int x, unsigned int y,
                        unsigned int kind, unsigned int id)
{
    unsigned int stage, next, roll;
    int road, power;
    if (kind == ZONE_NONE)
        return 0;
    if (kind == ZONE_RES) ++lots_res;
    else if (kind == ZONE_COM) ++lots_com;
    else if (kind == ZONE_IND) ++lots_ind;

    if (id != 0U && !(id >= 21U && id <= 43U))
        return 0; /* original 3x3 buildings are processed separately */

    stage = zoning_development_at(x,y);
    next = stage;
    roll = random16();
    /* Cache service checks once per tile: the original repeated four
     * neighbor lookups twice, expensive on the 7 MHz V20.
     */
    road = transport_access(x,y);
    power = has_power(x,y);
    if (!road)
        ++no_road_month;
    else if (!power)
        ++no_power_month;
    else
        ++ready_month;
    if (!road || !power) {
        /* Vacancy increases gradually with lost service, not instantly. */
        if (stage != 0U && (roll & 7U) == 0U)
            next = stage - 1U;
    } else if (stage < 3U) {
        int demand = 0;
        if (kind == ZONE_RES && growth_res < 12U)
            demand = 1;
        else if (kind == ZONE_IND && growth_ind < 8U &&
                 pop_res > (pop_ind / 2UL))
            demand = 1;
        else if (kind == ZONE_COM && growth_com < 8U &&
                 pop_res > (pop_com * 2UL + 2UL))
            demand = 1;
        if (demand && ((roll >> 8U) &
                       (stage == 0U ? 3U : 15U)) == 0U) {
            next = stage + 1U;
            if (kind == ZONE_RES) ++growth_res;
            else if (kind == ZONE_COM) ++growth_com;
            else ++growth_ind;
        }
    }
    if (next != stage) {
        (void)zoning_set_development(x,y,next);
        if (next > stage) {
            ++growth_this_month;
            if (kind == ZONE_RES) {
                last_home_x = x;
                last_home_y = y;
                last_home_valid = 1U;
            }
        }
        changed = 1U;
        last_x = x;
        last_y = y;
        last_radius = 0U;
        last_change_valid = 1U;
    }
    /* Newly developed houses can carry electricity to adjoining
     * developed small lots. Never mark an empty zone as a conductor.
     * If a previously powered lot becomes vacant, clear the 1500-byte
     * relay cache (not the expensive original-city electrical BFS).
     */
    if(next!=0U && power)
        set_lot_power(x,y,1);
    else if(stage!=0U && next==0U)
        clear_lot_power();
    if (kind == ZONE_RES)
        pop_res += zone_developed_population(kind,next);
    else if (kind == ZONE_COM)
        pop_com += zone_developed_population(kind,next);
    else
        pop_ind += zone_developed_population(kind,next);
    return next != stage;
}


/* The original sprite ranges contain complete 3x3 buildings at
 * 9-tile increments, with the zone center 4 tiles after the base.
 * Continue growing *existing* residential, commercial and industrial
 * buildings in-place, not just initially empty plots. Validate all
 * nine original sprite IDs before changing anything so edited cities,
 * roads and utilities are never overwritten.
 */
static int original_zone_transport(unsigned int x,unsigned int y)
{
    /* A 3x3 zone footprint extends 1 square from its center; the same
     * three-square walk includes the original frontage and the first
     * additional block beyond it.
     */
    return transport_access(x,y);
}

static int process_original_zone(unsigned int x, unsigned int y,
                                 unsigned short center)
{
    unsigned int id = center & TILE_MASK;
    unsigned int kind, base, delta, tiers, first, next_first, i, j;
    unsigned int *count;
    int road, power;
    if (!(center & ZONE_BIT) || x < 2U || y < 2U ||
        x + 2U >= CITY_WIDTH || y + 2U >= CITY_HEIGHT)
        return 0;

    if (id == 244U || (id >= 265U && id <= 400U &&
                        (id-265U) % 9U == 0U)) {
        kind = ZONE_RES;
        base = 265U;
        tiers = 4U;
        count = &growth_res;
        first = (id == 244U) ? 240U : id - 4U;
    } else if (id == 427U || (id >= 436U && id <= 607U &&
                               (id-436U) % 9U == 0U)) {
        kind = ZONE_COM;
        base = 436U;
        tiers = 5U;
        count = &growth_com;
        first = (id == 427U) ? 423U : id - 4U;
    } else if (id == 616U || (id >= 625U && id <= 688U &&
                               (id-625U) % 9U == 0U)) {
        kind = ZONE_IND;
        base = 625U;
        tiers = 4U;
        count = &growth_ind;
        first = (id == 616U) ? 612U : id - 4U;
    } else {
        return 0;
    }
    if (id == 244U || id == 427U || id == 616U)
        delta = 0U; /* empty original plot */
    else
        delta = ((id - base) / 9U) % tiers;
    if (delta + 1U == tiers)
        return 0; /* full-density original building */
    road = original_zone_transport(x,y);
    power = has_power(x,y);
    if (!road) ++no_road_month;
    else if (!power) ++no_power_month;
    else ++ready_month;
    if (!road || !power || *count >= (kind == ZONE_RES ? 12U : 8U))
        return 0;
    if (kind == ZONE_COM && pop_res <= pop_com * 2UL + 2UL)
        return 0;
    if (kind == ZONE_IND && pop_res <= pop_ind / 2UL)
        return 0;
    if ((random16() >> 8U & 15U) != 0U)
        return 0;
    next_first = (id == 244U || id == 427U || id == 616U) ?
                 base - 4U : first + 9U;

    for (j = 0U; j < 3U; ++j)
        for (i = 0U; i < 3U; ++i) {
            if ((city_at(x+i-1U,y+j-1U) & TILE_MASK) !=
                first + j*3U + i)
                return 0;
        }
    for (j = 0U; j < 3U; ++j)
        for (i = 0U; i < 3U; ++i) {
            unsigned short old_tile = city_at(x+i-1U,y+j-1U);
            unsigned short flags = old_tile & (unsigned short)~TILE_MASK;
            if (i == 1U && j == 1U) flags |= ZONE_BIT;
            city_set(x+i-1U,y+j-1U,
                     (unsigned short)((next_first+j*3U+i) | flags));
        }
    ++(*count);
    ++growth_this_month;
    if (kind == ZONE_RES) {
        last_home_x = x;
        last_home_y = y;
        last_home_valid = 1U;
    }
    changed = 1U;
    last_x = x;
    last_y = y;
    last_radius = 1U;
    last_change_valid = 1U;
    /* Upgrading a 3x3 building only changes its tile IDs. We preserve
     * the original conductive / power flags exactly, so the expensive
     * 120x100 BFS is NOT required after each growth event.
     */
    return 1;
}

static void scan_tile(unsigned int x, unsigned int y)
{
    unsigned short tile = city_at(x,y);
    unsigned int id = tile & TILE_MASK, kind = zoning_at(x,y);
    if (road_tile(id)) ++roads;
    if (wire_tile(id)) ++wires;
    if (id == 750U || id == 816U) ++plants;
    survey_original(tile);
    /* Simulate vacant authentic 3x3 zones in imported original cities. */
    if (tile & ZONE_BIT)
        (void)process_original_zone(x,y,tile);
    if (kind != ZONE_NONE)
        (void)process_zone(x,y,kind,id);
}

static void year_budget(void)
{
    long revenue, maintenance, balance, change, tax;
    tax = (long)city_get_tax_rate();
    /* First playable economy: tax base and infrastructure upkeep.
     * Scaled for 1x1 lot sizes, not the full original budget formula.
     */
    revenue = (long)((pop_res * 4UL + pop_com * 6UL +
                       pop_ind * 5UL) * (unsigned long)tax / 10UL);
    maintenance = (long)roads * 2L + (long)wires +
                  (long)plants * 30L;
    stats.last_income = revenue;
    stats.last_maintenance = maintenance;
    balance = city_get_funds();
    change = revenue - maintenance;
    if (change >= 0L) {
        if (balance > 2147483647L - change)
            balance = 2147483647L;
        else
            balance += change;
    } else {
        if (balance < -change) balance = 0L;
        else balance += change;
    }
    city_set_funds(balance);
}

static void update_date(void)
{
    long time = city_get_time();
    stats.year = (unsigned int)(1900L + time / 48L);
    stats.month = (unsigned int)((time / 4L) % 12L) + 1U;
}

/* A deliberately tiny powered neighborhood in the built-in demo
 * lets users see zoned development immediately on the Tandy, rather
 * than needing to locate a coal plant in an external original city.
 * Never call this when loading somebody's authentic .CTY file.
 */
void simulation_seed_demo(void)
{
    unsigned int x, y, i, j;
    unsigned short flags = 8192U | 16384U;
    for (x=39U; x<=65U; ++x)
        for (y=22U; y<=43U; ++y)
            city_set(x,y,0U);

    /* Original four-by-four coal-plant artwork and center zone flags. */
    for (j=0U; j<4U; ++j)
        for (i=0U; i<4U; ++i) {
            unsigned short tile =
                (unsigned short)(745U + 4U*j+i+flags);
            if (i==1U && j==1U) tile |= 1024U;
            city_set(41U+i,25U+j,tile);
        }
    for (x=45U; x<=49U; ++x)
        city_set(x,26U,210U|flags|4096U);
    for (y=27U; y<=30U; ++y)
        city_set(49U,y,211U|flags|4096U);
    for (x=49U; x<=61U; ++x)
        city_set(x,30U,210U|flags|4096U);
    for (y=31U; y<=40U; ++y)
        city_set(61U,y,211U|flags|4096U);
    for (x=49U; x<=59U; ++x)
        city_set(x,32U,66U|8192U|4096U);
    for (y=32U; y<=40U; ++y)
        city_set(63U,y,67U|8192U|4096U);
    /* RCI zones directly between a conductor and a road. */
    for (x=50U; x<=59U; ++x) {
        unsigned int kind =
            x <= 54U ? ZONE_RES : (x <= 57U ? ZONE_IND : ZONE_COM);
        (void)zoning_set(x,31U,kind);
    }
    for (y=33U; y<=40U; ++y) {
        unsigned int kind = y <= 36U ? ZONE_RES :
                            (y <= 38U ? ZONE_COM : ZONE_IND);
        (void)zoning_set(62U,y,kind);
    }
}

void simulation_init(void)
{
    cursor = 0U;
    scan_x = scan_y = 0U;
    changed = 0U;
    power_dirty = 1U;
    clear_lot_power();
    power_scan_count = 0U;
    last_change_valid = 0U;
    last_home_valid = 0U;
    paused = 0U;
    speed = 1U;
    rng = (unsigned int)(0x31a5U ^ (unsigned int)city_get_time());
    pop_res = pop_com = pop_ind = 0UL;
    lots_res = lots_com = lots_ind = 0U;
    roads = wires = plants = 0U;
    growth_res = growth_com = growth_ind = 0U;
    stats.residents = 0UL;
    stats.commercial_jobs = stats.industrial_jobs = 0UL;
    stats.last_income = stats.last_maintenance = 0L;
    stats.powered_tiles = 0U;
    stats.growth_last_month = stats.growth_total = 0U;
    stats.residential_growth_total = 0U;
    stats.lots_ready = stats.lots_no_road = stats.lots_no_power = 0U;
    growth_this_month = ready_month = no_road_month = no_power_month = 0U;
    stats.residential_lots = stats.commercial_lots = 0U;
    stats.industrial_lots = stats.road_tiles = 0U;
    stats.wire_tiles = stats.plants = 0U;
    update_date();
}
void simulation_mark_power_dirty(void)
{
    power_dirty=1U;
    clear_lot_power();
}
void simulation_mark_lots_dirty(void)
{
    /* Changing/removing 1x1 zone plots is not an electrical network
     * edit. Clear cached lot relays only (1500 bytes), NOT 12K BFS.
     */
    clear_lot_power();
}
const struct sim_statistics *simulation_stats(void) { return &stats; }
void simulation_last_change(unsigned int *x, unsigned int *y)
{
    *x = last_x; *y = last_y;
}
unsigned int simulation_last_change_radius(void) { return last_radius; }
unsigned int simulation_progress(void) { return cursor; }
unsigned int simulation_power_scan_count(void) { return power_scan_count; }
int simulation_has_last_change(void) { return last_change_valid != 0U; }
int simulation_last_home_change(unsigned int *x, unsigned int *y)
{
    if (!last_home_valid || x == NULL || y == NULL)
        return 0;
    *x = last_home_x;
    *y = last_home_y;
    return 1;
}
int simulation_is_paused(void) { return paused != 0U; }
void simulation_set_paused(int pause) { paused = pause ? 1U : 0U; }
unsigned int simulation_speed(void) { return speed; }
void simulation_set_speed(unsigned int next)
{
    if (next >= 1U && next <= 4U) speed = next;
}

unsigned int simulation_advance(unsigned int max_tiles)
{
    unsigned int steps = 0U, event = 0U, x, y;
    long next_time;
    if (paused)
        return 0U;
    if (max_tiles > SIM_CHUNK_LIMIT)
        max_tiles = SIM_CHUNK_LIMIT;
    if (power_dirty) {
        stats.powered_tiles = engine_power_scan();
        ++power_scan_count;
        power_dirty = 0U;
        ++power_visual_revision; /* electrical graph just changed */
    }
    changed = 0U;
    while (steps < max_tiles && cursor < CITY_TILES) {
        x = scan_x;
        y = scan_y;
        scan_tile(x,y);
        ++cursor;
        ++steps;
        if (++scan_y == CITY_HEIGHT) {
            scan_y = 0U;
            ++scan_x;
        }
        if (changed) {
            event |= SIM_EVENT_TILE;
            break; /* caller can redraw one changed square cheaply */
        }
    }
    if (cursor == CITY_TILES) {
        stats.residents = pop_res;
        stats.commercial_jobs = pop_com;
        stats.industrial_jobs = pop_ind;
        stats.residential_lots = lots_res;
        stats.commercial_lots = lots_com;
        stats.industrial_lots = lots_ind;
        stats.road_tiles = roads;
        stats.wire_tiles = wires;
        stats.plants = plants;
        stats.growth_last_month = growth_this_month;
        stats.growth_total += growth_this_month;
        stats.residential_growth_total += growth_res;
        stats.lots_ready = ready_month;
        stats.lots_no_road = no_road_month;
        stats.lots_no_power = no_power_month;
        next_time = city_get_time();
        if (next_time <= 2147483599L)
            city_set_time(next_time + 4L);
        update_date();
        if (city_get_time() % 48L == 0L)
            year_budget();
        cursor = 0U;
        scan_x = scan_y = 0U;
        pop_res = pop_com = pop_ind = 0UL;
        lots_res = lots_com = lots_ind = 0U;
        roads = wires = plants = 0U;
        growth_res = growth_com = growth_ind = 0U;
        growth_this_month = ready_month = 0U;
        no_road_month = no_power_month = 0U;
        event |= SIM_EVENT_MONTH;
    }
    return event;
}
