/* SPDX-License-Identifier: GPL-3.0-or-later
 * Safe DOS 8.3 Save/Resume for Micropolis Tandy.
 * Three original-compatible sidecars are saved as one logical city.
 */
#ifndef TANDY_GAME_SAVE_H
#define TANDY_GAME_SAVE_H
#define GAME_SAVE_OK 0U
#define GAME_SAVE_NO_SLOT 1U
#define GAME_SAVE_CTY_ERROR 2U
#define GAME_SAVE_ZON_ERROR 3U
#define GAME_SAVE_NBL_ERROR 4U
#define GAME_SAVE_COMMIT_ERROR 5U
#define GAME_SAVE_LAST_ERROR 6U

/* On first call (*slot==0), pick unused CITYnnnn.* names; later F2
 * overwrites that same slot with a retained .BCT/.BZN/.BNB backup.
 * Live files are never touched until ALL three temporary files close.
 * On error, *slot is unchanged and the previous set is recovered.
 */
unsigned int game_save_write(unsigned int *slot);

/* Local basename CITY0001.CTY (case-insensitive), not a path. */
int game_save_parse_slot(const char *filename,unsigned int *slot);

/* LASTCITY.CFG is written only after all three save files commit.
 * If the player launches MICROPOL.EXE without a filename, this can
 * restore the most recent named city. Explicit CTY argv wins.
 */
int game_save_last_slot(unsigned int *slot);
int game_save_load_last(unsigned int *slot);
#endif
