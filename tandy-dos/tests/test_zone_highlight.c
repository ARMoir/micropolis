/* SPDX-License-Identifier: GPL-3.0-or-later
 * Evolving visual footprint regression: 1x1, 2x2, 3x3 and original
 * Micropolis 3x3 groups, all four rotations, removal and fallback.
 */
#include "../city.h"
#include "../zoning.h"
#include "../newer_lot.h"
#include "../zone_highlight.h"
#include "../view_rotation.h"
#include <assert.h>
#include <stdio.h>

static void flat_city(void)
{
    unsigned int x,y;
    city_make_demo();
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y)
            city_set(x,y,0U);
    newer_lot_clear();
    zoning_clear();
}
static void mature(unsigned int x,unsigned int y,
                   unsigned int span,unsigned int kind)
{
    unsigned int i,j;
    for(j=0U;j<span;++j)
        for(i=0U;i<span;++i) {
            assert(zoning_set(x+i,y+j,kind));
            assert(zoning_set_development(x+i,y+j,3U));
        }
}
static void test_footprint(unsigned int x,unsigned int y,
                           unsigned int span)
{
    unsigned int i,j,t,mask,part,rotated,expected,has;
    int hatched;
    for(j=0U;j<span;++j)
        for(i=0U;i<span;++i) {
            mask=zone_highlight_footprint_edges(
                x+i,y+j,x,y,span);
            assert(mask!=0U || (span==3U && i==1U && j==1U));
            assert(zone_highlight_style(x+i,y+j,&hatched)==mask);
            assert(!hatched);
            for(t=0U;t<4U;++t) {
                part=j*span+i;
                rotated=view_rotation_upright_part(t,span,part);
                expected=zone_highlight_footprint_edges(
                    rotated%span,rotated/span,0U,0U,span);
                has=view_rotation_edges(t,mask);
                assert(has==expected);
            }
        }
}
static void test_classic(unsigned int x,unsigned int y,
                         unsigned int base,unsigned int kind)
{
    unsigned int i,j,id;
    int hatch;
    flat_city();
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i) {
            id=base+j*3U+i;
            if(i==1U && j==1U) id|=1024U;
            city_set(x+i,y+j,(unsigned short)id);
        }
    zoning_seed_from_city();
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            assert(zoning_at(x+i,y+j)==kind);
    test_footprint(x,y,3U);
    /* A false center flag must not make unowned fragments get borders. */
    id=city_at(x+1U,y+1U)&1023U;
    city_set(x+1U,y+1U,(unsigned short)id);
    assert(zone_highlight_style(x,y,&hatch)==0U);
}
int main(void)
{
    unsigned int kind,x,y,j,i,mask,stage,total=0U;
    int hatch;

    /* The nine interior stipple pixels never touch the perimeter. */
    for(y=0U;y<8U;++y)
        for(x=0U;x<8U;++x) {
            mask=zone_highlight_hatch_row(y);
            if(mask&(1U<<x)) {
                assert(y==1U || y==3U || y==5U);
                assert(x>=1U && x<=6U);
                ++total;
            }
        }
    assert(total==9U);
    assert(zone_highlight_footprint_edges(10U,10U,10U,10U,1U)==15U);
    assert(zone_highlight_footprint_edges(10U,10U,11U,10U,2U)==0U);
    assert(zone_highlight_footprint_edges(10U,10U,10U,10U,0U)==0U);

    flat_city();
    for(kind=ZONE_RES;kind<=ZONE_IND;++kind) {
        x=10U+kind*10U;
        y=15U;
        assert(zoning_set(x,y,kind));
        assert(zone_highlight_style(x,y,&hatch)==15U && hatch);
        /* Each developed stage retains the per-1x1 outline, with
         * no hatching over its actual 8x8 building art.
         */
        for(stage=1U;stage<=3U;++stage) {
            assert(zoning_set_development(x,y,stage));
            assert(zone_highlight_style(x,y,&hatch)==15U);
            assert(!hatch);
        }
        /* Rezoning the developed lot back to vacant resets its hatch. */
        assert(zoning_set_development(x,y,0U));
        assert(zone_highlight_style(x,y,&hatch)==15U && hatch);

        x=20U+kind*23U;
        y=32U;
        mature(x,y,2U,kind);
        assert(newer_lot_try_grow(x,y,kind,0,0)==2U);
        test_footprint(x,y,2U);
        /* Complete a 3x3 region; the old 2x2 internal outlines must
         * disappear when the footprint grows to 3x3.
         */
        for(j=0U;j<3U;++j)
            for(i=0U;i<3U;++i)
                if(zoning_at(x+i,y+j)==ZONE_NONE) {
                    assert(zoning_set(x+i,y+j,kind));
                    assert(zoning_set_development(x+i,y+j,3U));
                }
        assert(newer_lot_try_grow(x,y,kind,0,0)==3U);
        test_footprint(x,y,3U);
        assert(zone_highlight_style(x+1U,y+1U,&hatch)==0U);
        assert(!hatch);

        /* Demolishing ANY part restores the whole set of vacant
         * 1x1 outlines/hatches without altering zone assignments.
         */
        assert(newer_lot_demolish(x+1U,y+1U,0,0)==3U);
        for(j=0U;j<3U;++j)
            for(i=0U;i<3U;++i) {
                assert(zoning_at(x+i,y+j)==kind);
                assert(zoning_development_at(x+i,y+j)==0U);
                assert(zone_highlight_style(x+i,y+j,&hatch)==15U);
                assert(hatch);
            }
    }

    /* Genuine original 3x3 artwork has one perimeter too; never
     * draw internal yellow/green/blue lines through original roofs.
     */
    test_classic(30U,60U,240U,ZONE_RES);
    test_classic(30U,60U,261U,ZONE_RES);
    test_classic(30U,60U,423U,ZONE_COM);
    test_classic(30U,60U,432U,ZONE_COM);
    test_classic(30U,60U,612U,ZONE_IND);
    test_classic(30U,60U,621U,ZONE_IND);
    flat_city();
    assert(zone_highlight_style(0U,0U,&hatch)==0U && !hatch);
    assert(zone_highlight_style(CITY_WIDTH,0U,&hatch)==0U);
    assert(zone_highlight_style(0U,CITY_HEIGHT,&hatch)==0U);
    puts("1x1 hatching, evolving 1x1->2x2->3x3 perimeters, classic 3x3, rotation and demolition passed");
    return 0;
}
