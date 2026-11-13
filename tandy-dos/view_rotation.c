/* SPDX-License-Identifier: GPL-3.0-or-later
 * Integer-only, C89, tiny 8086-friendly coordinate transforms.
 * No bitmap rotations, buffering, map mutations or expensive redraws.
 */
#include "view_rotation.h"
#include "city.h"

unsigned int view_rotation_width(unsigned int turns)
{
    return (turns & 1U) ? CITY_HEIGHT : CITY_WIDTH;
}
unsigned int view_rotation_height(unsigned int turns)
{
    return (turns & 1U) ? CITY_WIDTH : CITY_HEIGHT;
}

void view_rotation_to_world(unsigned int turns, unsigned int vx,
                            unsigned int vy, unsigned int *wx,
                            unsigned int *wy)
{
    switch (turns & 3U) {
    case 1U: /* Clockwise: display x = H-1-world y */
        *wx = vy;
        *wy = CITY_HEIGHT - 1U - vx;
        break;
    case 2U:
        *wx = CITY_WIDTH - 1U - vx;
        *wy = CITY_HEIGHT - 1U - vy;
        break;
    case 3U:
        *wx = CITY_WIDTH - 1U - vy;
        *wy = vx;
        break;
    default:
        *wx = vx;
        *wy = vy;
        break;
    }
}

void view_rotation_from_world(unsigned int turns, unsigned int wx,
                              unsigned int wy, unsigned int *vx,
                              unsigned int *vy)
{
    switch (turns & 3U) {
    case 1U:
        *vx = CITY_HEIGHT - 1U - wy;
        *vy = wx;
        break;
    case 2U:
        *vx = CITY_WIDTH - 1U - wx;
        *vy = CITY_HEIGHT - 1U - wy;
        break;
    case 3U:
        *vx = wy;
        *vy = CITY_WIDTH - 1U - wx;
        break;
    default:
        *vx = wx;
        *vy = wy;
        break;
    }
}

void view_rotation_clamp(unsigned int turns, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top)
{
    unsigned int map_w = view_rotation_width(turns);
    unsigned int map_h = view_rotation_height(turns);
    unsigned int max_x = map_w > width ? map_w - width : 0U;
    unsigned int max_y = map_h > height ? map_h - height : 0U;
    if (*left > max_x) *left = max_x;
    if (*top > max_y) *top = max_y;
}

void view_rotation_focus(unsigned int turns, unsigned int wx,
                         unsigned int wy, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top, unsigned int *cursor_x,
                         unsigned int *cursor_y)
{
    unsigned int vx,vy;
    view_rotation_from_world(turns,wx,wy,&vx,&vy);
    *left = vx > width/2U ? vx-width/2U : 0U;
    *top = vy > height/2U ? vy-height/2U : 0U;
    view_rotation_clamp(turns,width,height,left,top);
    *cursor_x = vx-*left;
    *cursor_y = vy-*top;
}

unsigned int view_rotation_edges(unsigned int turns, unsigned int edges)
{
    unsigned int i;
    for (i=0U; i<(turns & 3U); ++i)
        edges = ((edges << 1U) | (edges >> 3U)) & 15U;
    return edges;
}

/* A multi-tile building's pieces occupy a square 3x3/4x4 grid.
 * Rotate only their POSITIONS; choose the original source piece at
 * the new screen-relative grid position so the composite stays upright.
 */
unsigned int view_rotation_upright_part(unsigned int turns,
                                         unsigned int span,
                                         unsigned int part)
{
    unsigned int x = part % span;
    unsigned int y = part / span;
    unsigned int sx,sy;
    switch (turns & 3U) {
    case 1U: sx = span - 1U - y; sy = x; break;
    case 2U: sx = span - 1U - x; sy = span - 1U - y; break;
    case 3U: sx = y; sy = span - 1U - x; break;
    default: sx = x; sy = y; break;
    }
    return sy * span + sx;
}

/* The original Micropolis tile definitions use distinct families:
 *   64..207  roads, bridges and traffic variants
 *   208..222 power conductors and road/rail-power crossings
 *   224..238 railway, bridges and road/rail crossings
 *
 * The first implementation incorrectly stopped at tile 223. Roads
 * rotated but railway tiles did not, producing mismatched roads/tracks
 * around intersections. Only map graphics are substituted, never the
 * saved world IDs or network connection bits.
 *
 * Source: original micropolis-activity/src/sim/headers/sim.h and
 * w_con.c (_RoadTable/_WireTable/_RailTable).
 */
static unsigned int rotate_connection_once(unsigned int id)
{
    static const unsigned char cw[16] = {
        1U,0U,3U,2U,5U,6U,7U,4U,
        9U,10U,11U,8U,12U,14U,13U,15U
    };
    unsigned int family;

    /* Animated bridge variants use separate graphics across the
     * traffic range. In particular, BRWH (79) and BRWV (95)
     * are a horizontal/vertical pair, NOT low-nibble siblings.
     */
    if (id == 79U) return 95U;
    if (id == 95U) return 79U;

    if (id >= 64U && id <= 207U) {
        /* Preserve other specialized 0x?F bridge animation frames
         * until their directional art has been individually verified.
         * Do not accidentally turn an animation into another asset.
         */
        if ((id & 15U) == 15U) return id;
        family = id & ~15U;
    } else if (id >= 208U && id <= 222U) {
        family = 208U;
    } else if (id >= 224U && id <= 238U) {
        family = 224U;
    } else {
        /* 223 unused, 239 legacy crossing, and all ordinary city
         * sprites do not have validated directional substitutes.
         */
        return id;
    }
    return family + cw[id & 15U];
}

unsigned int view_rotation_utility_tile(unsigned int turns,
                                         unsigned int graphic_id)
{
    unsigned int i;
    for (i = 0U; i < (turns & 3U); ++i)
        graphic_id = rotate_connection_once(graphic_id);
    return graphic_id;
}
