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
#endif
