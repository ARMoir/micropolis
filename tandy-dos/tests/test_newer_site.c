/* SPDX-License-Identifier: GPL-3.0-or-later
 * NewerSource small driveways: real road adjacency, footprint/rotation
 * geometry and actual source-green preservation in the original
 * checked-in sprite packs. No DOS hardware is required.
 */
#include "../city.h"
#include "../newer_site.h"
#include "../view_rotation.h"
#include "../zoning.h"
#include <assert.h>
#include <stdio.h>

static void blank(void)
{
    unsigned int x,y;
    city_make_demo();
    for(x=0U;x<CITY_WIDTH;++x)
        for(y=0U;y<CITY_HEIGHT;++y)
            city_set(x,y,0U);
}
static void test_roads(void)
{
    blank();
    /* No road: do not invent a disconnected access lane. */
    assert(newer_site_road_side(40U,40U,2U)==0U);
    city_set(40U,42U,226U); /* rail is not a driveway */
    city_set(40U,39U,210U); /* isolated power wire is not a road */
    assert(newer_site_road_side(40U,40U,2U)==0U);
    city_set(40U,42U,66U);
    assert(newer_site_road_side(40U,40U,2U)==4U);
    city_set(42U,40U,66U);
    assert(newer_site_road_side(40U,40U,2U)==4U); /* south ties east */
    city_set(42U,41U,237U); /* real rail-road crossing */
    assert(newer_site_road_side(40U,40U,2U)==2U);
    city_set(40U,42U,0U);
    city_set(42U,40U,0U);
    city_set(42U,41U,0U);
    city_set(39U,40U,66U);
    assert(newer_site_road_side(40U,40U,2U)==8U);
    city_set(39U,40U,0U);
    city_set(40U,39U,66U);
    assert(newer_site_road_side(40U,40U,2U)==1U);
    /* Edge cases may have only two/three available sides. */
    assert(newer_site_road_side(0U,0U,1U)==0U);
    assert(newer_site_road_side(CITY_WIDTH-1U,CITY_HEIGHT-1U,1U)==0U);
    assert(newer_site_road_side(119U,99U,3U)==0U);
    assert(newer_site_road_side(40U,40U,0U)==0U);
    assert(newer_site_road_side(40U,40U,4U)==0U);
}

static void test_geometry(void)
{
    unsigned int kind,span,side,tx,ty,x,y,turn,px,py,rotSide;
    unsigned int original, rotated,expected,part,screen_part,margin;
    const unsigned int sides[4]={1U,2U,4U,8U};
    for(kind=ZONE_RES;kind<=ZONE_IND;++kind)
        for(span=1U;span<=3U;++span)
            for(side=0U;side<4U;++side) {
                margin=span*8U;
                for(y=0U;y<margin;++y)
                    for(x=0U;x<margin;++x) {
                        original=newer_site_marker(
                            kind,span,sides[side],x,y);
                        assert(original==0U || original==7U ||
                               original==8U);
                        for(turn=0U;turn<4U;++turn) {
                            rotSide=view_rotation_edges(
                                turn,sides[side]);
                            /* Rotation of site *geometry*, not the
                             * upright building artwork underneath.
                             */
                            switch(turn) {
                            case 1U: px=margin-1U-y;py=x;break;
                            case 2U:
                                px=margin-1U-x;py=margin-1U-y;break;
                            case 3U:px=y;py=margin-1U-x;break;
                            default:px=x;py=y;break;
                            }
                            rotated=newer_site_marker(
                                kind,span,rotSide,px,py);
                            assert(original==rotated);
                        }
                        tx=x/8U;ty=y/8U;
                        expected=(original!=0U);
                        if(expected) {
                            /* All marks lie on the frontage tile row
                             * or column, never spread over each tile
                             * in a multi-tile building.
                             */
                            assert(newer_site_front_piece(
                                span,sides[side],tx,ty));
                            assert(x>0U && x<margin-1U &&
                                   y>0U && y<margin-1U);
                        }
                        part=ty*span+tx;
                        screen_part=view_rotation_upright_part(
                            0U,span,part);
                        assert(screen_part==part);
                    }
            }
    assert(newer_site_marker(ZONE_IND,1U,0U,4U,5U)==0U);
    assert(newer_site_marker(ZONE_IND,1U,4U,3U,6U)==8U);
    assert(newer_site_marker(ZONE_COM,1U,4U,2U,6U)==8U);
    assert(newer_site_marker(ZONE_RES,1U,4U,3U,6U)==0U);
    assert(newer_site_marker(ZONE_RES,2U,4U,8U,13U)==7U);
    assert(!newer_site_front_piece(3U,4U,0U,0U));
    assert(newer_site_front_piece(3U,4U,1U,2U));
}

static unsigned int read_color(const unsigned char *data,
                               unsigned int x,unsigned int y)
{
    unsigned int b=data[y*4U+x/2U];
    return (x&1U)?b&15U:b>>4U;
}

/* Count actual visible concrete pixels after source-art compositing.
 * The source nibble must be palette-green 2 for ANY mark to survive.
 * This protects the untouched published NEWER1/NEWERM8 sprites.
 */
static unsigned int verify_art(FILE *f,unsigned long start,
                                unsigned int kind,unsigned int span)
{
    unsigned int x,y,tx,ty,site_color,base,changed=0U;
    unsigned char bytes[32];
    for(ty=0U;ty<span;++ty)
        for(tx=0U;tx<span;++tx) {
            assert(fseek(f,(long)(start+(ty*span+tx)*32UL),
                         SEEK_SET)==0);
            assert(fread(bytes,1U,sizeof(bytes),f)==sizeof(bytes));
            for(y=0U;y<8U;++y)
                for(x=0U;x<8U;++x) {
                    site_color=newer_site_marker(
                        kind,span,4U,tx*8U+x,ty*8U+y);
                    base=read_color(bytes,x,y);
                    if(site_color!=0U && base==2U) {
                        assert(newer_site_composite(base,site_color)==
                               site_color);
                        ++changed;
                    } else
                        assert(newer_site_composite(base,site_color)==base);
                }
        }
    return changed;
}

static void test_source_art(void)
{
    unsigned int c;
    FILE *f;
    f=fopen("resources/sc2k-buildings/SC2K.DAT","rb");
    assert(f!=NULL);
    /* S2LT header followed by all twelve original NEWER1 8x8 tiles.
     * Commercial tile 0, industrial tile 8 are real deployed assets.
     */
    c=verify_art(f,8UL,ZONE_COM,1U);
    assert(c>0U);
    c=verify_art(f,8UL+8UL*32UL,ZONE_IND,1U);
    assert(c>0U);
    assert(fclose(f)==0);
    f=fopen("resources/sc2k-buildings/SC2KM8.DAT","rb");
    assert(f!=NULL);
    /* Complete 2x2 and 3x3 RCI sprites: start indexes from
     * catalog FOOTPRINTS.md. Do not overwrite non-green sprite pixels.
     */
    assert(verify_art(f,28UL*32UL,ZONE_RES,2U)>0U);
    assert(verify_art(f,60UL*32UL,ZONE_COM,2U)>0U);
    assert(verify_art(f,100UL*32UL,ZONE_IND,2U)>0U);
    assert(verify_art(f,164UL*32UL,ZONE_RES,3U)>0U);
    assert(verify_art(f,200UL*32UL,ZONE_COM,3U)>0U);
    assert(verify_art(f,290UL*32UL,ZONE_IND,3U)>0U);
    assert(fclose(f)==0);
}

int main(void)
{
    unsigned int color,marker;
    test_roads();
    test_geometry();
    for(color=0U;color<16U;++color)
        for(marker=0U;marker<16U;++marker) {
            unsigned int result=newer_site_composite(color,marker);
            if(color==2U&&(marker==7U||marker==8U))
                assert(result==marker);
            else assert(result==color);
        }
    test_source_art();
    puts("NEWER R/C/I paved access: road-facing, rotation-safe, ground-only source compositing passed");
    return 0;
}
