/* SPDX-License-Identifier: GPL-3.0-or-later
 * NewerSource concrete pads and driveway hints: procedural graphics
 * composited ONLY into exposed green pixels of original converted
 * NEWER artwork. No allocation, global city scan or data writes.
 */
#include "newer_site.h"
#include "city.h"
#include "zoning.h"

#define TILE_ID 1023U
static int is_road(unsigned int x,unsigned int y)
{
    unsigned int id;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    id=city_at(x,y)&TILE_ID;
    /* Original roads, bridges and traffic-density variants,
     * plus original road/rail crossings. Bare rail/wire is not a
     * legal visual driveway connection.
     */
    return (id>=64U && id<=207U) || id==237U || id==238U;
}

unsigned int newer_site_road_side(unsigned int left,unsigned int top,
                                   unsigned int span)
{
    unsigned int i,n=0U,e=0U,s=0U,w=0U,best=0U,side=0U;
    if(span==0U || span>3U || left+span>CITY_WIDTH ||
       top+span>CITY_HEIGHT)return 0U;
    for(i=0U;i<span;++i) {
        if(top>0U && is_road(left+i,top-1U)) ++n;
        if(left+span<CITY_WIDTH &&
           is_road(left+span,top+i)) ++e;
        if(top+span<CITY_HEIGHT &&
           is_road(left+i,top+span)) ++s;
        if(left>0U && is_road(left-1U,top+i)) ++w;
    }
    /* Avoid fake free-standing streets when there is no road.
     * Preserve traditional south-facing curb on ties.
     */
    if(s>best){best=s;side=4U;}
    if(e>best){best=e;side=2U;}
    if(w>best){best=w;side=8U;}
    if(n>best){best=n;side=1U;}
    return side;
}

int newer_site_front_piece(unsigned int span,unsigned int side,
                            unsigned int part_x,unsigned int part_y)
{
    if(span==0U || span>3U ||
       part_x>=span || part_y>=span) return 0;
    switch(side) {
    case 1U:return part_y==0U;
    case 2U:return part_x==span-1U;
    case 4U:return part_y==span-1U;
    case 8U:return part_x==0U;
    default:return 0;
    }
}

unsigned int newer_site_marker(unsigned int kind,unsigned int span,
                                unsigned int side,
                                unsigned int x,unsigned int y)
{
    unsigned int width,u,v,mid,spread;
    if(kind<ZONE_RES || kind>ZONE_IND ||
       span==0U || span>3U) return 0U;
    width=span*8U;
    if(x>=width || y>=width)return 0U;
    /* Normalize access from N/E/S/W to a common SOUTH-facing
     * coordinate system. The foreground sprite stays upright;
     * only the 1px paving detail turns to meet the actual road.
     */
    switch(side) {
    case 1U: u=width-1U-x;v=width-1U-y;break;
    case 2U: u=y;v=width-1U-x;break;
    case 4U: u=x;v=y;break;
    case 8U: u=width-1U-y;v=x;break;
    default:return 0U;
    }
    mid=width/2U;
    if(span==1U) {
        /* Small pads/short curb connections, not road tile art.
         * Residential 1x1 retains original Micropolis houses,
         * so only 1x1 COM and IND are used by the NEWER renderer.
         */
        if(kind==ZONE_RES) return 0U;
        /* The converted 1x1 office and warehouse often occupy the
         * CENTER near the southern curb. An uninterrupted center
         * stripe would be invisible (or cover the sprite), whereas
         * a little paved shoulder at both sides is consistently
         * visible through the remaining grass.
         */
        if(v==6U && u>=1U && u<=6U) return 8U;
        if(v==5U && (u==1U || u==6U))return 8U;
        if(v==4U && (u==1U || u==6U))return 7U;
        return 0U;
    }
    /* Complete 2x2/3x3 buildings get ONE shared frontage, not a
     * repeated driveway on each constituent 8x8 sprite. All marks
     * stay at least 2px inside the original zone perimeter.
     */
    spread=kind==ZONE_IND ? (span==2U?5U:6U) :
           (span==2U?3U:5U);
    /* The most reliable exposed ground in the actual converted
     * 2x2/3x3 sprites is the penultimate *footprint* row. Moving the
     * shared curb there also stops it from cutting across shop roofs.
     */
    if(v==width-2U && u>=mid-spread &&
       u<mid+spread) return kind==ZONE_RES?7U:8U;
    if(kind==ZONE_RES) {
        /* Single tiny walkway for large apartments. */
        if(v==width-3U && u==mid)return 7U;
        if(v==width-4U && u==mid)return 8U;
    } else {
        if(v==width-3U && (u==mid-1U || u==mid))
            return 7U;
        if(kind==ZONE_IND && v==width-4U &&
           (u==mid-3U || u==mid+2U))
            return 8U;
    }
    return 0U;
}

unsigned int newer_site_composite(unsigned int source_color,
                                   unsigned int marker)
{
    if(source_color==2U && (marker==7U || marker==8U))
        return marker;
    return source_color;
}
