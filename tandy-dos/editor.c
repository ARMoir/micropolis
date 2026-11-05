/* SPDX-License-Identifier: GPL-3.0-or-later
 * City construction tools adapted from Micropolis w_tool.c check3x3().
 * This is an offline city-map editor: it does NOT simulate city growth.
 * Original Micropolis: Copyright (C) 1989-2007 Electronic Arts Inc.
 */
#include "city.h"
#include "editor.h"

#define TILE_ID  1023U
#define BULLBIT  4096U
#define BURNBIT  8192U
#define CONDBIT 16384U
#define ZONEBIT  1024U

const char *edit_tool_name(enum edit_tool tool)
{
    switch (tool) {
    case EDIT_ROAD:        return "ROAD";
    case EDIT_POWER:       return "POWER";
    case EDIT_RESIDENTIAL: return "RES ZONE";
    case EDIT_COMMERCIAL:  return "COM ZONE";
    case EDIT_INDUSTRIAL:  return "IND ZONE";
    case EDIT_BULLDOZE:    return "DOZE";
    default:               return "INSPECT";
    }
}

/* Deliberately simple validation. Complex connections, terrain correction,
 * expensive bulldozes and original city-budget costs remain to be ported.
 * New zones have the correct 3x3 IDs, flags, and ZONEBIT on their center,
 * following the original w_tool.c check3x3() implementation.
 */
int edit_place(enum edit_tool tool, unsigned int x, unsigned int y)
{
    unsigned short tile, base;
    unsigned int i, j, left, top;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    tile = city_at(x, y) & TILE_ID;

    if (tool == EDIT_INSPECT)
        return 0;
    if (tool == EDIT_BULLDOZE) {
        if (tile == 0U || (tile >= 1U && tile <= 20U))
            return 0; /* don't delete waterways */
        city_set(x, y, 0U);
        return 1;
    }
    if (tool == EDIT_ROAD || tool == EDIT_POWER) {
        /* First milestone: only empty land or vegetation. */
        if (tile > 43U || (tile >= 1U && tile <= 20U))
            return 0;
        if (tool == EDIT_ROAD)
            city_set(x, y, 66U | BURNBIT | BULLBIT);
        else
            city_set(x, y, 208U | BURNBIT | BULLBIT | CONDBIT);
        return 1;
    }

    if (x == 0U || y == 0U ||
        x >= CITY_WIDTH - 1U || y >= CITY_HEIGHT - 1U)
        return 0;

    if (tool == EDIT_RESIDENTIAL)
        base = 240U;
    else if (tool == EDIT_COMMERCIAL)
        base = 423U;
    else if (tool == EDIT_INDUSTRIAL)
        base = 612U;
    else
        return 0;

    left = x - 1U;
    top = y - 1U;
    for (j = 0; j < 3U; ++j)
        for (i = 0; i < 3U; ++i)
            if ((city_at(left + i, top + j) & TILE_ID) != 0U)
                return 0;

    for (j = 0; j < 3U; ++j)
        for (i = 0; i < 3U; ++i) {
            tile = (unsigned short)(base + j * 3U + i + BURNBIT + CONDBIT);
            if (i == 1U && j == 1U)
                tile |= ZONEBIT;
            city_set(left + i, top + j, tile);
        }
    return 1;
}
