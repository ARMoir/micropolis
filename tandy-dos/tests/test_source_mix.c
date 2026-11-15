/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cross-source RCI growth: complete original/NEWER graphics, non-
 * destructive imported original 3x3, NBL compatibility and demolition.
 */
#include "../city.h"
#include "../zoning.h"
#include "../newer_lot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void flat(void)
{
    unsigned int x,y;
    city_make_demo();
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y)city_set(x,y,0U);
    zoning_clear();
    newer_lot_clear();
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
static void prefab(unsigned int x,unsigned int y,
                   unsigned int base,unsigned int kind)
{
    unsigned int i,j;
    flat();
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i) {
            unsigned short word=(unsigned short)
                (base+j*3U+i+8192U+16384U);
            if(i==1U&&j==1U)word|=1024U;
            city_set(x+i,y+j,word);
        }
    zoning_seed_from_city();
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            assert(zoning_at(x+i,y+j)==kind);
}
static void test_residential_one_tile(void)
{
    unsigned int x,y,stage,old,newer;
    const unsigned char NEWER_FAR *sprite;
    flat();
    assert(newer_lot_has_art());
    /* Approximately half of a region's 1x1 residential homes use
     * original TILES houses and half use NEWERM8. World-coordinate
     * mixing avoids an obvious checkerboard while keeping the art
     * stable through save/load and all map orientations.
     */
    for(stage=1U;stage<=3U;++stage) {
        old=0U;newer=0U;
        for(y=10U;y<20U;++y)
            for(x=10U;x<20U;++x) {
                sprite=newer_lot_residential_1x1(x,y,stage);
                if(sprite!=0) {
                    ++newer;
                    assert(sprite==newer_lot_residential_1x1(
                        x,y,stage));
                } else ++old;
            }
        assert(newer>=35U&&newer<=65U);
        assert(old+newer==100U);
    }
    assert(!newer_lot_residential_1x1(10U,10U,0U));
    assert(!newer_lot_residential_1x1(10U,10U,4U));
}

static void test_new_footprints(unsigned int kind,unsigned int bx)
{
    unsigned int a,b,span,id,wx,wy;
    flat();
    mature(bx,30U,3U,kind);
    assert(newer_lot_try_grow_with_style(bx,30U,kind,1U,
                                          &a,&b)==2U);
    assert(a==bx&&b==30U);
    assert(newer_lot_info(bx,30U,0,0,&span,0,&id));
    assert(span==2U);
    assert(!newer_lot_uses_original(bx,30U));
    /* The legacy 2x2 category does not have a complete TILES asset.
     * Choosing original at 2x2 must still use NEWER, not one cropped
     * center of an original 3x3 commercial/industrial structure.
     */
    assert(newer_lot_try_grow_with_style(bx,30U,kind,1U,
                                          &a,&b)==3U);
    assert(newer_lot_info(bx+2U,32U,0,0,&span,0,&id));
    assert(span==3U);
    assert(newer_lot_uses_original(bx,30U));
    assert(id>=1U&&id<=4U);
    assert(!newer_lot_pixels(bx,30U,0U));
    wx=newer_lot_original_tile(bx,30U,0U);
    assert(wx== (kind==ZONE_RES?261U:
                 kind==ZONE_COM?432U:621U)+9U*(id-1U));
    assert(newer_lot_original_tile(bx,30U,1U)==wx+2U);
    assert(newer_lot_original_tile(bx,30U,2U)==wx+8U);
    assert(newer_lot_original_tile(bx,30U,3U)==wx+6U);
    assert(zoning_save("mixnew.zon"));
    assert(newer_lot_save("mixnew.nbl"));
    newer_lot_clear();
    assert(newer_lot_load("mixnew.nbl"));
    assert(newer_lot_original_tile(bx,30U,0U)==wx);
    assert(newer_lot_uses_original(bx+2U,32U));
    assert(newer_lot_demolish(bx+1U,31U,&a,&b)==3U);
    assert(a==bx&&b==30U);
    for(wy=30U;wy<=32U;++wy)
        for(wx=bx;wx<=bx+2U;++wx) {
            assert(zoning_at(wx,wy)==kind);
            assert(zoning_development_at(wx,wy)==0U);
            assert((city_at(wx,wy)&1023U)==0U);
        }
    assert(remove("mixnew.nbl")==0);
    assert(remove("mixnew.zon")==0);

    /* 50% alternate source: explicitly choose the complete NEWER
     * 3x3 art and preserve that identity across .NBL reload.
     */
    flat();
    mature(bx,30U,3U,kind);
    assert(newer_lot_try_grow_with_style(bx,30U,kind,0U,0,0)==2U);
    assert(newer_lot_try_grow_with_style(bx,30U,kind,0U,0,0)==3U);
    assert(!newer_lot_uses_original(bx,30U));
    assert(newer_lot_pixels(bx,30U,0U)!=0);
}

static void test_imported(unsigned int base,unsigned int kind)
{
    unsigned int i,j,source,x=60U,y=60U;
    unsigned short previous[9];
    prefab(x,y,base,kind);
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            previous[j*3U+i]=city_at(x+i,y+j);
    assert(newer_lot_count()==0U);
    assert(newer_lot_retrofit_original(x+1U,y+1U,kind));
    assert(!newer_lot_retrofit_original(x+1U,y+1U,kind));
    assert(newer_lot_count()==1U);
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i) {
            assert(city_at(x+i,y+j)==previous[j*3U+i]);
            assert(newer_lot_at(x+i,y+j));
            assert(newer_lot_pixels(x+i,y+j,0U)!=0);
        }
    assert(newer_lot_info(x,y,0,0,0,0,&source));
    assert(source>=0xaeU&&source<=0xc1U);
    assert(zoning_save("mixold.zon"));
    assert(newer_lot_save("mixold.nbl"));
    newer_lot_clear();
    zoning_clear();
    assert(zoning_load("mixold.zon"));
    assert(newer_lot_load("mixold.nbl"));
    assert(newer_lot_at(x+2U,y+2U));
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            assert(city_at(x+i,y+j)==previous[j*3U+i]);
    assert(newer_lot_demolish(x+1U,y+1U,0,0)==3U);
    assert(newer_lot_count()==0U);
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i) {
            assert((city_at(x+i,y+j)&1023U)==0U);
            assert(zoning_at(x+i,y+j)==kind);
            assert(zoning_development_at(x+i,y+j)==0U);
        }
    assert(remove("mixold.zon")==0);
    assert(remove("mixold.nbl")==0);

    /* An incomplete original block is NEVER retrofitted: no stray
     * tiles or roads may be swallowed by full-footprint overlays.
     */
    prefab(x,y,base,kind);
    city_set(x+2U,y+2U,66U|8192U);
    assert(!newer_lot_retrofit_original(x+1U,y+1U,kind));
    assert(!newer_lot_at(x,y));
}
int main(void)
{
    assert(newer_lot_load_art(
        "resources/sc2k-buildings/SC2KM8.DAT"));
    test_residential_one_tile();
    test_new_footprints(ZONE_RES,20U);
    test_new_footprints(ZONE_COM,40U);
    test_new_footprints(ZONE_IND,65U);
    test_imported(261U,ZONE_RES);
    test_imported(432U,ZONE_COM);
    test_imported(621U,ZONE_IND);
    newer_lot_free_art();
    assert(!newer_lot_has_art());
    puts("50/50 complete TILES/NEWER RCI style: 1x1 homes, 2x2 fallback, 3x3 migration, original CTY preservation, NBL and demolition passed");
    return 0;
}
