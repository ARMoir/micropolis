/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ui.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(ui_header_hit(12U * 8U, 3U) == MENU_FILE);
    assert(ui_header_hit(19U * 8U, 3U) == MENU_TOOLS);
    assert(ui_header_hit(26U * 8U, 3U) == MENU_VIEW);
    assert(ui_header_hit(34U * 8U, 3U) == MENU_HELP);
    assert(ui_header_hit(0U, 4U) == MENU_NONE);
    assert(ui_header_hit(320U, 0U) == MENU_NONE);
    assert(ui_toolbar_hit(0U, 8U) == 0);
    assert(ui_toolbar_hit(37U * 8U, 12U) == 7);
    assert(ui_toolbar_hit(37U * 8U, 16U) == -1);
    assert(ui_menu_count(MENU_TOOLS) == 9U);
    assert(ui_dropdown_hit(MENU_TOOLS, 18U * 8U, 3U * 8U) == 1);
    assert(ui_dropdown_hit(MENU_TOOLS, 18U * 8U, 11U * 8U) == -1);
    assert(ui_menu_command(MENU_TOOLS, 1U) == UI_TOOL_ROAD);
    assert(ui_menu_command(MENU_TOOLS, 7U) == UI_TOOL_COAL);
    assert(ui_menu_command(MENU_FILE, 1U) == UI_EXIT);
    assert(ui_menu_command(MENU_VIEW, 0U) == UI_SCAN_POWER);
    assert(ui_menu_command(MENU_HELP, 1U) == UI_HELP_MOUSE);
    assert(ui_menu_command(MENU_FILE, 20U) == UI_NOTHING);
    puts("Tandy menu hit-testing and dispatch tests passed");
    return 0;
}
