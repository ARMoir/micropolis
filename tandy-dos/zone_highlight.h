/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure C89 vacant-lot highlight policy for 8x8 Tandy map tiles.
 * The preview-like hatching and ALL-FOUR-SIDES outline remain visible
 * after zoning is placed, including between adjacent same-kind lots.
 * Fully developed 1x1 and complete multi-tile artwork stays untouched.
 */
#ifndef TANDY_ZONE_HIGHLIGHT_H
#define TANDY_ZONE_HIGHLIGHT_H

/* The caller also checks zoning_at(x,y)!=ZONE_NONE.
 * Only clear ground or ordinary woodland with stage zero counts as a
 * vacant custom 1x1 lot. Do not scribble on ANY original .CTY building,
 * railway, utility or water art even when a zoning overlay exists.
 */
static unsigned int zone_highlight_mask(unsigned int tile_id,
                                        unsigned int growth_stage)
{
    if(growth_stage != 0U)
        return 0U;
    if(tile_id != 0U && (tile_id < 21U || tile_id > 43U))
        return 0U;
    return 15U; /* ALL four outline edges, even along joined lots */
}

/* Same alternating interior pixels as the funded zone placement
 * preview: y=1 and 5 => x=1,3,5; y=3 => x=2,4,6.
 * Each mask bit is the x coordinate on an 8px-wide tile.
 */
static unsigned int zone_highlight_hatch_row(unsigned int y)
{
    if(y==1U || y==5U) return 0x2aU;
    if(y==3U) return 0x54U;
    return 0U;
}
#endif
