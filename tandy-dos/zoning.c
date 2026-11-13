/* SPDX-License-Identifier: GPL-3.0-or-later
 * Experimental per-tile zoning editor and .ZON sidecars.
 * Original Micropolis © 1989-2007 Electronic Arts Inc.; GPLv3-or-later,
 * original repository COPYING additional terms apply.
 *
 * Crucially, .CTY city maps retain their original tile data, tile IDs,
 * roads, original sprite layouts and 3x3 buildings. This separate layer
 * permits irregular 1x1 zones without inventing a new Micropolis format.
 *
 * Simulated growth inside these new zones is NOT YET IMPLEMENTED.
 */
#include "city.h"
#include "zoning.h"
#include <stdio.h>

#define ZONING_LENGTH (CITY_WIDTH * CITY_HEIGHT)
#define ZONING_SIZE (8L + ZONING_LENGTH)
#define TILE_MASK 1023U
#define ZONE_BIT  1024U

/* Column-major, same layout as the historical Micropolis .CTY tile map.
 * Static memory rather than heap/recursion: exactly 12,000 bytes.
 */
static unsigned char zone_grid[ZONING_LENGTH];
static const unsigned char magic[8] = {
    'T', 'Z', 'O', 'N', 1, CITY_WIDTH, CITY_HEIGHT, 0
};

void zoning_clear(void)
{
    unsigned int i;
    for (i = 0U; i < ZONING_LENGTH; ++i)
        zone_grid[i] = ZONE_NONE;
}

unsigned int zoning_at(unsigned int x, unsigned int y)
{
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return ZONE_NONE;
    return zone_grid[x * CITY_HEIGHT + y];
}

/* Preserve zoning context in imported original cities, without touching
 * buildings, even though the original map has 3x3 RCI zone centers.
 */
void zoning_seed_from_city(void)
{
    unsigned int x, y, xx, yy, id, kind, t;
    zoning_clear();
    for (x = 1U; x + 1U < CITY_WIDTH; ++x) {
        for (y = 1U; y + 1U < CITY_HEIGHT; ++y) {
            t = city_at(x, y);
            if (!(t & ZONE_BIT))
                continue;
            id = t & TILE_MASK;
            kind = ZONE_NONE;
            if (id >= 240U && id < 409U) kind = ZONE_RES;
            else if (id >= 423U && id < 612U) kind = ZONE_COM;
            else if (id >= 612U && id <= 692U) kind = ZONE_IND;
            if (kind == ZONE_NONE)
                continue;
            for (xx = x - 1U; xx <= x + 1U; ++xx)
                for (yy = y - 1U; yy <= y + 1U; ++yy) {
                    t = city_at(xx, yy) & TILE_MASK;
                    if ((kind == ZONE_RES && t >= 240U && t < 409U) ||
                        (kind == ZONE_COM && t >= 423U && t < 612U) ||
                        (kind == ZONE_IND && t >= 612U && t <= 692U))
                        zone_grid[xx * CITY_HEIGHT + yy] =
                            (unsigned char)kind;
                }
        }
    }
}

int zoning_can_set(unsigned int x, unsigned int y, unsigned int kind)
{
    unsigned int id;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT || kind > ZONE_IND)
        return 0;
    if (kind == ZONE_NONE)
        return 1;

    id = city_at(x, y) & TILE_MASK;
    /* Never paint water, infrastructure, special buildings, debris or fire.
     * Existing developed zone sprites can receive the matching zone type,
     * without overwriting any source .CTY map tiles.
     */
    if (id == 0U || (id >= 21U && id <= 43U))
        return 1;
    if (kind == ZONE_RES && id >= 240U && id < 409U)
        return 1;
    if (kind == ZONE_COM && id >= 423U && id < 612U)
        return 1;
    if (kind == ZONE_IND && id >= 612U && id <= 692U)
        return 1;
    return 0;
}

/* Returns one only for a changed tile; zero if blocked or already set. */
int zoning_set(unsigned int x, unsigned int y, unsigned int kind)
{
    unsigned int offset;
    if (!zoning_can_set(x, y, kind))
        return 0;
    offset = x * CITY_HEIGHT + y;
    if (zone_grid[offset] == kind)
        return 0;
    zone_grid[offset] = (unsigned char)kind;
    return 1;
}

/* Integer Bresenham. If the COM mouse skips several tiles on the V20,
 * the road and zone strokes will remain continuous.
 * Clamp the longest stroke to the 120x100 map bounds.
 */
unsigned int zoning_paint_line(unsigned int x0, unsigned int y0,
                               unsigned int x1, unsigned int y1,
                               unsigned int kind)
{
    int dx, dy, sx, sy, error, twice, xx, yy;
    unsigned int count = 0U, steps = 0U;
    if (x0 >= CITY_WIDTH || x1 >= CITY_WIDTH ||
        y0 >= CITY_HEIGHT || y1 >= CITY_HEIGHT)
        return 0U;
    xx = (int)x0;
    yy = (int)y0;
    dx = (int)x1 - (int)x0;
    dy = (int)y1 - (int)y0;
    sx = dx >= 0 ? 1 : -1;
    sy = dy >= 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    error = dx - dy;
    for (;;) {
        if (zoning_set((unsigned int)xx, (unsigned int)yy, kind))
            ++count;
        if (xx == (int)x1 && yy == (int)y1)
            break;
        twice = error * 2;
        if (twice > -dy) {
            error -= dy;
            xx += sx;
        }
        if (twice < dx) {
            error += dx;
            yy += sy;
        }
        if (++steps > CITY_WIDTH + CITY_HEIGHT)
            break;
    }
    return count;
}

unsigned int zoning_edge_mask(unsigned int x, unsigned int y)
{
    unsigned int kind = zoning_at(x, y), mask = 0U;
    if (!kind || x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0U;
    if (y == 0U || zoning_at(x, y - 1U) != kind) mask |= 1U;
    if (x + 1U >= CITY_WIDTH || zoning_at(x + 1U, y) != kind) mask |= 2U;
    if (y + 1U >= CITY_HEIGHT || zoning_at(x, y + 1U) != kind) mask |= 4U;
    if (x == 0U || zoning_at(x - 1U, y) != kind) mask |= 8U;
    return mask;
}

unsigned int zoning_count(unsigned int kind)
{
    unsigned int i, count = 0U;
    if (kind == ZONE_NONE || kind > ZONE_IND)
        return 0U;
    for (i = 0U; i < ZONING_LENGTH; ++i)
        if (zone_grid[i] == kind)
            ++count;
    return count;
}

/* TZON v1: 8-byte signature/dimensions, 12,000 bytes of 0..3 values.
 * Validated fully BEFORE overwriting the current zone state on load.
 * A plain file is easier to inspect/recover on DOS than a compressed format.
 */
int zoning_save(const char *path)
{
    FILE *f;
    int okay;
    f = fopen(path, "wb");
    if (f == NULL)
        return 0;
    okay = fwrite(magic, 1U, sizeof(magic), f) == sizeof(magic) &&
           fwrite(zone_grid, 1U, ZONING_LENGTH, f) == ZONING_LENGTH;
    if (fclose(f) != 0)
        okay = 0;
    return okay;
}

int zoning_load(const char *path)
{
    FILE *f;
    long length;
    unsigned char header[8];
    unsigned int i;
    int value;
    f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    length = ftell(f);
    if (length != ZONING_SIZE || fseek(f, 0L, SEEK_SET) != 0 ||
        fread(header, 1U, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return 0;
    }
    for (i = 0U; i < sizeof(header); ++i) {
        if (header[i] != magic[i]) {
            fclose(f);
            return 0;
        }
    }
    for (i = 0U; i < ZONING_LENGTH; ++i) {
        value = fgetc(f);
        if (value < 0 || value > (int)ZONE_IND) {
            fclose(f);
            return 0;
        }
    }
    if (fseek(f, sizeof(header), SEEK_SET) != 0 ||
        fread(zone_grid, 1U, ZONING_LENGTH, f) != ZONING_LENGTH) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}
