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
    puts("120x100 rotation, zone edges, viewport bounds and map integrity passed");
    return 0;
}
