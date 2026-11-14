/* SPDX-License-Identifier: GPL-3.0-or-later
 * Detailed Tandy tile inspection using only real city/simulation
 * fields; do not fabricate land values, traffic, crime, or pollution.
 */
#include "tile_inspect.h"
#include "city.h"
#include "zoning.h"
#include "newer_lot.h"
#include "simulation.h"
#include "engine.h"
#include <stdio.h>
#include <string.h>

#define BASE_MASK 1023U
#define ZONE_BIT 1024U
#define ANIM_BIT 2048U
#define BULL_BIT 4096U
#define BURN_BIT 8192U
#define COND_BIT 16384U
#define PWR_BIT 32768U

static const char *zone_name(unsigned int z)
{
    if(z==ZONE_RES)return "RESIDENTIAL";
    if(z==ZONE_COM)return "COMMERCIAL";
    if(z==ZONE_IND)return "INDUSTRIAL";
    return "NONE";
}
static const char *yn(unsigned int v) {return v?"YES":"NO";}
static unsigned int road(unsigned int id)
{
    return (id>=64U&&id<=207U) || id==237U || id==238U ||
           id==239U || (id>=828U&&id<=831U) ||
           (id>=948U&&id<=951U);
}
static unsigned int rail(unsigned int id)
{
    return (id>=224U&&id<=238U);
}
static unsigned int wire(unsigned int id)
{
    return (id>=208U&&id<=222U) || id==77U || id==78U ||
           id==239U;
}
const char *tile_inspect_kind(unsigned int id)
{
    if(id==0U)return "CLEAR GROUND";
    if(id>=1U&&id<=20U)return "WATER / SHORE";
    if(id>=21U&&id<=43U)return "FOREST / TREES";
    if(id>=44U&&id<=47U)return "RUBBLE";
    if(id>=48U&&id<=51U)return "FLOOD";
    if(id==52U)return "RADIOACTIVE LAND";
    if(id>=53U&&id<=55U)return "UNUSED TERRAIN";
    if(id>=56U&&id<=63U)return "FIRE";
    if(id==64U||id==65U||id==79U||id==95U||
       (id>=828U&&id<=831U)||(id>=948U&&id<=951U))
        return "ROAD BRIDGE";
    if(id==77U||id==78U||id==239U)return "ROAD / POWER CROSS";
    if(id>=66U&&id<=207U)return "ROAD / TRAFFIC";
    if(id>=208U&&id<=220U)return "POWER LINE";
    if(id==221U||id==222U)return "RAIL / POWER CROSS";
    if(id==223U)return "UNUSED NETWORK";
    if(id==224U||id==225U)return "RAIL BRIDGE";
    if(id>=226U&&id<=236U)return "RAILWAY";
    if(id==237U||id==238U)return "ROAD / RAIL CROSS";
    if(id>=240U&&id<=408U)return "RESIDENTIAL ART";
    if(id>=409U&&id<=417U)return "HOSPITAL";
    if(id>=418U&&id<=422U)return "CHURCH";
    if(id>=423U&&id<=611U)return "COMMERCIAL ART";
    if(id>=612U&&id<=692U)return "INDUSTRIAL ART";
    if(id>=693U&&id<=708U)return "SEAPORT";
    if(id>=709U&&id<=744U)return "AIRPORT";
    if(id>=745U&&id<=760U)return "COAL POWER PLANT";
    if(id>=761U&&id<=769U)return "FIRE STATION";
    if(id>=770U&&id<=778U)return "POLICE STATION";
    if(id>=779U&&id<=810U)return "STADIUM";
    if(id>=811U&&id<=826U)return "NUCLEAR PLANT";
    if(id==827U)return "LIGHTNING SYMBOL";
    if(id>=832U&&id<=839U)return "RADAR ANIMATION";
    if(id>=840U&&id<=843U)return "FOUNTAIN";
    if(id>=844U&&id<=851U)return "TELEPHONE / WIRE";
    if(id>=852U&&id<=859U)return "SMOKE ANIMATION";
    if(id>=860U&&id<=915U)return "EXPLOSION ANIMATION";
    if(id>=916U&&id<=931U)return "COAL SMOKE";
    if(id>=932U&&id<=947U)return "FOOTBALL ANIMATION";
    if(id>=952U&&id<=959U)return "OTHER SOURCE ART";
    if(id>=960U)return "UNRECOGNIZED TILE ID";
    return "SOURCE ANIMATION";
}

/* Tile group bases are original tile IDs, not the synthesized NEWER
 * sidecar sprite IDs. Exact center ZONEBIT is required: a stray art
 * fragment is not a complete original building.
 */
static int original_group(unsigned int x,unsigned int y,
                          unsigned int id,struct tile_inspect *o)
{
    unsigned int first=0U,span=0U,part;
    int cx,cy,ax,ay;
    unsigned short center;
    if(id>=240U&&id<=248U) {first=240U;span=3U;}
    else if(id>=261U&&id<=404U) {
        first=261U+((id-261U)/9U)*9U;span=3U;
    } else if(id>=405U&&id<=422U) {
        first=405U+((id-405U)/9U)*9U;span=3U;
    } else if(id>=423U&&id<=431U) {first=423U;span=3U;}
    else if(id>=432U&&id<=611U) {
        first=432U+((id-432U)/9U)*9U;span=3U;
    } else if(id>=612U&&id<=620U){first=612U;span=3U;}
    else if(id>=621U&&id<=692U) {
        first=621U+((id-621U)/9U)*9U;span=3U;
    } else if(id>=693U&&id<=708U){first=693U;span=4U;}
    else if(id>=709U&&id<=744U){first=709U;span=6U;}
    else if(id>=745U&&id<=760U){first=745U;span=4U;}
    else if(id>=761U&&id<=769U){first=761U;span=3U;}
    else if(id>=770U&&id<=778U){first=770U;span=3U;}
    else if(id>=779U&&id<=810U){
        first=779U+((id-779U)/16U)*16U;span=4U;
    } else if(id>=811U&&id<=826U){first=811U;span=4U;}
    if(span==0U)return 0;
    part=id-first;
    cx=(int)x+1-(int)(part%span);
    cy=(int)y+1-(int)(part/span);
    ax=cx-1;ay=cy-1;
    if(ax<0||ay<0||ax+(int)span>=(int)CITY_WIDTH+1||
       ay+(int)span>=(int)CITY_HEIGHT+1)return 0;
    center=city_at((unsigned int)cx,(unsigned int)cy);
    if((center&(ZONE_BIT|BASE_MASK))!=(ZONE_BIT|first+span+1U))
        return 0;
    o->left=(unsigned int)ax;o->top=(unsigned int)ay;
    o->span=span;o->part_x=part%span;o->part_y=part/span;
    o->source_id=first;
    o->original_group=1U;
    return 1;
}
void tile_inspect_collect(unsigned int x,unsigned int y,
                          struct tile_inspect *o)
{
    unsigned int i,id;
    if(o==0)return;
    memset(o,0,sizeof(*o));
    o->x=x;o->y=y;o->left=x;o->top=y;o->span=1U;
    if(x>=CITY_WIDTH||y>=CITY_HEIGHT)return;
    o->word=city_at(x,y);
    o->id=o->word&BASE_MASK;
    o->zone=zoning_at(x,y);
    o->stage=zoning_development_at(x,y);
    o->network_valid=(unsigned int)engine_power_valid();
    o->needs_warning=(unsigned int)simulation_needs_power_warning(x,y);
    o->service=simulation_lot_service(x,y);
    if(newer_lot_info(x,y,&o->left,&o->top,&o->span,0,
                      &o->source_id)) {
        o->newer=1U;
        o->part_x=x-o->left;o->part_y=y-o->top;
    } else
        (void)original_group(x,y,o->id,o);
    o->neighbor[0]=y>0U?(city_at(x,y-1U)&BASE_MASK):1024U;
    o->neighbor[1]=x+1U<CITY_WIDTH?
                   (city_at(x+1U,y)&BASE_MASK):1024U;
    o->neighbor[2]=y+1U<CITY_HEIGHT?
                   (city_at(x,y+1U)&BASE_MASK):1024U;
    o->neighbor[3]=x>0U?(city_at(x-1U,y)&BASE_MASK):1024U;
    for(i=0U;i<4U;++i) {
        id=o->neighbor[i];
        if(road(id))++o->roads;
        if(rail(id))++o->rails;
        if(wire(id))++o->wires;
    }
}

void tile_inspect_line(const struct tile_inspect *o,
                       unsigned int page,unsigned int row,
                       char out[TILE_INSPECT_LINE+1U])
{
    const char *kind,*source;
    unsigned int flags,capacity;
    out[0]='\0';
    if(o==0||row>=TILE_INSPECT_ROWS)return;
    page%=TILE_INSPECT_PAGES;
    kind=tile_inspect_kind(o->id);
    source=o->newer?"NEWER COMPLETE ART":
           o->original_group?"ORIGINAL PREFAB":
           o->zone!=ZONE_NONE?"CUSTOM ZONED LOT":
           "ORIGINAL MAP";
    if(page==0U) {
        switch(row) {
        case 0U:sprintf(out,"IDENTITY          PAGE 1/4");break;
        case 1U:sprintf(out,"WORLD X:%03u  Y:%03u",o->x,o->y);break;
        case 2U:
            if(o->newer)
                sprintf(out,"TYPE: NEWER %s",zone_name(o->zone));
            else if(o->zone && o->id==0U)
                sprintf(out,"TYPE: %s LOT",zone_name(o->zone));
            else sprintf(out,"TYPE: %s",kind);
            break;
        case 3U:sprintf(out,"TILE ID: %u  HEX:%03X",o->id,o->id);break;
        case 4U:sprintf(out,"ZONING: %s",zone_name(o->zone));break;
        case 5U:sprintf(out,"SOURCE: %s",source);break;
        case 6U:sprintf(out,"FOOTPRINT: %u X %u",o->span,o->span);break;
        case 7U:sprintf(out,"ORIGIN: %03u,%03u",o->left,o->top);break;
        case 8U:sprintf(out,"PART: %u,%u IN FOOTPRINT",
                        o->part_x+1U,o->part_y+1U);break;
        case 9U:
            if(o->newer)
                sprintf(out,"NEWER SOURCE ID: %02X",o->source_id);
            else if(o->original_group)
                sprintf(out,"ORIGINAL BASE ID: %u",o->source_id);
            else sprintf(out,"GRAPHIC: ORIGINAL TILE");
            break;
        }
    } else if(page==1U) {
        switch(row) {
        case 0U:sprintf(out,"SERVICES          PAGE 2/4");break;
        case 1U:sprintf(out,"NETWORK: %s",
                        o->network_valid?"READY":"SCAN PENDING");break;
        case 2U:sprintf(out,"ROAD/RAIL <=6: %s",
                        yn(o->service&SIM_SERVICE_TRANSPORT));break;
        case 3U:sprintf(out,"POWER NEARBY: %s",
                        yn(o->service&SIM_SERVICE_POWER));break;
        case 4U:sprintf(out,"TILE ENERGIZED: %s",
                        yn(o->word&PWR_BIT));break;
        case 5U:sprintf(out,"TILE CONDUCTIVE: %s",
                        yn(o->word&COND_BIT));break;
        case 6U:sprintf(out,"NO POWER ICON: %s",
                        yn(o->needs_warning));break;
        case 7U:sprintf(out,"ADJ. ROAD:%u RAIL:%u WIRE:%u",
                        o->roads,o->rails,o->wires);break;
        case 8U:sprintf(out,"ROAD ACCESS = WITHIN 6 TILES");break;
        case 9U:sprintf(out,"NO CRIME/POLLUTION MODEL");break;
        }
    } else if(page==2U) {
        switch(row) {
        case 0U:sprintf(out,"DEVELOPMENT       PAGE 3/4");break;
        case 1U:sprintf(out,"ZONE: %s",zone_name(o->zone));break;
        case 2U:
            if(o->original_group)
                sprintf(out,"GROWTH: ORIGINAL PREFAB");
            else if(o->zone)
                sprintf(out,"LOT STAGE: %u / 3",o->stage);
            else sprintf(out,"STAGE: NOT ZONED");
            break;
        case 3U:
            if(o->newer)sprintf(out,"BUILDING: %uX%u NEWER",o->span,o->span);
            else if(o->original_group)
                sprintf(out,"BUILDING: %uX%u ORIGINAL",o->span,o->span);
            else if(o->zone)
                sprintf(out,"LOT: %s",
                        o->stage?"DEVELOPED":"VACANT");
            else sprintf(out,"NOT A CUSTOM RCI LOT");
            break;
        case 4U:sprintf(out,"ANCHOR: %03u,%03u",o->left,o->top);break;
        case 5U:sprintf(out,"SECTION: %u,%u OF %uX%u",
                        o->part_x+1U,o->part_y+1U,
                        o->span,o->span);break;
        case 6U:
            if(o->newer)
                sprintf(out,"NEWER DESIGN ID: %02X",o->source_id);
            else if(o->original_group)
                sprintf(out,"BASE GRAPHIC: %u",o->source_id);
            else sprintf(out,"BUILDING DATA: NONE");
            break;
        case 7U:
            if(o->zone && !o->original_group) {
                capacity=o->stage*(o->zone==ZONE_COM?3U:4U);
                sprintf(out,"1X1 EST. %s: %u",
                        o->zone==ZONE_RES?"PEOPLE":"JOBS",capacity);
            } else sprintf(out,"1X1 CAPACITY: NOT APPLICABLE");
            break;
        case 8U:sprintf(out,"ROAD: %s  POWER: %s",
                        yn(o->service&SIM_SERVICE_TRANSPORT),
                        yn(o->service&SIM_SERVICE_POWER));break;
        case 9U:sprintf(out,"ESTIMATES ARE SIMPLIFIED");break;
        }
    } else {
        flags=o->word;
        switch(row) {
        case 0U:sprintf(out,"RAW TILE DATA      PAGE 4/4");break;
        case 1U:sprintf(out,"WORD: %04X  BASE: %03X",
                        flags,o->id);break;
        case 2U:sprintf(out,"PWRBIT:%u    CONDBIT:%u",
                        !!(flags&PWR_BIT),!!(flags&COND_BIT));break;
        case 3U:sprintf(out,"BURNBIT:%u   BULLBIT:%u",
                        !!(flags&BURN_BIT),!!(flags&BULL_BIT));break;
        case 4U:sprintf(out,"ANIMBIT:%u   ZONEBIT:%u",
                        !!(flags&ANIM_BIT),!!(flags&ZONE_BIT));break;
        case 5U:sprintf(out,"N:%04u   E:%04u",
                        o->neighbor[0],o->neighbor[1]);break;
        case 6U:sprintf(out,"S:%04u   W:%04u",
                        o->neighbor[2],o->neighbor[3]);break;
        case 7U:sprintf(out,"SOURCE: %s",source);break;
        case 8U:sprintf(out,"FLAGS ARE ORIGINAL CTY BITS");break;
        case 9U:sprintf(out,"1024 NEIGHBOR = OUTSIDE MAP");break;
        }
    }
}
