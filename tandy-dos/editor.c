/* SPDX-License-Identifier: GPL-3.0-or-later
 * City construction tools adapted from Micropolis w_tool.c check3x3().
 * This is an offline city-map editor: it does NOT simulate city growth.
 * Original Micropolis: Copyright (C) 1989-2007 Electronic Arts Inc.
 */
#include "city.h"
#include "editor.h"
#include "connections.h"
#include "infrastructure.h"

#define TILE_ID  1023U
#define BULLBIT  4096U
#define BURNBIT  8192U
#define CONDBIT 16384U
#define ZONEBIT  1024U

const char *edit_tool_name(enum edit_tool tool)
{
    switch (tool) {
    case EDIT_ROAD:        return "ROAD";
    case EDIT_RAIL:        return "RAIL";
    case EDIT_POWER:       return "POWER";
    case EDIT_RESIDENTIAL: return "RES ZONE";
    case EDIT_COMMERCIAL:  return "COM ZONE";
    case EDIT_INDUSTRIAL:  return "IND ZONE";
    case EDIT_COAL:        return "COAL";
    case EDIT_NUCLEAR:     return "NUCLEAR";
    case EDIT_POLICE:      return "POLICE";
    case EDIT_FIRE:        return "FIRE";
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
        /* Removing a bridge uncovers the river, not a dirt tile.
         * BRWH/BRWV are original animated bridge variants.
         */
        if(tile==64U || tile==65U || tile==79U || tile==95U ||
           tile==208U || tile==209U || tile==224U || tile==225U ||
           (tile>=828U && tile<=831U) ||
           (tile>=948U && tile<=951U))
            city_set(x,y,2U);
        else
            city_set(x,y,0U);
        connections_fix_cross(x, y);
        return 1;
    }
    if (tool == EDIT_ROAD || tool == EDIT_RAIL || tool == EDIT_POWER) {
        unsigned int crossing, bridge=0U;
        unsigned short placed;
        if (!infrastructure_can_place(tool,x,y))
            return 0;
        if(tool==EDIT_ROAD && infrastructure_is_water(tile))
            bridge=infrastructure_road_bridge_id(x,y);
        if(tool==EDIT_RAIL && infrastructure_is_water(tile))
            bridge=infrastructure_rail_bridge_id(x,y);
        crossing = infrastructure_crossing_id(tool,tile);
        if(bridge!=0U) {
            /* Preserve the river: a bridge deck is bulldozable, but
             * it must not acquire conductor/flammability flags.
             */
            placed=(unsigned short)(bridge|BULLBIT);
        } else if (crossing != 0U) {
            /* Original _LayRoad/_LayWire perpendicular crossings:
             * retain both transport geometries and electrical
             * conductivity where the completed tile contains wire.
             */
            placed = (unsigned short)(crossing | BURNBIT | BULLBIT);
            if (crossing == 77U || crossing == 78U ||
                crossing == 221U || crossing == 222U)
                placed |= CONDBIT;
        } else if (tool == EDIT_ROAD) {
            placed = 66U | BURNBIT | BULLBIT;
        } else if (tool == EDIT_RAIL) {
            placed = 226U | BURNBIT | BULLBIT;
        } else {
            placed = 210U | BURNBIT | BULLBIT | CONDBIT;
        }
        city_set(x,y,placed);
        connections_fix_cross(x,y);
        return 1;
    }

    if(tool==EDIT_COAL || tool==EDIT_NUCLEAR ||
       tool==EDIT_FIRE || tool==EDIT_POLICE) {
        unsigned int width=(tool==EDIT_COAL || tool==EDIT_NUCLEAR)?4U:3U;
        unsigned int base=(tool==EDIT_COAL)?745U:
                          (tool==EDIT_NUCLEAR)?811U:
                          (tool==EDIT_FIRE)?761U:770U;
        if(x==0U || y==0U || x+width-1U>=CITY_WIDTH ||
           y+width-1U>=CITY_HEIGHT)
            return 0;
        left=x-1U;top=y-1U;
        for(j=0U;j<width;++j)
            for(i=0U;i<width;++i)
                if((city_at(left+i,top+j)&TILE_ID)!=0U)
                    return 0;
        /* Original 4x4 generator for coal/nuclear and 3x3
         * police/fire. The zone center is (1,1).
         */
        for(j=0U;j<width;++j)
            for(i=0U;i<width;++i) {
                tile=(unsigned short)(base+j*width+i+BURNBIT+CONDBIT);
                if(i==1U && j==1U) tile|=ZONEBIT;
                if(width==4U && i==1U && j==2U) tile|=2048U;
                city_set(left+i,top+j,tile);
            }
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
