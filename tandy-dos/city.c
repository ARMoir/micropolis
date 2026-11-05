/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native DOS city-map reader, not a simulation engine.
 * Original Micropolis map layout: see micropolis-activity/src/sim/s_fileio.c
 * Copyright (C) 1989-2007 Electronic Arts Inc. for the original game.
 */
#include "city.h"
#include <stdio.h>

/* 120x100 16-bit, column-major map, comfortably under a 64K DOS data segment. */
unsigned short city_tiles[CITY_TILES];
/* Preserve each original file's 3120 history and settings bytes when editing. */
static unsigned char city_header[3120];


#define CITY_FILE_LENGTH 27120L
#define CITY_MAP_OFFSET   3120L
#define TILE_ID_MASK      1023U

unsigned short city_at(unsigned int x, unsigned int y)
{
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    return city_tiles[x * CITY_HEIGHT + y];
}

int city_load(const char *path)
{
    FILE *f;
    long size;
    unsigned int x, y;
    int hi, lo;

    f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    if (size != CITY_FILE_LENGTH || fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    if (fread(city_header, 1U, sizeof(city_header), f) != sizeof(city_header)) {
        fclose(f);
        return 0;
    }

    /* s_fileio.c saves 3,120 bytes of histories and a column-major map.
     * Map entries are big-endian 16-bit values. Preserve status flags.
     */
    for (x = 0; x < CITY_WIDTH; ++x) {
        for (y = 0; y < CITY_HEIGHT; ++y) {
            hi = fgetc(f);
            lo = fgetc(f);
            if (hi == EOF || lo == EOF) {
                fclose(f);
                return 0;
            }
            city_tiles[x * CITY_HEIGHT + y] =
                (unsigned short)(((unsigned int)hi << 8) | (unsigned int)lo);
        }
    }

    fclose(f);
    return 1;
}

void city_set(unsigned int x, unsigned int y, unsigned short tile)
{
    if (x < CITY_WIDTH && y < CITY_HEIGHT)
        city_tiles[x * CITY_HEIGHT + y] = tile;
}

/* Export a separate editor copy; the application never writes its input path. */
int city_save_as(const char *path)
{
    FILE *f;
    unsigned int x, y;
    unsigned short tile;
    int okay = 1;

    f = fopen(path, "wb");
    if (f == NULL)
        return 0;
    if (fwrite(city_header, 1U, sizeof(city_header), f) != sizeof(city_header))
        okay = 0;
    for (x = 0; okay && x < CITY_WIDTH; ++x) {
        for (y = 0; y < CITY_HEIGHT; ++y) {
            tile = city_tiles[x * CITY_HEIGHT + y];
            if (fputc((tile >> 8) & 255U, f) == EOF ||
                fputc(tile & 255U, f) == EOF) {
                okay = 0;
                break;
            }
        }
    }
    if (fclose(f) != 0)
        okay = 0;
    return okay;
}

void city_make_demo(void)
{
    unsigned int n;
    for (n = 0; n < sizeof(city_header); ++n)
        city_header[n] = 0;

    unsigned int x, y;
    /* The demo uses real Micropolis tile-ID ranges for the render test.
     * Nothing here is saved to disk or simulated.
     */
    for (x = 0; x < CITY_WIDTH; ++x) {
        for (y = 0; y < CITY_HEIGHT; ++y) {
            unsigned short tile = 0;
            if (x < 9U + y / 11U || (x > 98U && y > 79U))
                tile = 2; /* water */
            else if ((x * 3U + y * 7U) % 37U < 5U)
                tile = 21; /* woods */

            if (x > 17U && x < 105U && y > 12U && y < 88U) {
                if ((x > 22U && x < 49U) || (x > 52U && x < 79U))
                    tile = 265; /* residential */
                if (x > 81U && x < 99U)
                    tile = 436; /* commercial */
                if (y > 50U && x > 53U && x < 78U)
                    tile = 625; /* industrial */
                if (x == 51U || x == 80U || y == 30U || y == 65U)
                    tile = 66; /* road */
                if (y == 46U && x > 20U && x < 99U)
                    tile = 208; /* power */
            }
            city_tiles[x * CITY_HEIGHT + y] = tile & TILE_ID_MASK;
        }
    }
}
