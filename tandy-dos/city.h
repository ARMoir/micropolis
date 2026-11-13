/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis for Tandy 1000: city map format and demonstration.
 * Original Micropolis code and city data: Copyright (C) 1989-2007 Electronic Arts Inc.
 * Refer to the repository's COPYING file and additional terms.
 */
#ifndef TANDY_CITY_H
#define TANDY_CITY_H

#define CITY_WIDTH   120
#define CITY_HEIGHT  100
#define CITY_TILES   (CITY_WIDTH * CITY_HEIGHT)

extern unsigned short city_tiles[CITY_TILES];

/* Loads the unmodified, normal-sized 27,120-byte Micropolis .CTY file.
 * Read-only. 0 means invalid, missing or unsupported city.
 */
int city_load(const char *path);
int city_save_as(const char *path);
void city_set(unsigned int x, unsigned int y, unsigned short tile);
void city_make_demo(void);
unsigned short city_at(unsigned int x, unsigned int y);

/* Original Micropolis MiscHis[50..51] funds field, big-endian signed 32.
 * Original .CTY funds are loaded and saved intact except when construction
 * explicitly spends money. Demo cities start with a test budget.
 */
long city_get_funds(void);
void city_set_funds(long funds);
int city_spend_funds(long cost);

#endif
