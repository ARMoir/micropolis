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
#endif
