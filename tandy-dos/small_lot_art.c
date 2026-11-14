/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "small_lot_art.h"
#include "zoning.h"
#include <stdio.h>

static unsigned char pixels[SMALL_LOT_ART_BYTES];
static int loaded = 0;

int small_lot_art_load(const char *path)
{
    static const unsigned char expected[8] = {'S','2','L','T',1,8,12,32};
    unsigned int i;
    FILE *f;
    loaded = 0;
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    for (i = 0U; i < sizeof(expected); ++i) {
        if (fgetc(f) != expected[i]) {
            fclose(f);
            return 0;
        }
    }
    if (fread(pixels, 1U, sizeof(pixels), f) == sizeof(pixels) &&
        fgetc(f) == EOF && !ferror(f)) loaded = 1;
    fclose(f);
    return loaded;
}

const unsigned char *small_lot_art_pixels(unsigned int tile,
    unsigned int kind, unsigned int stage, unsigned int x, unsigned int y)
{
    /* Source order: commercial 7C..83, industrial 84..87.
     * Stable world-coordinate variety survives reload and view rotation.
     * Bigger shops/offices and industrial plant appear at higher density.
     */
    static const unsigned char commercial[3][4] = {
        {0,2,3,2}, {1,6,7,6}, {4,5,7,5}
    };
    static const unsigned char industrial[3][4] = {
        {8,10,8,10}, {8,10,11,10}, {9,11,9,11}
    };
    unsigned int variant, index;
    if (!loaded || stage < 1U || stage > 3U ||
        (tile != 0U && !(tile >= 21U && tile <= 43U))) return NULL;
    variant = (x + (y << 1U) + (x >> 2U) + (y >> 2U)) & 3U;
    if (kind == ZONE_COM) index = commercial[stage-1U][variant];
    else if (kind == ZONE_IND) index = industrial[stage-1U][variant];
    else return NULL;
    return pixels + index * 32U;
}
