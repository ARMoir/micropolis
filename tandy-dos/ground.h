/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MICROPOL_GROUND_H
#define MICROPOL_GROUND_H
/* Packed grass-background pixels become uniform brown 6; all other ink stays. */
extern unsigned char ground_tint[2][256];
void ground_init(void);
/* Forest on the tile or any of its eight world neighbors keeps grass. */
int ground_keep_green(unsigned int x,unsigned int y);
int ground_is_clear(unsigned int x,unsigned int y);
#endif
