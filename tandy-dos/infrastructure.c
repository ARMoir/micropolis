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
 * Mixed triple crossings, corners and waterways are not supported
 * by these original sprites, and should be rejected rather than
 * silently destroying an existing transportation or power network.
 */
#include "city.h"
#include "infrastructure.h"

#define TILE_MASK 1023U

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
    /* Do not build on sea, bridges, rail junctions, buildings or
     * already combined crossings. Empty/wooded ground is unchanged.
     */
    if (id == 0U || (id >= 21U && id <= 43U))
        return 1;
    return infrastructure_crossing_id(tool,id) != 0U;
}
