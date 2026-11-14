/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original Micropolis roadway, railway and power crossings.
 * Separate validation for editor and drag-preview keeps both identical.
 */
#ifndef TANDY_INFRASTRUCTURE_H
#define TANDY_INFRASTRUCTURE_H
#include "editor.h"

/* Returns zero if this tool cannot be built at this location. */
int infrastructure_can_place(enum edit_tool tool,
                             unsigned int x, unsigned int y);
/* Return the intersection tile (0 if not a supported crossing).
 * Source: original Micropolis w_con.c _LayRoad and _LayWire.
 */
unsigned int infrastructure_crossing_id(enum edit_tool tool,
                                        unsigned int existing_id);
/* Original HBRIDGE=64 and VBRIDGE=65 over water tile IDs 2..4.
 * Zero means no compatible existing road/bridge anchor.
 */
int infrastructure_is_water(unsigned int tile_id);
unsigned int infrastructure_road_bridge_id(unsigned int x,unsigned int y);

/* Authentic HRAIL=224/VRAIL=225 bridges over river tiles 2..4. */
unsigned int infrastructure_rail_bridge_id(unsigned int x,unsigned int y);
#endif
