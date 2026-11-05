/* SPDX-License-Identifier: GPL-3.0-or-later
 * Standard DOS mouse services. Original Tandy 1000 EX graphics mode 9
 * does not always have a mouse-driver-supported pointer, so the program
 * draws its own sprite. The mouse driver is queried but never shown.
 *
 * Serial mouse input is owned by a TSR driver, not by this application.
 */
#include <dos.h>
#include "mouse.h"

static int mouse_available;

int mouse_init(void)
{
    union REGS r;
    mouse_available = 0;
    r.x.ax = 0U;              /* INT 33 / AX=0: reset / check driver */
    int86(0x33, &r, &r);
    if (r.x.ax != 0xffffU)
        return 0;

    mouse_available = 1;
    /* Standard 320x200 DOS mouse coordinates convention:
     * CX ranges 0..639 even when physical pixels are 0..319.
     * Clamp virtual X to 0..639 then divide by two in mouse_poll().
     */
    r.x.ax = 7U;
    r.x.cx = 0U;
    r.x.dx = 639U;
    int86(0x33, &r, &r);
    r.x.ax = 8U;
    r.x.cx = 0U;
    r.x.dx = 199U;
    int86(0x33, &r, &r);
    r.x.ax = 4U;
    r.x.cx = 320U;
    r.x.dx = 100U;
    int86(0x33, &r, &r);
    return 1;
}

int mouse_poll(struct mouse_state *state)
{
    union REGS r;
    if (!mouse_available || state == 0)
        return 0;
    r.x.ax = 3U;
    int86(0x33, &r, &r);
    state->x = r.x.cx / 2U;
    state->y = r.x.dx;
    if (state->x > 319U)
        state->x = 319U;
    if (state->y > 199U)
        state->y = 199U;
    state->buttons = r.x.bx & 3U;
    return 1;
}

void mouse_shutdown(void)
{
    /* Driver cursor was never shown, nothing to hide. */
    mouse_available = 0;
}
