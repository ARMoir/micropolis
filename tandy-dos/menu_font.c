/* SPDX-License-Identifier: GPL-3.0-or-later
 * Compact 5x7 uppercase strokes, drawn directly over existing Tandy
 * 320x200x16 pixels. We specifically don't call BIOS AH=09, which
 * paints an opaque black character background in graphics mode.
 *
 * With the previous AH=09 text call, the 29-character menu erased
 * x=0..231 of the cyan title strip. This font writes ONLY ink pixels;
 * the complete 320-pixel cyan row therefore remains visible.
 */
#include "menu_font.h"

/* Uppercase glyphs also serve the two narrow sidebar headings:
 * TOOLS / MAP. Thirteen glyphs use only 91 bytes.
 */
static const unsigned char font[13][7] = {
    {14,17,17,31,17,17,17}, /* A */
    {31,16,16,30,16,16,31}, /* E */
    {17,17,17,31,17,17,17}, /* H */
    {31, 4, 4, 4, 4, 4,31}, /* I */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {15,16,16,14, 1, 1,30}, /* S */
    {31, 4, 4, 4, 4, 4, 4}, /* T */
    {17,17,17,17,17,10, 4}, /* V */
    {17,17,17,21,21,21,10}, /* W */
    {17,17,10, 4, 4, 4, 4}  /* Y */
};

unsigned int menu_font_row(unsigned char ch, unsigned int row)
{
    static const char letters[] = "AEHILMOPSTVWY";
    unsigned int i;
    if (row >= 7U)
        return 0U;
    for (i = 0U; i < 13U; ++i)
        if (ch == (unsigned char)letters[i])
            return font[i][row];
    return 0U;
}
