/* SPDX-License-Identifier: GPL-3.0-or-later
 * One visual pointer at a time on the Tandy's 320x200 DOS display.
 * This pure C89 predicate is also host-testable without DOS services.
 */
#ifndef TANDY_POINTER_POLICY_H
#define TANDY_POINTER_POLICY_H
#include "ui.h"

/* The solid 8x8 yellow selection square identifies city tiles.
 * The arrow remains useful for the sidebar, top bar, bottom status,
 * and for menus/help pages overlaid on top of the city.
 */
static int pointer_should_show(unsigned int x, unsigned int y,
                               int menu_open, int info_open)
{
    if (menu_open || info_open)
        return 1;
    return x < UI_MAP_X ||
           x >= UI_MAP_X + UI_MAP_COLS * 8U ||
           y < UI_MAP_Y ||
           y >= UI_MAP_Y + UI_MAP_ROWS * 8U;
}

#endif
