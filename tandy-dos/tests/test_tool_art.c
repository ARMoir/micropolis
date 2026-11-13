/* SPDX-License-Identifier: GPL-3.0-or-later
 * Make sure every on-screen tool has recognizable, nonempty graphics,
 * matches its mouse hitbox, and uses valid Tandy mode-9 palette colors.
 */
#include "../ui.h"
#include "../tool_art.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned int key,slot,x,y,opaque,unique,count[16],pixels[5][256];
    unsigned int icons[5] = {2U,3U,4U,5U,7U};
    unsigned int i,j,different;
    for(key=0U;key<8U;++key) {
        slot=ui_toolbar_button(key);
        assert(slot<8U);
        assert(ui_toolbar_tool(slot)==key);
        assert(ui_toolbar_hit((slot&1U)*32U+10U,
                              24U+(slot>>1)*24U+10U)==(int)slot);
        assert(tool_art_has_icon(key)==(key==2U || key==3U ||
               key==4U || key==5U || key==7U));
    }
    /* Road, bulldozer and inspect must use the old known-good art. */
    assert(!tool_art_has_icon(0U));
    assert(!tool_art_has_icon(1U));
    assert(!tool_art_has_icon(6U));
    assert(tool_art_pixel(1U,8U,8U)==TOOL_ART_TRANSPARENT);
    assert(tool_art_pixel(2U,16U,8U)==TOOL_ART_TRANSPARENT);
    assert(tool_art_pixel(2U,8U,16U)==TOOL_ART_TRANSPARENT);
    assert(tool_art_pixel(99U,0U,0U)==TOOL_ART_TRANSPARENT);
    for(i=0U;i<5U;++i) {
        key=icons[i];
        opaque=unique=0U;
        for(j=0U;j<16U;++j)count[j]=0U;
        for(y=0U;y<16U;++y)
            for(x=0U;x<16U;++x) {
                unsigned int p=tool_art_pixel(key,x,y);
                pixels[i][y*16U+x]=p;
                if (p != TOOL_ART_TRANSPARENT) {
                    assert(p<16U);
                    ++opaque;
                    ++count[p];
                }
            }
        assert(opaque>=55U); /* no almost-empty or one-pixel icons */
        assert(opaque<=240U); /* transparent background survives */
        for(j=0U;j<16U;++j)
            if(count[j])++unique;
        assert(unique>=3U); /* proper multicolor icon, not plain tile */
    }
    for(i=0U;i<5U;++i)
        for(j=i+1U;j<5U;++j) {
            different=0U;
            for(x=0U;x<256U;++x)
                if(pixels[i][x]!=pixels[j][x]) ++different;
            assert(different>80U); /* uniquely identifiable tools */
        }
    /* Tool-specific visual cues: blue glass / green grass / yellow
     * lightning / red power plant can never silently disappear.
     */
    assert(tool_art_pixel(2U,9U,2U)==14U);
    assert(tool_art_pixel(3U,4U,13U)==10U);
    assert(tool_art_pixel(4U,5U,5U)==11U);
    assert(tool_art_pixel(5U,4U,10U)==14U);
    assert(tool_art_pixel(7U,4U,11U)==14U);
    puts("Five distinctive 16x16 Tandy tool icons and mouse mappings passed");
    return 0;
}
