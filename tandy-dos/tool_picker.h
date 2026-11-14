/* SPDX-License-Identifier: GPL-3.0-or-later
 * Tiny pure C89 model for Tandy grouped toolbar flyouts.
 * Avoid adding timer interrupts or menu logic to the idle hot path.
 */
#ifndef TANDY_TOOL_PICKER_H
#define TANDY_TOOL_PICKER_H
#include "editor.h"
#define TOOL_HOLD_TICKS 8U

static int tool_picker_button(int button)
{
    return button==1 || button==7; /* Road or Coal */
}
static int tool_picker_ready(unsigned int now,unsigned int started)
{
    return (unsigned short)(now-started)>=TOOL_HOLD_TICKS;
}
/* Shared pixel origin for hit tests, popup drawing and VRAM restore.
 * The ordinary top menu starts at y=16; these flyouts do not.
 */
static unsigned int tool_picker_top_y(unsigned int kind)
{
    return kind==1U?24U:96U;
}
static int tool_picker_hit(unsigned int kind,
                           unsigned int x,unsigned int y)
{
    unsigned int top=tool_picker_top_y(kind);
    unsigned int rows=kind==1U?2U:4U;
    if(kind==0U || kind>2U || x<64U || x>=192U ||
       y<top || y>=top+rows*8U) return -1;
    return (int)((y-top)/8U);
}
static enum edit_tool tool_picker_choice(unsigned int kind,
                                         unsigned int index)
{
    if(kind==1U) return index==1U?EDIT_RAIL:EDIT_ROAD;
    if(index==0U) return EDIT_COAL;
    if(index==1U) return EDIT_NUCLEAR;
    if(index==2U) return EDIT_POLICE;
    return EDIT_FIRE;
}
#endif
