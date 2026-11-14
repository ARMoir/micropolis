/* SPDX-License-Identifier: GPL-3.0-or-later
 * Vacant-zone hatching persists after placement and ALL four edges
 * surround EACH 1x1 tile. Developed art must stay unobstructed.
 */
#include "../zone_highlight.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    unsigned int stage,id,y,x,total=0U;
    for(id=0U;id<1024U;++id) {
        for(stage=0U;stage<=3U;++stage) {
            unsigned int expected=(stage==0U &&
                (id==0U||(id>=21U&&id<=43U)))?15U:0U;
            assert(zone_highlight_mask(id,stage)==expected);
        }
    }
    /* An interior tile in a solid 3x3 industrial district has no
     * external zoning boundary, but it STILL gets all four edges.
     * This applies equally in all four map orientations.
     */
    assert(zone_highlight_mask(0U,0U)==15U);
    assert(zone_highlight_mask(21U,0U)==15U);
    assert(zone_highlight_mask(43U,0U)==15U);
    for(y=0U;y<8U;++y)
        for(x=0U;x<8U;++x) {
            unsigned int mask=zone_highlight_hatch_row(y);
            int hatch=(mask&(1U<<x))!=0U;
            int expected= (y==1U||y==5U) ?
                (x==1U||x==3U||x==5U) :
                (y==3U && (x==2U||x==4U||x==6U));
            assert(hatch==expected);
            if(hatch) {
                assert(x>0U && x<7U && y>0U && y<7U);
                ++total;
            }
        }
    assert(total==9U);
    assert(zone_highlight_hatch_row(0U)==0U);
    assert(zone_highlight_hatch_row(7U)==0U);
    /* All three density stages, every source/composite tile, and all
     * existing original facilities show NONE of the vacant overlay.
     */
    for(stage=1U;stage<=3U;++stage)
        assert(zone_highlight_mask(0U,stage)==0U);
    assert(zone_highlight_mask(612U,0U)==0U);
    assert(zone_highlight_mask(625U,0U)==0U);
    assert(zone_highlight_mask(688U,0U)==0U);
    assert(zone_highlight_mask(750U,0U)==0U);
    assert(zone_highlight_mask(66U,0U)==0U);
    assert(zone_highlight_mask(2U,0U)==0U);
    puts("Persistent nine-pixel vacant-lot hatch, complete per-tile borders and unobstructed developed sprites passed");
    return 0;
}
