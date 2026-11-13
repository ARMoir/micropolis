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
    case MENU_FILE:  return 1U;
    case MENU_TOOLS: return 9U;
    case MENU_VIEW:  return 17U;
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

/* Row 0 is the menu; row 1 is reserved for city statistics.
 * Toolbox occupies x=0..63; city begins at x=64, y=16.
 */
int ui_header_hit(unsigned int x, unsigned int y)
{
    unsigned int column;
    /* City-statistics row y=8..15 is NOT a menu click target. */
    if (x >= 320U || y >= UI_MENU_HEIGHT)
        return MENU_NONE;
    column = x / 8U;
    if (column >= 1U && column <= 8U) return MENU_FILE;
    if (column >= 9U && column <= 16U) return MENU_TOOLS;
    if (column >= 17U && column <= 24U) return MENU_VIEW;
    if (column >= 25U && column <= 39U) return MENU_HELP;
    return MENU_NONE;
}

/* Two columns of 32x24 compact icon buttons. */
int ui_toolbar_hit(unsigned int x, unsigned int y)
{
    unsigned int col, row;
    if (x >= UI_TOOLBOX_W || y < UI_BUTTON_TOP ||
        y >= UI_BUTTON_TOP + UI_BUTTON_SIZE * 4U)
        return -1;
    col = x / (UI_TOOLBOX_W / 2U);
    row = (y - UI_BUTTON_TOP) / UI_BUTTON_SIZE;
    return (int)(row * 2U + col);
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
