/* SPDX-License-Identifier: GPL-3.0-or-later
 * Portable 40-column Tandy menu hit-testing and action definitions.
 */
#ifndef TANDY_UI_H
#define TANDY_UI_H

/* Native Tandy 320x200: menus 0..7; city stats 8..15; a narrow
 * 64-pixel toolbox alongside a 32x21-tile city window (y=16..183);
 * diagnostics at y=184..199. Minimap lives under compact tool grid.
 */
#define UI_MENU_HEIGHT  8U
#define UI_MAP_X       64U
#define UI_MAP_Y       16U
#define UI_MAP_COLS    32U
#define UI_MAP_ROWS    21U
#define UI_FOOTER_Y   184U
#define UI_TOOLBOX_W   64U
#define UI_BUTTON_TOP  24U
#define UI_BUTTON_SIZE 24U
#define UI_MINIMAP_X    4U
#define UI_MINIMAP_Y  136U
#define UI_MINIMAP_W   56U
#define UI_MINIMAP_H   44U

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
/* Visual button order differs from stable 0..7 keyboard shortcuts.
 * Keep mouse and keyboard tool selection equivalent.
 */
unsigned int ui_toolbar_tool(unsigned int button);
unsigned int ui_toolbar_button(unsigned int tool_key);
unsigned int ui_menu_origin(enum ui_menu menu);
unsigned int ui_menu_count(enum ui_menu menu);
int ui_dropdown_hit(enum ui_menu menu, unsigned int x, unsigned int y);
const char *ui_menu_item(enum ui_menu menu, unsigned int index);
enum ui_action ui_menu_command(enum ui_menu menu, unsigned int index);
#endif
