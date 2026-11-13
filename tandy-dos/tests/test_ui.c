/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ui.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(ui_header_hit(2U * 8U, 3U) == MENU_FILE);
    assert(ui_header_hit(12U * 8U, 3U) == MENU_TOOLS);
    assert(ui_header_hit(20U * 8U, 3U) == MENU_VIEW);
    assert(ui_header_hit(28U * 8U, 3U) == MENU_HELP);
    assert(ui_header_hit(0U, 4U) == MENU_NONE);
    assert(ui_header_hit(320U, 0U) == MENU_NONE);
    assert(ui_toolbar_hit(0U, 8U) == -1);
    assert(ui_toolbar_hit(10U, 30U) == 0);
    assert(ui_toolbar_hit(42U, 30U) == 1);
    assert(ui_toolbar_hit(10U, 55U) == 2);
    assert(ui_toolbar_hit(42U, 55U) == 3);
    assert(ui_toolbar_hit(10U, 106U) == 6);
    assert(ui_toolbar_hit(42U, 106U) == 7);
    assert(ui_toolbar_hit(63U, 119U) == 7);
    assert(ui_toolbar_hit(64U, 106U) == -1);
    assert(ui_toolbar_hit(42U, 120U) == -1);
    assert(ui_toolbar_hit(10U, 140U) == -1);
    assert(ui_menu_count(MENU_TOOLS) == 9U);
    assert(ui_dropdown_hit(MENU_TOOLS, 10U * 8U, 3U * 8U) == 1);
    assert(ui_dropdown_hit(MENU_TOOLS, 10U * 8U, 11U * 8U) == -1);
    assert(ui_dropdown_hit(MENU_TOOLS, 10U * 8U, 1U * 8U) == -1);
    assert(UI_MAP_X + 8U * UI_MAP_COLS == 320U);
    assert(UI_MAP_Y + 8U * UI_MAP_ROWS == UI_FOOTER_Y);
    assert(UI_TOOLBOX_W == UI_MAP_X);
    assert(UI_BUTTON_TOP + UI_BUTTON_SIZE * 4U < UI_MINIMAP_Y);
    assert(UI_MINIMAP_X + UI_MINIMAP_W <= UI_MAP_X);
    assert(UI_MINIMAP_Y + UI_MINIMAP_H <= UI_FOOTER_Y);
    assert(ui_menu_command(MENU_TOOLS, 1U) == UI_TOOL_ROAD);
    assert(ui_menu_command(MENU_TOOLS, 7U) == UI_TOOL_COAL);
    assert(ui_menu_command(MENU_FILE, 1U) == UI_EXIT);
    assert(ui_menu_command(MENU_VIEW, 0U) == UI_SCAN_POWER);
    assert(ui_menu_command(MENU_HELP, 1U) == UI_HELP_MOUSE);
    assert(ui_menu_command(MENU_FILE, 20U) == UI_NOTHING);
    puts("Tandy menu hit-testing and dispatch tests passed");
    return 0;
}
