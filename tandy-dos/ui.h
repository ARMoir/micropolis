/* SPDX-License-Identifier: GPL-3.0-or-later
 * Portable 40-column Tandy menu hit-testing and action definitions.
 */
#ifndef TANDY_UI_H
#define TANDY_UI_H

enum ui_menu {
    MENU_NONE = -1,
    MENU_FILE = 0,
    MENU_TOOLS = 1,
    MENU_VIEW = 2,
    MENU_HELP = 3
};

enum ui_action {
    UI_NOTHING = 0,
    UI_CLOSE,
    UI_SAVE,
    UI_EXIT,
    UI_TOOL_INSPECT,
    UI_TOOL_ROAD,
    UI_TOOL_POWER,
    UI_TOOL_RES,
    UI_TOOL_COM,
    UI_TOOL_IND,
    UI_TOOL_DOZE,
    UI_TOOL_COAL,
    UI_SCAN_POWER,
    UI_CENTER_CURSOR,
    UI_HELP_KEYS,
    UI_HELP_MOUSE,
    UI_HELP_ABOUT
};

/* All x/y arguments are actual 320x200 pixel coordinates.
 * Header text positions align with the top 40-column menu bar.
 */
int ui_header_hit(unsigned int x, unsigned int y);
int ui_toolbar_hit(unsigned int x, unsigned int y);
unsigned int ui_menu_origin(enum ui_menu menu);
unsigned int ui_menu_count(enum ui_menu menu);
int ui_dropdown_hit(enum ui_menu menu, unsigned int x, unsigned int y);
const char *ui_menu_item(enum ui_menu menu, unsigned int index);
enum ui_action ui_menu_command(enum ui_menu menu, unsigned int index);
#endif
