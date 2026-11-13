/* SPDX-License-Identifier: GPL-3.0-or-later
 * Experimental per-tile zoning editor and .ZON sidecars.
 * Original Micropolis © 1989-2007 Electronic Arts Inc.; GPLv3-or-later,
 * original repository COPYING additional terms apply.
 *
 * Crucially, .CTY city maps retain their original tile data, tile IDs,
 * roads, original sprite layouts and 3x3 buildings. This separate layer
 * permits irregular 1x1 zones without inventing a new Micropolis format.
 *
 * The first DOS-friendly phased simulator develops these 1x1 zones
 * using density levels; imported original city tiles remain intact.
 */
#include "city.h"
#include "zoning.h"
#include <stdio.h>

#define ZONING_LENGTH (CITY_WIDTH * CITY_HEIGHT)
#define ZONING_BYTES ((ZONING_LENGTH + 3U) / 4U)
#define ZONING_V2_SIZE (8L + ZONING_BYTES + 2L)
#define ZONING_V3_SIZE (8L + 2L * ZONING_BYTES + 2L)
#define TILE_MASK 1023U
#define ZONE_BIT  1024U

/* Four independent 2-bit zone values per byte: only 3,000 bytes for
 * 12,000 squares. This avoids overflowing the 16-bit Watcom DGROUP.
 * Column-major square IDs still match the classic Micropolis world.
 */
static unsigned char zone_grid[ZONING_BYTES];
/* Separate compact 2-bit development density: 0=empty, 1..3=occupied.
 * Other original .CTY sprites remain unchanged. */
static unsigned char density_grid[ZONING_BYTES];
static const unsigned char magic[8] = {
    'T', 'Z', 'O', 'N', 3, CITY_WIDTH, CITY_HEIGHT, 0
};

static unsigned int packed_get(const unsigned char *grid, unsigned int index)
{
    return (grid[index >> 2] >> ((index & 3U) * 2U)) & 3U;
}
static unsigned int zone_get(unsigned int index)
{
    return packed_get(zone_grid, index);
}
static void packed_put(unsigned char *grid, unsigned int index,
                       unsigned int value)
{
    unsigned int shift = (index & 3U) * 2U;
    unsigned int mask = 3U << shift;
    grid[index >> 2] =
        (unsigned char)((grid[index >> 2] & ~mask) |
                        ((value & 3U) << shift));
}

static void zone_put(unsigned int index, unsigned int value)
{
    packed_put(zone_grid, index, value);
}

/* A cheap rolling 16-bit digest for accidental file corruption.
 * Not cryptographic. Performed only on save/load, never while painting.
 */
static unsigned int hash_step(unsigned int hash, unsigned int value)
{
    return (((hash << 5) | (hash >> 11)) ^ value) & 65535U;
}
static unsigned int zoning_checksum(unsigned int version)
{
    unsigned int i, hash = 0x4d37U;
    for (i = 0U; i < ZONING_BYTES; ++i)
        hash = hash_step(hash, zone_grid[i]);
    if (version >= 3U)
        for (i = 0U; i < ZONING_BYTES; ++i)
            hash = hash_step(hash, density_grid[i]);
    return hash;
}

void zoning_clear(void)
{
    unsigned int i;
    for (i = 0U; i < ZONING_BYTES; ++i) {
        zone_grid[i] = 0U;
        density_grid[i] = 0U;
    }
}

unsigned int zoning_at(unsigned int x, unsigned int y)
{
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return ZONE_NONE;
    return zone_get(x * CITY_HEIGHT + y);
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
                        zone_put(xx * CITY_HEIGHT + yy, kind);
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
    if (zone_get(offset) == kind)
        return 0;
    zone_put(offset, kind);
    packed_put(density_grid, offset, 0U); /* changing zone clears old growth */
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

unsigned int zoning_development_at(unsigned int x, unsigned int y)
{
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0U;
    return packed_get(density_grid, x * CITY_HEIGHT + y);
}

int zoning_set_development(unsigned int x, unsigned int y,
                           unsigned int level)
{
    unsigned int index;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT ||
        level > 3U || zoning_at(x, y) == ZONE_NONE)
        return 0;
    index = x * CITY_HEIGHT + y;
    if (packed_get(density_grid, index) == level)
        return 0;
    packed_put(density_grid, index, level);
    return 1;
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
        if (zone_get(i) == kind)
            ++count;
    return count;
}

/* V3: 8-byte magic, 3KB zones + 3KB density, 16-bit checksum.
 * Backward-read V2 zoning files (with zeroed density). Validate checksum
 * and both file lengths before touching the live maps.
 */
int zoning_save(const char *path)
{
    FILE *f;
    int okay;
    unsigned int hash = zoning_checksum(3U);
    f = fopen(path, "wb");
    if (f == NULL)
        return 0;
    okay = fwrite(magic, 1U, sizeof(magic), f) == sizeof(magic) &&
           fwrite(zone_grid, 1U, ZONING_BYTES, f) == ZONING_BYTES &&
           fwrite(density_grid, 1U, ZONING_BYTES, f) == ZONING_BYTES &&
           fputc(hash & 255U, f) != EOF &&
           fputc(hash >> 8, f) != EOF;
    if (fclose(f) != 0)
        okay = 0;
    return okay;
}

int zoning_load(const char *path)
{
    FILE *f;
    long length;
    unsigned char header[8];
    unsigned int i, hash = 0x4d37U, expected, version;
    int value, low, high;
    f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    length = ftell(f);
    if ((length != ZONING_V2_SIZE && length != ZONING_V3_SIZE) ||
        fseek(f, 0L, SEEK_SET) != 0 ||
        fread(header, 1U, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return 0;
    }
    version = header[4];
    if ((version != 2U && version != 3U) ||
        length != (version == 2U ? ZONING_V2_SIZE : ZONING_V3_SIZE)) {
        fclose(f);
        return 0;
    }
    for (i = 0U; i < sizeof(header); ++i) {
        if (i == 4U) continue;
        if (header[i] != magic[i]) {
            fclose(f);
            return 0;
        }
    }
    for (i = 0U; i < (version == 2U ? ZONING_BYTES : 2U * ZONING_BYTES); ++i) {
        value = fgetc(f);
        if (value == EOF) {
            fclose(f);
            return 0;
        }
        hash = hash_step(hash, (unsigned int)value);
    }
    low = fgetc(f);
    high = fgetc(f);
    if (low == EOF || high == EOF) {
        fclose(f);
        return 0;
    }
    expected = (unsigned int)low | ((unsigned int)high << 8);
    if (hash != expected ||
        fseek(f, sizeof(header), SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    if (fread(zone_grid, 1U, ZONING_BYTES, f) != ZONING_BYTES) {
        fclose(f);
        return 0;
    }
    if (version == 3U) {
        if (fread(density_grid, 1U, ZONING_BYTES, f) != ZONING_BYTES) {
            fclose(f);
            return 0;
        }
    } else {
        for (i = 0U; i < ZONING_BYTES; ++i)
            density_grid[i] = 0U;
    }
    fclose(f);
    return 1;
}
