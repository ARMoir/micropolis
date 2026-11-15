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
/* Compact graphic-dialog layout. All coordinates in 320x200 pixels.
 * Click action: 0 = next page, 1 = previous, 2 = next, 3 = Done.
 */
#define TILE_DIALOG_X 8U
#define TILE_DIALOG_Y 32U
#define TILE_DIALOG_W 304U
#define TILE_DIALOG_H 144U
#define TILE_DIALOG_PREVIEW_X 224U
#define TILE_DIALOG_PREVIEW_Y 56U
#define TILE_DIALOG_PREVIEW_SIZE 80U
/* Same BIOS graphics-mode text as the readable DATE/POP/JOBS HUD:
 * 0x0F = white pixels. 0x70 used in the old inspector interpreted
 * as black ink / XOR in mode 9, producing unreadable glyph blocks.
 * Fill the TEXT CELL background explicitly; never request a
 * text-mode foreground/background attribute in graphics mode.
 */
#define TILE_DIALOG_TEXT_BG 0U
#define TILE_DIALOG_TEXT_FG 15U
#define TILE_DIALOG_TEXT_COLUMNS 24U
/* Flat dialog vertical layout in 320x200 pixels:
 * preview ends at y=136, buttons y=140..155, footer y=160..167.
 * The 8px BIOS labels sit centered at y=144..151, leaving 4px
 * breathing room above and below and no overlap with the footer.
 */
#define TILE_DIALOG_BUTTON_TOP 140U
#define TILE_DIALOG_BUTTON_HEIGHT 16U
#define TILE_DIALOG_BUTTON_LABEL_ROW 18U
#define TILE_DIALOG_FOOTER_ROW 20U
unsigned int tile_inspect_dialog_click(unsigned int px,unsigned int py);
unsigned int tile_inspect_preview_scale(unsigned int span);
unsigned int tile_inspect_preview_original_id(
    const struct tile_inspect *info,unsigned int col,unsigned int row);
/* Text fits to the left of the preview while retaining all 4 pages.
 * Buffers are at least TILE_DIALOG_TEXT_COLUMNS+1 bytes.
 */
void tile_inspect_dialog_line(const struct tile_inspect *info,
                              unsigned int page,unsigned int row,
                              char out[TILE_DIALOG_TEXT_COLUMNS+1U]);
void tile_inspect_dialog_title(const struct tile_inspect *info,
                               char out[TILE_DIALOG_TEXT_COLUMNS+1U]);

#endif
