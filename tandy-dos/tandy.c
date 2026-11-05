/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy 1000 EX DOS port: hardware/display milestone.
 * NOT YET A PLAYABLE GAME: read-only city viewer, native Tandy 16-color.
 * Original Micropolis: Copyright (C) 1989-2007 Electronic Arts Inc.
 * See repository COPYING and the original source's additional terms.
 */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "city.h"
#include "editor.h"
#include "engine.h"
#include "ui.h"
#include "mouse.h"

#define SCREEN_W 320U
#define SCREEN_H 200U
#define VIEW_X    UI_MAP_X
#define VIEW_Y    UI_MAP_Y
#define VIEW_W    UI_MAP_COLS
#define VIEW_H    UI_MAP_ROWS
#define TILE_MASK 1023U
#define ART_TILES 960U
#define ART_BYTES 30720U

/* Tandy mode 9: 320x200x16, two pixels/byte, four 8KB interlaced banks.
 * Rows 0,4,... at B800:0000, rows 1,5,... at B800:2000 and so on.
 * See the Tandy 1000 technical reference. Avoid CGA mode 4 and VGA.
 */
static volatile unsigned char far *video;
static unsigned int scroll_x = 0, scroll_y = 0;
static unsigned char old_mode;
static unsigned char far *art_data = NULL;
static unsigned int cursor_x = VIEW_W / 2U;
static unsigned int cursor_y = VIEW_H / 2U;
static enum edit_tool selected_tool = EDIT_INSPECT;
static const char *last_action = "READY";
static char power_message[12];

/* Menus and mouse input are optional; keyboard remains the default.
 * A menu is drawn only when opened. No scrolling performance penalty.
 */
static enum ui_menu open_menu = MENU_NONE;
static unsigned int menu_highlight = 0U;
static int info_page = 0;
static int exit_requested = 0;
static int mouse_on = 0;
static unsigned int mouse_x = 160U, mouse_y = 100U;
static unsigned int mouse_buttons = 0U;

/* Software 8x8 mouse pointer. Driver hardware cursor is never shown:
 * standard mouse drivers may not support Tandy 320x200 mode 9.
 * Saving 32 packed VRAM bytes allows exact recovery of original art.
 */
static unsigned char mouse_saved[32];
static unsigned int mouse_draw_x, mouse_draw_y;
static int mouse_drawn = 0;

static void set_video_mode(unsigned char mode)
{
    union REGS r;
    r.h.ah = 0;
    r.h.al = mode;
    int86(0x10, &r, &r);
}

static unsigned char current_video_mode(void)
{
    union REGS r;
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    return (unsigned char)(r.h.al & 0x7f);
}

static unsigned int vram_offset(unsigned int x, unsigned int y)
{
    return ((y & 3U) << 13) + ((y >> 2) * 160U) + (x >> 1);
}

static void pixel(unsigned int x, unsigned int y, unsigned char color)
{
    unsigned int off;
    unsigned char old;
    if (x >= SCREEN_W || y >= SCREEN_H)
        return;
    off = vram_offset(x, y);
    old = video[off];
    if (x & 1U)
        video[off] = (unsigned char)((old & 0xf0U) | (color & 15U));
    else
        video[off] = (unsigned char)((old & 0x0fU) | (color << 4));
}

static void solid_tile(unsigned int px, unsigned int py, unsigned char color)
{
    unsigned int row, n, off;
    unsigned char packed = (unsigned char)((color << 4) | color);
    for (row = 0; row < 8U; ++row) {
        off = vram_offset(px, py + row);
        for (n = 0; n < 4U; ++n)
            video[off + n] = packed;
    }
}

static void box_pixels(unsigned int px, unsigned int py,
                       unsigned int width, unsigned int height,
                       unsigned char color)
{
    unsigned int x, y;
    for (y = py; y < py + height; ++y)
        for (x = px; x < px + width; ++x)
            pixel(x, y, color);
}

static int road_at(unsigned int x, unsigned int y)
{
    unsigned short tile;
    if (x >= CITY_WIDTH || y >= CITY_HEIGHT)
        return 0;
    tile = city_at(x, y) & TILE_MASK;
    return tile >= 64U && tile <= 207U;
}

/* Original Micropolis 16x16 XPM tiles, scaled offline to 8x8 packed
 * Tandy framebuffer bytes in TILES.DAT. No X11, sprite library or VGA.
 * Keep tiles in a separate far heap allocation, outside the city map.
 */
static int load_original_art(void)
{
    FILE *f;
    long length;
    size_t count;
    f = fopen("TILES.DAT", "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    length = ftell(f);
    if (length != ART_BYTES || fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    art_data = (unsigned char far *)malloc(ART_BYTES);
    if (art_data == NULL) {
        fclose(f);
        return 0;
    }
    count = fread(art_data, 1U, ART_BYTES, f);
    fclose(f);
    if (count != ART_BYTES) {
        free(art_data);
        art_data = NULL;
        return 0;
    }
    return 1;
}

static void draw_original_tile(unsigned int px, unsigned int py, unsigned int tile)
{
    unsigned int src = tile * 32U;
    unsigned int row, byte_index, dst;
    for (row = 0; row < 8U; ++row) {
        dst = vram_offset(px, py + row);
        for (byte_index = 0; byte_index < 4U; ++byte_index)
            video[dst + byte_index] =
                art_data[src + row * 4U + byte_index];
    }
}

static void draw_city_tile(unsigned int px, unsigned int py,
                           unsigned int mx, unsigned int my)
{
    unsigned short t = city_at(mx, my) & TILE_MASK;
    unsigned char bg = 2;
    unsigned int i;
    int v, h;

    if (art_data != NULL && t < ART_TILES) {
        draw_original_tile(px, py, t);
        return;
    }

    /* First pass: semantic colors keyed to ORIGINAL Micropolis tile IDs.
     * Later milestones will replace symbols with converted original art.
     */
    if (t >= 1U && t <= 20U) bg = 1;        /* river */
    else if (t >= 21U && t <= 43U) bg = 2;   /* forest */
    else if (t >= 44U && t <= 55U) bg = 8;   /* rubble */
    else if (t >= 56U && t <= 63U) bg = 12;  /* fire */
    else if (t >= 64U && t <= 207U) bg = 8;  /* road */
    else if (t >= 224U && t <= 239U) bg = 7; /* railway */
    else if (t >= 240U && t <= 422U) bg = 10;/* residential */
    else if (t >= 423U && t <= 611U) bg = 9; /* commercial */
    else if (t >= 612U && t <= 692U) bg = 6; /* industrial */
    else if (t >= 693U) bg = 5;             /* civic/special */
    solid_tile(px, py, bg);

    if (t >= 1U && t <= 20U) {
        pixel(px + 1U, py + 2U, 9);
        pixel(px + 5U, py + 5U, 9);
    } else if (t >= 21U && t <= 43U) {
        box_pixels(px + 2U, py + 1U, 3U, 3U, 10);
        pixel(px + 3U, py + 5U, 6);
    } else if (t >= 64U && t <= 207U) {
        v = (my > 0U && road_at(mx, my - 1U)) ||
            (my + 1U < CITY_HEIGHT && road_at(mx, my + 1U));
        h = (mx > 0U && road_at(mx - 1U, my)) ||
            (mx + 1U < CITY_WIDTH && road_at(mx + 1U, my));
        if (v || !h) {
            for (i = 0; i < 8U; ++i) {
                pixel(px + 2U, py + i, 7);
                pixel(px + 5U, py + i, 7);
            }
        }
        if (h) {
            for (i = 0; i < 8U; ++i) {
                pixel(px + i, py + 2U, 7);
                pixel(px + i, py + 5U, 7);
            }
        }
        pixel(px + 3U, py + 3U, 14);
    } else if (t >= 208U && t <= 223U) {
        for (i = 0; i < 8U; i += 2U)
            pixel(px + i, py + 3U, 14);
    } else if (t >= 224U && t <= 239U) {
        for (i = 0; i < 8U; i += 2U)
            box_pixels(px + 1U, py + i, 6U, 1U, 0);
    } else if (t >= 240U && t <= 422U) {
        box_pixels(px + 2U, py + 2U, 4U, 4U, 15);
        box_pixels(px + 3U, py + 1U, 2U, 1U, 6);
    } else if (t >= 423U && t <= 611U) {
        box_pixels(px + 2U, py + 1U, 4U, 6U, 11);
        pixel(px + 3U, py + 3U, 1);
        pixel(px + 5U, py + 5U, 1);
    } else if (t >= 612U && t <= 692U) {
        box_pixels(px + 1U, py + 3U, 6U, 4U, 14);
        box_pixels(px + 4U, py + 1U, 2U, 2U, 8);
    } else if (t >= 693U) {
        box_pixels(px + 2U, py + 2U, 4U, 4U, 15);
        pixel(px + 3U, py + 3U, 12);
    }
}

static void bios_text(unsigned int row, unsigned int column,
                      const char *s, unsigned char color)
{
    union REGS r;
    r.h.ah = 2;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)column;
    int86(0x10, &r, &r);
    while (*s && column++ < 40U) {
        r.h.ah = 0x0e;
        r.h.al = (unsigned char)*s++;
        r.h.bh = 0;
        r.h.bl = color;
        int86(0x10, &r, &r);
    }
}

/* SimCity-style compact toolbox, custom-drawn in Tandy mode 9.
 * All of these pixels are OUTSIDE the map's VRAM scrolling rectangle.
 */
static const unsigned short tool_icons[8] = {
    0U, 66U, 210U, 244U, 427U, 616U, 44U, 750U
};
static const char *const tool_labels[8] = {
    "INS", "RD", "PWR", "RES", "COM", "IND", "DOZ", "PLT"
};

/* Small overview map uses the real city tiles. Unlike the city window,
 * it is a static, coarse preview and can be repainted after edits/pans.
 */
static void draw_overview(void)
{
    unsigned int px, py, mx, my, id, ox, oy, w, h;
    unsigned char color;
    for (py = 0U; py < 13U; ++py) {
        my = py * CITY_HEIGHT / 13U;
        for (px = 0U; px < 72U; ++px) {
            mx = px * CITY_WIDTH / 72U;
            id = city_at(mx, my) & TILE_MASK;
            color = 2U;
            if (id >= 1U && id <= 20U) color = 1U;
            else if (id >= 21U && id <= 43U) color = 10U;
            else if (id >= 64U && id <= 207U) color = 8U;
            else if (id >= 208U && id <= 239U) color = 7U;
            else if (id >= 240U && id <= 422U) color = 10U;
            else if (id >= 423U && id <= 611U) color = 9U;
            else if (id >= 612U && id <= 692U) color = 6U;
            else if (id >= 693U) color = 15U;
            pixel(4U + px, 169U + py, color);
        }
    }
    ox = 4U + scroll_x * 72U / CITY_WIDTH;
    oy = 169U + scroll_y * 13U / CITY_HEIGHT;
    w = VIEW_W * 72U / CITY_WIDTH;
    h = VIEW_H * 13U / CITY_HEIGHT;
    if (h < 2U) h = 2U;
    for (px = 0U; px < w; ++px) {
        pixel(ox + px, oy, 14U);
        pixel(ox + px, oy + h - 1U, 14U);
    }
    for (py = 0U; py < h; ++py) {
        pixel(ox, oy + py, 14U);
        pixel(ox + w - 1U, oy + py, 14U);
    }
}

static void draw_toolbox(void)
{
    unsigned int x, y, n, bx, by, ix, iy, src;
    unsigned char color, packed;
    /* Cool gray left bezel. No draw calls touch city pixels (x >= 80). */
    for (y = 8U; y < UI_FOOTER_Y; y += 8U)
        for (x = 0U; x < UI_TOOLBOX_W; x += 8U)
            solid_tile(x, y, 7U);

    for (y = 8U; y < UI_FOOTER_Y; ++y) {
        pixel(78U, y, 8U);
        pixel(79U, y, 0U);
    }
    bios_text(1U, 0U, "FUNDS: --", 0U);
    bios_text(2U, 0U, "EDIT MODE", 0U);
    bios_text(3U, 2U, "TOOLS", 1U);

    for (n = 0U; n < 8U; ++n) {
        bx = 4U + (n & 1U) * 40U;
        by = UI_BUTTON_TOP + (n >> 1) * UI_BUTTON_SIZE;
        color = (unsigned char)(((n == 6U && selected_tool == EDIT_BULLDOZE) ||
                                (n == 7U && selected_tool == EDIT_COAL) ||
                                (n < 6U && (unsigned int)selected_tool == n)) ?
                                14U : 8U);
        box_pixels(bx, by, 34U, 32U, color);
        box_pixels(bx + 2U, by + 2U, 30U, 27U, 15U);
        box_pixels(bx + 5U, by + 3U, 24U, 19U, 7U);
        /* Draw authentic 8x8 Micropolis artwork enlarged to 16x16.
         * Source stays in TILES.DAT, we do not need another sprites file.
         */
        if (art_data != NULL) {
            src = tool_icons[n] * 32U;
            for (iy = 0U; iy < 8U; ++iy)
                for (ix = 0U; ix < 8U; ++ix) {
                    packed = art_data[src + iy * 4U + (ix >> 1)];
                    color = (unsigned char)((ix & 1U) ?
                            (packed & 15U) : (packed >> 4));
                    box_pixels(bx + 9U + ix * 2U, by + 4U + iy * 2U,
                               2U, 2U, color);
                }
        }
        bios_text((by + 24U) / 8U,
                  (bx / 8U) + 1U, tool_labels[n],
                  (unsigned char)(((n == 6U && selected_tool == EDIT_BULLDOZE) ||
                                  (n == 7U && selected_tool == EDIT_COAL) ||
                                  (n < 6U && (unsigned int)selected_tool == n)) ?
                                  4U : 1U));
    }
    bios_text(20U, 2U, "MAP", 1U);
    draw_overview();
}

static void draw_footer(void)
{
    unsigned int x;
    for (x = 0U; x < SCREEN_W; x += 8U)
        solid_tile(x, UI_FOOTER_Y, 1U);
    bios_text(23U, 1U, "MICROPOLIS   TANDY 1000   16 COLOR", 15U);
}

/* Redraw only the moving edge.  For a one-tile scroll, use a direct
 * 8-pixel (four-byte) VRAM shift rather than re-rendering 840 map tiles.
 * Keep both title and bottom control bands untouched while scrolling.
 */
static void draw_cursor(void)
{
    unsigned int x, y;
    unsigned int px = VIEW_X + cursor_x * 8U;
    unsigned int py = VIEW_Y + cursor_y * 8U;

    /* Yellow hardware pixel outline, drawn after map scrolling/blitting. */
    for (x = 0U; x < 8U; ++x) {
        pixel(px + x, py, 14);
        pixel(px + x, py + 7U, 14);
    }
    for (y = 0U; y < 8U; ++y) {
        pixel(px, py + y, 14);
        pixel(px + 7U, py + y, 14);
    }
}

static void erase_cursor(void)
{
    draw_city_tile(VIEW_X + cursor_x * 8U, VIEW_Y + cursor_y * 8U,
                   scroll_x + cursor_x, scroll_y + cursor_y);
}

static void redraw_edit_area(void)
{
    unsigned int x, y, min_x, max_x, min_y, max_y;
    min_x = cursor_x > 2U ? cursor_x - 2U : 0U;
    min_y = cursor_y > 2U ? cursor_y - 2U : 0U;
    max_x = cursor_x + 2U < VIEW_W ? cursor_x + 2U : VIEW_W - 1U;
    max_y = cursor_y + 2U < VIEW_H ? cursor_y + 2U : VIEW_H - 1U;
    for (y = min_y; y <= max_y; ++y)
        for (x = min_x; x <= max_x; ++x)
            draw_city_tile(VIEW_X + x * 8U, VIEW_Y + y * 8U,
                           scroll_x + x, scroll_y + y);
}

static void draw_status(void)
{
    unsigned int x;
    char status[41];
    for (x = 0; x < SCREEN_W / 8U; ++x)
        solid_tile(x * 8U, 192U, 1);
    sprintf(status, "%-8s X:%03u Y:%03u  %-10s",
            edit_tool_name(selected_tool),
            scroll_x + cursor_x, scroll_y + cursor_y, last_action);
    bios_text(24, 0, status, 11);
}


/* The eight-pixel software pointer is much smaller than repainting a tile. */
static void mouse_pointer_hide(void)
{
    unsigned int row, n, dst;
    if (!mouse_drawn)
        return;
    for (row = 0U; row < 8U; ++row) {
        dst = vram_offset(mouse_draw_x, mouse_draw_y + row);
        for (n = 0U; n < 4U; ++n)
            video[dst + n] = mouse_saved[row * 4U + n];
    }
    mouse_drawn = 0;
}

static void mouse_pointer_show(void)
{
    unsigned int row, n, col, dst, px, py;
    static const char *const shape[8] = {
        "X       ", "XX      ", "X.X     ", "X..X    ",
        "X...X   ", "X....X  ", "X.....X ", "XXXXXXXX"
    };
    if (!mouse_on || mouse_drawn)
        return;
    px = (mouse_x > 311U ? 312U : mouse_x) & ~1U;
    py = mouse_y > 192U ? 192U : mouse_y;
    mouse_draw_x = px;
    mouse_draw_y = py;
    for (row = 0U; row < 8U; ++row) {
        dst = vram_offset(px, py + row);
        for (n = 0U; n < 4U; ++n)
            mouse_saved[row * 4U + n] = video[dst + n];
    }
    for (row = 0U; row < 8U; ++row)
        for (col = 0U; col < 8U; ++col)
            if (shape[row][col] != ' ')
                pixel(px + col, py + row,
                      (unsigned char)(shape[row][col] == 'X' ? 0U : 15U));
    mouse_drawn = 1;
}

/* Draw a 16-character popup over the map. It is rebuilt only on menu
 * selection changes, leaving the VRAM-shift scrolling path untouched.
 */
static void draw_menu_popup(void)
{
    unsigned int i, j, col, origin, count;
    if (open_menu == MENU_NONE)
        return;
    origin = ui_menu_origin(open_menu);
    count = ui_menu_count(open_menu);
    for (j = 0U; j < count; ++j) {
        for (col = 0U; col < 16U; ++col)
            solid_tile((origin + col) * 8U, (1U + j) * 8U,
                       (unsigned char)(j == menu_highlight ? 5U : 1U));
        bios_text(1U + j, origin + 1U, ui_menu_item(open_menu, j),
                  (unsigned char)(j == menu_highlight ? 15U : 14U));
    }
}

static void draw_info_page(void)
{
    unsigned int i, j;
    if (info_page == 0)
        return;
    for (j = 5U; j <= 17U; ++j)
        for (i = 1U; i < 39U; ++i)
            solid_tile(i * 8U, j * 8U, 1U);

    if (info_page == 1) {
        bios_text(6U, 3U, "KEYBOARD CONTROLS", 15U);
        bios_text(8U, 3U, "ARROWS / WASD  PAN CITY", 14U);
        bios_text(9U, 3U, "I J K L       MOVE CURSOR", 14U);
        bios_text(10U, 3U, "0..7          SELECT TOOLS", 14U);
        bios_text(11U, 3U, "SPACE         BUILD / DOZE", 14U);
        bios_text(12U, 3U, "F2            SAVE AS", 14U);
        bios_text(13U, 3U, "P             POWER SCAN", 14U);
        bios_text(14U, 3U, "F T V H       OPEN MENUS", 14U);
        bios_text(16U, 3U, "ESC / ANY KEY TO RETURN", 15U);
    } else if (info_page == 2) {
        bios_text(6U, 3U, "MOUSE SUPPORT: DOS INT 33H", 15U);
        bios_text(8U, 3U, "RUN A SERIAL MOUSE DRIVER", 14U);
        bios_text(9U, 3U, "BEFORE STARTING MICROPOL", 14U);
        bios_text(11U, 3U, "LEFT: MENU / BUILD / DRAW", 14U);
        bios_text(12U, 3U, "RIGHT: TOOLS MENU", 14U);
        bios_text(13U, 3U, "DRAG: ROADS, WIRES, DOZE", 14U);
        bios_text(14U, 3U, "KEYBOARD ALWAYS WORKS", 14U);
        bios_text(16U, 3U, "ESC / CLICK TO RETURN", 15U);
    } else {
        bios_text(6U, 3U, "MICROPOLIS TANDY 1000 DOS PORT", 15U);
        bios_text(8U, 3U, "NATIVE 8086 / V20, 16 COLORS", 14U);
        bios_text(9U, 3U, "MICROPOLIS GPLv3+ SOURCE", 14U);
        bios_text(10U, 3U, "ORIGINAL ART: ELECTRONIC ARTS", 14U);
        bios_text(12U, 3U, "EXPERIMENTAL CITY EDITOR", 14U);
        bios_text(13U, 3U, "FULL SIMULATION NOT YET PORTED", 14U);
        bios_text(16U, 3U, "ESC / CLICK TO RETURN", 15U);
    }
}

static void render_full(void);

static void scroll_one_tile(unsigned int previous_x, unsigned int previous_y)
{
    unsigned int x, y, i, dst, src;

    if (scroll_y == previous_y && scroll_x == previous_x + 1U) {
        /* Camera right, image left: work left-to-right. */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(VIEW_X, y);
            for (i = 0; i < VIEW_W * 4U - 4U; ++i)
                video[dst + i] = video[dst + i + 4U];
        }
        for (y = 0; y < VIEW_H; ++y)
            draw_city_tile(VIEW_X + (VIEW_W - 1U) * 8U, VIEW_Y + y * 8U,
                           scroll_x + VIEW_W - 1U, scroll_y + y);

    } else if (scroll_y == previous_y && previous_x == scroll_x + 1U) {
        /* Camera left, image right: work right-to-left. */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(VIEW_X, y);
            for (i = VIEW_W * 4U; i > 4U; --i)
                video[dst + i - 1U] = video[dst + i - 5U];
        }
        for (y = 0; y < VIEW_H; ++y)
            draw_city_tile(VIEW_X, VIEW_Y + y * 8U, scroll_x, scroll_y + y);

    } else if (scroll_x == previous_x && scroll_y == previous_y + 1U) {
        /* Camera down, image up: copy scanlines top-to-bottom. */
        for (y = VIEW_Y; y < VIEW_Y + (VIEW_H - 1U) * 8U; ++y) {
            dst = vram_offset(VIEW_X, y);
            src = vram_offset(VIEW_X, y + 8U);
            for (i = 0; i < VIEW_W * 4U; ++i)
                video[dst + i] = video[src + i];
        }
        for (x = 0; x < VIEW_W; ++x)
            draw_city_tile(VIEW_X + x * 8U, VIEW_Y + (VIEW_H - 1U) * 8U,
                           scroll_x + x, scroll_y + VIEW_H - 1U);

    } else if (scroll_x == previous_x && previous_y == scroll_y + 1U) {
        /* Camera up, image down: copy scanlines bottom-to-top. */
        for (y = VIEW_Y + VIEW_H * 8U - 1U; y >= VIEW_Y + 8U; --y) {
            dst = vram_offset(VIEW_X, y);
            src = vram_offset(VIEW_X, y - 8U);
            for (i = 0; i < VIEW_W * 4U; ++i)
                video[dst + i] = video[src + i];
        }
        for (x = 0; x < VIEW_W; ++x)
            draw_city_tile(VIEW_X + x * 8U, VIEW_Y,
                           scroll_x + x, scroll_y);
    } else {
        /* Reserved for future jumps/teleports; only 1-tile steps today. */
        render_full();
        return;
    }
}
 
static void render_full(void)
{
    unsigned int x, y;
    mouse_pointer_hide();

    /* Cyan DOS-style top menu bar spanning all 40 columns. */
    for (x = 0U; x < SCREEN_W; x += 8U)
        solid_tile(x, 0U, 3U);
    bios_text(0U, 0U, " SYSTEM   TOOLS   VIEW   HELP", 0U);

    /* City is a fixed rectangular window to the right of the toolbox. */
    for (y = 0U; y < VIEW_H; ++y)
        for (x = 0U; x < VIEW_W; ++x)
            draw_city_tile(VIEW_X + x * 8U, VIEW_Y + y * 8U,
                           scroll_x + x, scroll_y + y);

    draw_toolbox();
    draw_footer();
    draw_status();

    if (info_page != 0)
        draw_info_page();
    else if (open_menu != MENU_NONE)
        draw_menu_popup();
    else
        draw_cursor();

    mouse_pointer_show();
}

static int save_city_new_file(void)
{
    unsigned int slot;
    char filename[13];
    FILE *f;

    /* DOS 8.3 filenames, never overwrite any existing source or save. */
    for (slot = 1U; slot < 10000U; ++slot) {
        sprintf(filename, "CITY%04u.CTY", slot);
        f = fopen(filename, "rb");
        if (f != NULL) {
            fclose(f);
            continue;
        }
        return city_save_as(filename);
    }
    return 0;
}

/* Map number-row shortcuts, toolbar, and Tools menu onto one function. */
static void select_tool(unsigned int number)
{
    switch (number) {
    case 0U: selected_tool = EDIT_INSPECT; break;
    case 1U: selected_tool = EDIT_ROAD; break;
    case 2U: selected_tool = EDIT_POWER; break;
    case 3U: selected_tool = EDIT_RESIDENTIAL; break;
    case 4U: selected_tool = EDIT_COMMERCIAL; break;
    case 5U: selected_tool = EDIT_INDUSTRIAL; break;
    case 6U: selected_tool = EDIT_BULLDOZE; break;
    case 7U: selected_tool = EDIT_COAL; break;
    }
    last_action = "SELECT";
    draw_toolbox();
}

static void apply_selected_tool(void)
{
    if (selected_tool == EDIT_INSPECT) {
        last_action = "INSPECT";
    } else if (edit_place(selected_tool, scroll_x + cursor_x,
                          scroll_y + cursor_y)) {
        last_action = "PLACED";
        redraw_edit_area();
        draw_cursor();
        draw_overview();
    } else {
        last_action = "BLOCKED";
    }
    draw_status();
}

static void run_menu_action(enum ui_action action)
{
    unsigned int powered;
    switch (action) {
    case UI_SAVE:
        last_action = save_city_new_file() ? "SAVED" : "SAVE ERR";
        break;
    case UI_EXIT:
        exit_requested = 1;
        break;
    case UI_TOOL_INSPECT: select_tool(0U); break;
    case UI_TOOL_ROAD: select_tool(1U); break;
    case UI_TOOL_POWER: select_tool(2U); break;
    case UI_TOOL_RES: select_tool(3U); break;
    case UI_TOOL_COM: select_tool(4U); break;
    case UI_TOOL_IND: select_tool(5U); break;
    case UI_TOOL_DOZE: select_tool(6U); break;
    case UI_TOOL_COAL: select_tool(7U); break;
    case UI_SCAN_POWER:
        powered = engine_power_scan();
        sprintf(power_message, "PWR:%04u", powered);
        last_action = power_message;
        break;
    case UI_CENTER_CURSOR:
        cursor_x = VIEW_W / 2U;
        cursor_y = VIEW_H / 2U;
        last_action = "CENTER";
        break;
    case UI_HELP_KEYS: info_page = 1; break;
    case UI_HELP_MOUSE: info_page = 2; break;
    case UI_HELP_ABOUT: info_page = 3; break;
    default: break;
    }
}

static void show_menu(enum ui_menu menu)
{
    open_menu = menu;
    menu_highlight = 0U;
    render_full();
}

/* Mouse movement paints no pixels besides a tiny 8x8 arrow, unless it
 * crosses into a new map tile. Dragging only edits single-tile tools,
 * preventing accidental 3x3/4x4 buildings during a mouse stroke.
 */
static int drag_enabled(void)
{
    return selected_tool == EDIT_ROAD ||
           selected_tool == EDIT_POWER ||
           selected_tool == EDIT_BULLDOZE;
}

static void handle_mouse(void)
{
    struct mouse_state state;
    unsigned int next_x, next_y;
    int menu, item, toolbar;
    int left_press, right_press, moved;

    if (!mouse_poll(&state))
        return;
    left_press = (state.buttons & 1U) && !(mouse_buttons & 1U);
    right_press = (state.buttons & 2U) && !(mouse_buttons & 2U);
    moved = state.x != mouse_x || state.y != mouse_y;

    if (moved) {
        mouse_pointer_hide();
        if (info_page == 0 && open_menu == MENU_NONE &&
            state.x >= VIEW_X && state.y >= VIEW_Y &&
            state.y < VIEW_Y + VIEW_H * 8U) {
            next_x = (state.x - VIEW_X) / 8U;
            next_y = (state.y - VIEW_Y) / 8U;
            if (next_x != cursor_x || next_y != cursor_y) {
                erase_cursor();
                cursor_x = next_x;
                cursor_y = next_y;
                draw_cursor();
                if ((state.buttons & 1U) && (mouse_buttons & 1U) &&
                    drag_enabled())
                    apply_selected_tool();
                else
                    draw_status();
            }
        } else if (open_menu != MENU_NONE) {
            item = ui_dropdown_hit(open_menu, state.x, state.y);
            if (item >= 0 && (unsigned int)item != menu_highlight) {
                menu_highlight = (unsigned int)item;
                draw_menu_popup();
            }
        }
        mouse_x = state.x;
        mouse_y = state.y;
        mouse_pointer_show();
    }

    if (left_press) {
        mouse_pointer_hide();
        if (info_page != 0) {
            info_page = 0;
            render_full();
        } else if (open_menu != MENU_NONE) {
            menu = ui_header_hit(state.x, state.y);
            if (menu != MENU_NONE) {
                if (menu == open_menu) {
                    open_menu = MENU_NONE;
                    render_full();
                } else
                    show_menu((enum ui_menu)menu);
            } else {
                item = ui_dropdown_hit(open_menu, state.x, state.y);
                if (item >= 0) {
                    enum ui_action command =
                        ui_menu_command(open_menu, (unsigned int)item);
                    open_menu = MENU_NONE;
                    run_menu_action(command);
                } else
                    open_menu = MENU_NONE;
                if (!exit_requested)
                    render_full();
            }
        } else {
            menu = ui_header_hit(state.x, state.y);
            if (menu != MENU_NONE)
                show_menu((enum ui_menu)menu);
            else {
                toolbar = ui_toolbar_hit(state.x, state.y);
                if (toolbar >= 0) {
                    select_tool((unsigned int)toolbar);
                    draw_status();
                } else if (state.x >= VIEW_X && state.y >= VIEW_Y &&
                           state.y < VIEW_Y + VIEW_H * 8U) {
                    apply_selected_tool();
                }
            }
        }
        mouse_pointer_show();
    }
    if (right_press) {
        mouse_pointer_hide();
        if (info_page != 0) {
            info_page = 0;
            render_full();
        } else if (open_menu == MENU_TOOLS) {
            open_menu = MENU_NONE;
            render_full();
        } else
            show_menu(MENU_TOOLS);
        mouse_pointer_show();
    }

    mouse_buttons = state.buttons;
}

static void keyboard_loop(void)
{
    union REGS r;
    unsigned int prev_x, prev_y, prev_cx, prev_cy;
    unsigned int next_x, next_y, next_cx, next_cy;
    unsigned char ascii, scan;
    enum edit_tool old_tool;
    enum ui_menu next_menu;

    while (!exit_requested) {
        if (mouse_on) {
            handle_mouse();
            if (!kbhit())
                continue; /* poll mouse without blocking DOS keyboard */
        }

        r.h.ah = 0U;
        int86(0x16, &r, &r);
        ascii = r.h.al;
        scan = r.h.ah;
        mouse_pointer_hide();

        if (info_page != 0) {
            info_page = 0;
            render_full();
            continue;
        }

        if (open_menu != MENU_NONE) {
            next_menu = open_menu;
            if (ascii == 27U) {
                open_menu = MENU_NONE;
                render_full();
            } else if (ascii == 13U || ascii == ' ') {
                enum ui_action command =
                    ui_menu_command(open_menu, menu_highlight);
                open_menu = MENU_NONE;
                run_menu_action(command);
                if (!exit_requested)
                    render_full();
            } else if (scan == 0x48U) {
                if (menu_highlight > 0U)
                    --menu_highlight;
                draw_menu_popup();
            } else if (scan == 0x50U) {
                if (menu_highlight + 1U < ui_menu_count(open_menu))
                    ++menu_highlight;
                draw_menu_popup();
            } else if (scan == 0x4bU) {
                next_menu = (enum ui_menu)
                    ((open_menu + 3) % 4);
                show_menu(next_menu);
            } else if (scan == 0x4dU) {
                next_menu = (enum ui_menu)
                    ((open_menu + 1) % 4);
                show_menu(next_menu);
            } else if (ascii == 'f' || ascii == 'F')
                show_menu(MENU_FILE);
            else if (ascii == 't' || ascii == 'T')
                show_menu(MENU_TOOLS);
            else if (ascii == 'v' || ascii == 'V')
                show_menu(MENU_VIEW);
            else if (ascii == 'h' || ascii == 'H')
                show_menu(MENU_HELP);
            mouse_pointer_show();
            continue;
        }

        if (ascii == 27U) {
            exit_requested = 1;
            break;
        }
        if (ascii == 'f' || ascii == 'F' || scan == 0x44U) {
            show_menu(MENU_FILE);
            continue;
        }
        if (ascii == 't' || ascii == 'T') {
            show_menu(MENU_TOOLS);
            continue;
        }
        if (ascii == 'v' || ascii == 'V') {
            show_menu(MENU_VIEW);
            continue;
        }
        if (ascii == 'h' || ascii == 'H') {
            show_menu(MENU_HELP);
            continue;
        }

        prev_x = scroll_x;
        prev_y = scroll_y;
        prev_cx = cursor_x;
        prev_cy = cursor_y;
        old_tool = selected_tool;

        if ((scan == 0x4bU || ascii == 'a' || ascii == 'A') &&
            scroll_x > 0U)
            --scroll_x;
        else if ((scan == 0x4dU || ascii == 'd' || ascii == 'D') &&
                 scroll_x < CITY_WIDTH - VIEW_W)
            ++scroll_x;
        else if ((scan == 0x48U || ascii == 'w' || ascii == 'W') &&
                 scroll_y > 0U)
            --scroll_y;
        else if ((scan == 0x50U || ascii == 's' || ascii == 'S') &&
                 scroll_y < CITY_HEIGHT - VIEW_H)
            ++scroll_y;
        else if ((ascii == 'j' || ascii == 'J') && cursor_x > 0U)
            --cursor_x;
        else if ((ascii == 'l' || ascii == 'L') && cursor_x < VIEW_W - 1U)
            ++cursor_x;
        else if ((ascii == 'i' || ascii == 'I') && cursor_y > 0U)
            --cursor_y;
        else if ((ascii == 'k' || ascii == 'K') && cursor_y < VIEW_H - 1U)
            ++cursor_y;
        else if (ascii >= '0' && ascii <= '7')
            select_tool((unsigned int)(ascii - '0'));
        else if (ascii == ' ')
            apply_selected_tool();
        else if (ascii == 'p' || ascii == 'P')
            run_menu_action(UI_SCAN_POWER);
        else if (scan == 0x3cU)
            run_menu_action(UI_SAVE);

        if (scroll_x != prev_x || scroll_y != prev_y) {
            next_x = scroll_x;
            next_y = scroll_y;
            scroll_x = prev_x;
            scroll_y = prev_y;
            erase_cursor();
            scroll_x = next_x;
            scroll_y = next_y;
            scroll_one_tile(prev_x, prev_y);
            draw_cursor();
            draw_overview();
        } else if (cursor_x != prev_cx || cursor_y != prev_cy) {
            next_cx = cursor_x;
            next_cy = cursor_y;
            cursor_x = prev_cx;
            cursor_y = prev_cy;
            erase_cursor();
            cursor_x = next_cx;
            cursor_y = next_cy;
            draw_cursor();
        }

        if (old_tool != selected_tool || cursor_x != prev_cx ||
            cursor_y != prev_cy || scroll_x != prev_x ||
            scroll_y != prev_y || ascii == ' ' || scan == 0x3cU ||
            ascii == 'p' || ascii == 'P')
            draw_status();
        mouse_pointer_show();
    }
}

int main(int argc, char **argv)
{
    int i, have_city = 0, allow_mouse = 1;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "/NOMOUSE") == 0 ||
            strcmp(argv[i], "-NOMOUSE") == 0) {
            allow_mouse = 0;
        } else if (!have_city && city_load(argv[i])) {
            have_city = 1;
        } else {
            fprintf(stderr,
                    "Unrecognized argument or invalid 27120-byte city: %s\n",
                    argv[i]);
            return 1;
        }
    }
    if (!have_city)
        city_make_demo();

    old_mode = current_video_mode();
    set_video_mode(9);
    if (current_video_mode() != 9) {
        set_video_mode(old_mode);
        fputs("Tandy 1000 graphics mode 9 not available.\n", stderr);
        return 1;
    }
    video = (volatile unsigned char far *)MK_FP(0xb800, 0);
    (void)load_original_art();
    mouse_on = allow_mouse ? mouse_init() : 0;
    if (mouse_on) {
        struct mouse_state state;
        if (mouse_poll(&state)) {
            mouse_x = state.x;
            mouse_y = state.y;
            mouse_buttons = state.buttons;
        }
    }
    render_full();
    keyboard_loop();
    mouse_pointer_hide();
    mouse_shutdown();
    set_video_mode(old_mode);
    if (art_data != NULL)
        free(art_data); /* art buffer used only in graphics mode */
    return 0;
}
