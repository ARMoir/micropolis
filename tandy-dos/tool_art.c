/* SPDX-License-Identifier: GPL-3.0-or-later
 * Five recognizable 16x16 toolbar pictograms in Tandy's native 16 colors.
 * A city TILE is only a tiny 8x8 piece of a larger house or factory;
 * enlarging that arbitrary tile as a toolbox icon looked like nonsense.
 * These are original toolbar-only pixel drawings, never city sprites.
 */
#include "tool_art.h"

/* '.' is transparent. Other pixels use hexadecimal Tandy palette IDs.
 * Order: power wire, residence, commercial, industry, coal plant.
 * The pictures are drawn only when selecting tools or redrawing the UI.
 */
static const char icon[5][16][17] = {
    { /* Lightning bolt: POWER */
      "................",
      ".........00.....",
      "........0E0B....",
      ".......0EE0.B...",
      "..B...0EEE0.....",
      "..B..0EEEE0.....",
      "....0EEEE0000...",
      "...0EEEEEEEE0...",
      "...00000EEE0....",
      ".......0EE0..B..",
      "......0EE0...B..",
      ".....0EE0.......",
      "....0EE0........",
      "...0EE0.........",
      "...000..........",
      "................"
    },
    { /* Red roof, white walls and green grass: RESIDENTIAL */
      "................",
      ".......00.......",
      "......0CC0.60...",
      ".....0C..C060...",
      "....CC....C00...",
      "...C.CCCCCC.C...",
      "..C..CCCCCC..C..",
      ".0..0FFFFFF0..0.",
      "....0FFFFFF0....",
      "....099FF990....",
      "....099F6690....",
      "....0FFF6EF0....",
      "....000000000...",
      "..AAAAAAAAAAAA..",
      "..AAAAAAAAAAAA..",
      "................"
    },
    { /* Blue glass office/shop with a ground-floor door: COMMERCIAL */
      "................",
      "................",
      "...FFFFFFFFFF...",
      "...0111111110...",
      "...0111111110...",
      "...09BB9BB990...",
      "...09BB9BB990...",
      "...0999999990...",
      "...09BB9BB990...",
      "...09BB9BB990...",
      "...FFFFFFFFFF...",
      "...1BBB10B011...",
      "...0BBB90B090...",
      "..888888888888..",
      "..888888888888..",
      "................"
    },
    { /* Gray factory and golden windows: INDUSTRIAL */
      ".....7.....8....",
      "....8.......F...",
      "..........000...",
      "...060....060...",
      "...060....060...",
      "...0600...060...",
      "...060.0..060...",
      "...060..0.060...",
      "..000000000000..",
      "..077777777770..",
      "..07EE7EE7EE70..",
      "..07EE7EE7EE70..",
      "..077777777770..",
      "..666666666666..",
      "..666666666666..",
      "................"
    },
    { /* Red coal-fired plant, brown stack, yellow power motif: COAL */
      "...........7....",
      "..8.........8...",
      "...7.....0000...",
      ".........0880...",
      "...080...0880...",
      "...080...0880...",
      "...080...0880...",
      "...080...0880...",
      "..066666666660..",
      "..0CCCCCCCCCC0..",
      "..04440EE04440..",
      "..04E40E004E40..",
      "..044400E04440..",
      "..CCCCCCCCCCCC..",
      "..444444444444..",
      "................"
    }
};

static unsigned int art_index(unsigned int tool)
{
    switch (tool) {
    case 2U: return 0U;
    case 3U: return 1U;
    case 4U: return 2U;
    case 5U: return 3U;
    case 7U: return 4U;
    default: return 255U;
    }
}

int tool_art_has_icon(unsigned int tool)
{
    return art_index(tool) != 255U;
}

unsigned int tool_art_pixel(unsigned int tool, unsigned int x,
                            unsigned int y)
{
    unsigned int which = art_index(tool);
    unsigned char c;
    if (which == 255U || x >= 16U || y >= 16U)
        return TOOL_ART_TRANSPARENT;
    c = (unsigned char)icon[which][y][x];
    if (c == '.') return TOOL_ART_TRANSPARENT;
    if (c >= '0' && c <= '9') return (unsigned int)(c - '0');
    if (c >= 'A' && c <= 'F') return (unsigned int)(c - 'A' + 10U);
    return TOOL_ART_TRANSPARENT;
}
