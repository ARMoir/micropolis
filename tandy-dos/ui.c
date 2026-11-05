/* SPDX-License-Identifier: GPL-3.0-or-later
 * DOS-independent menu model to allow tests before touching actual Tandy VRAM.
 */
#include "ui.h"

static const char *const file_items[] = {
    "Save city  F2", "Exit to DOS", "Close menu"
};
static const enum ui_action file_cmds[] = {
    UI_SAVE, UI_EXIT, UI_CLOSE
};

static const char *const tools_items[] = {
    "Inspect     0", "Road        1", "Power line  2",
    "Residential 3", "Commercial  4", "Industrial  5",
    "Bulldoze    6", "Coal plant  7", "Close menu"
};
static const enum ui_action tools_cmds[] = {
    UI_TOOL_INSPECT, UI_TOOL_ROAD, UI_TOOL_POWER,
    UI_TOOL_RES, UI_TOOL_COM, UI_TOOL_IND,
    UI_TOOL_DOZE, UI_TOOL_COAL, UI_CLOSE
};

static const char *const view_items[] = {
    "Power scan  P", "Center cursor", "Close menu"
};
static const enum ui_action view_cmds[] = {
    UI_SCAN_POWER, UI_CENTER_CURSOR, UI_CLOSE
};

static const char *const help_items[] = {
    "Key controls", "Mouse info", "About port", "Close menu"
};
static const enum ui_action help_cmds[] = {
    UI_HELP_KEYS, UI_HELP_MOUSE, UI_HELP_ABOUT, UI_CLOSE
};

unsigned int ui_menu_origin(enum ui_menu menu)
{
    switch (menu) {
    case MENU_FILE:  return 10U;
    case MENU_TOOLS: return 16U;
    case MENU_VIEW:  return 22U;
    case MENU_HELP:  return 24U;
    default: return 0U;
    }
}

unsigned int ui_menu_count(enum ui_menu menu)
{
    switch (menu) {
    case MENU_FILE: return 3U;
    case MENU_TOOLS: return 9U;
    case MENU_VIEW: return 3U;
    case MENU_HELP: return 4U;
    default: return 0U;
    }
}

/* Row zero: "MICROPOLIS FILE  TOOLS  VIEW  HELP", 40 columns.
 * These header boundaries intentionally include padding for easy clicking.
 */
int ui_header_hit(unsigned int x, unsigned int y)
{
    unsigned int column;
    if (x >= 320U || y >= 8U)
        return MENU_NONE;
    column = x / 8U;
    if (column >= 11U && column <= 16U) return MENU_FILE;
    if (column >= 17U && column <= 23U) return MENU_TOOLS;
    if (column >= 24U && column <= 30U) return MENU_VIEW;
    if (column >= 31U && column <= 39U) return MENU_HELP;
    return MENU_NONE;
}

/* Row one, top toolbar: 0 INS 1 RD 2 WIR 3 R 4 C 5 I 6 DZ 7 PL */
int ui_toolbar_hit(unsigned int x, unsigned int y)
{
    unsigned int column;
    if (x >= 320U || y < 8U || y >= 16U)
        return -1;
    column = x / 8U;
    if (column < 6U)  return 0;
    if (column < 11U) return 1;
    if (column < 18U) return 2;
    if (column < 22U) return 3;
    if (column < 26U) return 4;
    if (column < 30U) return 5;
    if (column < 35U) return 6;
    return 7;
}

int ui_dropdown_hit(enum ui_menu menu, unsigned int x, unsigned int y)
{
    unsigned int row, col;
    if (x >= 320U || y >= 200U || ui_menu_count(menu) == 0U)
        return -1;
    col = x / 8U;
    row = y / 8U;
    if (row < 2U || row >= 2U + ui_menu_count(menu))
        return -1;
    if (col < ui_menu_origin(menu) ||
        col >= ui_menu_origin(menu) + 16U)
        return -1;
    return (int)(row - 2U);
}

const char *ui_menu_item(enum ui_menu menu, unsigned int index)
{
    if (index >= ui_menu_count(menu))
        return "";
    switch (menu) {
    case MENU_FILE: return file_items[index];
    case MENU_TOOLS: return tools_items[index];
    case MENU_VIEW: return view_items[index];
    case MENU_HELP: return help_items[index];
    default: return "";
    }
}

enum ui_action ui_menu_command(enum ui_menu menu, unsigned int index)
{
    if (index >= ui_menu_count(menu))
        return UI_NOTHING;
    switch (menu) {
    case MENU_FILE: return file_cmds[index];
    case MENU_TOOLS: return tools_cmds[index];
    case MENU_VIEW: return view_cmds[index];
    case MENU_HELP: return help_cmds[index];
    default: return UI_NOTHING;
    }
}
