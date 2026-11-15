/* SPDX-License-Identifier: GPL-3.0-or-later
 * Evolving per-building zone border policy. Pure geometry and bounded
 * local tile lookups: no global zone scan, buffer, or saved map edits.
 */
#include "zone_highlight.h"
#include "city.h"
#include "zoning.h"
#include "newer_lot.h"

#define TILE_ID 1023U
#define ZONEBIT 1024U

unsigned int zone_highlight_footprint_edges(unsigned int x,
                                             unsigned int y,
                                             unsigned int left,
                                             unsigned int top,
                                             unsigned int span)
{
    unsigned int edges=0U;
    if(span==0U || x<left || y<top ||
       x-left>=span || y-top>=span)
        return 0U;
    if(y==top) edges|=1U; /* world north */
    if(x-left==span-1U) edges|=2U; /* east */
    if(y-top==span-1U) edges|=4U; /* south */
    if(x==left) edges|=8U; /* west */
    return edges;
}
unsigned int zone_highlight_hatch_row(unsigned int y)
{
    if(y==1U || y==5U) return 0x2aU;
    if(y==3U) return 0x54U;
    return 0U;
}

/* The ORIGINAL full-size residential/commercial/industrial art is
 * arranged in nine-tile sets. Hospital and civic sprites are excluded.
 * Zero is a sentinel (no original RCI footprint).
 */
static unsigned int original_rci_base(unsigned int id)
{
    if(id>=240U && id<=248U) return 240U;
    if(id>=261U && id<=404U)
        return 261U+((id-261U)/9U)*9U;
    if(id>=423U && id<=431U) return 423U;
    if(id>=432U && id<=611U)
        return 432U+((id-432U)/9U)*9U;
    if(id>=612U && id<=620U) return 612U;
    if(id>=621U && id<=692U)
        return 621U+((id-621U)/9U)*9U;
    return 0U;
}
unsigned int zone_highlight_style(unsigned int x,unsigned int y,
                                   int *hatched)
{
    unsigned int id,left,top,span,base,part;
    int cx,cy;
    unsigned short center;
    if(hatched!=0) *hatched=0;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       zoning_at(x,y)==ZONE_NONE)
        return 0U;
    id=city_at(x,y)&TILE_ID;

    /* New 2x2/3x3 structures own their full zone footprint.
     * The lookup works even when a building is temporarily vacant
     * because of loss of services; no internal lines ever appear.
     */
    if(newer_lot_info(x,y,&left,&top,&span,0,0))
        return zone_highlight_footprint_edges(x,y,left,top,span);

    /* Individually zoned 1x1 plots keep all four sides, regardless
     * of their neighbors. Only stage-zero vacant land is hatched.
     */
    if(id==0U || (id>=21U && id<=43U)) {
        if(hatched!=0)
            *hatched=zoning_development_at(x,y)==0U;
        return 15U;
    }

    /* Authentic original 3x3 RCI prefabs also use ONE coherent
     * perimeter. An actual matching zone-center tile and ZONEBIT
     * are required, preventing stray artwork from being bordered.
     */
    base=original_rci_base(id);
    if(base==0U) return 0U;
    part=id-base;
    cx=(int)x+1-(int)(part%3U);
    cy=(int)y+1-(int)(part/3U);
    /* A 3x3 source building's center must have a full one-cell
     * footprint on all four sides. Guard before subtracting 1U.
     */
    if(cx<=0 || cy<=0 ||
       cx+1>=CITY_WIDTH || cy+1>=CITY_HEIGHT)
        return 0U;
    center=city_at((unsigned int)cx,(unsigned int)cy);
    if((center&(TILE_ID|ZONEBIT)) != (base+4U+ZONEBIT))
        return 0U;
    return zone_highlight_footprint_edges(
        x,y,(unsigned int)(cx-1),(unsigned int)(cy-1),3U);
}

/* Only developed-to-developed boundaries may resemble gray local
 * streets. Both vacant-to-vacant and vacant-to-developed boundaries
 * keep the original R/C/I zone color and permanent vacant-lot hatch.
 * This is only a visual distinction, never a real road in simulation.
 *
 * A developed 1x1 is one with stage 1..3. A complete NewerSource
 * 2x2/3x3 is identified by its .NBL owner. Intact ORIGINAL 3x3
 * structures are also developed even if their custom zoning stage
 * is zero: zone_highlight_style() validates their original ZONEBIT.
 */
static int developed_zone_tile(unsigned int x,unsigned int y)
{
    unsigned int id;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       zoning_at(x,y)==ZONE_NONE)
        return 0;
    if(newer_lot_at(x,y))return 1;
    id=city_at(x,y)&TILE_ID;
    if(id==0U || (id>=21U && id<=43U))
        return zoning_development_at(x,y)!=0U;
    /* A stray original art fragment does not count as a complete
     * developed structure without a real matching source center.
     */
    return zone_highlight_style(x,y,0)!=0U;
}

/* The caller supplies this tile's true footprint edges. An adjacent
 * tile shares a gray line only if BOTH sides are developed, both
 * belong to the SAME R/C/I zone, and both actually draw facing edges.
 * Composite structures expose only their OUTSIDE edges, so no new
 * lines ever appear across a 2x2/3x3 building's artwork.
 *
 * Only local checks on tiles already being drawn: no buffer or scan.
 */
unsigned int zone_highlight_shared_edges(unsigned int x,unsigned int y,
                                          unsigned int edges)
{
    unsigned int kind,shared=0U;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT || edges==0U)
        return 0U;
    kind=zoning_at(x,y);
    if(kind==ZONE_NONE || !developed_zone_tile(x,y))
        return 0U;
    if((edges&1U) && y>0U &&
       zoning_at(x,y-1U)==kind &&
       developed_zone_tile(x,y-1U) &&
       (zone_highlight_style(x,y-1U,0)&4U))
        shared|=1U;
    if((edges&2U) && x+1U<CITY_WIDTH &&
       zoning_at(x+1U,y)==kind &&
       developed_zone_tile(x+1U,y) &&
       (zone_highlight_style(x+1U,y,0)&8U))
        shared|=2U;
    if((edges&4U) && y+1U<CITY_HEIGHT &&
       zoning_at(x,y+1U)==kind &&
       developed_zone_tile(x,y+1U) &&
       (zone_highlight_style(x,y+1U,0)&1U))
        shared|=4U;
    if((edges&8U) && x>0U &&
       zoning_at(x-1U,y)==kind &&
       developed_zone_tile(x-1U,y) &&
       (zone_highlight_style(x-1U,y,0)&2U))
        shared|=8U;
    return shared;
}
