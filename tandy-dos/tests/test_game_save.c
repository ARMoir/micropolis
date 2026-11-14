/* SPDX-License-Identifier: GPL-3.0-or-later
 * DOS 8.3 city saving: first save gets a slot, subsequent saves
 * replace same CTY/ZON/NBL with backups; F3/no-argument restores.
 */
#include "../game_save.h"
#include "../city.h"
#include "../zoning.h"
#include "../newer_lot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void filename(unsigned int slot,const char *ext,char *out)
{
    sprintf(out,"CITY%04u.%s",slot,ext);
}
static long size_of(const char *path)
{
    FILE *f=fopen(path,"rb");
    long n;
    if(!f)return -1L;
    assert(fseek(f,0L,SEEK_END)==0);
    n=ftell(f);
    assert(fclose(f)==0);
    return n;
}
static void cleanup(unsigned int slot)
{
    static const char *const exts[]={
        "CTY","ZON","NBL","BCT","BZN","BNB",
        "TCT","TZN","TNB"
    };
    char out[13];
    unsigned int i;
    for(i=0U;i<sizeof(exts)/sizeof(exts[0]);++i){
        filename(slot,exts[i],out);
        assert(remove(out)==0 || size_of(out)<0L);
    }
    (void)remove("LASTCITY.CFG");
    (void)remove("LASTCITY.TMP");
}
static void mature_region(unsigned int x,unsigned int y)
{
    unsigned int i,j;
    for(j=0U;j<2U;++j)
        for(i=0U;i<2U;++i) {
            /* Demo city includes original RCI buildings at x=70,y=70.
             * Clear the test rectangle before placing NEWER overlays.
             */
            city_set(x+i,y+j,0U);
            assert(zoning_set(x+i,y+j,ZONE_IND));
            assert(zoning_set_development(x+i,y+j,3U));
        }
    assert(newer_lot_try_grow(x,y,ZONE_IND,0,0)==2U);
}
static void corrupt_last_marker(void)
{
    FILE *f=fopen("LASTCITY.CFG","r+b");
    assert(f!=NULL);
    assert(fseek(f,7L,SEEK_SET)==0);
    assert(fputc(0,f)!=EOF);
    assert(fclose(f)==0);
}

int main(void)
{
    char cty[13],zon[13],nbl[13],bak[13];
    unsigned int slot=0U,last=0U,parsed=97U;
    FILE *f;
    unsigned char raw[4];
    assert(game_save_parse_slot("CITY0001.CTY",&parsed));
    assert(parsed==1U);
    assert(game_save_parse_slot("city9999.cty",&parsed));
    assert(parsed==9999U);
    assert(!game_save_parse_slot("HAPPISLE.CTY",&parsed));
    assert(!game_save_parse_slot("C:/CITY0001.CTY",&parsed));
    assert(!game_save_parse_slot("CITY0000.CTY",&parsed));
    assert(!game_save_parse_slot("CITY001X.CTY",&parsed));
    assert(!game_save_parse_slot("CITY0001.ZON",&parsed));
    assert(!game_save_parse_slot("CITY0001.CTY",0));

    city_make_demo();
    newer_lot_clear();
    zoning_clear();
    city_set_funds(98765L);
    city_set_time(12240L);
    city_set(8U,8U,66U|8192U);
    mature_region(70U,70U);
    assert(game_save_write(&slot)==GAME_SAVE_OK);
    assert(slot>=1U && slot<=9999U);
    filename(slot,"CTY",cty);
    filename(slot,"ZON",zon);
    filename(slot,"NBL",nbl);
    assert(size_of(cty)==27120L);
    assert(size_of(zon)==6010L);
    assert(size_of(nbl)==17L);
    assert(game_save_last_slot(&last) && last==slot);
    /* Exact original big-endian tile bytes in the 27,120-byte .CTY. */
    f=fopen(cty,"rb");assert(f!=NULL);
    assert(fseek(f,3120L+((long)8U*CITY_HEIGHT+8U)*2L,SEEK_SET)==0);
    assert(fread(raw,1U,2U,f)==2U);
    assert(raw[0]==((66U|8192U)>>8U));
    assert(raw[1]==66U);
    assert(fclose(f)==0);

    /* Change all in-memory data, load from last-save marker, and
     * verify CTY, its historical money/time, ZON density, and NBL
     * complete-building ownership survive together.
     */
    city_set(8U,8U,0U);
    city_set_funds(1L);
    city_set_time(1L);
    zoning_clear();
    newer_lot_clear();
    assert(game_save_load_last(&last) && last==slot);
    assert(city_at(8U,8U)==(66U|8192U));
    assert(city_get_funds()==98765L);
    assert(city_get_time()==12240L);
    assert(zoning_at(70U,70U)==ZONE_IND);
    assert(zoning_development_at(70U,70U)==3U);
    assert(newer_lot_at(71U,71U));
    assert(newer_lot_count()==1U);

    /* Second F2 stays in the same numbered file, keeping a complete
     * previous-version backup instead of generating CITY0002.
     */
    city_set(8U,8U,67U|8192U);
    assert(game_save_write(&slot)==GAME_SAVE_OK);
    filename(slot,"BCT",bak);
    assert(size_of(bak)==27120L);
    filename(slot,"BZN",bak);
    assert(size_of(bak)==6010L);
    filename(slot,"BNB",bak);
    assert(size_of(bak)==17L);
    city_set(8U,8U,0U);
    assert(game_save_load_last(&last));
    assert(last==slot);
    assert(city_at(8U,8U)==(67U|8192U));
    assert(newer_lot_count()==1U);

    /* Marker corruption must not be mistaken for a valid last save.
     * All three actual city components remain unharmed.
     */
    corrupt_last_marker();
    assert(!game_save_last_slot(&last));
    assert(size_of(cty)==27120L);
    assert(size_of(zon)==6010L);
    assert(size_of(nbl)==17L);
    cleanup(slot);
    puts("Tandy save/resume: stable slot, exact CTY bytes, ZON/NBL round-trip, previous backups and marker validation passed");
    return 0;
}
