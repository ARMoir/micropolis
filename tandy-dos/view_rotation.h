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
void view_rotation_clamp(unsigned int turns, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top);
void view_rotation_focus(unsigned int turns, unsigned int wx,
                         unsigned int wy, unsigned int width,
                         unsigned int height, unsigned int *left,
                         unsigned int *top, unsigned int *cursor_x,
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
