/* SPDX-License-Identifier: GPL-3.0-or-later
 * Every possible tile ID has a useful, bounded four-page inspector.
 * Test factual original/NEWER footprints, services, raw bits and
 * non-destructive reads. No BIOS/Tandy display hardware needed.
 */
#include "../tile_inspect.h"
#include "../city.h"
#include "../zoning.h"
#include "../newer_lot.h"
#include "../engine.h"
#include "../simulation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void reset(void)
{
    unsigned int x,y;
    city_make_demo();
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y)city_set(x,y,0U);
    newer_lot_clear();
    zoning_clear();
    simulation_init();
}
static void pages(const struct tile_inspect *info)
{
    unsigned int page,row,i;
    char title[TILE_DIALOG_TEXT_COLUMNS+2U];
    memset(title,0xa5,sizeof(title));
    title[TILE_DIALOG_TEXT_COLUMNS+1U]=0x5a;
    tile_inspect_dialog_title(info,title);
    assert(title[TILE_DIALOG_TEXT_COLUMNS+1U]==0x5a);
    assert(strlen(title)<=TILE_DIALOG_TEXT_COLUMNS);
    for(page=0U;page<TILE_INSPECT_PAGES;++page)
        for(row=0U;row<TILE_INSPECT_ROWS;++row) {
            char line[TILE_INSPECT_LINE+2U];
            char compact[TILE_DIALOG_TEXT_COLUMNS+2U];
            memset(line,0xa5,sizeof(line));
            line[TILE_INSPECT_LINE+1U]=0x5a;
            tile_inspect_line(info,page,row,line);
            assert(line[TILE_INSPECT_LINE+1U]==0x5a);
            assert(strlen(line)<=TILE_INSPECT_LINE);
            for(i=0U;i<strlen(line);++i)
                assert((unsigned char)line[i]>=32U);
            if(row==0U)assert(strstr(line,"PAGE")!=0);
            memset(compact,0xa5,sizeof(compact));
            compact[TILE_DIALOG_TEXT_COLUMNS+1U]=0x5a;
            tile_inspect_dialog_line(info,page,row,compact);
            assert(compact[TILE_DIALOG_TEXT_COLUMNS+1U]==0x5a);
            assert(strlen(compact)<=TILE_DIALOG_TEXT_COLUMNS);
            for(i=0U;i<strlen(compact);++i)
                assert((unsigned char)compact[i]>=32U);
            if(row==0U)assert(strchr(compact,'/')!=0);
        }
}
static void check_type(unsigned int id,const char *name)
{
    assert(strcmp(tile_inspect_kind(id),name)==0);
}
int main(void)
{
    unsigned int id,x,y,i,j,kind;
    struct tile_inspect t;
    char line[TILE_INSPECT_LINE+1U];
    /* Simple flat gray dialog: clickable Done/Next/Prev, plus
     * any other click still advances as previous inspector did.
     */
    /* Do not regress to graphics-mode BIOS BL=0x70; this made
     * black/inverted overlapping glyphs on DOSBox and real Tandy.
     * DATE/POP uses text ink 15 with an explicitly cleared background.
     */
    assert(TILE_DIALOG_TEXT_BG==0U);
    assert(TILE_DIALOG_TEXT_FG==15U);
    /* Frame/preview, button labels and footer now occupy distinct
     * non-overlapping pixel rows with physical CRT breathing room.
     */
    assert(TILE_DIALOG_PREVIEW_Y+TILE_DIALOG_PREVIEW_SIZE+4U==
           TILE_DIALOG_BUTTON_TOP);
    assert(TILE_DIALOG_BUTTON_TOP<
           TILE_DIALOG_BUTTON_LABEL_ROW*8U);
    assert(TILE_DIALOG_BUTTON_LABEL_ROW*8U+8U<
           TILE_DIALOG_BUTTON_TOP+TILE_DIALOG_BUTTON_HEIGHT);
    assert(TILE_DIALOG_BUTTON_TOP+TILE_DIALOG_BUTTON_HEIGHT+4U==
           TILE_DIALOG_FOOTER_ROW*8U);
    assert(TILE_DIALOG_FOOTER_ROW*8U+8U<
           TILE_DIALOG_Y+TILE_DIALOG_H);
    assert(tile_inspect_dialog_click(235U,150U)==3U);
    assert(tile_inspect_dialog_click(303U,155U)==3U);
    assert(tile_inspect_dialog_click(232U,139U)==0U);
    assert(tile_inspect_dialog_click(232U,156U)==0U);
    assert(tile_inspect_dialog_click(304U,150U)==0U);
    assert(tile_inspect_dialog_click(24U,140U)==1U);
    assert(tile_inspect_dialog_click(87U,155U)==1U);
    assert(tile_inspect_dialog_click(112U,140U)==2U);
    assert(tile_inspect_dialog_click(183U,155U)==2U);
    assert(tile_inspect_dialog_click(10U,90U)==0U);
    assert(tile_inspect_preview_scale(1U)==6U);
    assert(tile_inspect_preview_scale(2U)==4U);
    assert(tile_inspect_preview_scale(3U)==2U);
    assert(tile_inspect_preview_scale(4U)==2U);
    assert(tile_inspect_preview_scale(6U)==1U);
    for(i=1U;i<=6U;++i)
        assert(i*8U*tile_inspect_preview_scale(i)<=
               TILE_DIALOG_PREVIEW_SIZE);
    reset();
    /* Exhaust every possible original 10-bit tile value: even unknown
     * IDs have a safe type and always produce complete info pages.
     */
    for(id=0U;id<=1023U;++id) {
        const char *name=tile_inspect_kind(id);
        assert(name!=0&&name[0]!=0);
        city_set(11U,12U,(unsigned short)id);
        tile_inspect_collect(11U,12U,&t);
        assert(t.id==id);
        assert(t.word==id);
        pages(&t);
    }
    check_type(0U,"CLEAR GROUND");
    check_type(2U,"WATER / SHORE");
    check_type(37U,"FOREST / TREES");
    check_type(44U,"RUBBLE");
    check_type(52U,"RADIOACTIVE LAND");
    check_type(56U,"FIRE");
    check_type(64U,"ROAD BRIDGE");
    check_type(66U,"ROAD / TRAFFIC");
    check_type(77U,"ROAD / POWER CROSS");
    check_type(208U,"POWER LINE");
    check_type(221U,"RAIL / POWER CROSS");
    check_type(224U,"RAIL BRIDGE");
    check_type(226U,"RAILWAY");
    check_type(237U,"ROAD / RAIL CROSS");
    check_type(261U,"RESIDENTIAL ART");
    check_type(427U,"COMMERCIAL ART");
    check_type(625U,"INDUSTRIAL ART");
    check_type(693U,"SEAPORT");
    check_type(716U,"AIRPORT");
    check_type(745U,"COAL POWER PLANT");
    check_type(761U,"FIRE STATION");
    check_type(770U,"POLICE STATION");
    check_type(779U,"STADIUM");
    check_type(811U,"NUCLEAR PLANT");
    check_type(932U,"FOOTBALL ANIMATION");
    check_type(1001U,"UNRECOGNIZED TILE ID");

    reset();
    /* World boundaries never dereference a nonexistent neighbor.
     * 1024 is the explicit OUTSIDE sentinel, distinct from dirt 0.
     */
    tile_inspect_collect(0U,0U,&t);
    assert(t.neighbor[0]==1024U&&t.neighbor[3]==1024U);
    tile_inspect_collect(119U,99U,&t);
    assert(t.neighbor[1]==1024U&&t.neighbor[2]==1024U);
    tile_inspect_collect(120U,40U,&t);
    assert(t.span==1U&&t.id==0U);

    reset();
    city_set(15U,20U,66U|8192U|16384U|32768U);
    city_set(15U,19U,226U);
    city_set(16U,20U,208U|16384U);
    tile_inspect_collect(15U,20U,&t);
    assert(t.neighbor[0]==226U);
    assert(t.neighbor[1]==208U);
    assert(t.roads==0U&&t.rails==1U&&t.wires==1U);
    assert(t.word==(66U|8192U|16384U|32768U));
    tile_inspect_line(&t,3U,2U,line);
    assert(strstr(line,"PWRBIT:1")!=0);
    assert(strstr(line,"CONDBIT:1")!=0);
    tile_inspect_line(&t,3U,3U,line);
    assert(strstr(line,"BURNBIT:1")!=0);
    tile_inspect_line(&t,1U,7U,line);
    assert(strstr(line,"RAIL:1")!=0);

    reset();
    for(kind=ZONE_RES;kind<=ZONE_IND;++kind) {
        x=20U+kind*20U; y=30U;
        assert(zoning_set(x,y,kind));
        tile_inspect_collect(x,y,&t);
        assert(t.zone==kind&&t.stage==0U&&t.span==1U);
        tile_inspect_line(&t,2U,3U,line);
        assert(strstr(line,"VACANT")!=0);
        assert(zoning_set_development(x,y,2U));
        tile_inspect_collect(x,y,&t);
        assert(t.stage==2U);
        tile_inspect_line(&t,2U,7U,line);
        assert(strstr(line,"1X1 EST.")!=0);
        for(j=0U;j<2U;++j)
            for(i=0U;i<2U;++i) {
                if(i||j)assert(zoning_set(x+i,y+j,kind));
                assert(zoning_set_development(x+i,y+j,3U));
            }
        assert(newer_lot_try_grow(x,y,kind,0,0)==2U);
        tile_inspect_collect(x+1U,y+1U,&t);
        assert(t.newer&&t.span==2U);
        assert(t.left==x&&t.top==y);
        assert(t.part_x==1U&&t.part_y==1U);
        pages(&t);
        for(j=0U;j<3U;++j)
            for(i=0U;i<3U;++i)
                if(i>=2U||j>=2U) {
                    assert(zoning_set(x+i,y+j,kind));
                    assert(zoning_set_development(x+i,y+j,3U));
                }
        assert(newer_lot_try_grow(x,y,kind,0,0)==3U);
        tile_inspect_collect(x+2U,y+2U,&t);
        assert(t.newer&&t.span==3U&&t.part_x==2U);
        assert(t.part_y==2U);
        assert(tile_inspect_preview_original_id(&t,0U,0U)==65535U);
        pages(&t);
        assert(newer_lot_demolish(x+1U,y+1U,0,0)==3U);
        tile_inspect_collect(x+2U,y+2U,&t);
        assert(!t.newer&&t.span==1U&&t.stage==0U);
    }

    reset();
    /* Recognize every subtile of valid original 3x3, 4x4 and 6x6
     * prefabs through its center ZONEBIT without altering the art.
     */
    {
        static const unsigned int base[6]={
            261U,432U,621U,761U,745U,709U
        };
        static const unsigned int sizes[6]={3U,3U,3U,3U,4U,6U};
        for(kind=0U;kind<6U;++kind) {
            unsigned int span=sizes[kind],id0=base[kind];
            reset();
            for(j=0U;j<span;++j)
                for(i=0U;i<span;++i) {
                    unsigned short word=(unsigned short)
                        (id0+j*span+i);
                    if(i==1U&&j==1U)word|=1024U;
                    city_set(70U+i,40U+j,word);
                }
            zoning_seed_from_city();
            tile_inspect_collect(71U,41U,&t);
            assert(t.original_group&&t.span==span);
            tile_inspect_collect(70U+span-1U,40U+span-1U,&t);
            assert(t.original_group&&t.span==span);
            assert(t.part_x==span-1U&&t.part_y==span-1U);
            for(j=0U;j<span;++j)
                for(i=0U;i<span;++i)
                    assert(tile_inspect_preview_original_id(&t,i,j)==
                           id0+j*span+i);
            assert(tile_inspect_preview_original_id(&t,span,0U)==65535U);
            tile_inspect_line(&t,0U,6U,line);
            assert(strstr(line,"FOOTPRINT")!=0);
            /* A removed center ZONEBIT invalidates all fragments. */
            city_set(71U,41U,(unsigned short)(id0+span+1U));
            tile_inspect_collect(70U,40U,&t);
            assert(!t.original_group);
        }
    }
    puts("Inspector exhaustive 1024 tile types, compact gray dialog, clickable DONE/PREV/NEXT, original/NEWER whole-building preview and 4 pages passed");
    return 0;
}
