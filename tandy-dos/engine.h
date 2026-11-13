/* SPDX-License-Identifier: GPL-3.0-or-later
 * First independent portion of the DOS Micropolis simulation port.
 * NOT a complete city simulation, growth or economic model.
 */
#ifndef TANDY_ENGINE_H
#define TANDY_ENGINE_H
unsigned int engine_power_scan(void);
int engine_is_powered(unsigned int x, unsigned int y);
void engine_shutdown(void);
#endif
