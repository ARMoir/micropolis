/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original Micropolis stadium game artwork in a display-only Tandy
 * animation. The city's .CTY tiles are never rewritten by effects.
 */
#ifndef TANDY_STADIUM_FX_H
#define TANDY_STADIUM_FX_H
#define STADIUM_FX_NONE 65535U
/* 60s cadence, 12s of occasional stadium games; original 8-frame
 * FOOTBALLGAME1 (932..939), FOOTBALLGAME2 (940..947) art.
 */
#define STADIUM_FX_PERIOD 1092U
#define STADIUM_FX_LENGTH 216U
#define STADIUM_FX_FRAME_TICKS 8U
unsigned int stadium_fx_frame(unsigned int ticks,unsigned int cx,
                              unsigned int cy,int powered);
int stadium_fx_center(unsigned int x,unsigned int y,
                      unsigned int *cx,unsigned int *cy);
/* Returns STADIUM_FX_NONE for non-stadium tiles, otherwise the
 * correctly-oriented source tile to draw. */
unsigned int stadium_fx_art(unsigned int x,unsigned int y,
                            unsigned int turns,unsigned int ticks);
#endif
