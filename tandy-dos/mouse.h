/* SPDX-License-Identifier: GPL-3.0-or-later
 * Optional Microsoft-compatible DOS INT 33h mouse interface.
 * Requires external mouse driver: e.g., CTMOUSE / MOUSE.COM (serial mouse).
 * No driver => all calls do nothing; keyboard is always available.
 */
#ifndef TANDY_MOUSE_H
#define TANDY_MOUSE_H

struct mouse_state {
    unsigned int x;       /* 0..319 Tandy pixel coordinates */
    unsigned int y;       /* 0..199 */
    unsigned int buttons; /* bit0 left, bit1 right */
};

int mouse_init(void);
int mouse_poll(struct mouse_state *state);
void mouse_shutdown(void);

#endif
