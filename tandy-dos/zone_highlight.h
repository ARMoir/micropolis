/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pixel-overlay policy for freely painted one-square zones.
 * Do not draw bright zone boundary lines across completed buildings:
 * at 8x8 Tandy resolution those lines obscure original roof details,
 * split 2x2 / 3x3 structures, and produce odd yellow L-shapes.
 * All original/source artwork remains byte-for-byte unchanged.
 */
#ifndef TANDY_ZONE_HIGHLIGHT_H
#define TANDY_ZONE_HIGHLIGHT_H
/* Only empty/wooded, completely VACANT custom lots draw boundaries.
 * Original 3x3 city sprites, small developed lots, and NewerSource
 * 2x2/3x3 buildings retain their art and no overpainted zone frame.
 * Pass the full source tile ID masked to 1023 and 0..3 growth stage.
 */
static unsigned int zone_highlight_mask(unsigned int edges,
                                        unsigned int tile_id,
                                        unsigned int growth_stage)
{
    if (growth_stage != 0U)
        return 0U;
    if (tile_id != 0U && (tile_id < 21U || tile_id > 43U))
        return 0U;
    return edges & 15U;
}
#endif
