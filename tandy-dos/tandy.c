/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy 1000 EX DOS port: hardware/display milestone.
 * NOT YET A PLAYABLE GAME: read-only city viewer, native Tandy 16-color.
 * Original Micropolis: Copyright (C) 1989-2007 Electronic Arts Inc.
 * See repository COPYING and the original source's additional terms.
 */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include "city.h"
#include "editor.h"

#define SCREEN_W 320U
#define SCREEN_H 200U
#define VIEW_W    40U
#define VIEW_H    21U
#define VIEW_Y    16U
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

/* Redraw only the moving edge.  For a one-tile scroll, use a direct
 * 8-pixel (four-byte) VRAM shift rather than re-rendering 840 map tiles.
 * Keep both title and bottom control bands untouched while scrolling.
 */
static void draw_cursor(void)
{
    unsigned int x, y;
    unsigned int px = cursor_x * 8U;
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
    draw_city_tile(cursor_x * 8U, VIEW_Y + cursor_y * 8U,
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
            draw_city_tile(x * 8U, VIEW_Y + y * 8U,
                           scroll_x + x, scroll_y + y);
}

static void draw_status(void)
{
    unsigned int x;
    char status[41];
    for (x = 0; x < VIEW_W; ++x)
        solid_tile(x * 8U, 192U, 1);
    sprintf(status, "%-8s X:%03u Y:%03u  %-10s",
            edit_tool_name(selected_tool),
            scroll_x + cursor_x, scroll_y + cursor_y, last_action);
    bios_text(24, 0, status, 11);
}

static void render_full(void);

static void scroll_one_tile(unsigned int previous_x, unsigned int previous_y)
{
    unsigned int x, y, i, dst, src;

    if (scroll_y == previous_y && scroll_x == previous_x + 1U) {
        /* Camera right, image left: work left-to-right. */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(0U, y);
            for (i = 0; i < 156U; ++i)
                video[dst + i] = video[dst + i + 4U];
        }
        for (y = 0; y < VIEW_H; ++y)
            draw_city_tile(312U, VIEW_Y + y * 8U,
                           scroll_x + VIEW_W - 1U, scroll_y + y);

    } else if (scroll_y == previous_y && previous_x == scroll_x + 1U) {
        /* Camera left, image right: work right-to-left. */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(0U, y);
            for (i = 160U; i > 4U; --i)
                video[dst + i - 1U] = video[dst + i - 5U];
        }
        for (y = 0; y < VIEW_H; ++y)
            draw_city_tile(0U, VIEW_Y + y * 8U, scroll_x, scroll_y + y);

    } else if (scroll_x == previous_x && scroll_y == previous_y + 1U) {
        /* Camera down, image up: copy scanlines top-to-bottom. */
        for (y = VIEW_Y; y < VIEW_Y + (VIEW_H - 1U) * 8U; ++y) {
            dst = vram_offset(0U, y);
            src = vram_offset(0U, y + 8U);
            for (i = 0; i < 160U; ++i)
                video[dst + i] = video[src + i];
        }
        for (x = 0; x < VIEW_W; ++x)
            draw_city_tile(x * 8U, VIEW_Y + (VIEW_H - 1U) * 8U,
                           scroll_x + x, scroll_y + VIEW_H - 1U);

    } else if (scroll_x == previous_x && previous_y == scroll_y + 1U) {
        /* Camera up, image down: copy scanlines bottom-to-top. */
        for (y = VIEW_Y + VIEW_H * 8U - 1U; y >= VIEW_Y + 8U; --y) {
            dst = vram_offset(0U, y);
            src = vram_offset(0U, y - 8U);
            for (i = 0; i < 160U; ++i)
                video[dst + i] = video[src + i];
        }
        for (x = 0; x < VIEW_W; ++x)
            draw_city_tile(x * 8U, VIEW_Y,
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


    /* Draw all 25 text-row-equivalent bands. The map occupies 21 rows. */
    for (x = 0; x < VIEW_W; ++x) {
        solid_tile(x * 8U, 0U, 1);
        solid_tile(x * 8U, 8U, 1);
        solid_tile(x * 8U, 184U, 1);
        solid_tile(x * 8U, 192U, 1);
    }
    for (y = 0; y < VIEW_H; ++y) {
        for (x = 0; x < VIEW_W; ++x) {
            draw_city_tile(x * 8U, VIEW_Y + y * 8U,
                           scroll_x + x, scroll_y + y);
        }
    }
    if (art_data != NULL)
        bios_text(0, 0, "MICROPOLIS TANDY / ORIGINAL 16C ART", 15);
    else
        bios_text(0, 0, "MICROPOLIS TANDY / FALLBACK GLYPHS", 15);
    bios_text(1, 0, "0 VIEW 1 RD 2 WIRE 3 R 4 C 5 I 6 DOZE", 14);
    bios_text(23, 0, "ARROWS PAN  IJKL CURSOR  SPACE  F2 SAVE", 15);
    draw_status();
    draw_cursor();
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

static void keyboard_loop(void)
{
    union REGS r;
    unsigned int prev_x, prev_y, prev_cx, prev_cy;
    unsigned char ascii, scan;
    enum edit_tool old_tool;

    for (;;) {
        r.h.ah = 0; /* BIOS keyboard, no DOS resident TSR required */
        int86(0x16, &r, &r);
        ascii = r.h.al;
        scan = r.h.ah;
        if (ascii == 27U)
            break;

        prev_x = scroll_x;
        prev_y = scroll_y;
        prev_cx = cursor_x;
        prev_cy = cursor_y;
        old_tool = selected_tool;

        if ((scan == 0x4bU || ascii == 'a' || ascii == 'A')
            && scroll_x > 0U)
            --scroll_x;
        else if ((scan == 0x4dU || ascii == 'd' || ascii == 'D')
                 && scroll_x < CITY_WIDTH - VIEW_W)
            ++scroll_x;
        else if ((scan == 0x48U || ascii == 'w' || ascii == 'W')
                 && scroll_y > 0U)
            --scroll_y;
        else if ((scan == 0x50U || ascii == 's' || ascii == 'S')
                 && scroll_y < CITY_HEIGHT - VIEW_H)
            ++scroll_y;
        else if ((ascii == 'j' || ascii == 'J') && cursor_x > 0U)
            --cursor_x;
        else if ((ascii == 'l' || ascii == 'L') && cursor_x < VIEW_W - 1U)
            ++cursor_x;
        else if ((ascii == 'i' || ascii == 'I') && cursor_y > 0U)
            --cursor_y;
        else if ((ascii == 'k' || ascii == 'K') && cursor_y < VIEW_H - 1U)
            ++cursor_y;
        else if (ascii >= '0' && ascii <= '6') {
            if (ascii == '0') selected_tool = EDIT_INSPECT;
            if (ascii == '1') selected_tool = EDIT_ROAD;
            if (ascii == '2') selected_tool = EDIT_POWER;
            if (ascii == '3') selected_tool = EDIT_RESIDENTIAL;
            if (ascii == '4') selected_tool = EDIT_COMMERCIAL;
            if (ascii == '5') selected_tool = EDIT_INDUSTRIAL;
            if (ascii == '6') selected_tool = EDIT_BULLDOZE;
            last_action = "SELECT";
        } else if (ascii == ' ') {
            if (selected_tool == EDIT_INSPECT) {
                last_action = "INSPECT";
            } else if (edit_place(selected_tool,
                                  scroll_x + cursor_x,
                                  scroll_y + cursor_y)) {
                last_action = "PLACED";
                redraw_edit_area();
                draw_cursor();
            } else
                last_action = "BLOCKED";
        } else if (scan == 0x3cU) { /* F2, never overwrite input city. */
            last_action = save_city_new_file() ? "SAVED" : "SAVE ERR";
        }

        if (scroll_x != prev_x || scroll_y != prev_y) {
            /* Remove overlay before shifting VRAM, then redraw the overlay. */
            /* Undo scroll coordinate to redraw old tile before panning. */
            unsigned int next_x = scroll_x, next_y = scroll_y;
            scroll_x = prev_x;
            scroll_y = prev_y;
            erase_cursor();
            scroll_x = next_x;
            scroll_y = next_y;
            scroll_one_tile(prev_x, prev_y);
            draw_cursor();
        } else if (cursor_x != prev_cx || cursor_y != prev_cy) {
            unsigned int next_cx = cursor_x, next_cy = cursor_y;
            cursor_x = prev_cx;
            cursor_y = prev_cy;
            erase_cursor();
            cursor_x = next_cx;
            cursor_y = next_cy;
            draw_cursor();
        }
        if (old_tool != selected_tool || cursor_x != prev_cx ||
            cursor_y != prev_cy || scroll_x != prev_x ||
            scroll_y != prev_y || ascii == ' ' || scan == 0x3cU)
            draw_status();
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!city_load(argv[1])) {
            fprintf(stderr,
                    "Cannot open normal 27120-byte Micropolis city: %s\n",
                    argv[1]);
            return 1;
        }
    } else {
        city_make_demo();
    }

    old_mode = current_video_mode();
    set_video_mode(9);
    if (current_video_mode() != 9) {
        set_video_mode(old_mode);
        fputs("Tandy 1000 graphics mode 9 not available.\n", stderr);
        return 1;
    }
    video = (volatile unsigned char far *)MK_FP(0xb800, 0);
    (void)load_original_art();
    render_full();
    keyboard_loop();
    set_video_mode(old_mode);
    if (art_data != NULL)
        free(art_data); /* art buffer used only in graphics mode */
    return 0;
}
