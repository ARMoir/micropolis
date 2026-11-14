/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host-side mouse-hold and popup hit regression (no DOS APIs needed).
 */
#include "../tool_picker.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int row;
    assert(tool_picker_button(1) && tool_picker_button(7));
    assert(!tool_picker_button(0) && !tool_picker_button(6));
    assert(!tool_picker_ready(107U,100U));
    assert(tool_picker_ready(108U,100U));
    assert(tool_picker_ready(0U,65528U)); /* 16-bit BIOS tick wraps */
    /* Important VRAM regression: the Road and civic popup begin
     * below y=16, the ordinary main-menu origin. Snapshot and restore
     * must both use these same popup origins, or stale rows survive.
     */
    assert(tool_picker_top_y(1U)==24U);
    assert(tool_picker_top_y(2U)==96U);
    assert(tool_picker_top_y(1U)!=16U &&
           tool_picker_top_y(2U)!=16U);
    assert(tool_picker_hit(1U,63U,24U)==-1);
    assert(tool_picker_hit(1U,64U,24U)==0);
    assert(tool_picker_hit(1U,191U,39U)==1);
    assert(tool_picker_hit(1U,192U,39U)==-1);
    assert(tool_picker_hit(1U,65U,40U)==-1);
    assert(tool_picker_hit(2U,64U,95U)==-1);
    for(row=0U;row<4U;++row)
        assert(tool_picker_hit(2U,64U,96U+row*8U)==(int)row);
    assert(tool_picker_hit(2U,191U,127U)==3);
    assert(tool_picker_hit(2U,64U,128U)==-1);
    assert(tool_picker_choice(1U,0U)==EDIT_ROAD);
    assert(tool_picker_choice(1U,1U)==EDIT_RAIL);
    assert(tool_picker_choice(2U,0U)==EDIT_COAL);
    assert(tool_picker_choice(2U,1U)==EDIT_NUCLEAR);
    assert(tool_picker_choice(2U,2U)==EDIT_POLICE);
    assert(tool_picker_choice(2U,3U)==EDIT_FIRE);
    puts("Road/Rail and civic tool picker hold/hit/selection passed");
    return 0;
}
