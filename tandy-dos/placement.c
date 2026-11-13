/* SPDX-License-Identifier: GPL-3.0-or-later
 * Preview, price cap and commit stage for roads, wires and RCI zones.
 * Each cell is a 2-bit preview state; full 120x100 map needs only 3KB.
 * No city or zone state is modified until placement_commit().
 */
#include "placement.h"
#include "city.h"
#include "zoning.h"

#define PLAN_BYTES ((CITY_TILES + 3U) / 4U)
#define TILE_ID 1023U

static unsigned char plan[PLAN_BYTES];
static enum edit_tool current_tool;
static int active = 0;
static unsigned int accepted = 0U, rejected = 0U;
static unsigned int min_x, min_y, max_x, max_y;
static long committed_cost = 0L;
static long limit_funds = 0L;

static unsigned int state_get(unsigned int i)
{
    return (plan[i >> 2] >> ((i & 3U) * 2U)) & 3U;
}
static void state_put(unsigned int i, unsigned int value)
{
    unsigned int shift = (i & 3U) * 2U;
    unsigned int mask = 3U << shift;
    plan[i >> 2] = (unsigned char)((plan[i >> 2] & ~mask) |
                                   ((value & 3U) << shift));
}
static unsigned int zone_type(enum edit_tool tool)
{
    if (tool == EDIT_RESIDENTIAL) return ZONE_RES;
    if (tool == EDIT_COMMERCIAL) return ZONE_COM;
    if (tool == EDIT_INDUSTRIAL) return ZONE_IND;
    return ZONE_NONE;
}

long placement_unit_cost(enum edit_tool tool)
{
    switch (tool) {
    case EDIT_ROAD: return 10L;
    case EDIT_POWER: return 5L;
    /* Original Micropolis RCI tools are $100 per 3x3 plot.
     * This per-square brush prorates those costs: ceil(100/9)=12.
     */
    case EDIT_RESIDENTIAL:
    case EDIT_COMMERCIAL:
    case EDIT_INDUSTRIAL: return 12L;
    default: return 0L;
    }
}

static int can_place(unsigned int x, unsigned int y)
{
    unsigned int kind = zone_type(current_tool);
    unsigned int tile = city_at(x, y) & TILE_ID;
    if (kind) {
        if (zoning_at(x, y) == kind)
            return 0; /* already zoned, never charge again */
        return zoning_can_set(x, y, kind);
    }
    if (zoning_at(x, y) != ZONE_NONE)
        return 0; /* infrastructure must not silently delete zoning */
    return tile == 0U || (tile >= 21U && tile <= 43U);
}

int placement_active(void) { return active; }
unsigned int placement_at(unsigned int x, unsigned int y)
{
    if (!active || x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return PREVIEW_NONE;
    return state_get(x * CITY_HEIGHT + y);
}
unsigned int placement_count(void) { return accepted; }
unsigned int placement_rejected(void) { return rejected; }
long placement_cost(void) { return committed_cost; }
long placement_remaining(void)
{
    return limit_funds > committed_cost ? limit_funds - committed_cost : 0L;
}

void placement_bounds(unsigned int *x0, unsigned int *y0,
                      unsigned int *x1, unsigned int *y1)
{
    *x0 = min_x;
    *y0 = min_y;
    *x1 = max_x;
    *y1 = max_y;
}

void placement_cancel(void)
{
    active = 0;
    accepted = rejected = 0U;
    committed_cost = 0L;
}

void placement_begin(enum edit_tool tool)
{
    unsigned int i;
    current_tool = tool;
    limit_funds = city_get_funds();
    accepted = rejected = 0U;
    committed_cost = 0L;
    min_x = CITY_WIDTH - 1U;
    min_y = CITY_HEIGHT - 1U;
    max_x = 0U;
    max_y = 0U;
    for (i = 0U; i < PLAN_BYTES; ++i)
        plan[i] = 0U;
    active = placement_unit_cost(tool) > 0L;
}

static unsigned int preview_one(unsigned int x, unsigned int y)
{
    unsigned int offset, state;
    long price = placement_unit_cost(current_tool);
    if (!active || x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0U;
    offset = x * CITY_HEIGHT + y;
    if (state_get(offset) != PREVIEW_NONE)
        return 0U;
    if (x < min_x) min_x = x;
    if (x > max_x) max_x = x;
    if (y < min_y) min_y = y;
    if (y > max_y) max_y = y;
    if (!can_place(x, y)) {
        state = PREVIEW_BLOCKED;
    } else if (price > limit_funds - committed_cost) {
        state = PREVIEW_NO_FUNDS;
    } else {
        state = PREVIEW_FUNDED;
        committed_cost += price;
        ++accepted;
    }
    if (state != PREVIEW_FUNDED)
        ++rejected;
    state_put(offset, state);
    return 1U;
}

/* Bresenham filling prevents mouse polling gaps on slow V20 hardware. */
unsigned int placement_extend(unsigned int x0, unsigned int y0,
                              unsigned int x1, unsigned int y1)
{
    int dx, dy, sx, sy, err, twice, x, y;
    unsigned int changes = 0U, steps = 0U;
    if (!active || x0 >= CITY_WIDTH || x1 >= CITY_WIDTH ||
        y0 >= CITY_HEIGHT || y1 >= CITY_HEIGHT)
        return 0U;
    x = (int)x0;
    y = (int)y0;
    dx = (int)x1 - (int)x0;
    dy = (int)y1 - (int)y0;
    sx = dx >= 0 ? 1 : -1;
    sy = dy >= 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;
    for (;;) {
        changes += preview_one((unsigned int)x, (unsigned int)y);
        if (x == (int)x1 && y == (int)y1)
            break;
        twice = 2 * err;
        if (twice > -dy) { err -= dy; x += sx; }
        if (twice < dx) { err += dx; y += sy; }
        if (++steps > CITY_WIDTH + CITY_HEIGHT)
            break;
    }
    return changes;
}

/* The map must be unchanged until commit; emit all funded cells in one
 * transaction, then debit only actual successfully applied cells.
 */
unsigned int placement_commit(void)
{
    unsigned int x, y, done = 0U, kind = zone_type(current_tool);
    long spent = 0L, unit = placement_unit_cost(current_tool);
    if (!active)
        return 0U;
    for (x = min_x; x <= max_x; ++x)
        for (y = min_y; y <= max_y; ++y) {
            if (state_get(x * CITY_HEIGHT + y) != PREVIEW_FUNDED)
                continue;
            if (kind) {
                if (zoning_set(x, y, kind)) {
                    ++done;
                    spent += unit;
                }
            } else if (edit_place(current_tool, x, y)) {
                ++done;
                spent += unit;
            }
        }
    if (spent != 0L)
        (void)city_spend_funds(spent);
    placement_cancel();
    return done;
}
