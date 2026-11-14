/* SPDX-License-Identifier: GPL-3.0-or-later
 * Low-frequency static-badge recoloring: 24 BIOS ticks per phase
 * (~1.32s bright, ~1.32s dim). Never rescans power or city cells.
 */
#ifndef TANDY_BADGE_FLASH_H
#define TANDY_BADGE_FLASH_H
#define BADGE_FLASH_TICKS 24U
#define BADGE_FLASH_BRIGHT 14U
#define BADGE_FLASH_DIM 8U
static unsigned int badge_flash_color(unsigned int tick)
{
    return ((tick/BADGE_FLASH_TICKS)&1U)?
           BADGE_FLASH_DIM:BADGE_FLASH_BRIGHT;
}
#endif
