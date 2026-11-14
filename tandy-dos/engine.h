/* SPDX-License-Identifier: GPL-3.0-or-later
 * First independent portion of the DOS Micropolis simulation port.
 * NOT a complete city simulation, growth or economic model.
 */
#ifndef TANDY_ENGINE_H
#define TANDY_ENGINE_H
unsigned int engine_power_scan(void);
int engine_is_powered(unsigned int x, unsigned int y);
/* Fast addition-only BFS (no full-city scan). Removing a conductor or
 * zoned relay still requires engine_power_scan().
 */
void engine_power_invalidate(void);
int engine_power_valid(void);
unsigned int engine_powered_count(void);
unsigned int engine_power_extend_rect(unsigned int x0,unsigned int y0,
                                      unsigned int x1,unsigned int y1);
void engine_shutdown(void);
#endif
