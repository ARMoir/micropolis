/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transactional drag-to-build preview (no city changes until release).
 */
#ifndef TANDY_PLACEMENT_H
#define TANDY_PLACEMENT_H
#include "editor.h"

#define PREVIEW_NONE     0U
#define PREVIEW_FUNDED   1U
#define PREVIEW_NO_FUNDS 2U
#define PREVIEW_BLOCKED  3U

void placement_begin(enum edit_tool tool);
unsigned int placement_extend(unsigned int x0, unsigned int y0,
                              unsigned int x1, unsigned int y1);
unsigned int placement_commit(void);
void placement_cancel(void);
int placement_active(void);
unsigned int placement_at(unsigned int x, unsigned int y);
unsigned int placement_count(void);
unsigned int placement_rejected(void);
long placement_cost(void);
long placement_remaining(void);
void placement_bounds(unsigned int *x0, unsigned int *y0,
                      unsigned int *x1, unsigned int *y1);
long placement_unit_cost(enum edit_tool tool);
/* Newly touched preview squares from the LAST extend only. This list
 * contains at most 221 world indices, even if the mouse skipped tiles.
 */
unsigned int placement_new_count(void);
void placement_new_cell(unsigned int entry, unsigned int *x,
                        unsigned int *y);
/* The previous preview remains readable immediately after commit/cancel
 * for selective cleanup, until the next placement_begin().
 * PREVIEW_FUNDED indicates a square that was actually committed after
 * placement_commit().
 */
unsigned int placement_recent_at(unsigned int x, unsigned int y);
/* True only if the last commit changed electrical conductivity. */
int placement_changed_power(void);

#endif
