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
/* Neighboring same-kind buildings/empty parcels form a district.
 * Keep exterior R/C/I colored, but turn every SHARED 1x1 lot border
 * road gray. Original/NewerSource multi-tile building interiors stay
 * completely unmarked. The facing shared edge is symmetric.
 */
static void test_shared_local_roads(void)
{
    unsigned int kind,x,y,edges,shared,t,hatched;
    int hatch;
    for(kind=ZONE_RES;kind<=ZONE_IND;++kind) {
        flat_city();
        x=25U+kind*12U;
        y=22U;
        /* A 3x3 square of VACANT 1x1 zoned lots has a road-gray
         * shared interior grid, but full colored exterior outline.
         */
        {
            unsigned int i,j;
            for(j=0U;j<3U;++j)
                for(i=0U;i<3U;++i)
                    assert(zoning_set(x+i,y+j,kind));
        }
        edges=zone_highlight_style(x+1U,y+1U,&hatch);
        assert(edges==15U && hatch);
        assert(zone_highlight_shared_edges(x+1U,y+1U,edges)==15U);
        edges=zone_highlight_style(x,y,&hatch);
        assert(edges==15U && hatch);
        assert(zone_highlight_shared_edges(x,y,edges)==6U);
        edges=zone_highlight_style(x+1U,y,&hatch);
        assert(zone_highlight_shared_edges(x+1U,y,edges)==14U);
        edges=zone_highlight_style(x+2U,y+2U,&hatch);
        assert(zone_highlight_shared_edges(x+2U,y+2U,edges)==9U);
        /* The actual road next to a district is NOT an internal
         * zoning border. Its facing zone edge stays R/C/I colored.
         */
        city_set(x+1U,y-1U,66U);
        assert((zone_highlight_shared_edges(x+1U,y,15U)&1U)==0U);
        assert((zone_highlight_shared_edges(x+1U,y+1U,15U)&1U)!=0U);
        for(t=0U;t<4U;++t) {
            /* Both masks transform with the same camera direction. */
            shared=zone_highlight_shared_edges(x+1U,y,15U);
            assert((view_rotation_edges(t,shared) &
                    view_rotation_edges(t,15U))==
                   view_rotation_edges(t,shared));
        }
        /* Development stage changes hatch but not street geometry. */
        for(hatched=1U;hatched<=3U;++hatched) {
            assert(zoning_set_development(x+1U,y+1U,hatched));
            assert(zone_highlight_style(x+1U,y+1U,&hatch)==15U);
            assert(!hatch);
            assert(zone_highlight_shared_edges(x+1U,y+1U,15U)==15U);
        }
        /* Erase one plot: its four adjoining boundary colors turn
         * back to exterior zoning. Existing partners must redraw.
         */
        assert(zoning_set(x+1U,y,ZONE_NONE));
        assert((zone_highlight_shared_edges(x+1U,y+1U,15U)&1U)==0U);
        assert((zone_highlight_shared_edges(x,y,15U)&2U)==0U);
        assert((zone_highlight_shared_edges(x+2U,y,15U)&8U)==0U);
        assert(zoning_set(x+1U,y,kind));
        assert((zone_highlight_shared_edges(x,y,15U)&2U)!=0U);
    }

    /* A complete NEWER 2x2 or 3x3 never gets inner-road grid lines.
     * Its exterior edge becomes gray only if a DIFFERENT structure
     * or independent same-zone lot actually adjoins it.
     */
    flat_city();
    mature(42U,52U,2U,ZONE_IND);
    assert(newer_lot_try_grow(42U,52U,ZONE_IND,0,0)==2U);
    assert(zone_highlight_style(42U,52U,0)==9U);
    assert(zone_highlight_shared_edges(42U,52U,9U)==0U);
    assert(zone_highlight_style(43U,52U,0)==3U);
    assert(zone_highlight_shared_edges(43U,52U,3U)==0U);
    assert(zone_highlight_style(43U,53U,0)==6U);
    assert(zone_highlight_shared_edges(43U,53U,6U)==0U);
    assert(zoning_set(44U,52U,ZONE_IND));
    assert((zone_highlight_shared_edges(43U,52U,3U)&2U)==2U);
    assert((zone_highlight_shared_edges(44U,52U,15U)&8U)==8U);
    /* Maturing the right/bottom lots permits the 3x3 upgrade. */
    {
        unsigned int i,j;
        for(j=0U;j<3U;++j)
            for(i=0U;i<3U;++i)
                if(zoning_at(42U+i,52U+j)==ZONE_NONE)
                    assert(zoning_set(42U+i,52U+j,ZONE_IND));
        for(j=0U;j<3U;++j)
            for(i=0U;i<3U;++i)
                assert(zoning_set_development(42U+i,52U+j,3U) ||
                       zoning_development_at(42U+i,52U+j)==3U);
    }
    assert(newer_lot_try_grow(42U,52U,ZONE_IND,0,0)==3U);
    assert(zone_highlight_style(43U,53U,0)==0U);
    assert(zone_highlight_shared_edges(43U,53U,0U)==0U);
    assert(zone_highlight_style(44U,52U,0)==3U);
    assert(zone_highlight_shared_edges(44U,52U,3U)==0U);
    assert(zoning_set(45U,52U,ZONE_IND));
    assert((zone_highlight_shared_edges(44U,52U,3U)&2U)!=0U);
    assert((zone_highlight_shared_edges(45U,52U,15U)&8U)!=0U);
    /* Whole-building demolition returns separate vacancy grid. */
    assert(newer_lot_demolish(43U,53U,0,0)==3U);
    assert(zone_highlight_style(43U,53U,&hatch)==15U && hatch);
    assert(zone_highlight_shared_edges(43U,53U,15U)==15U);

    /* The same policy applies to an intact original 3x3 RCI prefab:
     * no gray roads through its artwork, but gray on an adjoining
     * separately zoned lot where both perimeters meet.
     */
    flat_city();
    for(y=70U;y<73U;++y)
        for(x=60U;x<63U;++x) {
            unsigned int id=612U+(y-70U)*3U+(x-60U);
            city_set(x,y,(unsigned short)
                (id|((x==61U&&y==71U)?1024U:0U)));
        }
    zoning_seed_from_city();
    assert(zone_highlight_style(61U,71U,0)==0U);
    assert(zone_highlight_shared_edges(61U,71U,15U)==0U);
    assert(zone_highlight_style(62U,71U,0)==2U);
    assert(zoning_set(63U,71U,ZONE_IND));
    assert(zone_highlight_shared_edges(62U,71U,2U)==2U);
    assert(zone_highlight_shared_edges(63U,71U,15U)==8U);
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
    test_shared_local_roads();
    puts("Shared R/C/I lot boundaries become local asphalt-gray lines; district edges and complete buildings preserved"); 
    puts("1x1 hatching, evolving 1x1->2x2->3x3 perimeters, classic 3x3, rotation and demolition passed");
    return 0;
}
