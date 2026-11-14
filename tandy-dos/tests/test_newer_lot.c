/* SPDX-License-Identifier: GPL-3.0-or-later
 * Complete new RCI footprint growth, destruction, saved records and art.
 * Host test; no Tandy BIOS or graphics hardware needed.
 */
#include "../city.h"
#include "../zoning.h"
#include "../editor.h"
#include "../newer_lot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static void flat(void)
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
static void expect_building(unsigned int x,unsigned int y,
                            unsigned int span,unsigned int kind)
{
    unsigned int a,b,w,k,id,i,j;
    for(j=0U;j<span;++j)
        for(i=0U;i<span;++i) {
            assert(newer_lot_info(x+i,y+j,&a,&b,&w,&k,&id));
            assert(a==x && b==y && w==span && k==kind);
            assert(zoning_at(x+i,y+j)==kind);
            assert((city_at(x+i,y+j)&1023U)==0U);
        }
}
static unsigned int first_tile(unsigned int id)
{
    if(id<0x8cU)return id-0x70U;
    if(id<0xaeU)return 28U+4U*(id-0x8cU);
    return 164U+9U*(id-0xaeU);
}
static void check_art(unsigned int x,unsigned int y,
                      unsigned int span)
{
    unsigned int ax,ay,s,k,id,turn,index;
    unsigned char disk[32];
    FILE *f;
    assert(newer_lot_info(x,y,&ax,&ay,&s,&k,&id));
    assert(s==span);
    f=fopen("resources/sc2k-buildings/SC2KM8.DAT","rb");
    assert(f!=NULL);
    for(turn=0U;turn<4U;++turn) {
        const unsigned char *p=newer_lot_pixels(x,y,turn);
        unsigned int part=turn==0U?0U:
            turn==1U?span-1U:turn==2U?span*span-1U:
            (span-1U)*span;
        index=first_tile(id)+part;
        assert(p!=NULL);
        assert(fseek(f,(long)index*32L,SEEK_SET)==0);
        assert(fread(disk,1U,32U,f)==32U);
        assert(memcmp(p,disk,32U)==0);
    }
    assert(fclose(f)==0);
}
int main(void)
{
    unsigned int x,y,w,k,id,span;
    FILE *f;
    flat();
    assert(newer_lot_load_art("resources/sc2k-buildings/SC2KM8.DAT"));
    assert(newer_lot_has_art());
    /* No one-square crop of an original large structure: group must
     * be mature, same zone and clear of protected roads/wires.
     */
    for(x=30U;x<=31U;++x)
        for(y=30U;y<=31U;++y) {
            assert(zoning_set(x,y,ZONE_RES));
            assert(zoning_set_development(x,y,2U));
        }
    assert(newer_lot_try_grow(30U,30U,ZONE_RES,0,0)==0U);
    assert(zoning_set_development(30U,30U,3U));
    assert(zoning_set_development(31U,30U,3U));
    assert(zoning_set_development(30U,31U,3U));
    assert(zoning_set_development(31U,31U,3U));
    assert(newer_lot_try_grow(30U,30U,ZONE_RES,&x,&y)==2U);
    assert(x==30U && y==30U);
    expect_building(30U,30U,2U,ZONE_RES);
    assert(newer_lot_count()==1U);
    check_art(30U,30U,2U);
    assert(newer_lot_try_grow(30U,30U,ZONE_RES,0,0)==0U);
    /* Add the missing five surrounding lots; 2x2 evolves in-place
     * into a real 3x3 building with complete 9-piece artwork.
     */
    for(y=30U;y<=32U;++y)
        for(x=30U;x<=32U;++x)
            if(zoning_at(x,y)==ZONE_NONE) {
                assert(zoning_set(x,y,ZONE_RES));
                assert(zoning_set_development(x,y,3U));
            }
    assert(newer_lot_try_grow(30U,30U,ZONE_RES,&x,&y)==3U);
    assert(x==30U && y==30U);
    expect_building(30U,30U,3U,ZONE_RES);
    assert(newer_lot_count()==1U);
    check_art(30U,30U,3U);

    mature(48U,30U,3U,ZONE_COM);
    assert(newer_lot_try_grow(48U,30U,ZONE_COM,0,0)==2U);
    assert(newer_lot_try_grow(48U,30U,ZONE_COM,0,0)==3U);
    expect_building(48U,30U,3U,ZONE_COM);
    mature(65U,30U,3U,ZONE_IND);
    assert(newer_lot_try_grow(65U,30U,ZONE_IND,0,0)==2U);
    assert(newer_lot_try_grow(65U,30U,ZONE_IND,0,0)==3U);
    expect_building(65U,30U,3U,ZONE_IND);
    assert(newer_lot_count()==3U);

    /* A genuine obstacle invalidates the build; a mixed zone cannot
     * be consumed in the process.
     */
    mature(80U,30U,2U,ZONE_COM);
    city_set(81U,31U,210U|16384U);
    assert(newer_lot_try_grow(80U,30U,ZONE_COM,0,0)==0U);
    assert(newer_lot_count()==3U);

    assert(zoning_save("testnew.zon"));
    assert(newer_lot_save("testnew.nbl"));
    newer_lot_clear();
    zoning_clear();
    assert(zoning_load("testnew.zon"));
    assert(newer_lot_load("testnew.nbl"));
    assert(newer_lot_count()==3U);
    expect_building(48U,30U,3U,ZONE_COM);
    expect_building(65U,30U,3U,ZONE_IND);
    /* Bad data cannot destroy an existing valid populated overlay. */
    f=fopen("testnew.nbl","r+b");
    assert(f!=NULL);
    assert(fseek(f,10L,SEEK_SET)==0);
    assert(fputc(255,f)!=EOF);
    assert(fclose(f)==0);
    assert(!newer_lot_load("testnew.nbl"));
    assert(newer_lot_count()==3U);
    assert(newer_lot_info(49U,31U,&x,&y,&span,&k,&id));
    assert(span==3U && k==ZONE_COM);
    assert(remove("testnew.nbl")==0);
    assert(remove("testnew.zon")==0);

    /* Demolition from any sub-tile clears the WHOLE graphic, leaving
     * every individual zoned lot VACANT, not deleting its designation.
     */
    assert(edit_place(EDIT_BULLDOZE,49U,31U));
    assert(newer_lot_count()==2U);
    for(y=30U;y<=32U;++y)
        for(x=48U;x<=50U;++x) {
            assert(zoning_at(x,y)==ZONE_COM);
            assert(zoning_development_at(x,y)==0U);
            assert(!newer_lot_at(x,y));
        }
    /* An explicit right-button unzone removes the entire overlay,
     * then removes only the chosen lot's zone assignment.
     */
    assert(zoning_set(66U,31U,ZONE_NONE));
    assert(!newer_lot_at(65U,30U));
    assert(newer_lot_count()==1U);
    assert(zoning_at(65U,30U)==ZONE_IND);
    assert(zoning_development_at(65U,30U)==0U);
    assert(zoning_at(66U,31U)==ZONE_NONE);

    assert(!newer_lot_at(CITY_WIDTH,0U));
    assert(!newer_lot_at(0U,CITY_HEIGHT));
    newer_lot_free_art();
    assert(!newer_lot_has_art());
    puts("NewerSource R/C/I 2x2->3x3 growth, rotation, persistence and demolition passed");
    return 0;
}
