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

#define TILE_MASK 1023U
#define ZONE_BIT 1024U
#define PWR_BIT 32768U
#define SIM_CHUNK_LIMIT 320U

static struct sim_statistics stats;
static unsigned int cursor = 0U;
static unsigned int last_x, last_y;
static unsigned int changed = 0U;
static unsigned int power_dirty = 1U;
static unsigned int paused = 0U;
static unsigned int speed = 1U;
static unsigned int growth_res, growth_com, growth_ind;
static unsigned int rng = 0x31a5U;
static unsigned long pop_res, pop_com, pop_ind;
static unsigned int lots_res, lots_com, lots_ind;
static unsigned int roads, wires, plants;

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
static int wire_tile(unsigned int id)
{
    return id >= 208U && id <= 223U;
}
static int road_access(unsigned int x, unsigned int y)
{
    if (x > 0U && road_tile(city_at(x-1U,y) & TILE_MASK)) return 1;
    if (x+1U < CITY_WIDTH && road_tile(city_at(x+1U,y) & TILE_MASK)) return 1;
    if (y > 0U && road_tile(city_at(x,y-1U) & TILE_MASK)) return 1;
    if (y+1U < CITY_HEIGHT && road_tile(city_at(x,y+1U) & TILE_MASK)) return 1;
    return 0;
}
static int has_power(unsigned int x, unsigned int y)
{
    if (engine_is_powered(x,y)) return 1;
    if (x > 0U && engine_is_powered(x-1U,y)) return 1;
    if (x+1U < CITY_WIDTH && engine_is_powered(x+1U,y)) return 1;
    if (y > 0U && engine_is_powered(x,y-1U)) return 1;
    if (y+1U < CITY_HEIGHT && engine_is_powered(x,y+1U)) return 1;
    return 0;
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
    if (kind == ZONE_NONE)
        return 0;
    if (kind == ZONE_RES) ++lots_res;
    else if (kind == ZONE_COM) ++lots_com;
    else if (kind == ZONE_IND) ++lots_ind;

    if (id != 0U && !(id >= 21U && id <= 43U))
        return 0; /* authentic buildings are managed by original logic later */

    stage = zoning_development_at(x,y);
    next = stage;
    roll = random16();
    if (!road_access(x,y) || !has_power(x,y)) {
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
        if (demand && (roll & (stage == 0U ? 7U : 31U)) == 0U) {
            next = stage + 1U;
            if (kind == ZONE_RES) ++growth_res;
            else if (kind == ZONE_COM) ++growth_com;
            else ++growth_ind;
        }
    }
    if (next != stage) {
        (void)zoning_set_development(x,y,next);
        changed = 1U;
        last_x = x;
        last_y = y;
    }
    if (kind == ZONE_RES)
        pop_res += zone_developed_population(kind,next);
    else if (kind == ZONE_COM)
        pop_com += zone_developed_population(kind,next);
    else
        pop_ind += zone_developed_population(kind,next);
    return next != stage;
}

static void scan_tile(unsigned int x, unsigned int y)
{
    unsigned short tile = city_at(x,y);
    unsigned int id = tile & TILE_MASK, kind = zoning_at(x,y);
    if (road_tile(id)) ++roads;
    if (wire_tile(id)) ++wires;
    if (id == 750U || id == 816U) ++plants;
    survey_original(tile);
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

void simulation_init(void)
{
    cursor = 0U;
    changed = 0U;
    power_dirty = 1U;
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
    stats.residential_lots = stats.commercial_lots = 0U;
    stats.industrial_lots = stats.road_tiles = 0U;
    stats.wire_tiles = stats.plants = 0U;
    update_date();
}
void simulation_mark_power_dirty(void) { power_dirty = 1U; }
const struct sim_statistics *simulation_stats(void) { return &stats; }
void simulation_last_change(unsigned int *x, unsigned int *y)
{
    *x = last_x; *y = last_y;
}
unsigned int simulation_progress(void) { return cursor; }
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
        power_dirty = 0U;
    }
    changed = 0U;
    while (steps < max_tiles && cursor < CITY_TILES) {
        x = cursor / CITY_HEIGHT;
        y = cursor % CITY_HEIGHT;
        scan_tile(x,y);
        ++cursor;
        ++steps;
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
        next_time = city_get_time();
        if (next_time <= 2147483599L)
            city_set_time(next_time + 4L);
        update_date();
        if (city_get_time() % 48L == 0L)
            year_budget();
        cursor = 0U;
        pop_res = pop_com = pop_ind = 0UL;
        lots_res = lots_com = lots_ind = 0U;
        roads = wires = plants = 0U;
        growth_res = growth_com = growth_ind = 0U;
        event |= SIM_EVENT_MONTH;
    }
    return event;
}
