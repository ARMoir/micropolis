/* SPDX-License-Identifier: GPL-3.0-or-later
 * Regression: industrial highlighting must not cut through original
 * buildings, developed 1x1s or newer complete 2x2/3x3 footprints.
 * Vacant zone edges are still visible for editing.
 */
#include "../zone_highlight.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    unsigned int edges;
    for(edges=0U;edges<16U;++edges) {
        /* Empty/wooded undisturbed sites retain accurate boundaries. */
        assert(zone_highlight_mask(edges,0U,0U)==edges);
        assert(zone_highlight_mask(edges,21U,0U)==edges);
        assert(zone_highlight_mask(edges,43U,0U)==edges);
        /* All developed independent lots, including multi-tile art,
         * have a density stage >=1 and must NEVER be overpainted.
         */
        assert(zone_highlight_mask(edges,0U,1U)==0U);
        assert(zone_highlight_mask(edges,0U,2U)==0U);
        assert(zone_highlight_mask(edges,0U,3U)==0U);
        assert(zone_highlight_mask(edges,37U,3U)==0U);
        /* Original Micropolis 3x3 industrial sprite IDs 612..692
         * and other original completed buildings must be pristine.
         */
        assert(zone_highlight_mask(edges,612U,0U)==0U);
        assert(zone_highlight_mask(edges,625U,0U)==0U);
        assert(zone_highlight_mask(edges,688U,0U)==0U);
        assert(zone_highlight_mask(edges,692U,0U)==0U);
        assert(zone_highlight_mask(edges,240U,0U)==0U);
        assert(zone_highlight_mask(edges,427U,0U)==0U);
        assert(zone_highlight_mask(edges,750U,0U)==0U);
        assert(zone_highlight_mask(edges,66U,0U)==0U);
        assert(zone_highlight_mask(edges,2U,0U)==0U);
    }
    puts("Industrial/RCI zone borders limited to vacant land; original and multi-tile art unobstructed");
    return 0;
}
