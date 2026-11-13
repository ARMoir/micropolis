/* SPDX-License-Identifier: GPL-3.0-or-later
 * Tile-by-tile zoning overlay for the experimental Tandy DOS Micropolis port.
 * Does not replace the original 3x3-zone simulation or modify original .CTY data.
 */
#ifndef TANDY_ZONING_H
#define TANDY_ZONING_H

#define ZONE_NONE 0U
#define ZONE_RES  1U
#define ZONE_COM  2U
#define ZONE_IND  3U

void zoning_clear(void);
void zoning_seed_from_city(void);
unsigned int zoning_at(unsigned int x, unsigned int y);
int zoning_set(unsigned int x, unsigned int y, unsigned int kind);
int zoning_can_set(unsigned int x, unsigned int y, unsigned int kind);
unsigned int zoning_paint_line(unsigned int x0, unsigned int y0,
                               unsigned int x1, unsigned int y1,
                               unsigned int kind);
int zoning_save(const char *path);
int zoning_load(const char *path);
unsigned int zoning_count(unsigned int kind);
/* N=1 E=2 S=4 W=8; omit internal borders of same-type zones. */
unsigned int zoning_edge_mask(unsigned int x, unsigned int y);

#endif
