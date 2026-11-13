/* SPDX-License-Identifier: GPL-3.0-or-later
 * Tiny native graphics font for the Tandy top menu and sidebar headings.
 * Only the required uppercase letters are stored. 5x7 cells inside
 * existing 8x8 screen-character positions; no graphics mode changes.
 */
#ifndef TANDY_MENU_FONT_H
#define TANDY_MENU_FONT_H

/* Returns a 5-bit glyph row for SYSTEM / TOOLS / VIEW / HELP / MAP,
 * or zero for a space/unsupported character. Row range 0..6.
 * Bit 4 is the leftmost pixel, bit 0 the rightmost pixel.
 */
unsigned int menu_font_row(unsigned char ch, unsigned int row);

#endif
