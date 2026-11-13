/* SPDX-License-Identifier: GPL-3.0-or-later
 * Proves rotation is view-only and all click/paint/world coordinates
 * round-trip exactly, including the asymmetrical 120x100 city edges.
 */
#include "../city.h"
#include "../view_rotation.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int turn,x,y,vx,vy,rx,ry,left,top,cx,cy;
    const unsigned int w=32U,h=21U;
    city_make_demo();
    for (turn=0U;turn<4U;++turn) {
        assert(view_rotation_width(turn)==((turn&1U)?100U:120U));
        assert(view_rotation_height(turn)==((turn&1U)?120U:100U));
        for (x=0U; x<CITY_WIDTH; ++x)
            for (y=0U;y<CITY_HEIGHT;++y) {
                unsigned short original = city_at(x,y);
                view_rotation_from_world(turn,x,y,&vx,&vy);
                assert(vx<view_rotation_width(turn));
                assert(vy<view_rotation_height(turn));
                view_rotation_to_world(turn,vx,vy,&rx,&ry);
                assert(rx==x && ry==y);
                assert(city_at(x,y)==original); /* no map mutation */
            }
        left=65535U;top=65535U;
        view_rotation_clamp(turn,w,h,&left,&top);
        assert(left==view_rotation_width(turn)-w);
        assert(top==view_rotation_height(turn)-h);
        view_rotation_focus(turn,63U,68U,w,h,
                            &left,&top,&cx,&cy);
        assert(cx<w && cy<h);
        view_rotation_to_world(turn,left+cx,top+cy,&rx,&ry);
        assert(rx==63U && ry==68U);
        view_rotation_focus(turn,0U,0U,w,h,&left,&top,&cx,&cy);
        view_rotation_to_world(turn,left+cx,top+cy,&rx,&ry);
        assert(rx==0U && ry==0U);
        view_rotation_focus(turn,CITY_WIDTH-1U,CITY_HEIGHT-1U,
                            w,h,&left,&top,&cx,&cy);
        view_rotation_to_world(turn,left+cx,top+cy,&rx,&ry);
        assert(rx==CITY_WIDTH-1U && ry==CITY_HEIGHT-1U);
        /* At any rotation a screen-click maps to exactly one tile. */
        left=0U;top=0U;
        for(x=0U;x<w;++x)
            for(y=0U;y<h;++y) {
                view_rotation_to_world(turn,left+x,top+y,&rx,&ry);
                view_rotation_from_world(turn,rx,ry,&vx,&vy);
                assert(vx==x && vy==y);
            }
    }
    /* Cardinal edge permutations: world N becomes screen E at 90 CW. */
    assert(view_rotation_edges(0U,1U)==1U);
    assert(view_rotation_edges(1U,1U)==2U);
    assert(view_rotation_edges(2U,1U)==4U);
    assert(view_rotation_edges(3U,1U)==8U);
    assert(view_rotation_edges(1U,15U)==15U);
    assert(view_rotation_edges(2U,10U)==10U);
    assert(view_rotation_width(4U)==120U);
    assert(view_rotation_width(5U)==100U);
    /* A multi-tile sprite stays visually upright by selecting its
     * matching source piece at the new screen-relative position.
     */
    for(turn=0U;turn<4U;++turn) {
        unsigned int span;
        for(span=3U;span<=4U;++span)
            for(x=0U;x<span*span;++x) {
                unsigned int part=view_rotation_upright_part(turn,span,x);
                unsigned int inverse=(4U-turn)&3U;
                assert(part<span*span);
                assert(view_rotation_upright_part(inverse,span,part)==x);
            }
    }
    assert(view_rotation_upright_part(1U,3U,0U)==2U);
    assert(view_rotation_upright_part(1U,3U,4U)==4U);
    assert(view_rotation_upright_part(1U,4U,0U)==3U);
    /* Grounded in original sim.h and w_con.c, where railway graphics
     * are 224..238, NOT 64..207. The old renderer stopped at 223,
     * leaving straight railway tracks horizontal after a quarter turn.
     */
    assert(view_rotation_utility_tile(1U,64U)==65U);  /* road bridge */
    assert(view_rotation_utility_tile(1U,66U)==67U);  /* road */
    assert(view_rotation_utility_tile(1U,68U)==69U);  /* corner */
    assert(view_rotation_utility_tile(1U,69U)==70U);
    assert(view_rotation_utility_tile(1U,72U)==73U);  /* tee */
    assert(view_rotation_utility_tile(1U,76U)==76U);  /* crossing */
    assert(view_rotation_utility_tile(1U,77U)==78U);  /* road/wire */
    assert(view_rotation_utility_tile(1U,79U)==95U);  /* BRWH/BRWV */
    assert(view_rotation_utility_tile(1U,95U)==79U);
    assert(view_rotation_utility_tile(1U,80U)==81U);  /* light traffic */
    assert(view_rotation_utility_tile(1U,144U)==145U);/* heavy traffic */

    assert(view_rotation_utility_tile(1U,208U)==209U); /* power bridge */
    assert(view_rotation_utility_tile(1U,210U)==211U); /* power wire */
    assert(view_rotation_utility_tile(1U,212U)==213U); /* power corner */
    assert(view_rotation_utility_tile(1U,216U)==217U); /* power tee */
    assert(view_rotation_utility_tile(1U,220U)==220U); /* power cross */
    assert(view_rotation_utility_tile(1U,221U)==222U); /* rail/power */

    assert(view_rotation_utility_tile(1U,224U)==225U); /* rail bridge */
    assert(view_rotation_utility_tile(1U,226U)==227U); /* straight rail */
    assert(view_rotation_utility_tile(1U,228U)==229U); /* rail corner */
    assert(view_rotation_utility_tile(1U,229U)==230U);
    assert(view_rotation_utility_tile(1U,232U)==233U); /* rail tee */
    assert(view_rotation_utility_tile(1U,233U)==234U);
    assert(view_rotation_utility_tile(1U,236U)==236U); /* rail cross */
    assert(view_rotation_utility_tile(1U,237U)==238U); /* road/rail */

    assert(view_rotation_utility_tile(1U,223U)==223U); /* unused */
    assert(view_rotation_utility_tile(1U,239U)==239U); /* legacy */
    assert(view_rotation_utility_tile(2U,245U)==245U); /* house */
    assert(view_rotation_utility_tile(0U,226U)==226U);
    assert(view_rotation_utility_tile(2U,226U)==226U);
    assert(view_rotation_utility_tile(3U,226U)==227U);
    for(x=0U;x<960U;++x) {
        unsigned int id=x;
        for(turn=0U;turn<4U;++turn) {
            unsigned int tile=view_rotation_utility_tile(turn,x);
            /* A rotated road must remain a road, train track a track,
             * wire a wire. Never change the actual city tile.
             */
            if(x>=64U && x<=207U)
                assert(tile>=64U && tile<=207U);
            else if(x>=208U && x<=222U)
                assert(tile>=208U && tile<=222U);
            else if(x>=224U && x<=238U)
                assert(tile>=224U && tile<=238U);
            else
                assert(tile==x);
        }
        for(turn=0U;turn<4U;++turn)
            id=view_rotation_utility_tile(1U,id);
        assert(id==x); /* four 90-degree turns restore all sprites */
    }
    puts("120x100 rotation, zone edges, viewport bounds and map integrity passed");
    return 0;
}
