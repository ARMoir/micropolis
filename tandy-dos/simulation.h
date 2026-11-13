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
    unsigned int growth_last_month;
    unsigned int growth_total;
    unsigned int lots_ready;
    unsigned int lots_no_road;
    unsigned int lots_no_power;
    unsigned int month;
    unsigned int year;
    long last_income;
    long last_maintenance;
};

/* Add a ready-to-grow example neighborhood to the built-in demo only. */
void simulation_seed_demo(void);
void simulation_init(void);
void simulation_mark_power_dirty(void);
unsigned int simulation_advance(unsigned int max_tiles);
const struct sim_statistics *simulation_stats(void);
void simulation_last_change(unsigned int *x, unsigned int *y);
unsigned int simulation_last_change_radius(void);
unsigned int simulation_progress(void);
/* Debug/profiling: full power-network scans only for network edits, not
 * ordinary building density growth. */
unsigned int simulation_power_scan_count(void);
/* The coordinates of the most recent visible growth event remain valid
 * until the next city is loaded and simulation_init() is called.
 */
int simulation_has_last_change(void);
int simulation_is_paused(void);
void simulation_set_paused(int paused);
unsigned int simulation_speed(void);
void simulation_set_speed(unsigned int speed);

#endif
