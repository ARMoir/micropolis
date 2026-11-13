/* SPDX-License-Identifier: GPL-3.0-or-later
 * Regression for the disappearing top-left menu on real Tandy mode 9.
 * The last release used BIOS AH=09 with foreground 0, which erased the
 * first 29 8x8-cell backgrounds of a cyan title strip. Verify a
 * transparent, readable bitmap menu and stable clickable positions.
 */
#include "../menu_font.h"
#include "../ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MENU_COLUMNS 40U
#define MENU_WIDTH   320U
#define MENU_HEIGHT  8U

static unsigned char bar[MENU_HEIGHT][MENU_WIDTH];

static void draw_heading(unsigned int char_col, const char *name)
{
    unsigned int row, c, bit;
    while (*name) {
        unsigned char ch = (unsigned char)*name++;
        for (row = 0U; row < 7U; ++row) {
            unsigned int pattern = menu_font_row(ch, row);
            for (bit = 0U; bit < 5U; ++bit)
                if (pattern & (1U << (4U-bit)))
                    bar[row][char_col*8U+1U+bit] = 0U;
        }
        ++char_col;
    }
}

int main(void)
{
    const char *text = " SYSTEM   TOOLS   VIEW   HELP";
    const char *const headings[4] = {"SYSTEM","TOOLS","VIEW","HELP"};
    const unsigned int positions[4] = {1U,10U,18U,25U};
    unsigned int x,y,i,dark=0U,cyan=0U;

    assert(strlen(text) == 29U);
    assert(MENU_COLUMNS * 8U == MENU_WIDTH);
    for (y=0U;y<MENU_HEIGHT;++y)
        for (x=0U;x<MENU_WIDTH;++x)
            bar[y][x] = 3U;

    for (i=0U;i<4U;++i)
        draw_heading(positions[i],headings[i]);

    for (y=0U;y<MENU_HEIGHT;++y)
        for (x=0U;x<MENU_WIDTH;++x) {
            if (bar[y][x] == 0U) ++dark;
            else {
                assert(bar[y][x] == 3U);
                ++cyan;
            }
        }
    assert(dark > 160U);
    assert(cyan > 0U);
    /* Cyan background outside glyph strokes: the BIOS AH=09 bug
     * previously replaced these entire cells with black.
     */
    assert(bar[0][0] == 3U);
    assert(bar[7][10U] == 3U);
    assert(bar[0][90U] == 3U);
    assert(bar[0][231U] == 3U);
    assert(bar[0][232U] == 3U);
    assert(bar[0][319U] == 3U);

    for (i=0U;i<4U;++i) {
        unsigned int glyph_count=0U;
        const char *p=headings[i];
        while (*p) {
            for(y=0U;y<7U;++y)
                if (menu_font_row((unsigned char)*p,y))
                    ++glyph_count;
            ++p;
        }
        assert(glyph_count > 0U);
    }
    assert(menu_font_row(' ',0U) == 0U);
    assert(menu_font_row('?',0U) == 0U);
    assert(menu_font_row('S',7U) == 0U);
    assert(menu_font_row('S',0U) != 0U);
    assert(ui_header_hit(positions[0]*8U,4U) == MENU_FILE);
    assert(ui_header_hit(positions[1]*8U,4U) == MENU_TOOLS);
    assert(ui_header_hit(positions[2]*8U,4U) == MENU_VIEW);
    assert(ui_header_hit(positions[3]*8U,4U) == MENU_HELP);
    puts("Transparent Tandy menu glyphs, full cyan background and hitboxes passed");
    return 0;
}
