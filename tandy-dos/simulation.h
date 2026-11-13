/* SPDX-License-Identifier: GPL-3.0-or-later
 * Incremental city simulation for low-clocked 16-bit DOS.
 * Adapted from Micropolis' phased MapScan, RCI valves and tax cadence.
 * Not a full, exact port of the original simulation.
 */
#ifndef TANDY_SIMULATION_H
#define TANDY_SIMULATION_H

#define SIM_EVENT_TILE  1U
#define SIM_EVENT_MONTH 2U

struct sim_statistics {
    unsigned long residents;
    unsigned long commercial_jobs;
    unsigned long industrial_jobs;
    unsigned int residential_lots;
    unsigned int commercial_lots;
    unsigned int industrial_lots;
    unsigned int road_tiles;
    unsigned int wire_tiles;
    unsigned int plants;
    unsigned int powered_tiles;
    unsigned int month;
    unsigned int year;
    long last_income;
    long last_maintenance;
};

void simulation_init(void);
void simulation_mark_power_dirty(void);
unsigned int simulation_advance(unsigned int max_tiles);
const struct sim_statistics *simulation_stats(void);
void simulation_last_change(unsigned int *x, unsigned int *y);
unsigned int simulation_last_change_radius(void);
unsigned int simulation_progress(void);
int simulation_is_paused(void);
void simulation_set_paused(int paused);
unsigned int simulation_speed(void);
void simulation_set_speed(unsigned int speed);

#endif
