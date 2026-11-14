/* SPDX-License-Identifier: GPL-3.0-or-later
 * Optional NewerSource multi-square RCI structures. The original .CTY
 * tile data and original TILES.DAT remain completely untouched.
 */
#ifndef TANDY_NEWER_LOT_H
#define TANDY_NEWER_LOT_H
#if defined(__WATCOMC__) && defined(__I86__)
#define NEWER_FAR far
#else
#define NEWER_FAR
#endif
#define NEWER_LOT_MAX 224U
#define NEWER_LOT_ART_BYTES 25504U

void newer_lot_clear(void);
int newer_lot_load_art(const char *path);
void newer_lot_free_art(void);
int newer_lot_has_art(void);
/* A 1x1 developed C/I lot uses the separate existing NEWER1/legacy pack.
 * All multi-tile new artwork comes from NEWERM8.DAT.
 */
const unsigned char NEWER_FAR *newer_lot_pixels(
    unsigned int x,unsigned int y,unsigned int turns);
int newer_lot_at(unsigned int x,unsigned int y);
int newer_lot_info(unsigned int x,unsigned int y,
                   unsigned int *left,unsigned int *top,
                   unsigned int *span,unsigned int *kind,
                   unsigned int *source_id);
/* Try a mature 1x1->2x2 or existing 2x2->3x3 growth step.
 * Return the new footprint width, zero when no valid expansion.
 * The anchor is reported via left/top on success.
 */
unsigned int newer_lot_try_grow(unsigned int x,unsigned int y,
                                 unsigned int kind,
                                 unsigned int *left,unsigned int *top);
/* Demolition of ANY piece removes the whole building and resets
 * its original 1x1 zoned footprint to vacant, without removing zoning.
 */
unsigned int newer_lot_demolish(unsigned int x,unsigned int y,
                                unsigned int *left,unsigned int *top);
int newer_lot_save(const char *path);
int newer_lot_load(const char *path);
unsigned int newer_lot_count(void);
#endif
