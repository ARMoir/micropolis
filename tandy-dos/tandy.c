/* SPDX-License-Identifier: GPL-3.0-or-later
 * Micropolis Tandy 1000 EX DOS port: experimental live city builder.
 * Native 320x200x16 rendering, flexible zoning and phased simulation.
 * Full original Micropolis simulation fidelity is still in progress.
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
#include "zoning.h"
#include "placement.h"
#include "simulation.h"
#include "video_ops.h"
#include "menu_font.h"
#include "view_rotation.h"

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
/* View orientation only: tile artwork and the saved world stay fixed. */
static unsigned int map_turns = 0U;
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
static int zone_drag_active = 0;
static int zone_erase_active = 0;
static unsigned int zone_drag_x = 0U, zone_drag_y = 0U;
static int zone_minimap_dirty = 0;
static int build_drag_active = 0;
static unsigned int build_drag_x = 0U, build_drag_y = 0U;
static unsigned int sim_last_clock = 0U;
static int sim_clock_started = 0;

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

/* Same addresses as before, from a 400-byte startup lookup table.
 * Zero division/multiplication in pixel, cursor and tile hot paths.
 * All call sites already validate screen coordinates where needed.
 */
static unsigned int vram_offset(unsigned int x, unsigned int y)
{
    return tandy_scanlines[y] + (x >> 1);
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
    unsigned int row, off;
    unsigned char packed = (unsigned char)((color << 4) | color);
    unsigned short pair = (unsigned short)packed |
                          ((unsigned short)packed << 8);
    volatile unsigned short far *out;
    for (row = 0U; row < 8U; ++row) {
        off = vram_offset(px, py + row);
        out = (volatile unsigned short far *)(video + off);
        out[0] = pair;
        out[1] = pair;
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
    unsigned int row, dst;
    const unsigned short far *art =
        (const unsigned short far *)(art_data + src);
    volatile unsigned short far *out;
    for (row = 0U; row < 8U; ++row) {
        dst = vram_offset(px, py + row);
        out = (volatile unsigned short far *)(video + dst);
        /* 2 aligned word copies per scanline, rather than 4 byte copies.
         * Original TILES.DAT and all overlay graphics remain unchanged.
         */
        out[0] = art[0];
        out[1] = art[1];
        art += 2;
    }
}

/* Zoning is a separate, non-destructive overlay on original Micropolis
 * sprites. Draw only the OUTSIDE edges of adjoining same-type zones.
 * This allows a single 8x8 tile to have a complete green/blue/yellow border.
 */
/* Display coordinates use a rotated virtual 100x120/120x100 map.
 * The existing tile renderer, editing, power and simulation always
 * consume original world coordinates. Identity orientation stays cheap.
 */
static void display_to_world(unsigned int sx, unsigned int sy,
                             unsigned int *wx, unsigned int *wy)
{
    view_rotation_to_world(map_turns,scroll_x+sx,scroll_y+sy,wx,wy);
}

static int world_to_visible(unsigned int wx, unsigned int wy,
                            unsigned int *sx, unsigned int *sy)
{
    unsigned int dx,dy;
    view_rotation_from_world(map_turns,wx,wy,&dx,&dy);
    if (dx<scroll_x || dy<scroll_y ||
        dx-scroll_x>=VIEW_W || dy-scroll_y>=VIEW_H)
        return 0;
    *sx = dx-scroll_x;
    *sy = dy-scroll_y;
    return 1;
}

static void draw_zone_border(unsigned int px, unsigned int py,
                             unsigned int mx, unsigned int my)
{
    unsigned int kind = zoning_at(mx, my);
    unsigned int edge = view_rotation_edges(map_turns,
                                            zoning_edge_mask(mx, my));
    unsigned int x, y;
    unsigned char color;
    if (kind == ZONE_NONE || edge == 0U)
        return;
    color = kind == ZONE_RES ? 10U :
            kind == ZONE_COM ? 9U : 14U;
    if (edge & 1U)
        for (x = 0U; x < 8U; ++x) pixel(px + x, py, color);
    if (edge & 2U)
        for (y = 0U; y < 8U; ++y) pixel(px + 7U, py + y, color);
    if (edge & 4U)
        for (x = 0U; x < 8U; ++x) pixel(px + x, py + 7U, color);
    if (edge & 8U)
        for (y = 0U; y < 8U; ++y) pixel(px, py + y, color);
}

/* 2-bit pending construction state, rendered translucently over existing
 * Micropolis graphics. Only tiles participating in the current drag are
 * touched. Red diagonal markers indicate blocked/out-of-money squares.
 */
static void draw_pending_tile(unsigned int px, unsigned int py,
                              unsigned int mx, unsigned int my)
{
    unsigned int state = placement_at(mx, my);
    unsigned int ix, iy;
    unsigned char tint;
    if (state == PREVIEW_NONE) return;
    if (state == PREVIEW_FUNDED) {
        if (selected_tool == EDIT_RESIDENTIAL) tint = 10U;
        else if (selected_tool == EDIT_COMMERCIAL) tint = 9U;
        else if (selected_tool == EDIT_INDUSTRIAL) tint = 14U;
        else if (selected_tool == EDIT_POWER) tint = 11U;
        else tint = 7U; /* ghost gray road */
        /* Alternating translucent-looking stipple keeps original tile
         * graphics readable without extra off-screen compositing RAM.
         */
        for (iy = 1U; iy < 7U; iy += 2U)
            for (ix = 1U + ((iy >> 1U) & 1U); ix < 7U; ix += 2U)
                pixel(px + ix, py + iy, tint);
        for (ix = 0U; ix < 8U; ix += 2U) {
            pixel(px + ix, py, tint);
            pixel(px + ix, py + 7U, tint);
        }
        for (iy = 0U; iy < 8U; iy += 2U) {
            pixel(px, py + iy, tint);
            pixel(px + 7U, py + iy, tint);
        }
    } else {
        tint = state == PREVIEW_NO_FUNDS ? 12U : 4U;
        for (ix = 0U; ix < 8U; ++ix) {
            pixel(px + ix, py + ix, tint);
            pixel(px + 7U - ix, py + ix, tint);
        }
    }
}

/* A classic original building is composed of 3x3 or 4x4 individual
 * Micropolis graphic tiles. Simply rotating those positions scrambles
 * the picture. Keep each bitmap upright and select the corresponding
 * ORIGINAL tile art at its new screen-relative position instead.
 * This touches the renderer only when the player rotates the camera;
 * simulation data, roads, zones and .CTY tile IDs are never changed.
 */
static unsigned int upright_building_graphic(unsigned int wx,
                                              unsigned int wy,
                                              unsigned int graphic)
{
    unsigned int first=0U,span=0U,part,center_id;
    unsigned int center_offset;
    int center_x,center_y;
    unsigned short center;
    if (map_turns == 0U)
        return graphic;
    if (graphic >= 240U && graphic <= 248U) {
        first=240U; span=3U;
    } else if (graphic >= 261U && graphic <= 404U) {
        first=261U + ((graphic-261U)/9U)*9U; span=3U;
    } else if (graphic >= 423U && graphic <= 431U) {
        first=423U; span=3U;
    } else if (graphic >= 432U && graphic <= 611U) {
        first=432U + ((graphic-432U)/9U)*9U; span=3U;
    } else if (graphic >= 612U && graphic <= 620U) {
        first=612U; span=3U;
    } else if (graphic >= 621U && graphic <= 692U) {
        first=621U + ((graphic-621U)/9U)*9U; span=3U;
    } else if (graphic >= 745U && graphic <= 760U) {
        first=745U; span=4U; /* coal plant */
    } else if (graphic >= 811U && graphic <= 826U) {
        first=811U; span=4U; /* nuclear plant */
    } else {
        return graphic; /* normal one-tile sprites remain upright */
    }
    part=graphic-first;
    center_offset=span == 3U ? 1U : 1U; /* original zone flag center */
    center_x=(int)wx + (int)center_offset - (int)(part%span);
    center_y=(int)wy + (int)center_offset - (int)(part/span);
    if (center_x<0 || center_y<0 || center_x>=CITY_WIDTH ||
        center_y>=CITY_HEIGHT)
        return graphic;
    center=city_at((unsigned int)center_x,(unsigned int)center_y);
    center_id=first+center_offset*span+center_offset;
    if (!(center & 1024U) || (center & TILE_MASK) != center_id)
        return graphic;
    return first+view_rotation_upright_part(map_turns,span,part);
}

static void draw_city_tile(unsigned int px, unsigned int py,
                           unsigned int mx, unsigned int my)
{
    unsigned short t = city_at(mx, my) & TILE_MASK;
    unsigned char bg = 2;
    unsigned int i;
    int v, h;

    if (art_data != NULL && t < ART_TILES) {
        unsigned int graphic = t;
        /* For an empty 1x1 lot, use ORIGINAL Micropolis small tile art.
         * Residential has actual detached house tiles. Commercial and
         * industrial use their original empty-zone center symbols until
         * the new small-building growth rules are implemented.
         * Never change the underlying .CTY sprite or map tile itself.
         */
        if (t == 0U || (t >= 21U && t <= 43U)) {
            unsigned int kind = zoning_at(mx, my);
            unsigned int stage = zoning_development_at(mx, my);
            /* The small-lot simulator changes ONLY the art displayed in
             * custom-zoned empty/wooded squares. Original 3x3 .CTY
             * building sprites are never overwritten or reinterpreted.
             * Higher stages reuse suitable center tiles of the original
             * 3x3 commercial/industrial building art.
             */
            if (kind == ZONE_RES) {
                static const unsigned short res[4] = {
                    244U, 249U, 252U, 258U
                };
                graphic = res[stage];
            } else if (kind == ZONE_COM) {
                static const unsigned short com[4] = {
                    427U, 440U, 449U, 458U
                };
                graphic = com[stage];
            } else if (kind == ZONE_IND) {
                static const unsigned short ind[4] = {
                    616U, 629U, 638U, 647U
                };
                graphic = ind[stage];
            }
        }
        draw_original_tile(px,py,
                           upright_building_graphic(mx,my,graphic));
        draw_zone_border(px, py, mx, my);
        draw_pending_tile(px, py, mx, my);
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
    /* Fallback artwork retains the same zone boundaries as TILES.DAT. */
    draw_zone_border(px, py, mx, my);
    draw_pending_tile(px, py, mx, my);
}

/* AH=0Eh teletype can SCROLL the ENTIRE graphics screen when its
 * cursor wraps at column 40 / row 24. That was corrupting the Tandy
 * toolbar, top bar and city graphics as status text changed.
 * AH=09h writes ONE glyph at a fixed cursor without auto-advancing.
 * Explicit positioning also guarantees no accidental screen scroll.
 */
static void draw_display_tile(unsigned int sx, unsigned int sy)
{
    unsigned int wx,wy;
    display_to_world(sx,sy,&wx,&wy);
    draw_city_tile(VIEW_X+sx*8U,VIEW_Y+sy*8U,wx,wy);
}

static void bios_text(unsigned int row, unsigned int column,
                      const char *s, unsigned char color)
{
    union REGS r;
    if (row >= 25U || column >= 40U)
        return;
    while (*s && column < 40U) {
        r.h.ah = 2U;
        r.h.bh = 0U;
        r.h.dh = (unsigned char)row;
        r.h.dl = (unsigned char)column;
        int86(0x10, &r, &r);
        r.h.ah = 9U;
        r.h.al = (unsigned char)*s++;
        r.h.bh = 0U;
        r.h.bl = color;
        r.x.cx = 1U;
        int86(0x10, &r, &r);
        ++column;
    }
}

/* SimCity-style compact toolbox, custom-drawn in Tandy mode 9.
 * All of these pixels are OUTSIDE the map's VRAM scrolling rectangle.
 */
/* Indices are stable DOS numeric tool shortcuts, NOT visual buttons.
 * Prefer recognizable occupied RCI sprites over empty-zone centers.
 * Inspect and Bulldoze use native 16x16 tool pictograms below.
 */
static const unsigned short tool_icons[8] = {
    0U, 66U, 210U, 249U, 449U, 638U, 44U, 750U
};
static const char *const tool_labels[8] = {
    "INS", "RD", "PWR", "RES", "COM", "IND", "DOZ", "PLT"
};

/* Small authentic 16-color world overview below the compact tools.
 * Cache the previous camera outline so panning only restores/draws its
 * border, instead of repainting thousands of pixels each keypress.
 */
static unsigned int overview_old_x, overview_old_y;
static unsigned int overview_old_w, overview_old_h;
static int overview_valid = 0;

static unsigned char overview_color(unsigned int px, unsigned int py)
{
    unsigned int mx,my;
    unsigned int dx = px * view_rotation_width(map_turns) / UI_MINIMAP_W;
    unsigned int dy = py * view_rotation_height(map_turns) / UI_MINIMAP_H;
    view_rotation_to_world(map_turns,dx,dy,&mx,&my);
    unsigned int id = city_at(mx, my) & TILE_MASK;
    unsigned int kind = zoning_at(mx, my);
    unsigned char color = 2U;
    if (id >= 1U && id <= 20U) color = 1U;
    else if (id >= 21U && id <= 43U) color = 10U;
    else if (id >= 64U && id <= 207U) color = 8U;
    else if (id >= 208U && id <= 239U) color = 7U;
    else if (id >= 240U && id <= 422U) color = 10U;
    else if (id >= 423U && id <= 611U) color = 9U;
    else if (id >= 612U && id <= 692U) color = 6U;
    else if (id >= 693U) color = 15U;
    if (kind == ZONE_RES) color = 10U;
    if (kind == ZONE_COM) color = 9U;
    if (kind == ZONE_IND) color = 14U;
    return color;
}

static void overview_restore_outline(unsigned int x, unsigned int y,
                                      unsigned int width, unsigned int height)
{
    unsigned int p;
    for (p = 0U; p < width; ++p) {
        pixel(UI_MINIMAP_X + x + p, UI_MINIMAP_Y + y,
              overview_color(x+p,y));
        pixel(UI_MINIMAP_X + x + p, UI_MINIMAP_Y + y + height-1U,
              overview_color(x+p,y+height-1U));
    }
    for (p = 0U; p < height; ++p) {
        pixel(UI_MINIMAP_X + x, UI_MINIMAP_Y + y+p,
              overview_color(x,y+p));
        pixel(UI_MINIMAP_X + x + width-1U, UI_MINIMAP_Y + y+p,
              overview_color(x+width-1U,y+p));
    }
}

static void overview_draw_outline(unsigned int x, unsigned int y,
                                   unsigned int width, unsigned int height)
{
    unsigned int p;
    for (p = 0U; p < width; ++p) {
        pixel(UI_MINIMAP_X + x + p,UI_MINIMAP_Y + y,14U);
        pixel(UI_MINIMAP_X + x + p,UI_MINIMAP_Y + y + height-1U,14U);
    }
    for (p = 0U; p < height; ++p) {
        pixel(UI_MINIMAP_X + x,UI_MINIMAP_Y + y + p,14U);
        pixel(UI_MINIMAP_X + x + width-1U,UI_MINIMAP_Y + y + p,14U);
    }
}

static void draw_overview_camera(void)
{
    unsigned int world_w = view_rotation_width(map_turns);
    unsigned int world_h = view_rotation_height(map_turns);
    unsigned int x = scroll_x * UI_MINIMAP_W / world_w;
    unsigned int y = scroll_y * UI_MINIMAP_H / world_h;
    unsigned int w = VIEW_W * UI_MINIMAP_W / world_w;
    unsigned int h = VIEW_H * UI_MINIMAP_H / world_h;
    if (w < 2U) w = 2U;
    if (h < 2U) h = 2U;
    if (overview_valid && overview_old_x == x && overview_old_y == y &&
        overview_old_w == w && overview_old_h == h)
        return;
    if (overview_valid)
        overview_restore_outline(overview_old_x,overview_old_y,
                                 overview_old_w,overview_old_h);
    overview_draw_outline(x,y,w,h);
    overview_old_x = x;
    overview_old_y = y;
    overview_old_w = w;
    overview_old_h = h;
    overview_valid = 1;
}

static void draw_overview(void)
{
    unsigned int px, py;
    for (py = 0U; py < UI_MINIMAP_H; ++py)
        for (px = 0U; px < UI_MINIMAP_W; ++px)
            pixel(UI_MINIMAP_X + px,UI_MINIMAP_Y + py,
                  overview_color(px,py));
    overview_valid = 0;
    draw_overview_camera();
}

/* Only the selected tool's TWO affected buttons need repainting.
 * A drag commit never repaints the 64-pixel-wide sidebar controls;
 * it updates the minimap and top treasury figures only.
 */
/* The keyboard numbers are intentionally stable even though the
 * arrangement follows the original SimCity's bulldozer-first palette:
 * DOZ / Road, Power / Res, Com / Ind, Inspect / Plant.
 */
static unsigned int tool_key_from_enum(enum edit_tool tool)
{
    if (tool == EDIT_BULLDOZE) return 6U;
    if (tool == EDIT_COAL) return 7U;
    return (unsigned int)tool;
}

static unsigned int tool_button_index(enum edit_tool tool)
{
    return ui_toolbar_button(tool_key_from_enum(tool));
}

static unsigned char toolbar_accent(unsigned int tool)
{
    switch (tool) {
    case 3U: return 10U; /* green residential */
    case 4U: return 9U;  /* blue commercial */
    case 5U: return 14U; /* yellow industrial */
    case 6U: return 12U; /* red/orange demolition */
    case 2U: return 11U; /* pale cyan electric utility */
    case 1U: return 8U;  /* asphalt */
    case 7U: return 6U;  /* brown coal */
    default: return 15U;
    }
}

/* These two actions have no recognisable city MAP sprites, so draw
 * genuine 16x16 pixel pictograms rather than a grass/rubble tile.
 * Everything is calculated only when a tool button is repainted.
 */
static void draw_tool_pictogram(unsigned int tool,
                                unsigned int px, unsigned int py)
{
    unsigned int x, y;
    if (tool == 6U) { /* yellow bulldozer, black tracks, steel blade */
        box_pixels(px+3U,py+11U,11U,3U,0U);
        box_pixels(px+4U,py+12U,9U,1U,8U);
        box_pixels(px+4U,py+6U,10U,5U,14U);
        box_pixels(px+8U,py+2U,5U,5U,14U);
        box_pixels(px+9U,py+3U,3U,3U,9U);
        box_pixels(px+1U,py+7U,2U,7U,8U);
        box_pixels(px+1U,py+11U,3U,3U,15U);
        pixel(px+6U,py+10U,6U);
        pixel(px+11U,py+10U,6U);
    } else if (tool == 0U) { /* inspector magnifying glass */
        for (y=2U;y<11U;++y)
            for (x=2U;x<11U;++x) {
                int dx = (int)x-6;
                int dy = (int)y-6;
                int d = dx*dx+dy*dy;
                if (d <= 20 && d >= 12)
                    pixel(px+x,py+y,0U);
                else if (d < 12)
                    pixel(px+x,py+y,11U);
            }
        box_pixels(px+10U,py+10U,2U,2U,0U);
        box_pixels(px+11U,py+12U,3U,2U,6U);
        pixel(px+12U,py+14U,0U);
    }
}

/* Keep original tile artwork for road, power, housing, commerce,
 * industry and plant. The active icon stays crisply identifiable in
 * the narrow panel, and no extra graphic assets need shipping.
 */
static void draw_tool_art(unsigned int tool, unsigned int px,
                          unsigned int py)
{
    unsigned int ix,iy,src;
    unsigned char packed,color;
    if (tool == 0U || tool == 6U) {
        draw_tool_pictogram(tool,px,py);
        return;
    }
    if (art_data == NULL)
        return;
    src = tool_icons[tool]*32U;
    for (iy=0U;iy<8U;++iy)
        for (ix=0U;ix<8U;++ix) {
            packed=art_data[src+iy*4U+(ix>>1)];
            color=(unsigned char)((ix&1U) ?
                                  (packed&15U) : (packed>>4));
            box_pixels(px+ix*2U,py+iy*2U,2U,2U,color);
        }
}

static void draw_tool_button(unsigned int n)
{
    unsigned int bx,by,x,y,tool;
    unsigned char accent;
    int selected;
    if (n >= 8U)
        return;
    tool = ui_toolbar_tool(n);
    selected = tool_button_index(selected_tool) == n;
    accent = toolbar_accent(tool);
    bx = 1U+(n&1U)*(UI_TOOLBOX_W/2U);
    by = UI_BUTTON_TOP+(n>>1)*UI_BUTTON_SIZE;
    /* 30x24 beveled icon buttons, styled after both SimCity examples.
     * No opaque BIOS label erases a lower portion of an icon.
     * Selected button is 'pressed', outlined in yellow.
     */
    box_pixels(bx,by,30U,24U,0U);          /* dark outside edge */
    box_pixels(bx+1U,by+1U,28U,22U,
               (unsigned char)(selected?8U:7U));
    for (x=1U;x<29U;++x) {
        pixel(bx+x,by+1U,(unsigned char)(selected?14U:15U));
        pixel(bx+x,by+22U,(unsigned char)(selected?14U:8U));
    }
    for (y=1U;y<23U;++y) {
        pixel(bx+1U,by+y,(unsigned char)(selected?14U:15U));
        pixel(bx+28U,by+y,(unsigned char)(selected?14U:8U));
    }
    draw_tool_art(tool,bx+(selected?8U:7U),by+(selected?4U:3U));
    /* Classic R/C/I colors; other tools get restrained accents. */
    box_pixels(bx+3U,by+20U,24U,2U,accent);
}

/* These two narrow sidebar headings share the transparent 5x7
 * graphics font used for the now-fixed top menu. BIOS AH=09 would
 * create unwanted black rectangles under the glyphs in mode 9.
 */
static void draw_sidebar_label(unsigned int x, unsigned int y,
                                const char *message)
{
    unsigned int row, col;
    while (*message) {
        unsigned char ch = (unsigned char)*message++;
        for (row=0U;row<7U;++row) {
            unsigned int bits = menu_font_row(ch,row);
            for (col=0U;col<5U;++col)
                if (bits & (1U << (4U-col)))
                    pixel(x+col,y+row,15U);
        }
        x+=6U;
    }
}

static void draw_toolbox(void)
{
    unsigned int x, y, n;
    for (y = UI_MAP_Y; y < UI_FOOTER_Y; y += 8U)
        for (x = 0U; x < UI_TOOLBOX_W; x += 8U)
            solid_tile(x,y,7U);
    for (y = UI_MAP_Y; y < UI_FOOTER_Y; ++y) {
        pixel(UI_TOOLBOX_W-2U,y,8U);
        pixel(UI_TOOLBOX_W-1U,y,0U);
    }
    box_pixels(2U,16U,59U,8U,1U);
    draw_sidebar_label(16U,17U,"TOOLS");
    for (n = 0U; n < 8U; ++n)
        draw_tool_button(n);
    box_pixels(2U,128U,59U,8U,8U);
    draw_sidebar_label(24U,129U,"MAP");
    draw_overview();
}

/* BIOS glyph rendering is slower than changing pixels directly.
 * Cache the city stats and both 40-column debug rows. Repaint ONLY
 * characters whose contents change. Particularly important with the 7 MHz V20
 * and an actively moving serial mouse.
 */
static char city_shadow[41], debug_shadow[41], status_shadow[41];
static int city_valid = 0, debug_valid = 0, status_valid = 0;

static void draw_status_text(unsigned int row, const char *message,
                             char *shadow, int *valid,
                             unsigned char color)
{
    unsigned int col;
    char character[2];
    character[1] = '\0';
    for (col = 0U; col < 40U; ++col) {
        unsigned char current = (unsigned char)*message;
        if (*message) ++message;
        if (!current) current = ' ';
        if (!*valid || shadow[col] != (char)current) {
            solid_tile(col*8U,row*8U,1U);
            if (current != ' ') {
                character[0] = (char)current;
                bios_text(row,col,character,color);
            }
            shadow[col] = (char)current;
        }
    }
    shadow[40] = '\0';
    *valid = 1;
}

/* City statistics belong immediately beneath the top menu. Keeping
 * this row independent of the map and sidebar means the monthly update
 * only touches glyphs that actually changed.
 */
static void draw_city_stats(void)
{
    char summary[80];
    const struct sim_statistics *st = simulation_stats();
    static const char *const months[12] = {
        "JAN","FEB","MAR","APR","MAY","JUN",
        "JUL","AUG","SEP","OCT","NOV","DEC"
    };
    sprintf(summary,"%s %u POP:%lu JOB:%lu $%ld",
            months[st->month-1U],st->year,st->residents,
            st->commercial_jobs+st->industrial_jobs,city_get_funds());
    draw_status_text(1U,summary,city_shadow,&city_valid,15U);
}

/* Redraw only the moving edge.  For a one-tile scroll, use a direct
 * 8-pixel (four-byte) VRAM shift rather than re-rendering 672 map tiles.
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
    draw_display_tile(cursor_x,cursor_y);
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
            draw_display_tile(x,y);
}

/* Repaint the changed zone cells plus immediate neighbors, because
 * adjacent same-zone boundaries join or disappear on every brush stroke.
 * The world zone and city graphics are never modified during a repaint.
 */
static void draw_status(void);

static void redraw_zoning_brush(unsigned int x0, unsigned int y0,
                                unsigned int x1, unsigned int y1)
{
    unsigned int left, right, top, bottom, x, y, mx, my;
    left = x0 < x1 ? x0 : x1;
    right = x0 > x1 ? x0 : x1;
    top = y0 < y1 ? y0 : y1;
    bottom = y0 > y1 ? y0 : y1;
    if (left > 0U) --left;
    if (top > 0U) --top;
    if (right + 1U < CITY_WIDTH) ++right;
    if (bottom + 1U < CITY_HEIGHT) ++bottom;
    for (mx = left; mx <= right; ++mx)
        for (my = top; my <= bottom; ++my)
            if (world_to_visible(mx,my,&x,&y))
                draw_city_tile(VIEW_X+x*8U,VIEW_Y+y*8U,mx,my);
    draw_cursor();
}

static unsigned int selected_zone_kind(void)
{
    if (selected_tool == EDIT_RESIDENTIAL) return ZONE_RES;
    if (selected_tool == EDIT_COMMERCIAL) return ZONE_COM;
    if (selected_tool == EDIT_INDUSTRIAL) return ZONE_IND;
    return ZONE_NONE;
}

static void apply_zone_stroke(unsigned int x0, unsigned int y0,
                              unsigned int x1, unsigned int y1,
                              unsigned int kind)
{
    unsigned int changed = zoning_paint_line(x0, y0, x1, y1, kind);
    if (changed) {
        redraw_zoning_brush(x0, y0, x1, y1);
        last_action = kind == ZONE_NONE ? "UNZONED" : "ZONED";
        zone_minimap_dirty = 1;
    } else {
        last_action = "BLOCKED";
    }
    draw_status();
}

/* Bottom row 23 is input/debug state, row 24 is the latest action,
 * simulation diagnostic or the live construction preview. Statistics
 * (date, population, employment and funds) never appear here.
 */
static void draw_status(void)
{
    char debug[80], status[80];
    unsigned int wx,wy;
    const struct sim_statistics *st = simulation_stats();
    display_to_world(cursor_x,cursor_y,&wx,&wy);
    if (mouse_on) {
        sprintf(debug,"TOOL:%s MAP:%03u,%03u M:%03u,%03u",
                tool_labels[tool_key_from_enum(selected_tool)],
                wx,wy,mouse_x,mouse_y);
    } else {
        sprintf(debug,"TOOL:%s MAP:%03u,%03u M:OFF",
                tool_labels[tool_button_index(selected_tool)],wx,wy);
    }
    draw_status_text(23U,debug,debug_shadow,&debug_valid,11U);
    if (placement_active()) {
        sprintf(status,"C:$%ld LEFT:$%ld +%u -%u",
                placement_cost(),placement_remaining(),
                placement_count(),placement_rejected());
    } else {
        sprintf(status,"G:%u R:%u X%u %s %s",
                st->growth_total,st->lots_ready,
                simulation_is_paused() ? 0U : simulation_speed(),
                simulation_is_paused() ? "PAUSE" : "RUN",
                last_action);
    }
    draw_status_text(24U,status,status_shadow,&status_valid,11U);
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
            solid_tile((origin + col) * 8U, (2U + j) * 8U,
                       (unsigned char)(j == menu_highlight ? 5U : 1U));
        bios_text(2U + j, origin + 1U, ui_menu_item(open_menu, j),
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
        bios_text(11U, 3U, "SPACE         BUILD 1 TILE", 14U);
        bios_text(12U, 3U, "G  PAUSE   +/-  SPEED", 14U);
        bios_text(13U, 3U, "F2 SAVE     P POWER SCAN", 14U);
        bios_text(14U, 3U, "G=GROWTH  R=READY LOTS", 14U);
        bios_text(15U, 3U, "r CW   SHIFT+R CCW", 14U);
        bios_text(16U, 3U, "ESC / ANY KEY TO RETURN", 15U);
    } else if (info_page == 2) {
        bios_text(6U, 3U, "MOUSE SUPPORT: DOS INT 33H", 15U);
        bios_text(8U, 3U, "RUN A SERIAL MOUSE DRIVER", 14U);
        bios_text(9U, 3U, "BEFORE STARTING MICROPOL", 14U);
        bios_text(11U, 3U, "LEFT DRAG: PREVIEW BUILD", 14U);
        bios_text(12U, 3U, "RELEASE: BUY / BUILD", 14U);
        bios_text(13U, 3U, "RIGHT: ERASE ZONE / TOOLS", 14U);
        bios_text(14U, 3U, "KEYBOARD ALWAYS WORKS", 14U);
        bios_text(16U, 3U, "ESC / CLICK TO RETURN", 15U);
    } else {
        bios_text(6U, 3U, "MICROPOLIS TANDY 1000 DOS PORT", 15U);
        bios_text(8U, 3U, "NATIVE 8086 / V20, 16 COLORS", 14U);
        bios_text(9U, 3U, "MICROPOLIS GPLv3+ SOURCE", 14U);
        bios_text(10U, 3U, "ORIGINAL ART: ELECTRONIC ARTS", 14U);
        bios_text(12U, 3U, "LIVE PHASED RCI SIMULATION", 14U);
        bios_text(13U, 3U, "TRAFFIC/DISASTERS PENDING", 14U);
        bios_text(16U, 3U, "ESC / CLICK TO RETURN", 15U);
    }
}

static void render_full(void);

static void scroll_one_tile(unsigned int previous_x, unsigned int previous_y)
{
    unsigned int x, y, dst, src;

    if (scroll_y == previous_y && scroll_x == previous_x + 1U) {
        /* Camera right, image left. Source and destination overlap,
         * so always copy forward from left to right.
         */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(VIEW_X, y);
            tandy_move_words(video,dst,dst+4U,
                             (VIEW_W*4U-4U)/2U,0);
        }
        for (y = 0U; y < VIEW_H; ++y)
            draw_display_tile(VIEW_W-1U,y);

    } else if (scroll_y == previous_y && previous_x == scroll_x + 1U) {
        /* Camera left, image right: copy backward to retain overlapping
         * source bytes; original bytewise result is preserved exactly.
         */
        for (y = VIEW_Y; y < VIEW_Y + VIEW_H * 8U; ++y) {
            dst = vram_offset(VIEW_X, y);
            tandy_move_words(video,dst+4U,dst,
                             (VIEW_W*4U-4U)/2U,1);
        }
        for (y = 0U; y < VIEW_H; ++y)
            draw_display_tile(0U,y);

    } else if (scroll_x == previous_x && scroll_y == previous_y + 1U) {
        /* Camera down, image up: each four-bank scanline is 128 bytes
         * wide. Copy full rows in existing top-to-bottom order.
         */
        for (y = VIEW_Y; y < VIEW_Y + (VIEW_H-1U)*8U; ++y) {
            dst = vram_offset(VIEW_X,y);
            src = vram_offset(VIEW_X,y+8U);
            tandy_move_words(video,dst,src,VIEW_W*2U,0);
        }
        for (x = 0U; x < VIEW_W; ++x)
            draw_display_tile(x,VIEW_H-1U);

    } else if (scroll_x == previous_x && previous_y == scroll_y + 1U) {
        /* Camera up, image down: work bottom-to-top. */
        for (y = VIEW_Y+VIEW_H*8U-1U; y >= VIEW_Y+8U; --y) {
            dst = vram_offset(VIEW_X,y);
            src = vram_offset(VIEW_X,y-8U);
            tandy_move_words(video,dst,src,VIEW_W*2U,0);
        }
        for (x = 0U; x < VIEW_W; ++x)
            draw_display_tile(x,0U);
    } else {
        render_full();
        return;
    }
}

/* Top navigation must be rendered transparently over cyan VRAM.
 * BIOS INT 10h/AH=09h writes an OPAQUE black background for each
 * character in Tandy graphics mode 9, which hid SYSTEM/TOOLS/VIEW/HELP
 * and left only the last 88 pixels of the cyan strip visible.
 * Use a tiny 5x7 bitmap font (84 bytes) for these fixed captions,
 * preserving the 40-column alignment and existing mouse hitboxes.
 */
static void draw_menu_caption(unsigned int column, const char *caption)
{
    unsigned int x, y;
    while (*caption != '\0' && column < 40U) {
        unsigned char ch = (unsigned char)*caption++;
        for (y = 0U; y < 7U; ++y) {
            unsigned int bits = menu_font_row(ch,y);
            for (x = 0U; x < 5U; ++x)
                if ((bits & (1U << (4U-x))) != 0U)
                    pixel(column*8U + 1U + x,y,0U);
        }
        ++column;
    }
}

static void draw_top_menu_bar(void)
{
    unsigned int x;
    for (x = 0U; x < SCREEN_W; x += 8U)
        solid_tile(x,0U,3U);
    draw_menu_caption(1U,"SYSTEM");
    draw_menu_caption(10U,"TOOLS");
    draw_menu_caption(18U,"VIEW");
    draw_menu_caption(25U,"HELP");
}

static void render_full(void)
{
    unsigned int x, y;
    mouse_pointer_hide();

    /* The graphics-mode BIOS text API previously erased the cyan bar
     * underneath all 29 menu characters. Transparent drawing fixes it.
     */
    draw_top_menu_bar();

    /* City is a fixed rectangular window to the right of the toolbox. */
    for (y = 0U; y < VIEW_H; ++y)
        for (x = 0U; x < VIEW_W; ++x)
            draw_display_tile(x,y);

    draw_toolbox();
    city_valid = debug_valid = status_valid = 0;
    draw_city_stats();
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
    char cityname[13], zonename[13];
    FILE *f;

    /* DOS 8.3 names, skip a slot if EITHER file exists. Don't clobber
     * an orphan .ZON file left by a failed previous save operation.
     */
    for (slot = 1U; slot < 10000U; ++slot) {
        sprintf(cityname, "CITY%04u.CTY", slot);
        sprintf(zonename, "CITY%04u.ZON", slot);
        f = fopen(cityname, "rb");
        if (f != NULL) {
            fclose(f);
            continue;
        }
        f = fopen(zonename, "rb");
        if (f != NULL) {
            fclose(f);
            continue;
        }
        if (!city_save_as(cityname))
            return 0;
        if (!zoning_save(zonename)) {
            last_action = "ZON ERR";
            return 0;
        }
        return 1;
    }
    return 0;
}

/* If another tool/menu is chosen while the mouse is held, the unfinished
 * preview must be cancelled rather than charged by its later release.
 */
static void build_preview_cancel(void);

/* Map number-row shortcuts, toolbar, and Tools menu onto one function. */
static void select_tool(unsigned int number)
{
    unsigned int previous = tool_button_index(selected_tool);
    if (build_drag_active)
        build_preview_cancel();
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
    if (previous != tool_button_index(selected_tool)) {
        draw_tool_button(previous);
        draw_tool_button(tool_button_index(selected_tool));
    }
}

static void preview_refresh(unsigned int x0, unsigned int y0,
                            unsigned int x1, unsigned int y1)
{
    /* Repaint only stroke neighborhood; the translucent preview is
     * composited by draw_city_tile and never edits the map.
     */
    redraw_zoning_brush(x0, y0, x1, y1);
    draw_status();
}

static void build_preview_start(unsigned int x, unsigned int y)
{
    placement_begin(selected_tool);
    build_drag_active = placement_active();
    if (!build_drag_active)
        return;
    build_drag_x = x;
    build_drag_y = y;
    (void)placement_extend(x, y, x, y);
    preview_refresh(x, y, x, y);
}

static void build_preview_move(unsigned int x, unsigned int y)
{
    unsigned int prev_x, prev_y;
    if (!build_drag_active)
        return;
    prev_x = build_drag_x;
    prev_y = build_drag_y;
    (void)placement_extend(prev_x, prev_y, x, y);
    build_drag_x = x;
    build_drag_y = y;
    preview_refresh(prev_x, prev_y, x, y);
}

static void build_preview_finish(void)
{
    unsigned int x0, y0, x1, y1, applied;
    if (!build_drag_active)
        return;
    placement_bounds(&x0, &y0, &x1, &y1);
    applied = placement_commit();
    build_drag_active = 0;
    last_action = applied ? "BUILT" : "BLOCKED";
    redraw_zoning_brush(x0, y0, x1, y1);
    if (applied != 0U) {
        /* Zoning-only edits cannot alter conductor flags. Only actual
         * roads/wires (or the legacy bulldoze/coal path) invalidate the
         * expensive full-city electricity map.
         */
        if (selected_tool == EDIT_ROAD || selected_tool == EDIT_POWER)
            simulation_mark_power_dirty();
        /* Static minimap and sidebar funds are only repainted once
         * per completed drag, not on each V20 mouse polling event.
         */
        draw_overview();
        draw_city_stats();
    }
    draw_status();
}

static void build_preview_cancel(void)
{
    unsigned int x0, y0, x1, y1;
    if (!build_drag_active)
        return;
    placement_bounds(&x0, &y0, &x1, &y1);
    placement_cancel();
    build_drag_active = 0;
    redraw_zoning_brush(x0, y0, x1, y1);
    last_action = "CANCELLED";
    draw_status();
}

static void apply_selected_tool(void)
{
    unsigned int kind = selected_zone_kind();
    unsigned int mx,my;
    int erased = 0;
    display_to_world(cursor_x,cursor_y,&mx,&my);
    if (kind != ZONE_NONE || selected_tool == EDIT_ROAD ||
        selected_tool == EDIT_POWER) {
        build_preview_start(mx, my);
        build_preview_finish();
        return;
    }
    if (selected_tool == EDIT_INSPECT) {
        last_action = "INSPECT";
    } else {
        if (selected_tool == EDIT_BULLDOZE)
            erased = zoning_set(mx, my, ZONE_NONE);
        if (edit_place(selected_tool, mx, my)) {
            simulation_mark_power_dirty();
            last_action = "PLACED";
            redraw_edit_area();
            draw_cursor();
            draw_overview();
        } else if (erased) {
            last_action = "UNZONED";
        } else {
            last_action = "BLOCKED";
        }
        if (erased) {
            redraw_zoning_brush(mx, my, mx, my);
            draw_overview();
        }
    }
    draw_status();
}

/* Rotate the camera about the selected WORLD square, never the city
 * map itself. Render original TILES.DAT sprites upright in new positions.
 * The 120x100 map becomes a 100x120 virtual map at 90/270 degrees.
 */
static void rotate_view(int clockwise)
{
    unsigned int wx,wy;
    if (build_drag_active)
        build_preview_cancel(); /* never change view mid-purchase */
    zone_erase_active = 0;
    display_to_world(cursor_x,cursor_y,&wx,&wy);
    map_turns = (map_turns + (clockwise ? 1U : 3U)) & 3U;
    view_rotation_focus(map_turns,wx,wy,VIEW_W,VIEW_H,
                        &scroll_x,&scroll_y,&cursor_x,&cursor_y);
    zone_minimap_dirty = 0;
    last_action = clockwise ? "ROT CW" : "ROT CCW";
    /* An explicit quarter-turn costs one full redraw. Normal panning
     * still uses the fast one-tile VRAM scrolling branches.
     */
    render_full();
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
    case UI_ROTATE_CW: rotate_view(1); break;
    case UI_ROTATE_CCW: rotate_view(0); break;
    case UI_HELP_KEYS: info_page = 1; break;
    case UI_HELP_MOUSE: info_page = 2; break;
    case UI_HELP_ABOUT: info_page = 3; break;
    default: break;
    }
}

static void show_menu(enum ui_menu menu)
{
    if (build_drag_active)
        build_preview_cancel();
    open_menu = menu;
    menu_highlight = 0U;
    render_full();
}

/* Mouse movement paints no pixels besides a tiny 8x8 arrow, unless it
 * crosses into a new map tile. Dragging only edits single-tile tools,
 * preventing accidental 3x3/4x4 buildings during a mouse stroke.
 */
/* Roads, wires and RCI zones now share the mouse preview transaction;
 * bulldoze continues as immediate single-cell erasing.
 */
static void handle_mouse(void)
{
    struct mouse_state state;
    unsigned int next_x, next_y, world_x, world_y;
    int menu, item, toolbar;
    int left_press, right_press, left_release, right_release, moved, inside;

    if (!mouse_poll(&state))
        return;
    left_press = (state.buttons & 1U) && !(mouse_buttons & 1U);
    right_press = (state.buttons & 2U) && !(mouse_buttons & 2U);
    left_release = !(state.buttons & 1U) && (mouse_buttons & 1U);
    right_release = !(state.buttons & 2U) && (mouse_buttons & 2U);
    moved = state.x != mouse_x || state.y != mouse_y;
    inside = state.x >= VIEW_X && state.y >= VIEW_Y &&
             state.y < VIEW_Y + VIEW_H * 8U;

    if (moved) {
        mouse_pointer_hide();
        mouse_x = state.x;
        mouse_y = state.y;
        if (info_page == 0 && open_menu == MENU_NONE && inside) {
            next_x = (state.x - VIEW_X) / 8U;
            next_y = (state.y - VIEW_Y) / 8U;
            if (next_x != cursor_x || next_y != cursor_y) {
                erase_cursor();
                cursor_x = next_x;
                cursor_y = next_y;
                draw_cursor();
                display_to_world(cursor_x,cursor_y,&world_x,&world_y);
                if (build_drag_active && (state.buttons & 1U ||
                                         left_release)) {
                    /* Mouse polling on a 7 MHz V20 can skip cells.
                     * The preview model fills each interpolated path.
                     * Even the final release position counts.
                     */
                    build_preview_move(world_x, world_y);
                } else if (zone_erase_active && (state.buttons & 2U) &&
                           (mouse_buttons & 2U)) {
                    apply_zone_stroke(zone_drag_x, zone_drag_y,
                                      world_x, world_y, ZONE_NONE);
                    zone_drag_x = world_x;
                    zone_drag_y = world_y;
                } else if ((state.buttons & 1U) && (mouse_buttons & 1U) &&
                           selected_tool == EDIT_BULLDOZE) {
                    apply_selected_tool();
                } else {
                    draw_status();
                }
            }
        } else if (open_menu != MENU_NONE) {
            item = ui_dropdown_hit(open_menu, state.x, state.y);
            if (item >= 0 && (unsigned int)item != menu_highlight) {
                menu_highlight = (unsigned int)item;
                draw_menu_popup();
            }
        }
        /* Pointer coordinates are diagnostics, not city statistics.
         * Diffed glyph writes keep mouse motion inexpensive.
         */
        if (info_page == 0 && open_menu == MENU_NONE)
            draw_status();
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
            if (menu != MENU_NONE) {
                show_menu((enum ui_menu)menu);
            } else {
                toolbar = ui_toolbar_hit(state.x, state.y);
                if (toolbar >= 0) {
                    select_tool(ui_toolbar_tool((unsigned int)toolbar));
                    draw_status();
                } else if (inside) {
                    if (selected_zone_kind() != ZONE_NONE ||
                        selected_tool == EDIT_ROAD ||
                        selected_tool == EDIT_POWER) {
                        display_to_world(cursor_x,cursor_y,
                                         &world_x,&world_y);
                        build_preview_start(world_x,world_y);
                    } else {
                        apply_selected_tool();
                    }
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
        } else if (open_menu == MENU_NONE && inside &&
                   selected_zone_kind() != ZONE_NONE &&
                   !build_drag_active) {
            zone_erase_active = 1;
            display_to_world(cursor_x,cursor_y,
                             &zone_drag_x,&zone_drag_y);
            apply_zone_stroke(zone_drag_x, zone_drag_y,
                              zone_drag_x, zone_drag_y, ZONE_NONE);
        } else if (open_menu == MENU_TOOLS) {
            open_menu = MENU_NONE;
            render_full();
        } else if (!build_drag_active) {
            show_menu(MENU_TOOLS);
        }
        mouse_pointer_show();
    }

    if (left_release && build_drag_active) {
        mouse_pointer_hide();
        build_preview_finish();
        mouse_pointer_show();
    }
    if (right_release)
        zone_erase_active = 0;
    if (right_release && zone_minimap_dirty) {
        mouse_pointer_hide();
        draw_overview();
        mouse_pointer_show();
        zone_minimap_dirty = 0;
    }
    mouse_buttons = state.buttons;
}


/* BIOS time-of-day ticks are ~18.2 Hz and wrap at 16-bit CX:DX.
 * Work is limited to 80..320 cells per tick, keeping keyboard and serial
 * mouse handling responsive on the 7 MHz V20. Never process during a
 * construction drag or over an open menu, which would otherwise be
 * overwritten by a background tile update.
 */
static void simulation_idle(void)
{
    union REGS r;
    unsigned int now, event, tx, ty, amount;
    r.h.ah = 0U;
    int86(0x1a, &r, &r);
    now = r.x.dx;
    if (!sim_clock_started) {
        sim_clock_started = 1;
        sim_last_clock = now;
        return;
    }
    if ((unsigned int)(now - sim_last_clock) < 2U)
        return;
    sim_last_clock = now;
    if (simulation_is_paused() || open_menu != MENU_NONE ||
        info_page != 0 || build_drag_active || zone_erase_active)
        return;
    amount = 80U * simulation_speed();
    mouse_pointer_hide();
    event = simulation_advance(amount);
    if (event & SIM_EVENT_TILE) {
        simulation_last_change(&tx, &ty);
        if (simulation_last_change_radius() != 0U) {
            /* Redraw each of the nine actual original Micropolis
             * building sprite cells, not just its zone center.
             */
            redraw_zoning_brush(tx-1U,ty-1U,tx+1U,ty+1U);
        } else {
            unsigned int sx,sy;
            if (world_to_visible(tx,ty,&sx,&sy)) {
                draw_display_tile(sx,sy);
                if (sx == cursor_x && sy == cursor_y)
                    draw_cursor();
            }
        }
    }
    if (event & SIM_EVENT_MONTH) {
        draw_city_stats();
        draw_status();
    }
    mouse_pointer_show();
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
        simulation_idle(); /* runs even with /NOMOUSE and no keyboard */
        if (mouse_on)
            handle_mouse();
        if (!kbhit())
            continue;

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
            if (build_drag_active) {
                build_preview_cancel(); /* Esc cancels before paying */
                mouse_pointer_show();
                continue;
            }
            exit_requested = 1;
            break;
        }
        if (ascii == 9U) {
            /* Tab follows the last simulated development without
             * automatically yanking the camera around during play.
             * The normal yellow selection cursor highlights its tile.
             */
            if (simulation_has_last_change()) {
                unsigned int gx, gy;
                int house = simulation_last_home_change(&gx, &gy);
                if (!house)
                    simulation_last_change(&gx, &gy);
                view_rotation_focus(map_turns,gx,gy,VIEW_W,VIEW_H,
                                    &scroll_x,&scroll_y,
                                    &cursor_x,&cursor_y);
                last_action = house ? "LAST HOUSE" : "LAST GROWTH";
                render_full();
            } else {
                last_action = "NO GROWTH";
                draw_status();
            }
            mouse_pointer_show();
            continue;
        }
        if (ascii == 'r' || ascii == 'R') {
            rotate_view(ascii == 'r');
            mouse_pointer_show();
            continue;
        }
        if (ascii == 'g' || ascii == 'G') {
            simulation_set_paused(!simulation_is_paused());
            draw_status();
            mouse_pointer_show();
            continue;
        }
        if (ascii == '+' || ascii == '=') {
            simulation_set_speed(simulation_speed() < 4U ?
                                 simulation_speed() + 1U : 4U);
            draw_status();
            mouse_pointer_show();
            continue;
        }
        if (ascii == '-' || ascii == '_') {
            simulation_set_speed(simulation_speed() > 1U ?
                                 simulation_speed() - 1U : 1U);
            draw_status();
            mouse_pointer_show();
            continue;
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
                 scroll_x < view_rotation_width(map_turns) - VIEW_W)
            ++scroll_x;
        else if ((scan == 0x48U || ascii == 'w' || ascii == 'W') &&
                 scroll_y > 0U)
            --scroll_y;
        else if ((scan == 0x50U || ascii == 's' || ascii == 'S') &&
                 scroll_y < view_rotation_height(map_turns) - VIEW_H)
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
        else if (ascii == ' ') {
            apply_selected_tool();
            if (zone_minimap_dirty) {
                draw_overview();
                zone_minimap_dirty = 0;
            }
        }
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
            draw_overview_camera();
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

/* /BENCH: an in-place VRAM copy microbenchmark on real Tandy hardware.
 * Runs a full redraw and equivalent traditional byte vs optimized word
 * scroll copies, with the same 32x21 viewport and graphics mode.
 * No changes to any .CTY/.ZON file or simulation state.
 * DOS BIOS clock resolution is ~55ms; runs multiple passes for accuracy.
 */
static unsigned long bench_full_ticks = 0UL;
static unsigned long bench_byte_ticks = 0UL;
static unsigned long bench_word_ticks = 0UL;

static unsigned long bench_bios_ticks(void)
{
    union REGS r;
    r.h.ah = 0U;
    int86(0x1a, &r, &r);
    return ((unsigned long)r.x.cx << 16) | (unsigned long)r.x.dx;
}

static void benchmark_video(void)
{
    unsigned int pass, row, index, off, x, y;
    unsigned long start;
    /* The benchmark uses REAL B800 VRAM, but never renders the main
     * toolbar, minimap, menu or status HUD. Previous builds repeatedly
     * called render_full() between timed passes; that was responsible
     * for distracting toolbox refreshes and apparent screen corruption.
     */
    mouse_pointer_hide();
    bios_text(0U, 1U, "MICROPOL VRAM BENCHMARK", 15U);
    bios_text(24U, 1U, "1/3 CITY TILE REDRAW", 11U);

    /* Only redraw the 32x21 city window, not the game interface.
     * Same original 8x8 sprites/overlays and hardware video mode.
     */
    start = bench_bios_ticks();
    for (pass = 0U; pass < 2U; ++pass)
        for (y = 0U; y < VIEW_H; ++y)
            for (x = 0U; x < VIEW_W; ++x)
                draw_display_tile(x,y);
    bench_full_ticks = bench_bios_ticks() - start;

    /* The test copies the LIVE video RAM, not a normal RAM buffer.
     * Blank just the city viewport before each copy trial. Repeated
     * shifts of identical black pixels have the same video-memory
     * read/write cost, but produce no visible smearing/flickering.
     * The toolbar/menu/status rows are never touched.
     */
    tandy_fill_region(video, VIEW_X/2U, VIEW_Y,
                      VIEW_W*4U, VIEW_H*8U, 0U);
    bios_text(24U, 1U, "2/3 BYTE VRAM COPY  ", 11U);
    start = bench_bios_ticks();
    for (pass = 0U; pass < 48U; ++pass)
        for (row = VIEW_Y; row < VIEW_Y + VIEW_H*8U; ++row) {
            off = vram_offset(VIEW_X,row);
            for (index = 0U; index < VIEW_W*4U-4U; ++index)
                video[off+index] = video[off+index+4U];
        }
    bench_byte_ticks = bench_bios_ticks() - start;

    tandy_fill_region(video, VIEW_X/2U, VIEW_Y,
                      VIEW_W*4U, VIEW_H*8U, 0U);
    bios_text(24U, 1U, "3/3 WORD VRAM COPY  ", 11U);
    start = bench_bios_ticks();
    for (pass = 0U; pass < 48U; ++pass)
        for (row = VIEW_Y; row < VIEW_Y + VIEW_H*8U; ++row) {
            off = vram_offset(VIEW_X,row);
            tandy_move_words(video,off,off+4U,(VIEW_W*4U-4U)/2U,0);
        }
    bench_word_ticks = bench_bios_ticks() - start;
    /* The DOS text mode is restored immediately after this function.
     * Do not redraw the entire game just to erase benchmark pixels.
     */
}

/* Map a normal DOS 8.3 city save to its optional zone sidecar. */
static int zoning_sidecar_name(const char *cityname,
                               char *dest, unsigned int capacity)
{
    char *extension;
    unsigned int length = (unsigned int)strlen(cityname);
    if (length + 5U >= capacity)
        return 0;
    strcpy(dest, cityname);
    extension = strrchr(dest, '.');
    if (extension != NULL)
        strcpy(extension, ".ZON");
    else
        strcat(dest, ".ZON");
    return 1;
}

/* DOS options are case-insensitive regardless of the case typed in
 * COMMAND.COM; keep argument parsing independent of DOS C extensions.
 */
static int dos_option_is(const char *argument, const char *name)
{
    unsigned char a, b;
    if (*argument != '/' && *argument != '-')
        return 0;
    ++argument;
    while (*argument && *name) {
        a = (unsigned char)*argument++;
        b = (unsigned char)*name++;
        if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = (unsigned char)(b - 'a' + 'A');
        if (a != b)
            return 0;
    }
    return *argument == '\0' && *name == '\0';
}

int main(int argc, char **argv)
{
    int i, have_city = 0, allow_mouse = 1, bench_mode = 0;
    char sidecar[256];
    for (i = 1; i < argc; ++i) {
        if (dos_option_is(argv[i], "NOMOUSE")) {
            allow_mouse = 0;
        } else if (dos_option_is(argv[i], "BENCH")) {
            bench_mode = 1;
            allow_mouse = 0;
        } else if (!have_city && city_load(argv[i])) {
            have_city = 1;
            /* Original HAPPISLE starts with water in the upper left.
             * Start near actual homes so growth is visible to players.
             * This is camera-only; it never changes the city file.
             */
            (void)city_suggest_view(VIEW_W, VIEW_H,
                                    &scroll_x, &scroll_y);
            zoning_seed_from_city();
            if (zoning_sidecar_name(argv[i], sidecar,
                                    (unsigned int)sizeof(sidecar)))
                (void)zoning_load(sidecar);
        } else {
            fprintf(stderr,
                    "Unrecognized argument or invalid 27120-byte city: %s\n",
                    argv[i]);
            return 1;
        }
    }
    if (!have_city) {
        city_make_demo();
        zoning_seed_from_city();
        simulation_seed_demo();
        scroll_x = 38U;
        scroll_y = 20U;
    }
    simulation_init();

    old_mode = current_video_mode();
    set_video_mode(9);
    if (current_video_mode() != 9) {
        set_video_mode(old_mode);
        fputs("Tandy 1000 graphics mode 9 not available.\n", stderr);
        return 1;
    }
    video = (volatile unsigned char far *)MK_FP(0xb800, 0);
    tandy_init_scanlines();
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
    if (bench_mode) {
        /* Do not create/redraw toolbar or HUD in benchmark mode. */
        benchmark_video();
    } else {
        render_full();
        keyboard_loop();
    }
    mouse_pointer_hide();
    mouse_shutdown();
    engine_shutdown();
    set_video_mode(old_mode);
    if (art_data != NULL)
        free(art_data); /* art buffer used only in graphics mode */
    if (bench_mode) {
        puts("MICROPOL /BENCH - Tandy mode 9, 32x21 city viewport");
        printf("City tile draw x2  : %lu ticks (~%lu ms)\n",
               bench_full_ticks, bench_full_ticks*55UL);
        printf("VRAM bytes x48     : %lu ticks (~%lu ms)\n",
               bench_byte_ticks, bench_byte_ticks*55UL);
        printf("VRAM 16-bit x48    : %lu ticks (~%lu ms)\n",
               bench_word_ticks, bench_word_ticks*55UL);
        if (bench_word_ticks != 0UL)
            printf("Copy speed ratio   : %lu%% (higher is faster)\n",
                   (bench_byte_ticks*100UL)/bench_word_ticks);
        puts("Clock resolution ~55ms. All copies preserve identical pixels.");
    }
    return 0;
}
