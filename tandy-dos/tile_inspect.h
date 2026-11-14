/* SPDX-License-Identifier: GPL-3.0-or-later
 * Full original/NEWER city tile inspector. No BIOS or UI dependencies.
 * Data collected ONCE on Inspect; page formatting never rescans the map.
 */
#ifndef TANDY_TILE_INSPECT_H
#define TANDY_TILE_INSPECT_H
#define TILE_INSPECT_PAGES 4U
#define TILE_INSPECT_ROWS 10U
#define TILE_INSPECT_LINE 39U

struct tile_inspect {
    unsigned int x,y,word,id,zone,stage;
    unsigned int left,top,span,part_x,part_y,source_id;
    unsigned int newer,original_group;
    unsigned int service,network_valid,needs_warning;
    unsigned int neighbor[4];
    unsigned int roads,rails,wires;
};
void tile_inspect_collect(unsigned int x,unsigned int y,
                          struct tile_inspect *result);
/* Format exactly one numbered display row (0..9) of page 0..3.
 * out must hold TILE_INSPECT_LINE+1 chars. All lines <=38 columns.
 */
void tile_inspect_line(const struct tile_inspect *info,
                       unsigned int page,unsigned int row,
                       char out[TILE_INSPECT_LINE+1U]);
const char *tile_inspect_kind(unsigned int id);
#endif
