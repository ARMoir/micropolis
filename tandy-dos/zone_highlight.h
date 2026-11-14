/* SPDX-License-Identifier: GPL-3.0-or-later
 * Evolving zone footprints for the 8x8 Tandy mode-9 renderer.
 * Outline bits are world N/E/S/W=1/2/4/8, rotated by the caller.
 * Every vacant and developed 1x1 has a complete border. Complete
 * 2x2/3x3 buildings have ONLY their exterior edges highlighted.
 * Vacant parcels retain a nine-pixel placement-style hatch.
 */
#ifndef TANDY_ZONE_HIGHLIGHT_H
#define TANDY_ZONE_HIGHLIGHT_H

/* Return world-space perimeter edges for this tile, or zero.
 * The optional hatched output is true only for vacant 1x1 land.
 * Uses the real NewerSource owner/footprint records and validates
 * the existing original Micropolis 3x3 zone center.
 */
unsigned int zone_highlight_style(unsigned int x,unsigned int y,
                                   int *hatched);

/* Pure footprint geometry: each original 1x1 and 2x2/3x3 sub-tile
 * only draws the exterior edges of its OWN structure.
 */
unsigned int zone_highlight_footprint_edges(unsigned int x,
                                             unsigned int y,
                                             unsigned int left,
                                             unsigned int top,
                                             unsigned int span);

/* World-space edges within a continuous SAME-TYPE zone group.
 * These shared edges get a muted road-asphalt gray in the renderer;
 * all genuine outer neighborhood edges stay the original zone color.
 * A 2x2/3x3 building has no internal edges to recolor.
 * Only mark shared edges when both actual tiles have a complete
 * facing perimeter, avoiding asymmetric gray lines on damaged art.
 */
unsigned int zone_highlight_shared_edges(unsigned int x,unsigned int y,
                                          unsigned int edges);

/* Same interior stipple as a funded construction preview. */
unsigned int zone_highlight_hatch_row(unsigned int y);

#endif
