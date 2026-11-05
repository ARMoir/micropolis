/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy DOS editor milestone, NOT the city simulation engine.
 */
#ifndef TANDY_EDITOR_H
#define TANDY_EDITOR_H

enum edit_tool {
    EDIT_INSPECT = 0,
    EDIT_ROAD,
    EDIT_POWER,
    EDIT_RESIDENTIAL,
    EDIT_COMMERCIAL,
    EDIT_INDUSTRIAL,
    EDIT_BULLDOZE
};

const char *edit_tool_name(enum edit_tool tool);
int edit_place(enum edit_tool tool, unsigned int x, unsigned int y);

#endif
