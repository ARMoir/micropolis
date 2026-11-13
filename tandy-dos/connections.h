/* SPDX-License-Identifier: GPL-3.0-or-later
 * Road and power line autotiling adapted from original Micropolis w_con.c.
 */
#ifndef TANDY_CONNECTIONS_H
#define TANDY_CONNECTIONS_H

/* Recalculate the selected map tile and up to four adjacent road/wire tiles. */
void connections_fix_cross(unsigned int x, unsigned int y);

#endif
