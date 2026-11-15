/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TANDY_SMALL_LOT_ART_H
#define TANDY_SMALL_LOT_ART_H

/* Optional neutral NEWER1.DAT (legacy SC2K.DAT accepted): 8-byte S2LT/version/size/count/stride header,
 * followed by twelve 8x8, high-nibble-first 16-color tiles.
 * Display-only art: never allocated a CTY tile ID or saved in a city.
 */
#define SMALL_LOT_ART_COUNT 12U
#define SMALL_LOT_ART_BYTES (SMALL_LOT_ART_COUNT * 32U)
int small_lot_art_load(const char *path);
/* NULL means use the normal original-art renderer. */
const unsigned char *small_lot_art_pixels(unsigned int tile,
    unsigned int kind, unsigned int stage, unsigned int x, unsigned int y);

unsigned int small_lot_art_index(const unsigned char *p);
#endif
