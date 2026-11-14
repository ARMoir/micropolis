/* SPDX-License-Identifier: GPL-3.0-or-later
 * Use authentic Micropolis crossing sprite IDs (w_con.c).
 * No reinterpreted road/rail art, additional graphics, or map rewrites.
 *
 *   ROAD over horizontal power (210) -> vertical road / H power 78
 *   ROAD over vertical power (211)   -> horizontal road / V power 77
 *   ROAD over horizontal rail (226) -> vertical road / H rail 237
 *   ROAD over vertical rail (227)   -> horizontal road / V rail 238
 *   POWER over horizontal road (66) -> horizontal road / V wire 77
 *   POWER over vertical road (67)   -> vertical road / H wire 78
 *   POWER over horizontal rail (226)-> horizontal rail / V wire 221
 *   POWER over vertical rail (227)  -> vertical rail / H wire 222
 *
 * Mixed triple crossings and incompatible rail junctions remain
 * unsupported. Original road bridge IDs 64/65 cross water tiles
 * 2..4, matching _LayRoad in the original w_con.c.
 */
#include "city.h"
#include "infrastructure.h"

#define TILE_MASK 1023U

#define HBRIDGE 64U
#define VBRIDGE 65U

int infrastructure_is_water(unsigned int tile_id)
{
    return tile_id>=2U && tile_id<=4U; /* river, river edge, channel */
}

/* Traffic-loaded road graphics carry their base connection shape
 * in the low nibble, as in original NeutralizeRoad().
 */
static unsigned int road_shape(unsigned int id)
{
    if(id>=80U && id<=207U) return 64U+(id&15U);
    return id;
}

/* Direction-sensitive anchors for bridge spans. Road junctions and
 * straight land roads can connect; only same-axis water bridges and
 * compatible crossed roads can extend an existing span.
 */
static int road_anchor(unsigned int id,int vertical)
{
    id=road_shape(id);
    if(vertical) {
        if(id==VBRIDGE || id==95U || id==237U || id==78U) return 1;
    } else {
        if(id==HBRIDGE || id==79U || id==238U || id==77U) return 1;
    }
    return id>=66U && id<=76U;
}

unsigned int infrastructure_road_bridge_id(unsigned int x,unsigned int y)
{
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       !infrastructure_is_water(city_at(x,y)&TILE_MASK)) return 0U;
    /* Same preference as original _LayRoad: horizontal first.
     * Never build isolated road squares in open water.
     */
    if(x+1U<CITY_WIDTH &&
       road_anchor(city_at(x+1U,y)&TILE_MASK,0)) return HBRIDGE;
    if(x>0U &&
       road_anchor(city_at(x-1U,y)&TILE_MASK,0)) return HBRIDGE;
    if(y+1U<CITY_HEIGHT &&
       road_anchor(city_at(x,y+1U)&TILE_MASK,1)) return VBRIDGE;
    if(y>0U &&
       road_anchor(city_at(x,y-1U)&TILE_MASK,1)) return VBRIDGE;
    return 0U;
}

unsigned int infrastructure_crossing_id(enum edit_tool tool,
                                        unsigned int existing_id)
{
    if (tool == EDIT_ROAD) {
        if (existing_id == 210U) return 78U;
        if (existing_id == 211U) return 77U;
        if (existing_id == 226U) return 237U;
        if (existing_id == 227U) return 238U;
    } else if (tool == EDIT_POWER) {
        /* Traffic-density road sprites share the same low-nibble
         * street geometry as original straight roads 66 and 67.
         */
        if (existing_id >= 66U && existing_id <= 207U) {
            if ((existing_id & 15U) == 2U) return 77U;
            if ((existing_id & 15U) == 3U) return 78U;
        }
        if (existing_id == 226U) return 221U;
        if (existing_id == 227U) return 222U;
    }
    return 0U;
}

int infrastructure_can_place(enum edit_tool tool,
                             unsigned int x, unsigned int y)
{
    unsigned int id;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT ||
        (tool != EDIT_ROAD && tool != EDIT_POWER))
        return 0;
    id = city_at(x,y) & TILE_MASK;
    /* Water bridge construction is distinct from road/wire
     * crossings. Preview may additionally use funded planned banks.
     */
    if(tool==EDIT_ROAD && infrastructure_is_water(id))
        return infrastructure_road_bridge_id(x,y)!=0U;
    if(id==0U || (id>=21U && id<=43U))
        return 1;
    return infrastructure_crossing_id(tool,id)!=0U;
}
