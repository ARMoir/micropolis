/* SPDX-License-Identifier: GPL-3.0-or-later
 * View-only 90-degree map rotation for original 120x100 Micropolis maps.
 * Tile graphics are never rotated and original .CTY/.ZON stays untouched.
 */
#ifndef TANDY_VIEW_ROTATION_H
#define TANDY_VIEW_ROTATION_H

unsigned int view_rotation_width(unsigned int quarter_turns);
unsigned int view_rotation_height(unsigned int quarter_turns);
void view_rotation_to_world(unsigned int turns, unsigned int vx,
                            unsigned int vy, unsigned int *wx,
                            unsigned int *wy);
void view_rotation_from_world(unsigned int turns, unsigned int wx,
                              unsigned int wy, unsigned int *vx,
                              unsigned int *vy);
/* Pixel area sampled by one world square on a nearest-neighbor
 * minimap. Ranges are [x0,x1), [y0,y1), and can be empty because
 * the miniature map has fewer pixels than world tiles.
 */
void view_rotation_minimap_bounds(unsigned int turns,
                                  unsigned int wx,unsigned int wy,
                                  unsigned int width,unsigned int height,
                                  unsigned int *x0,unsigned int *y0,
                                  unsigned int *x1,unsigned int *y1);

/* The exact minimap pixel rectangle sampling an inclusive world
 * rectangle, for all 0/90/180/270 degree view orientations.
 * Pixel bounds are half-open and clipped to minimap dimensions.
 */
void view_rotation_minimap_rect(unsigned int turns,
                                unsigned int left,unsigned int top,
                                unsigned int right,unsigned int bottom,
                                unsigned int width,unsigned int height,
                                unsigned int *px0,unsigned int *py0,
                                unsigned int *px1,unsigned int *py1);

void view_rotation_clamp(unsigned int turns, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top);
void view_rotation_focus(unsigned int turns, unsigned int wx,
                         unsigned int wy, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top, unsigned int *cursor_x,
                         unsigned int *cursor_y);
/* Return zero for an invalid minimap pixel, leaving outputs unchanged.
 * Otherwise focus the exact world tile represented by that pixel.
 */
int view_rotation_minimap_focus(unsigned int turns,
                                 unsigned int px,unsigned int py,
                                 unsigned int minimap_w,
                                 unsigned int minimap_h,
                                 unsigned int view_w,unsigned int view_h,
                                 unsigned int *left,unsigned int *top,
                                 unsigned int *cursor_x,
                                 unsigned int *cursor_y);
/* World top/right/bottom/left border bits 1/2/4/8 to screen edges. */
unsigned int view_rotation_edges(unsigned int turns, unsigned int edges);
/* Keep an assembled 3x3 or 4x4 building graphic upright on screen,
 * without rotating any pixel sprites. 'part' is 0..span*span-1.
 */
unsigned int view_rotation_upright_part(unsigned int turns,
                                         unsigned int span,
                                         unsigned int part);
/* Pick existing pre-rendered road/wire tile art in the new direction.
 * No bitmap rotation, change to world tiles, or extra memory buffers.
 */
unsigned int view_rotation_utility_tile(unsigned int turns,
                                         unsigned int graphic_id);
#endif
