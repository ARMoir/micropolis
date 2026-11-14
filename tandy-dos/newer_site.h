/* SPDX-License-Identifier: GPL-3.0-or-later
 * NewerSource 1x1/2x2/3x3 site access detailing for Tandy 320x200.
 * Only recolor existing green (#2) pixels in optional NEWER sprite
 * data: all walls, windows, roofs, outlines, source files and .CTY
 * map tiles remain byte-for-byte unchanged.
 */
#ifndef TANDY_NEWER_SITE_H
#define TANDY_NEWER_SITE_H

/* Bit values follow existing view_rotation_edges: N=1 E=2 S=4 W=8.
 * Return 0 if NO actual road touches the building footprint. Rail
 * and plain power conductors are deliberately NOT driveways.
 * Prefer a side with more adjoining roads; on ties S, E, W, N.
 */
unsigned int newer_site_road_side(unsigned int left,unsigned int top,
                                   unsigned int span);

/* True iff this sprite piece lies along the side facing the road.
 * part_x/part_y are DISPLAY-space footprint coordinates (0..span-1)
 * after view_rotation_upright_part() has rearranged source tiles.
 */
int newer_site_front_piece(unsigned int span,unsigned int side,
                            unsigned int part_x,unsigned int part_y);

/* Thin, display-oriented concrete/access pattern. x,y are pixel
 * coordinates within the ENTIRE assembled 8*span square; the
 * returned color is 0 (leave unchanged), 7 or 8 (EGA grays).
 */
unsigned int newer_site_marker(unsigned int kind,unsigned int span,
                                unsigned int display_side,
                                unsigned int x,unsigned int y);

/* Source-art guard: a marker can replace GREEN only, never another
 * color. This ensures building walls, windows and roofs stay exact.
 */
unsigned int newer_site_composite(unsigned int source_color,
                                   unsigned int marker);

#endif
