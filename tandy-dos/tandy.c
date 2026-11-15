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
#include "tool_art.h"
#include "tool_picker.h"
#include "pointer_policy.h"
#include "small_lot_art.h"
#include "newer_lot.h"
#include "tile_inspect.h"
#include "game_save.h"
#include "newer_site.h"
#include "zone_highlight.h"
#include "badge_flash.h"
#include "stadium_fx.h"

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
/* Quick clicks repeat the last member picked in each tool group. */
static enum edit_tool favorite_road = EDIT_ROAD;
static enum edit_tool favorite_civic = EDIT_COAL;
static int hold_button = -1;
static unsigned int hold_tick = 0U;
static unsigned int picker_kind = 0U; /* 0=closed, 1=road, 2=civic */
static unsigned int picker_highlight = 0U;
static const char *last_action = "READY";
static unsigned int active_save_slot = 0U;
static char save_action[24];
static char power_message[12];

/* Menus and mouse input are optional; keyboard remains the default.
 * A menu is drawn only when opened. No scrolling performance penalty.
 */
static enum ui_menu open_menu = MENU_NONE;
static unsigned int menu_highlight = 0U;
/* Help pages are 1..3. Inspector panels are 4..7 (four pages).
 * Collect tile facts ONCE at the click, not once per BIOS glyph.
 */
static int info_page = 0;
static struct tile_inspect inspected_tile;
/* Repaint 8x8 graphics and dialog frame only when opening Inspect.
 * Changing the info page rewrites just the ten text rows.
 */
static int inspector_window_drawn=0;
static int exit_requested = 0;
static int mouse_on = 0;
static unsigned int mouse_x = 160U, mouse_y = 100U;
static unsigned int mouse_buttons = 0U;
static int zone_drag_active = 0;
static int zone_erase_active = 0;
static unsigned int zone_drag_x = 0U, zone_drag_y = 0U;
static int zone_minimap_dirty = 0;
/* Accumulate only the touched world rectangle while right-dragging a
 * zoning eraser; flush those sampled minimap pixels on release.
 */
static unsigned int zone_dirty_x0,zone_dirty_y0,zone_dirty_x1,zone_dirty_y1;
static int build_drag_active = 0;
/* Do not run a whole-city power scan in the middle of a mouse-held
 * bulldozer stroke. A single dirty flag is processed after release.
 */
static int doze_mouse_active = 0;
static unsigned int build_drag_x = 0U, build_drag_y = 0U;
static unsigned int sim_last_clock = 0U;
static int sim_clock_started = 0;
/* Pure display effects: never modify any .CTY tile or the power graph.
 * Slow yellow/dim warning alternation, and occasional authentic 4x4
 * Micropolis stadium/football graphics.
 */
static unsigned int badge_visible_color=BADGE_FLASH_BRIGHT;
static unsigned int stadium_clock=0U;
static unsigned int stadium_last_update=0U;
#define STADIUM_MAX_VISIBLE 12U
static unsigned char stadium_x[STADIUM_MAX_VISIBLE];
static unsigned char stadium_y[STADIUM_MAX_VISIBLE];
static unsigned char stadium_frame[STADIUM_MAX_VISIBLE];
static unsigned int stadium_count=0U;
static unsigned int stadium_last_scroll_x=65535U;
static unsigned int stadium_last_scroll_y=65535U;
static unsigned int stadium_last_turns=65535U;

/* One bit per visible 8x8 tile (84 bytes for the 32x21 viewport).
 * The badge is STATIC; a power-state change triggers a budgeted scan
 * and only tiles whose badge changes are repainted.
 */
#define BADGE_ROW_BYTES (VIEW_W / 8U)
#define BADGE_CACHE_BYTES (BADGE_ROW_BYTES * VIEW_H)
#define BADGE_CHECKS_PER_TICK 24U
static unsigned char badge_shadow[BADGE_CACHE_BYTES];
static unsigned int badge_revision = 0U;
static unsigned int badge_sweep_pos = VIEW_W * VIEW_H;

static int badge_shadow_get(unsigned int position)
{
    return (badge_shadow[position>>3] & (1U<<(position&7U)))!=0U;
}
static void badge_shadow_set(unsigned int position, int warning)
{
    unsigned char mask=(unsigned char)(1U<<(position&7U));
    if(warning) badge_shadow[position>>3] |= mask;
    else badge_shadow[position>>3] &= (unsigned char)~mask;
}

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

static void draw_packed_tile(unsigned int px, unsigned int py,
                             const unsigned char far *data)
{
    unsigned int row, dst;
    const unsigned short far *art =
        (const unsigned short far *)data;
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

static void draw_original_tile(unsigned int px, unsigned int py, unsigned int tile)
{
    draw_packed_tile(px, py, art_data + tile * 32U);
}

/* Zoning overlay stays visual-only and never modifies the original
 * or new source sprite bytes or saved city tiles. Vacant 1x1 parcels
 * display hatching and a full per-cell outline until developed.
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

/* The zone outline now reads as a small neighborhood street grid:
 *   - exterior district border = original R/C/I color;
 *   - shared boundary between separate SAME-KIND lots = road gray 8;
 *   - complete 2x2 / 3x3 building = perimeter only, never an inner
 *     road across its four/nine original sprite subtiles;
 *   - vacant 1x1 plots retain their original 9-pixel zone hatch.
 *
 * Compute both world edge sets once, then rotate them together.
 * Draw at most four packed bytes per scanline, with no buffer or
 * map changes, and give the outside zoning color priority at corners.
 */
static void draw_zone_border(unsigned int px, unsigned int py,
                             unsigned int mx, unsigned int my)
{
    unsigned int kind=zoning_at(mx,my),edges,shared,row,off,pattern;
    int hatched=0;
    unsigned char zone_color,road_color=8U,color,packed,hi,lo;
    if(kind==ZONE_NONE)return;
    edges=zone_highlight_style(mx,my,&hatched);
    if(!edges && !hatched)return;
    shared=zone_highlight_shared_edges(mx,my,edges);
    edges=view_rotation_edges(map_turns,edges);
    shared=view_rotation_edges(map_turns,shared);
    zone_color=(unsigned char)(kind==ZONE_RES?10U:
                               kind==ZONE_COM?9U:14U);
    hi=(unsigned char)(zone_color<<4U);
    lo=zone_color;
    for(row=0U;row<8U;++row) {
        off=vram_offset(px,py+row);
        if((row==0U && (edges&1U)) ||
           (row==7U && (edges&4U))) {
            unsigned int side=row==0U?1U:4U;
            color=(unsigned char)((shared&side)?road_color:zone_color);
            packed=(unsigned char)((color<<4U)|color);
            video[off]=packed;
            video[off+1U]=packed;
            video[off+2U]=packed;
            video[off+3U]=packed;
            /* The exterior district border must remain continuous:
             * where a gray internal edge meets an outside edge at
             * a corner, the original zoning color takes priority.
             */
            if((edges&8U) && !(shared&8U))
                video[off]=(unsigned char)((video[off]&15U)|hi);
            if((edges&2U) && !(shared&2U))
                video[off+3U]=(unsigned char)
                    ((video[off+3U]&240U)|lo);
        } else {
            /* Independent sides may be asphalt or an outer border;
             * only existing footprint edges are painted.
             */
            if(edges&8U) {
                color=(unsigned char)
                    ((shared&8U)?road_color:zone_color);
                video[off]=(unsigned char)
                    ((video[off]&15U)|(color<<4U));
            }
            if(edges&2U) {
                color=(unsigned char)
                    ((shared&2U)?road_color:zone_color);
                video[off+3U]=(unsigned char)
                    ((video[off+3U]&240U)|color);
            }
            if(!hatched)continue;
            pattern=zone_highlight_hatch_row(row);
            if(pattern==0x2aU) {
                /* x=1,3,5 stay zone-colored for empty parcels. */
                video[off]=(unsigned char)((video[off]&240U)|lo);
                video[off+1U]=(unsigned char)
                    ((video[off+1U]&240U)|lo);
                video[off+2U]=(unsigned char)
                    ((video[off+2U]&240U)|lo);
            } else if(pattern==0x54U) {
                /* x=2,4,6 stay zone-colored for empty parcels. */
                video[off+1U]=(unsigned char)
                    ((video[off+1U]&15U)|hi);
                video[off+2U]=(unsigned char)
                    ((video[off+2U]&15U)|hi);
                video[off+3U]=(unsigned char)
                    ((video[off+3U]&15U)|hi);
            }
        }
    }
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

/* A light 1px drive/access pad visible on the *ground* around
 * developed NewerSource buildings, not on the building sprite.
 * The optional NEWER art is already opaque 8x8: painting before it
 * would be overwritten. Instead, after drawing the unmodified sprite
 * we replace only pixels which are STILL grass-green (EGA 2).
 *
 * Road adjacency is resolved once per affected source tile. The
 * footprint gets one connected frontage, not four independent roads.
 * Road-facing edges rotate with the camera; no new offscreen buffer,
 * sprite copy, bitfield, or changes to .DAT/.CTY/.NBL.
 */
static void draw_newer_site_access(unsigned int px,unsigned int py,
                                   unsigned int wx,unsigned int wy,
                                   unsigned int left,unsigned int top,
                                   unsigned int span,unsigned int kind)
{
    unsigned int side,part,screen_part,sx,sy,x,y,mark,off;
    unsigned char source;
    if(span==0U || span>3U || wx<left || wy<top ||
       wx-left>=span || wy-top>=span)
        return;
    side=newer_site_road_side(left,top,span);
    if(side==0U)return; /* never paint a fake disconnected road */
    side=view_rotation_edges(map_turns,side);
    part=(wy-top)*span+(wx-left);
    screen_part=view_rotation_upright_part(map_turns,span,part);
    sx=screen_part%span;
    sy=screen_part/span;
    if(!newer_site_front_piece(span,side,sx,sy))return;
    /* Composite details onto ALL 8x8 source pixels: the middle
     * of a shared 2x2/3x3 curb can cross a subtile boundary. The
     * marker policy itself excludes the OUTER building perimeter,
     * so colored zoning edges remain intact after drawing.
     * Most candidate pixels reject before any VRAM read.
     */
    for(y=0U;y<8U;++y)
        for(x=0U;x<8U;++x) {
            mark=newer_site_marker(kind,span,side,
                                   sx*8U+x,sy*8U+y);
            if(mark==0U)continue;
            off=vram_offset(px+x,py+y);
            source=video[off];
            if(x&1U) {
                if((source&15U)==2U)
                    video[off]=(unsigned char)
                        ((source&240U)|newer_site_composite(2U,mark));
            } else {
                if((source>>4U)==2U)
                    video[off]=(unsigned char)
                        ((source&15U)|(newer_site_composite(2U,mark)<<4U));
            }
        }
}

/* All original multi-tile facility art uses the same host-tested
 * view_rotation_building_tile transform, including fire and police.
 */
static void draw_city_tile(unsigned int px, unsigned int py,
                           unsigned int mx, unsigned int my)
{
    unsigned short t = city_at(mx, my) & TILE_MASK;
    unsigned char bg = 2;
    unsigned int i;
    int v, h;
    const unsigned char *small = NULL;
    const unsigned char far *large = newer_lot_pixels(mx,my,map_turns);
    unsigned int kind=ZONE_NONE,stage=0U;

    /* The multi-lot overlay selects a complete 2x2 or 3x3 building
     * from NEWERM8.DAT in the current view rotation. The original
     * .CTY terrain remains underneath. Fall back to an intentional
     * compact silhouette if the optional art pack is absent.
     */
    if(newer_lot_at(mx,my)) {
        unsigned int left=0U,top=0U,span=0U;
        kind=zoning_at(mx,my);
        (void)newer_lot_info(mx,my,&left,&top,&span,0,0);
        if(large != NULL) draw_packed_tile(px,py,large);
        else {
            solid_tile(px,py,kind==ZONE_RES?8U:
                             kind==ZONE_COM?1U:6U);
            box_pixels(px+1U,py+1U,6U,6U,7U);
            pixel(px+2U,py+2U,15U);
            pixel(px+5U,py+2U,15U);
        }
        draw_newer_site_access(px,py,mx,my,left,top,span,kind);
        draw_zone_border(px,py,mx,my);
        draw_pending_tile(px,py,mx,my);
        return;
    }
    if (t == 0U || (t >= 21U && t <= 43U)) {
        kind=zoning_at(mx,my);
        stage=zoning_development_at(mx,my);
        small=small_lot_art_pixels(t,kind,stage,mx,my);
        /* Never use a single center piece of a legacy 3x3
         * commercial/industrial zone as a 1x1 structure. When the
         * optional original 12-sprite pack is missing, use a clean
         * 8x8 office/factory silhouette instead.
         */
        if((kind==ZONE_COM || kind==ZONE_IND) && stage!=0U) {
            if(small != NULL) draw_packed_tile(px,py,small);
            else {
                unsigned char shade=(unsigned char)
                    (kind==ZONE_COM?8U:6U);
                solid_tile(px,py,2U);
                box_pixels(px+1U,py+1U,6U,6U,shade);
                box_pixels(px+2U,py+2U,4U,1U,15U);
                if(kind==ZONE_COM) {
                    pixel(px+2U,py+4U,11U);
                    pixel(px+5U,py+4U,11U);
                } else {
                    box_pixels(px+2U,py+4U,4U,2U,4U);
                }
            }
            draw_newer_site_access(px,py,mx,my,mx,my,1U,kind);
            draw_zone_border(px,py,mx,my);
            draw_pending_tile(px,py,mx,my);
            return;
        }
    }

    if (small != NULL) {
        draw_packed_tile(px,py,small);
        draw_zone_border(px,py,mx,my);
        draw_pending_tile(px,py,mx,my);
        return;
    }

    if (art_data != NULL && t < ART_TILES) {
        unsigned int graphic = t;
        /* Original art remains the fallback when SC2K.DAT is absent.
         * Residential uses the original detached houses. Never change
         * the underlying .CTY sprite or map tile itself.
         */
        if (t == 0U || (t >= 21U && t <= 43U)) {
            kind = zoning_at(mx, my);
            stage = zoning_development_at(mx, my);
            /* The small-lot simulator changes ONLY the art displayed in
             * custom-zoned empty/wooded squares. Original 3x3 .CTY
             * building sprites are never overwritten or reinterpreted.
             * Higher stages reuse suitable center tiles of the original
             * 3x3 commercial/industrial building art.
             */
            if (kind == ZONE_RES) {
                static const unsigned short res[4] = {
                    0U, 249U, 252U, 258U
                };
                graphic = stage?res[stage]:t;
            } else if (kind == ZONE_COM || kind == ZONE_IND) {
                /* Vacant C/I remains genuine terrain, never a crop
                 * of an incomplete original 3x3 sprite. */
                graphic = t;
            }
        }
        /* Road and wire connections turn using ORIGINAL tiles.
         * Multi-tile building art stays upright with no pixel rotation.
         */
        graphic = view_rotation_utility_tile(map_turns,graphic);
        /* Games temporarily swap in original FULLSTADIUM and its
         * two 8-frame FOOTBALLGAME strips. The world keeps its
         * authentic STADIUM tile IDs; rotations select matching
         * subtile positions without rotating the source pixels.
         */
        if((t>=779U && t<=810U) ||
           (t>=932U && t<=947U)) {
            unsigned int art=stadium_fx_art(mx,my,map_turns,stadium_clock);
            if(art!=STADIUM_FX_NONE)
                graphic=art;
            else
                graphic=view_rotation_building_tile(map_turns,mx,my,graphic);
        } else
            graphic=view_rotation_building_tile(map_turns,mx,my,graphic);
        draw_original_tile(px,py,graphic);
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
/* Alternate only seven color pixels, never repaint the city/tile or
 * recompute its warning every time the icon flashes. Keep the black
 * silhouette as a permanent backdrop so it remains legible on grass.
 */
static void recolor_no_power_badge(unsigned int px,unsigned int py,
                                    unsigned int color)
{
    pixel(px+5U,py+1U,color);
    pixel(px+4U,py+2U,color);
    pixel(px+3U,py+3U,color);
    pixel(px+6U,py+3U,color);
    pixel(px+5U,py+4U,color);
    pixel(px+4U,py+5U,color);
    pixel(px+3U,py+6U,color);
}
static void draw_no_power_badge(unsigned int px, unsigned int py)
{
    box_pixels(px+3U,py,5U,7U,0U);
    recolor_no_power_badge(px,py,badge_visible_color);
}

static void draw_display_tile(unsigned int sx, unsigned int sy)
{
    unsigned int wx,wy;
    int warning;
    display_to_world(sx,sy,&wx,&wy);
    draw_city_tile(VIEW_X+sx*8U,VIEW_Y+sy*8U,wx,wy);
    warning=simulation_needs_power_warning(wx,wy);
    badge_shadow_set(sy*VIEW_W+sx,warning);
    if(warning)
        draw_no_power_badge(VIEW_X+sx*8U,VIEW_Y+sy*8U);
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
/* Road art is already easy to recognize, so retain it verbatim.
 * Others use complete 16x16 pictograms instead of one enlarged map cell.
 */
#define ROAD_TOOL_TILE 66U
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
    unsigned int id, kind;
    unsigned char color = 2U;
    view_rotation_to_world(map_turns,dx,dy,&mx,&my);
    id = city_at(mx,my) & TILE_MASK;
    kind = zoning_at(mx,my);
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

/* A single bulldozed square changes at most one sampled minimap pixel
 * at 56x44 resolution (many original 120x100 map squares are smaller
 * than one minimap pixel). Avoid repainting all 2464 pixels for every
 * mouse movement while dragging the bulldozer.
 *
 * Inverted nearest-neighbor coordinate ranges are exact, including at
 * 90/270-degree rotated views; leave the yellow viewport outline intact.
 */
static void draw_overview_pixel(unsigned int px, unsigned int py)
{
    unsigned char color=overview_color(px,py);
    if (overview_valid &&
        px>=overview_old_x &&
        px<overview_old_x+overview_old_w &&
        py>=overview_old_y &&
        py<overview_old_y+overview_old_h &&
        (px==overview_old_x ||
         px==overview_old_x+overview_old_w-1U ||
         py==overview_old_y ||
         py==overview_old_y+overview_old_h-1U))
        color=14U;
    pixel(UI_MINIMAP_X+px,UI_MINIMAP_Y+py,color);
}

static void draw_overview_world_tile(unsigned int wx, unsigned int wy)
{
    unsigned int px,py,x0,x1,y0,y1;
    if (wx>=CITY_WIDTH || wy>=CITY_HEIGHT)
        return;
    view_rotation_minimap_bounds(map_turns,wx,wy,
                                 UI_MINIMAP_W,UI_MINIMAP_H,
                                 &x0,&y0,&x1,&y1);
    for(py=y0;py<y1;++py)
        for(px=x0;px<x1;++px)
            draw_overview_pixel(px,py);
}

/* Refresh only the minimap pixels that sample a changed world region.
 * Full 56x44 redraw remains reserved for first load or view rotation.
 * During construction, refresh only successfully funded locations
 * from the retained 2-bit preview map, not blocked candidate squares.
 */
static void draw_overview_world_rect(unsigned int x0,unsigned int y0,
                                     unsigned int x1,unsigned int y1,
                                     int funded_only)
{
    unsigned int left,top,right,bottom,px0,py0,px1,py1;
    unsigned int px,py,mx,my,mw,mh;
    left=x0<x1?x0:x1;
    right=x0>x1?x0:x1;
    top=y0<y1?y0:y1;
    bottom=y0>y1?y0:y1;
    if(right>=CITY_WIDTH)right=CITY_WIDTH-1U;
    if(bottom>=CITY_HEIGHT)bottom=CITY_HEIGHT-1U;
    view_rotation_minimap_rect(map_turns,left,top,right,bottom,
                               UI_MINIMAP_W,UI_MINIMAP_H,
                               &px0,&py0,&px1,&py1);
    mw=view_rotation_width(map_turns);
    mh=view_rotation_height(map_turns);
    for(py=py0;py<py1;++py)
        for(px=px0;px<px1;++px) {
            view_rotation_to_world(map_turns,
                                   px*mw/UI_MINIMAP_W,
                                   py*mh/UI_MINIMAP_H,&mx,&my);
            if(!funded_only ||
               placement_recent_at(mx,my)==PREVIEW_FUNDED)
                draw_overview_pixel(px,py);
        }
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
    if (tool == EDIT_RAIL) return 1U;
    if (tool == EDIT_COAL || tool == EDIT_NUCLEAR ||
        tool == EDIT_POLICE || tool == EDIT_FIRE) return 7U;
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

/* Preserve the three hardware-approved icons: original road tile,
 * bulldozer and inspector. The other five are genuinely readable
 * 16x16 artwork, not an 8x8 fragment of an original 3x3 building.
 */
static void draw_tool_art(unsigned int tool, unsigned int px,
                          unsigned int py)
{
    unsigned int ix,iy,src,color;
    unsigned char packed;
    if (tool == 0U || tool == 6U) {
        draw_tool_pictogram(tool,px,py);
        return;
    }
    /* Real 2x2 blocks of the original city art identify alternate
     * civic tools, instead of showing the coal icon for everything.
     */
    if(tool==7U && art_data!=NULL && selected_tool!=EDIT_COAL) {
        unsigned int base=(selected_tool==EDIT_NUCLEAR)?811U:
                          (selected_tool==EDIT_POLICE)?770U:761U;
        unsigned int span=selected_tool==EDIT_NUCLEAR?4U:3U;
        for(iy=0U;iy<16U;++iy)
            for(ix=0U;ix<16U;++ix) {
                unsigned int id=base+(iy>>3U)*span+(ix>>3U);
                src=id*32U+(iy&7U)*4U+((ix&7U)>>1U);
                packed=art_data[src];
                color=(ix&1U)?(packed&15U):(packed>>4U);
                pixel(px+ix,py+iy,(unsigned char)color);
            }
        return;
    }
    if (tool_art_has_icon(tool)) {
        for (iy=0U;iy<16U;++iy)
            for (ix=0U;ix<16U;++ix) {
                color=tool_art_pixel(tool,ix,iy);
                if (color != TOOL_ART_TRANSPARENT)
                    pixel(px+ix,py+iy,(unsigned char)color);
            }
        return;
    }
    /* Rail is the authentic straight-track sprite at twice scale. */
    if (tool != 1U || art_data == NULL)
        return;
    src=(selected_tool==EDIT_RAIL ? 226U : ROAD_TOOL_TILE)*32U;
    for (iy=0U;iy<8U;++iy)
        for (ix=0U;ix<8U;++ix) {
            packed=art_data[src+iy*4U+(ix>>1)];
            color=(unsigned int)((ix&1U) ?
                                 (packed&15U) : (packed>>4));
            box_pixels(px+ix*2U,py+iy*2U,2U,2U,
                       (unsigned char)color);
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
    /* Small corner marker identifies the two expandable buttons. */
    if(n==1U || n==7U) {
        pixel(bx+26U,by+15U,0U);
        pixel(bx+26U,by+16U,15U);
        pixel(bx+25U,by+16U,15U);
        pixel(bx+24U,by+16U,15U);
    }
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

static void redraw_edit_area(unsigned int radius)
{
    unsigned int x, y, min_x, max_x, min_y, max_y;
    /* Bulldozer's neighbor repair touches only this square and its
     * four cardinal neighbors: 3x3 is plenty. Keep 5x5 for the 4x4
     * coal-plant artwork because it spans farther than the cursor.
     */
    min_x = cursor_x > radius ? cursor_x - radius : 0U;
    min_y = cursor_y > radius ? cursor_y - radius : 0U;
    max_x = cursor_x + radius < VIEW_W ? cursor_x + radius : VIEW_W - 1U;
    max_y = cursor_y + radius < VIEW_H ? cursor_y + radius : VIEW_H - 1U;
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
        {
            /* A newly painted (or erased) zone changes the color of
             * its already-drawn neighbors' shared boundaries too.
             * Refresh a one-tile margin so gray internal roads and
             * colored exteriors update immediately, even if the
             * neighboring zone's actual kind did not change.
             * Erasure uses two tiles for demolished 3x3 prefabs.
             */
            unsigned int margin=kind==ZONE_NONE?2U:1U;
            unsigned int xlo=x0<x1?x0:x1;
            unsigned int ylo=y0<y1?y0:y1;
            unsigned int xhi=x0>x1?x0:x1;
            unsigned int yhi=y0>y1?y0:y1;
            xlo=xlo>margin?xlo-margin:0U;
            ylo=ylo>margin?ylo-margin:0U;
            xhi=xhi+margin<CITY_WIDTH?xhi+margin:CITY_WIDTH-1U;
            yhi=yhi+margin<CITY_HEIGHT?yhi+margin:CITY_HEIGHT-1U;
            redraw_zoning_brush(xlo,ylo,xhi,yhi);
        }
        last_action = kind == ZONE_NONE ? "UNZONED" : "ZONED";
        if (!zone_minimap_dirty) {
            simulation_mark_lots_dirty(); /* once per eraser stroke */
            zone_dirty_x0=x0<x1?x0:x1;
            zone_dirty_y0=y0<y1?y0:y1;
            zone_dirty_x1=x0>x1?x0:x1;
            zone_dirty_y1=y0>y1?y0:y1;
        } else {
            unsigned int lo=x0<x1?x0:x1, hi=x0>x1?x0:x1;
            unsigned int ly=y0<y1?y0:y1, hy=y0>y1?y0:y1;
            if(lo<zone_dirty_x0)zone_dirty_x0=lo;
            if(ly<zone_dirty_y0)zone_dirty_y0=ly;
            if(hi>zone_dirty_x1)zone_dirty_x1=hi;
            if(hy>zone_dirty_y1)zone_dirty_y1=hy;
        }
        zone_minimap_dirty=1;
    } else {
        last_action = "BLOCKED";
    }
    draw_status();
}

/* A drag preview changes the pixels of ONLY its newly touched cells.
 * The placement module gives us a compact list for the most recent
 * Bresenham segment; repeated mouse polling of the same square emits
 * no work. Road/zone neighbors are repaired after the actual commit.
 */
static void redraw_new_preview(void)
{
    unsigned int i,mx,my,sx,sy;
    for(i=0U;i<placement_new_count();++i) {
        placement_new_cell(i,&mx,&my);
        if(world_to_visible(mx,my,&sx,&sy))
            draw_display_tile(sx,sy);
    }
    if(placement_new_count()!=0U)
        draw_cursor();
}

/* The existing packed 3KB placement grid survives until the next
 * preview begins. Walk only 32x21 visible screen squares after mouse-up,
 * rather than traversing an enormous diagonal drag's world bounding box.
 * Its changed squares plus direct neighbors cover zone border and
 * road/wire adjacency changes; canceled previews need only their own
 * modified pixels restored, but one-neighbor restoration is harmless.
 */
static void redraw_recent_preview(void)
{
    unsigned int x0,y0,x1,y1,dx0,dy0,dx1,dy1,mw,mh;
    unsigned int dx,dy,mx,my,sx,sy;
    placement_bounds(&x0,&y0,&x1,&y1);
    mw=view_rotation_width(map_turns);
    mh=view_rotation_height(map_turns);
    /* Passing virtual map dimensions yields exact screen-tile bounds
     * for the rotated original-world rectangle, with no fractions.
     */
    view_rotation_minimap_rect(map_turns,x0,y0,x1,y1,
                               mw,mh,&dx0,&dy0,&dx1,&dy1);
    if(dx0>0U)--dx0;
    if(dy0>0U)--dy0;
    if(dx1<mw)++dx1;
    if(dy1<mh)++dy1;
    if(dx0<scroll_x)dx0=scroll_x;
    if(dy0<scroll_y)dy0=scroll_y;
    if(dx1>scroll_x+VIEW_W)dx1=scroll_x+VIEW_W;
    if(dy1>scroll_y+VIEW_H)dy1=scroll_y+VIEW_H;
    /* A single tile edit checks only 3x3 displayed squares; the
     * fallback is bounded to 32x21 even for a long diagonal stroke.
     */
    for(dy=dy0;dy<dy1;++dy)
        for(dx=dx0;dx<dx1;++dx) {
            sx=dx-scroll_x;
            sy=dy-scroll_y;
            display_to_world(sx,sy,&mx,&my);
            if(placement_recent_at(mx,my)!=PREVIEW_NONE ||
               (mx>0U && placement_recent_at(mx-1U,my)!=PREVIEW_NONE) ||
               (mx+1U<CITY_WIDTH &&
                placement_recent_at(mx+1U,my)!=PREVIEW_NONE) ||
               (my>0U && placement_recent_at(mx,my-1U)!=PREVIEW_NONE) ||
               (my+1U<CITY_HEIGHT &&
                placement_recent_at(mx,my+1U)!=PREVIEW_NONE))
                draw_display_tile(sx,sy);
        }
    draw_cursor();
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
                edit_tool_name(selected_tool),
                wx,wy,mouse_x,mouse_y);
    } else {
        sprintf(debug,"TOOL:%s MAP:%03u,%03u M:OFF",
                edit_tool_name(selected_tool),wx,wy);
    }
    draw_status_text(23U,debug,debug_shadow,&debug_valid,11U);
    if (placement_active()) {
        sprintf(status,"C:$%ld LEFT:$%ld +%u -%u",
                placement_cost(),placement_remaining(),
                placement_count(),placement_rejected());
    } else {
        sprintf(status,"G:%u R:%u X%u %s V:%u %s",
                st->growth_total,st->lots_ready,
                simulation_is_paused() ? 0U : simulation_speed(),
                simulation_is_paused() ? "PAUSE" : "RUN",
                map_turns * 90U, last_action);
    }
    draw_status_text(24U,status,status_shadow,&status_valid,11U);
}

/* The eight-pixel software arrow is only needed for UI navigation.
 * Over the playable city, the solid yellow tile-selection square is
 * already the pointer: do not draw a second, flickering arrow there.
 */
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
    if (!mouse_on || mouse_drawn ||
        !pointer_should_show(mouse_x,mouse_y,
                             open_menu!=MENU_NONE || picker_kind!=0U,
                             info_page!=0))
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

/* The entire dropdown is only 128 pixels wide. Remember the 64 bytes
 * underneath each scanline so hiding or switching a menu restores
 * exactly those city pixels, not 672 tiles + 8 buttons + the minimap.
 * Max 9 rows * 8 pixels * 64 packed bytes = 4608 bytes of DOS RAM.
 * Popup origins are entirely in the map viewport, never the toolbox.
 */
#define MENU_BACK_BYTES (9U * 8U * 64U)
static unsigned char menu_back[MENU_BACK_BYTES];
/* Remember the REAL pixel top of this popup, not just the column.
 * Main menus begin at y=16; long-press tool flyouts begin y=24/96.
 * Restoring every popup at y=16 left stale flyout stripes on the map.
 */
static unsigned int menu_back_origin=0U, menu_back_top_y=0U;
static unsigned int menu_back_lines=0U;
static int menu_back_valid=0;

static void menu_overlay_restore(void)
{
    unsigned int row,n,off,base;
    if (!menu_back_valid)
        return;
    mouse_pointer_hide();
    for (row=0U;row<menu_back_lines;++row) {
        off=vram_offset(menu_back_origin*8U,menu_back_top_y+row);
        base=row*64U;
        for (n=0U;n<64U;++n)
            video[off+n]=menu_back[base+n];
    }
    menu_back_valid=0;
}

/* Shared capture makes menu and flyout restore exactly the rectangle
 * they covered. All coordinate and size parameters are pixel units
 * except origin_col, which is an 8-pixel text column. Max: 128x72.
 */
static void menu_overlay_capture_rect(unsigned int origin_col,
                                      unsigned int top_y,
                                      unsigned int lines)
{
    unsigned int row,n,off,base;
    mouse_pointer_hide();
    menu_back_origin=origin_col;
    menu_back_top_y=top_y;
    menu_back_lines=lines;
    for(row=0U;row<lines;++row) {
        off=vram_offset(origin_col*8U,top_y+row);
        base=row*64U;
        for(n=0U;n<64U;++n)
            menu_back[base+n]=video[off+n];
    }
    menu_back_valid=1;
}

static void menu_overlay_capture(void)
{
    if (open_menu==MENU_NONE)
        return;
    menu_overlay_capture_rect(ui_menu_origin(open_menu),UI_MAP_Y,
                              ui_menu_count(open_menu)*8U);
}

static void menu_overlay_close(void)
{
    menu_overlay_restore();
    open_menu=MENU_NONE;
}

static void draw_menu_row(unsigned int j)
{
    unsigned int col,origin;
    if(open_menu==MENU_NONE || j>=ui_menu_count(open_menu))
        return;
    origin=ui_menu_origin(open_menu);
    for(col=0U;col<16U;++col)
        solid_tile((origin+col)*8U,(2U+j)*8U,
                   (unsigned char)(j==menu_highlight?5U:1U));
    bios_text(2U+j,origin+1U,ui_menu_item(open_menu,j),
              (unsigned char)(j==menu_highlight?15U:14U));
}

static void draw_menu_popup(void)
{
    unsigned int j;
    if (open_menu==MENU_NONE)
        return;
    for(j=0U;j<ui_menu_count(open_menu);++j)
        draw_menu_row(j);
}

/* Recolor only the two menu entries which changed, rather than
 * rebuilding every row of a nine-item TOOLS dropdown on mouse motion.
 */
static void menu_set_highlight(unsigned int next)
{
    unsigned int old;
    if (open_menu==MENU_NONE || next==menu_highlight ||
        next>=ui_menu_count(open_menu))
        return;
    old=menu_highlight;
    menu_highlight=next;
    draw_menu_row(old);
    draw_menu_row(next);
}

/* The graphics preview uses the ACTUAL current sprite tables.
 * For original source prefabs, assemble every 8x8 original subtile.
 * For NewerSource compounds, use the real .NBL footprint and art.
 * Never scale or copy any sprite into a permanent image buffer.
 *
 * 1x1 -> 48px, 2x2/4x4 -> 64px, 3x3/6x6 -> 48px.
 * The Tandy 320x200 mode-9 preview is one-time per Inspect open.
 */
static void draw_inspector_preview(void)
{
    unsigned int span=inspected_tile.span,scale,width,origin_x,origin_y;
    unsigned int i,j,x,y,col,b,graphic,px,py;
    const unsigned char far *sprite;
    const unsigned char *small;
    if(span==0U || span>6U)span=1U;
    scale=tile_inspect_preview_scale(span);
    width=span*8U*scale;
    origin_x=TILE_DIALOG_PREVIEW_X+
             (TILE_DIALOG_PREVIEW_SIZE-width)/2U;
    origin_y=TILE_DIALOG_PREVIEW_Y+
             (TILE_DIALOG_PREVIEW_SIZE-width)/2U;
    for(j=0U;j<span;++j)
        for(i=0U;i<span;++i) {
            sprite=0;
            if(inspected_tile.newer) {
                sprite=newer_lot_pixels(inspected_tile.left+i,
                                        inspected_tile.top+j,0U);
            } else if(span==1U) {
                small=small_lot_art_pixels(inspected_tile.id,
                                          inspected_tile.zone,
                                          inspected_tile.stage,
                                          inspected_tile.x,inspected_tile.y);
                if(small!=0)sprite=small;
            }
            if(sprite==0 && !inspected_tile.newer && art_data!=0) {
                graphic=tile_inspect_preview_original_id(
                    &inspected_tile,i,j);
                if(graphic<ART_TILES)
                    sprite=art_data+graphic*32U;
            }
            if(sprite==0)continue;
            px=origin_x+i*8U*scale;
            py=origin_y+j*8U*scale;
            if(scale==1U) {
                draw_packed_tile(px,py,sprite);
            } else {
                /* Sprite pixels are packed high/low nibbles.
                 * The little preview is nearest-neighbor only.
                 * All 16 original colors and pixel shapes survive.
                 */
                for(y=0U;y<8U;++y)
                    for(x=0U;x<8U;++x) {
                        b=sprite[y*4U+(x>>1U)];
                        col=(x&1U)?(b&15U):(b>>4U);
                        box_pixels(px+x*scale,py+y*scale,
                                   scale,scale,(unsigned char)col);
                    }
            }
        }
}

/* Simple flat gray dialog: no drop shadow, beveling, animation,
 * extra asset or framebuffer. 2px edge and three DOS-style buttons.
 */
static void inspector_button(unsigned int px,unsigned int w,
                             const char *caption)
{
    unsigned int x,y;
    for(y=144U;y<160U;y+=8U)
        for(x=px;x<px+w;x+=8U)
            solid_tile(x,y,7U);
    for(x=px;x<px+w;++x) {
        pixel(x,144U,0U);
        pixel(x,159U,0U);
    }
    for(y=144U;y<160U;++y) {
        pixel(px,y,0U);
        pixel(px+w-1U,y,0U);
    }
    bios_text(19U,px/8U+1U,caption,0x70U);
}
static void draw_inspector_shell(void)
{
    unsigned int x,y;
    for(y=4U;y<22U;++y)
        for(x=1U;x<39U;++x)
            solid_tile(x*8U,y*8U,7U);
    for(x=8U;x<312U;++x) {
        pixel(x,32U,0U);
        pixel(x,175U,0U);
    }
    for(y=32U;y<176U;++y) {
        pixel(8U,y,0U);
        pixel(311U,y,0U);
    }
    /* Name field and picture surround are deliberately flat. */
    for(x=16U;x<216U;++x) {
        pixel(x,39U,8U);
        pixel(x,49U,8U);
    }
    for(y=39U;y<50U;++y) {
        pixel(16U,y,8U);
        pixel(215U,y,8U);
    }
    for(y=TILE_DIALOG_PREVIEW_Y;
        y<TILE_DIALOG_PREVIEW_Y+TILE_DIALOG_PREVIEW_SIZE;
        y+=8U)
        for(x=TILE_DIALOG_PREVIEW_X;
            x<TILE_DIALOG_PREVIEW_X+TILE_DIALOG_PREVIEW_SIZE;
            x+=8U)
            solid_tile(x,y,8U);
    for(x=TILE_DIALOG_PREVIEW_X;x<TILE_DIALOG_PREVIEW_X+
                                      TILE_DIALOG_PREVIEW_SIZE;++x) {
        pixel(x,TILE_DIALOG_PREVIEW_Y,0U);
        pixel(x,TILE_DIALOG_PREVIEW_Y+
                TILE_DIALOG_PREVIEW_SIZE-1U,0U);
    }
    for(y=TILE_DIALOG_PREVIEW_Y;y<TILE_DIALOG_PREVIEW_Y+
                                       TILE_DIALOG_PREVIEW_SIZE;++y) {
        pixel(TILE_DIALOG_PREVIEW_X,y,0U);
        pixel(TILE_DIALOG_PREVIEW_X+
              TILE_DIALOG_PREVIEW_SIZE-1U,y,0U);
    }
    inspector_button(24U,64U,"PREV");
    inspector_button(112U,72U,"NEXT");
    inspector_button(232U,72U,"DONE");
    bios_text(21U,3U,"ARROWS / TAB: PAGES   ESC: DONE",0x70U);
    {
        char title[TILE_DIALOG_TEXT_COLUMNS+1U];
        tile_inspect_dialog_title(&inspected_tile,title);
        bios_text(5U,3U,title,0x70U);
    }
    draw_inspector_preview();
}

static void draw_info_page(void)
{
    unsigned int i, j;
    if(info_page == 0)return;
    if(info_page>=4 && info_page<8) {
        char line[TILE_DIALOG_TEXT_COLUMNS+1U];
        unsigned int page=(unsigned int)(info_page-4);
        if(!inspector_window_drawn) {
            draw_inspector_shell();
            inspector_window_drawn=1;
        }
        /* Page changes touch only 24x10 text cells; the original
         * graphics preview, gray frame and buttons remain intact.
         */
        for(j=7U;j<17U;++j)
            for(i=2U;i<2U+TILE_DIALOG_TEXT_COLUMNS;++i)
                solid_tile(i*8U,j*8U,7U);
        for(i=0U;i<TILE_INSPECT_ROWS;++i) {
            tile_inspect_dialog_line(&inspected_tile,page,i,line);
            bios_text(7U+i,2U,line,0x70U);
        }
        return;
    }
    inspector_window_drawn=0;
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
static void redraw_viewport_only(int refresh_minimap);

static void scroll_one_tile(unsigned int previous_x, unsigned int previous_y)
{
    unsigned int x, y, dst, src;

    if (scroll_y == previous_y && scroll_x == previous_x + 1U) {
        /* Shift the 84-byte badge shadow with the image, not with the
         * world; only the newly exposed edge needs a fresh lookup.
         */
        for(y=0U;y<VIEW_H;++y) {
            unsigned int row=y*BADGE_ROW_BYTES;
            for(x=0U;x<BADGE_ROW_BYTES;++x) {
                unsigned char carry=(x+1U<BADGE_ROW_BYTES) ?
                    (unsigned char)(badge_shadow[row+x+1U]<<7U) : 0U;
                badge_shadow[row+x]=(unsigned char)
                    ((badge_shadow[row+x]>>1U)|carry);
            }
        }
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
        for(y=0U;y<VIEW_H;++y) {
            unsigned int row=y*BADGE_ROW_BYTES;
            for(x=BADGE_ROW_BYTES;x>0U;--x) {
                unsigned int i=x-1U;
                unsigned char carry=i>0U ?
                    (unsigned char)(badge_shadow[row+i-1U]>>7U) : 0U;
                badge_shadow[row+i]=(unsigned char)
                    ((badge_shadow[row+i]<<1U)|carry);
            }
        }
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
        memmove(badge_shadow,badge_shadow+BADGE_ROW_BYTES,
                BADGE_CACHE_BYTES-BADGE_ROW_BYTES);
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
        memmove(badge_shadow+BADGE_ROW_BYTES,badge_shadow,
                BADGE_CACHE_BYTES-BADGE_ROW_BYTES);
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

/* Minimap jumps and quarter-turns need a refreshed city, not a
 * repeated toolbar/top-bar redraw. Rotation also rebuilds the minimap
 * pixels; jumps move only the existing camera outline.
 */
static void redraw_viewport_only(int refresh_minimap)
{
    unsigned int x,y;
    mouse_pointer_hide();
    for(y=0U;y<VIEW_H;++y)
        for(x=0U;x<VIEW_W;++x)
            draw_display_tile(x,y);
    if(refresh_minimap) draw_overview();
    else draw_overview_camera();
    draw_cursor();
    draw_status();
    badge_revision=simulation_power_visual_revision();
    badge_sweep_pos=VIEW_W*VIEW_H;
    mouse_pointer_show();
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

    badge_revision=simulation_power_visual_revision();
    badge_sweep_pos=VIEW_W*VIEW_H;
    mouse_pointer_show();
}

/* F2 creates one new numbered three-file city only ONCE. Every
 * subsequent save updates that SAME slot, with previous .BCT/.BZN/
 * .BNB backups. The status line shows the actual DOS 8.3 file.
 * Source HAPPISLE.CTY is never silently overwritten.
 */
static void save_current_city(void)
{
    unsigned int result;
    last_action="SAVING...";
    draw_status();
    result=game_save_write(&active_save_slot);
    if(result==GAME_SAVE_OK) {
        sprintf(save_action,"SAVED CITY%04u",active_save_slot);
        last_action=save_action;
    } else if(result==GAME_SAVE_LAST_ERROR) {
        sprintf(save_action,"CITY%04u SAVED*",active_save_slot);
        last_action=save_action; /* CTY/ZON/NBL intact; index failed */
    } else if(result==GAME_SAVE_CTY_ERROR)last_action="CTY DISK ERROR";
    else if(result==GAME_SAVE_ZON_ERROR)last_action="ZON DISK ERROR";
    else if(result==GAME_SAVE_NBL_ERROR)last_action="NBL DISK ERROR";
    else if(result==GAME_SAVE_COMMIT_ERROR)last_action="SAVE COMMIT ERR";
    else last_action="NO FREE SLOT";
}

/* Reopen the newest fully committed save without restarting DOS.
 * This works even if the player's startup BAT always launches
 * HAPPISLE.CTY. The currently loaded unsaved state is replaced.
 */
static void load_saved_city(void)
{
    unsigned int slot=0U;
    if(!game_save_load_last(&slot)) {
        last_action="LOAD ERROR";
        return;
    }
    active_save_slot=slot;
    simulation_init();
    map_turns=0U;
    if(!city_suggest_view(VIEW_W,VIEW_H,&scroll_x,&scroll_y)) {
        scroll_x=0U;scroll_y=0U;
    }
    cursor_x=VIEW_W/2U;
    cursor_y=VIEW_H/2U;
    zone_minimap_dirty=0;
    badge_sweep_pos=VIEW_W*VIEW_H;
    stadium_last_scroll_x=65535U;
    sim_clock_started=0;
    sprintf(save_action,"LOADED CITY%04u",slot);
    last_action=save_action;
    render_full();
}

/* If another tool/menu is chosen while the mouse is held, the unfinished
 * preview must be cancelled rather than charged by its later release.
 */
static void build_preview_cancel(void);

/* Map number-row shortcuts, toolbar, and Tools menu onto one function. */
/* Group members share a physical toolbar button, but the tool
 * selection (and icon art) reflect the actual chosen variant.
 */
static void select_variant(enum edit_tool next)
{
    unsigned int previous=tool_button_index(selected_tool);
    enum edit_tool old=selected_tool;
    if(build_drag_active) build_preview_cancel();
    selected_tool=next;
    if(next==EDIT_ROAD || next==EDIT_RAIL) favorite_road=next;
    if(next==EDIT_COAL || next==EDIT_NUCLEAR ||
       next==EDIT_POLICE || next==EDIT_FIRE) favorite_civic=next;
    last_action="SELECT";
    if(previous!=tool_button_index(selected_tool)) {
        draw_tool_button(previous);
        draw_tool_button(tool_button_index(selected_tool));
    } else if(old!=next) {
        draw_tool_button(previous); /* new variant picture only */
    }
}

static void select_tool(unsigned int number)
{
    switch(number) {
    case 0U: select_variant(EDIT_INSPECT); break;
    case 1U: select_variant(EDIT_ROAD); break;
    case 2U: select_variant(EDIT_POWER); break;
    case 3U: select_variant(EDIT_RESIDENTIAL); break;
    case 4U: select_variant(EDIT_COMMERCIAL); break;
    case 5U: select_variant(EDIT_INDUSTRIAL); break;
    case 6U: select_variant(EDIT_BULLDOZE); break;
    case 7U: select_variant(EDIT_COAL); break;
    }
}

static void preview_refresh(void)
{
    /* The planner already knows which squares were NEW on this mouse
     * movement. Repaint only those sprites, never the bounding box.
     */
    redraw_new_preview();
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
    preview_refresh();
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
    preview_refresh();
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
    if (applied != 0U) {
        /* Positive additions can only GROW the electrical network.
         * Extend it from the newly constructed line/empty lots using
         * a bounded local BFS, not the expensive 12K full city scan.
         * Deletions still use an exact global rebuild on the next
         * simulation tick to prevent phantom power islands.
         */
        if (placement_changed_power() &&
            selected_tool == EDIT_POWER)
            (void)simulation_power_added_rect(x0,y0,x1,y1);
        else if (placement_changed_power())
            simulation_mark_power_dirty();
        else if (selected_zone_kind()!=ZONE_NONE)
            (void)simulation_power_added_rect(x0,y0,x1,y1);
        /* The 56x44 minimap is sampled from world tile coordinates;
         * update only its pixels corresponding to funded edits.
         */
        draw_overview_world_rect(x0,y0,x1,y1,1);
        draw_city_stats();
    }
    /* Show the new power state on painted tiles immediately after
     * the fast extension rather than repainting stale no-power icons.
     */
    redraw_recent_preview();
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
    redraw_recent_preview();
    last_action = "CANCELLED";
    draw_status();
}

static void apply_selected_tool(void)
{
    unsigned int kind = selected_zone_kind();
    unsigned int mx,my;
    unsigned short previous_tile;
    int erased = 0, placed;
    display_to_world(cursor_x,cursor_y,&mx,&my);
    if (kind != ZONE_NONE || selected_tool == EDIT_ROAD ||
        selected_tool == EDIT_RAIL || selected_tool == EDIT_POWER) {
        build_preview_start(mx, my);
        build_preview_finish();
        return;
    }
    if(selected_tool==EDIT_INSPECT) {
        /* Four detailed pages are populated from one bounded local
         * snapshot. Do not force a city-wide power recalculation,
         * and never change original map/zone/save data on inspection.
         */
        tile_inspect_collect(mx,my,&inspected_tile);
        info_page=4;
        inspector_window_drawn=0;
        last_action="INSPECT TILE";
        draw_info_page();
    } else {
        unsigned int larger=0U,bx=0U,by=0U;
        previous_tile=city_at(mx,my);
        if(selected_tool==EDIT_BULLDOZE)
            (void)newer_lot_info(mx,my,&bx,&by,&larger,0,0);
        /* A multi-lot bulldoze must restore every underlying 1x1
         * empty zone, not delete the one clicked zone assignment.
         */
        if (selected_tool == EDIT_BULLDOZE && !larger)
            erased = zoning_set(mx, my, ZONE_NONE);
        placed = edit_place(selected_tool,mx,my);
        if (placed) {
            /* Ordinary bulldozing of trees, roads, rail, or housing
             * with no CONDBIT does NOT affect electricity. Scanning
             * all 12,000 city cells in that case caused V20 pauses.
             * Coal construction and removing conductive crossings,
             * wires or powered buildings still invalidate the graph.
             */
            if (selected_tool==EDIT_COAL ||
                selected_tool==EDIT_NUCLEAR ||
                selected_tool==EDIT_POLICE ||
                selected_tool==EDIT_FIRE ||
                ((previous_tile ^ city_at(mx,my)) & 16384U)!=0U)
                simulation_mark_power_dirty();
            last_action = larger?"CLEARED":"PLACED";
            if(larger) {
                redraw_zoning_brush(bx,by,bx+larger-1U,
                                   by+larger-1U);
            } else
                redraw_edit_area(selected_tool==EDIT_BULLDOZE?1U:2U);
            draw_cursor();
        } else {
            last_action=erased?"UNZONED":"BLOCKED";
        }
        if (erased) {
            simulation_mark_lots_dirty();
            redraw_zoning_brush(mx,my,mx,my);
        }
        if (placed || erased) {
            if(selected_tool==EDIT_BULLDOZE) {
                if(larger)
                    draw_overview_world_rect(bx,by,bx+larger-1U,
                                             by+larger-1U,0);
                else draw_overview_world_tile(mx,my);
            } else if(selected_tool==EDIT_COAL ||
                      selected_tool==EDIT_NUCLEAR ||
                      selected_tool==EDIT_POLICE ||
                      selected_tool==EDIT_FIRE) {
                /* No 2,464-pixel minimap refresh for a 3x3/4x4 site. */
                unsigned int reach=(selected_tool==EDIT_COAL ||
                                    selected_tool==EDIT_NUCLEAR)?2U:1U;
                draw_overview_world_rect(mx-1U,my-1U,
                                         mx+reach,my+reach,0);
            } else {
                draw_overview_world_tile(mx,my);
            }
        }
    }
    draw_status();
}

/* Rotate around the world tile in the CENTER of the visible city,
 * regardless of where the yellow selection cursor happens to be.
 */
static void rotate_view(int clockwise)
{
    unsigned int wx,wy;
    if (build_drag_active)
        build_preview_cancel(); /* never change view mid-purchase */
    zone_erase_active = 0;
    display_to_world(VIEW_W/2U,VIEW_H/2U,&wx,&wy);
    map_turns = (map_turns + (clockwise ? 1U : 3U)) & 3U;
    view_rotation_focus(map_turns,wx,wy,VIEW_W,VIEW_H,
                        &scroll_x,&scroll_y,&cursor_x,&cursor_y);
    zone_minimap_dirty = 0;
    last_action = clockwise ? "ROT CW" : "ROT CCW";
    /* Rerender the rotated city and overview, not the toolbar.
     * Ordinary tile-by-tile panning still uses the fast VRAM shifts.
     */
    redraw_viewport_only(1);
}

static void run_menu_action(enum ui_action action)
{
    unsigned int powered;
    switch (action) {
    case UI_SAVE:
        save_current_city();
        break;
    case UI_LOAD_LAST:
        load_saved_city();
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
        /* The old popup path used render_full() to erase the cursor.
         * Redraw just its previous and new tiles instead.
         */
        erase_cursor();
        cursor_x = VIEW_W / 2U;
        cursor_y = VIEW_H / 2U;
        draw_cursor();
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

/* Popup shares the 4,608-byte screen-underlay buffer with the main
 * menus. These are mutually exclusive and therefore allocate no new
 * 128x32 framebuffer in precious 16-bit DOS conventional memory.
 */
#define PICKER_COLUMN 8U
#define PICKER_PIXELS_X (PICKER_COLUMN*8U)
static unsigned int picker_top_row(void)
{
    return tool_picker_top_y(picker_kind)/8U;
}
static unsigned int picker_count(void)
{
    return picker_kind==1U?2U:4U;
}
static const char *picker_text(unsigned int index)
{
    static const char *const road_items[2]={" ROAD"," RAIL"};
    static const char *const civic_items[4]={
        " COAL"," NUCLEAR"," POLICE"," FIRE DEPT"
    };
    return picker_kind==1U?road_items[index]:civic_items[index];
}
static int picker_hit(unsigned int x,unsigned int y)
{
    return tool_picker_hit(picker_kind,x,y);
}
static void picker_row(unsigned int index)
{
    unsigned int col,py;
    if(!picker_kind || index>=picker_count()) return;
    py=(picker_top_row()+index)*8U;
    for(col=0U;col<16U;++col)
        solid_tile(PICKER_PIXELS_X+col*8U,py,
                   (unsigned char)(index==picker_highlight?5U:1U));
    bios_text(picker_top_row()+index,PICKER_COLUMN+1U,
              picker_text(index),
              (unsigned char)(index==picker_highlight?15U:14U));
}
static void picker_close(void)
{
    if(!picker_kind) return;
    mouse_pointer_hide();
    menu_overlay_restore();
    picker_kind=0U;
    hold_button=-1;
}
static void picker_open(unsigned int button)
{
    unsigned int row;
    if(picker_kind || open_menu!=MENU_NONE) return;
    mouse_pointer_hide();
    picker_kind=button==1U?1U:2U;
    picker_highlight=0U;
    /* Capture the SAME y=24/96 region picker_row will paint; the
     * generic restore must not assume a normal main-menu y=16.
     */
    menu_overlay_capture_rect(PICKER_COLUMN,
                              tool_picker_top_y(picker_kind),
                              picker_count()*8U);
    for(row=0U;row<picker_count();++row) picker_row(row);
    mouse_pointer_show();
}
static void picker_hover(int item)
{
    unsigned int old;
    if(item<0 || (unsigned int)item>=picker_count() ||
       (unsigned int)item==picker_highlight) return;
    old=picker_highlight;
    picker_highlight=(unsigned int)item;
    picker_row(old);
    picker_row(picker_highlight);
}
static void picker_choose(unsigned int item)
{
    enum edit_tool next;
    if(!picker_kind || item>=picker_count()) return;
    next=tool_picker_choice(picker_kind,item);
    picker_close();
    select_variant(next);
    draw_status();
}
static unsigned int mouse_bios_tick(void)
{
    union REGS r;
    r.h.ah=0U;
    int86(0x1a,&r,&r);
    return r.x.dx;
}

static void show_menu(enum ui_menu menu)
{
    if (picker_kind) picker_close();
    if (build_drag_active)
        build_preview_cancel();
    /* A change of menus restores just the old 128-pixel-wide popup,
     * not the city, toolbox, or minimap.
     */
    menu_overlay_restore();
    open_menu=menu;
    menu_highlight=0U;
    menu_overlay_capture();
    draw_menu_popup();
    mouse_pointer_show();
}

/* Mouse movement paints only the solid tile-selection square in the city.
 * The software arrow appears only over menus, toolbox and non-city UI.
 * Dragging only edits single-tile tools, preventing accidental 3x3/4x4
 * buildings during a mouse stroke.
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
        if (info_page == 0 && open_menu == MENU_NONE &&
            !picker_kind && inside) {
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
        } else if (picker_kind) {
            picker_hover(picker_hit(state.x,state.y));
        } else if (open_menu != MENU_NONE) {
            item = ui_dropdown_hit(open_menu, state.x, state.y);
            if (item >= 0 && (unsigned int)item != menu_highlight) {
                menu_set_highlight((unsigned int)item);
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
        if (picker_kind) {
            item=picker_hit(state.x,state.y);
            if(item>=0) picker_choose((unsigned int)item);
            else picker_close();
        } else if (info_page >= 4 && info_page < 8) {
            /* The lower-right DONE button closes the panel.
             * PREV/NEXT navigate. A click elsewhere still advances
             * pages for users accustomed to the previous inspector.
             */
            unsigned int action=tile_inspect_dialog_click(
                state.x,state.y);
            if(action==3U) {
                info_page=0;
                inspector_window_drawn=0;
                render_full();
            } else {
                if(action==1U)
                    info_page=4+((info_page-1)%4);
                else
                    info_page=4+((info_page-3)%4);
                draw_info_page();
            }
        } else if (info_page != 0) {
            info_page = 0;
            render_full();
        } else if (open_menu != MENU_NONE) {
            menu = ui_header_hit(state.x, state.y);
            if (menu != MENU_NONE) {
                if (menu == open_menu) {
                    menu_overlay_close();
                } else
                    show_menu((enum ui_menu)menu);
            } else {
                item = ui_dropdown_hit(open_menu, state.x, state.y);
                if (item >= 0) {
                    enum ui_action command =
                        ui_menu_command(open_menu, (unsigned int)item);
                    menu_overlay_close();
                    run_menu_action(command);
                    if (info_page != 0)
                        draw_info_page();
                    else if (!exit_requested &&
                             command != UI_ROTATE_CW &&
                             command != UI_ROTATE_CCW)
                        draw_status();
                } else {
                    menu_overlay_close();
                }
            }
        } else {
            menu = ui_header_hit(state.x, state.y);
            if (menu != MENU_NONE) {
                show_menu((enum ui_menu)menu);
            } else {
                toolbar = ui_toolbar_hit(state.x, state.y);
                if (toolbar >= 0) {
                    if(tool_picker_button(toolbar)) {
                        /* Immediate selection with no click delay.
                         * Holding on this button opens the variants.
                         */
                        select_variant(toolbar==1?favorite_road:
                                                 favorite_civic);
                        hold_button=toolbar;
                        hold_tick=mouse_bios_tick();
                    } else {
                        select_tool(ui_toolbar_tool((unsigned int)toolbar));
                        hold_button=-1;
                    }
                    draw_status();
                } else if (state.x>=UI_MINIMAP_X &&
                           state.x<UI_MINIMAP_X+UI_MINIMAP_W &&
                           state.y>=UI_MINIMAP_Y &&
                           state.y<UI_MINIMAP_Y+UI_MINIMAP_H) {
                    /* A click in the small MAP panel jumps to that
                     * tile in the currently rotated world. Keep the
                     * toolbar intact and redraw only the city view.
                     */
                    if(view_rotation_minimap_focus(
                          map_turns,state.x-UI_MINIMAP_X,
                          state.y-UI_MINIMAP_Y,
                          UI_MINIMAP_W,UI_MINIMAP_H,VIEW_W,VIEW_H,
                          &scroll_x,&scroll_y,&cursor_x,&cursor_y)) {
                        last_action="MAP JUMP";
                        redraw_viewport_only(0);
                    }
                } else if (inside) {
                    if (selected_zone_kind() != ZONE_NONE ||
                        selected_tool == EDIT_ROAD ||
                        selected_tool == EDIT_RAIL ||
                        selected_tool == EDIT_POWER) {
                        display_to_world(cursor_x,cursor_y,
                                         &world_x,&world_y);
                        build_preview_start(world_x,world_y);
                    } else {
                        if (selected_tool==EDIT_BULLDOZE)
                            doze_mouse_active=1;
                        apply_selected_tool();
                    }
                }
            }
        }
        mouse_pointer_show();
    }

    if (right_press) {
        mouse_pointer_hide();
        if (picker_kind) {
            picker_close();
        } else if (info_page != 0) {
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
            menu_overlay_close();
        } else if (!build_drag_active) {
            show_menu(MENU_TOOLS);
        }
        mouse_pointer_show();
    }

    if (left_release) {
        doze_mouse_active=0;
        if(picker_kind) {
            item=picker_hit(state.x,state.y);
            if(item>=0) {
                mouse_pointer_hide();
                picker_choose((unsigned int)item);
                mouse_pointer_show();
            }
            /* Releasing on the original button leaves options
             * open for a later click.
             */
        }
        hold_button=-1;
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
        draw_overview_world_rect(zone_dirty_x0,zone_dirty_y0,
                                 zone_dirty_x1,zone_dirty_y1,0);
        mouse_pointer_show();
        zone_minimap_dirty = 0;
    }
    /* Read the BIOS clock only while one group button is held.
     * Regular mouse movements incur no extra clock interrupts.
     */
    if(hold_button>=0 && (state.buttons&1U) && !picker_kind) {
        if(ui_toolbar_hit(state.x,state.y)!=hold_button)
            hold_button=-1;
        else if(tool_picker_ready(mouse_bios_tick(),hold_tick)) {
            picker_open((unsigned int)hold_button);
            hold_button=-1;
        }
    }
    mouse_buttons = state.buttons;
}


/* Check a small fraction of the visible city only after real changes
 * to the electrical network or adjacent-lot power relays. Draw only
 * changed 8x8 tiles, leaving the toolbar and rest of the city intact.
 */
static void update_power_badges(void)
{
    unsigned int revision=simulation_power_visual_revision();
    unsigned int checked=0U;
    int redrawn=0;
    if(revision!=badge_revision) {
        badge_revision=revision;
        badge_sweep_pos=0U;
    }
    while(badge_sweep_pos<VIEW_W*VIEW_H &&
          checked++<BADGE_CHECKS_PER_TICK) {
        unsigned int index=badge_sweep_pos++;
        unsigned int sx=index%VIEW_W,sy=index/VIEW_W;
        unsigned int wx,wy;
        int warning;
        display_to_world(sx,sy,&wx,&wy);
        warning=simulation_needs_power_warning(wx,wy);
        if(warning!=badge_shadow_get(index)) {
            /* Usually only an offscreen cursor is showing, but the
             * arrow can straddle the city/sidebar border. Protect its
             * saved background only when an actual badge changes.
             */
            mouse_pointer_hide();
            redrawn=1;
            draw_display_tile(sx,sy);
            if(sx==cursor_x && sy==cursor_y) draw_cursor();
        }
    }
    if(redrawn) mouse_pointer_show();
}

/* Gently pulse the *cached* visible warnings once per ~1.3 sec.
 * No power or zone lookups. Recolor only seven VRAM pixels per icon,
 * with one mouse pointer hide/show for the entire phase transition.
 */
static void update_badge_flash(unsigned int now)
{
    unsigned int color=badge_flash_color(now),i;
    int hidden=0;
    if(color==badge_visible_color) return;
    badge_visible_color=color;
    for(i=0U;i<VIEW_W*VIEW_H;++i) {
        if(!badge_shadow_get(i))continue;
        if(!hidden){mouse_pointer_hide();hidden=1;}
        recolor_no_power_badge(VIEW_X+(i%VIEW_W)*8U,
                               VIEW_Y+(i/VIEW_W)*8U,color);
    }
    if(hidden) {
        draw_cursor();
        mouse_pointer_show();
    }
}

/* Cache visible authentic stadium center coordinates only when the
 * view moves/rotates. Do not scan all 12,000 city tiles or every 8x8
 * city square on every football animation frame.
 */
static void stadium_find_visible(unsigned int now)
{
    unsigned int sx,sy,wx,wy,cx,cy,i;
    stadium_count=0U;
    stadium_last_scroll_x=scroll_x;
    stadium_last_scroll_y=scroll_y;
    stadium_last_turns=map_turns;
    for(sy=0U;sy<VIEW_H;++sy)
        for(sx=0U;sx<VIEW_W;++sx) {
            display_to_world(sx,sy,&wx,&wy);
            if(!stadium_fx_center(wx,wy,&cx,&cy))continue;
            for(i=0U;i<stadium_count;++i)
                if(stadium_x[i]==cx && stadium_y[i]==cy)break;
            if(i!=stadium_count ||
               stadium_count>=STADIUM_MAX_VISIBLE)continue;
            stadium_x[stadium_count]=(unsigned char)cx;
            stadium_y[stadium_count]=(unsigned char)cy;
            /* Force one synchronized redraw. This is infrequent;
             * subsequent frames only touch stadiums that changed.
             */
            stadium_frame[stadium_count]=255U;
            ++stadium_count;
        }
    (void)now;
}

/* About two football frames/second during short, staggered games.
 * Four-by-four original-art redraws are limited to visible stadiums,
 * and unchanged periods cost only a handful of integer comparisons.
 */
static void update_stadium_games(unsigned int now)
{
    unsigned int i,j,k,cx,cy,phase,sx,sy;
    int hidden=0;
    if((unsigned int)(now-stadium_last_update)<STADIUM_FX_FRAME_TICKS)
        return;
    stadium_last_update=now;
    stadium_clock=now;
    if(stadium_last_scroll_x!=scroll_x ||
       stadium_last_scroll_y!=scroll_y ||
       stadium_last_turns!=map_turns)
        stadium_find_visible(now);
    for(i=0U;i<stadium_count;++i) {
        cx=stadium_x[i];cy=stadium_y[i];
        if(!stadium_fx_center(cx,cy,0,0)) {
            stadium_frame[i]=0U;
            continue;
        }
        phase=stadium_fx_frame(now,cx,cy,
                               (city_at(cx,cy)&32768U)!=0U);
        if(phase==stadium_frame[i])continue;
        stadium_frame[i]=(unsigned char)phase;
        if(!hidden){mouse_pointer_hide();hidden=1;}
        for(j=0U;j<4U;++j)
            for(k=0U;k<4U;++k) {
                unsigned int wx=cx-1U+k,wy=cy-1U+j;
                if(world_to_visible(wx,wy,&sx,&sy))
                    draw_display_tile(sx,sy);
            }
    }
    if(hidden) {
        draw_cursor();
        mouse_pointer_show();
    }
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
        picker_kind != 0U || info_page != 0 ||
        build_drag_active || zone_erase_active ||
        doze_mouse_active)
        return;
    amount = 80U * simulation_speed();
    event = simulation_advance(amount);
    /* The old unconditional hide/show blinked the arrow ~9 times/sec
     * even when the simulation did not redraw a single pixel.
     * Only interrupt the arrow for real tile/status rendering.
     */
    if (event & (SIM_EVENT_TILE | SIM_EVENT_MONTH))
        mouse_pointer_hide();
    if (event & SIM_EVENT_TILE) {
        simulation_last_change(&tx, &ty);
        if(simulation_last_change_radius() >= 2U) {
            unsigned int span=simulation_last_change_radius();
            redraw_zoning_brush(tx,ty,tx+span-1U,ty+span-1U);
        } else if (simulation_last_change_radius() != 0U) {
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
    update_power_badges();
    if (event & (SIM_EVENT_TILE | SIM_EVENT_MONTH))
        mouse_pointer_show();
    /* Separate visual effects from simulation: neither changes
     * electrical connectivity, city tile IDs, or the save files.
     */
    update_badge_flash(now);
    update_stadium_games(now);
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
            if(info_page>=4 && info_page<8 &&
               (scan==0x4dU || scan==0x50U || ascii==9U ||
                ascii==13U || ascii==' ')) {
                /* Right/down/Tab/Enter = next inspector page. */
                info_page=4+((info_page-3)%4);
                draw_info_page();
            } else if(info_page>=4 && info_page<8 &&
                      (scan==0x4bU || scan==0x48U)) {
                /* Left/up = previous inspector page. */
                info_page=4+((info_page-1)%4);
                draw_info_page();
            } else {
                info_page=0;
                inspector_window_drawn=0;
                render_full();
            }
            mouse_pointer_show();
            continue;
        }

        if (picker_kind) {
            if(ascii==27U) picker_close();
            else if(ascii>='1' && ascii<='4' &&
                    (unsigned int)(ascii-'1')<picker_count())
                picker_choose((unsigned int)(ascii-'1'));
            else if(ascii==13U || ascii==' ')
                picker_choose(picker_highlight);
            else if(scan==0x48U && picker_highlight>0U) {
                mouse_pointer_hide();
                picker_hover((int)picker_highlight-1);
            } else if(scan==0x50U &&
                      picker_highlight+1U<picker_count()) {
                mouse_pointer_hide();
                picker_hover((int)picker_highlight+1);
            }
            mouse_pointer_show();
            continue;
        }

        if (open_menu != MENU_NONE) {
            next_menu = open_menu;
            if (ascii == 27U) {
                menu_overlay_close();
            } else if (ascii == 13U || ascii == ' ') {
                enum ui_action command =
                    ui_menu_command(open_menu, menu_highlight);
                menu_overlay_close();
                run_menu_action(command);
                if (info_page != 0)
                    draw_info_page();
                else if (!exit_requested &&
                         command != UI_ROTATE_CW &&
                         command != UI_ROTATE_CCW)
                    draw_status();
            } else if (scan == 0x48U) {
                if (menu_highlight > 0U)
                    menu_set_highlight(menu_highlight-1U);
            } else if (scan == 0x50U) {
                if (menu_highlight + 1U < ui_menu_count(open_menu))
                    menu_set_highlight(menu_highlight+1U);
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
                draw_overview_world_rect(zone_dirty_x0,zone_dirty_y0,
                                         zone_dirty_x1,zone_dirty_y1,0);
                zone_minimap_dirty = 0;
            }
        }
        else if (ascii == 'p' || ascii == 'P')
            run_menu_action(UI_SCAN_POWER);
        else if (scan == 0x3cU)
            run_menu_action(UI_SAVE);
        else if (scan == 0x3dU) {
            run_menu_action(UI_LOAD_LAST);
            mouse_pointer_show();
            continue;
        }

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

/* WolfensteinCGA v1.0 used a repeatable TIMEDemo to measure real
 * drawing work, rather than only timing isolated memory operations.
 * Reuse our existing single-tile VRAM scrolling, 32x21 map, original
 * assets and GUI. No extra framebuffer or render rewrite.
 *
 * The path is fixed: right 12, down 8, left 12, up 8, twice (80 pans).
 * Initial camera is centered so the route always stays in bounds in
 * both a 120x100 original city and the built-in demo. Every turn
 * returns the camera to the same world square. Simulation, cursor and
 * mouse are excluded; the second pass measures ordinary minimap/HUD
 * refresh overhead separately from city drawing itself.
 */
#define TIMEDEMO_PANS 80U
static unsigned long timedemo_city_ticks, timedemo_hud_ticks;

static void timedemo_route(int include_hud)
{
    unsigned int frame, position, old_x, old_y;
    for (frame=0U;frame<TIMEDEMO_PANS;++frame) {
        position=frame%40U;
        old_x=scroll_x;
        old_y=scroll_y;
        if (position<12U)
            ++scroll_x;
        else if (position<20U)
            ++scroll_y;
        else if (position<32U)
            --scroll_x;
        else
            --scroll_y;
        scroll_one_tile(old_x,old_y);
        if (include_hud) {
            draw_overview_camera();
            draw_status();
        }
    }
}

static void benchmark_timdemo(void)
{
    unsigned long start;
    /* Single initial screen draw, never a redraw on each frame.
     * Cursor removal avoids carrying an 8x8 selection box while the
     * camera moves. This intentionally shows live city scrolling.
     */
    map_turns=0U;
    scroll_x=(CITY_WIDTH-VIEW_W)/2U;
    scroll_y=(CITY_HEIGHT-VIEW_H)/2U;
    render_full();
    mouse_pointer_hide();
    erase_cursor();

    start=bench_bios_ticks();
    timedemo_route(0);
    timedemo_city_ticks=bench_bios_ticks()-start;

    /* Reset pixels outside the timed block to compare like for like. */
    render_full();
    mouse_pointer_hide();
    erase_cursor();
    start=bench_bios_ticks();
    timedemo_route(1);
    timedemo_hud_ticks=bench_bios_ticks()-start;
    /* Do not rebuild the screen on exit; DOS text mode is restored. */
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
    int timedemo_mode = 0;
    char sidecar[256];
    newer_lot_clear();
    for (i = 1; i < argc; ++i) {
        if (dos_option_is(argv[i], "NOMOUSE")) {
            allow_mouse = 0;
        } else if (dos_option_is(argv[i], "BENCH")) {
            bench_mode = 1;
            allow_mouse = 0;
        } else if (dos_option_is(argv[i], "TIMEDEMO")) {
            timedemo_mode = 1;
            allow_mouse = 0;
        } else if (!have_city && city_load(argv[i])) {
            have_city = 1;
            /* A local saved CITYnnnn.CTY can be updated via F2.
             * Explicit original HAPPISLE.CTY is copied instead.
             */
            (void)game_save_parse_slot(argv[i],&active_save_slot);
            /* Original HAPPISLE starts with water in the upper left.
             * Start near actual homes so growth is visible to players.
             * This is camera-only; it never changes the city file.
             */
            (void)city_suggest_view(VIEW_W, VIEW_H,
                                    &scroll_x, &scroll_y);
            zoning_seed_from_city();
            if (zoning_sidecar_name(argv[i], sidecar,
                                    (unsigned int)sizeof(sidecar))) {
                char *ext;
                (void)zoning_load(sidecar);
                ext=strrchr(sidecar,'.');
                if(ext!=NULL) {
                    strcpy(ext,".NBL");
                    (void)newer_lot_load(sidecar);
                }
            }
        } else {
            fprintf(stderr,
                    "Unrecognized argument or invalid 27120-byte city: %s\n",
                    argv[i]);
            return 1;
        }
    }
    if (!have_city && game_save_load_last(&active_save_slot)) {
        have_city=1;
        (void)city_suggest_view(VIEW_W,VIEW_H,
                                &scroll_x,&scroll_y);
        sprintf(save_action,"RESUME CITY%04u",active_save_slot);
        last_action=save_action;
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
    /* Canonical neutral filenames. The unchanged first-generation
     * 1x1 byte pack remains accepted for existing installations.
     */
    if(!small_lot_art_load("NEWER1.DAT"))
        (void)small_lot_art_load("SC2K.DAT");
    (void)newer_lot_load_art("NEWERM8.DAT");
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
    } else if (timedemo_mode) {
        benchmark_timdemo();
    } else {
        render_full();
        keyboard_loop();
    }
    mouse_pointer_hide();
    mouse_shutdown();
    engine_shutdown();
    newer_lot_free_art();
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
    if (timedemo_mode) {
        unsigned long fps10;
        puts("MICROPOL /TIMEDEMO - 80 real one-tile pans");
        printf("City-only render  : %lu ticks (~%lu ms)\n",
               timedemo_city_ticks,timedemo_city_ticks*55UL);
        printf("City + HUD/map    : %lu ticks (~%lu ms)\n",
               timedemo_hud_ticks,timedemo_hud_ticks*55UL);
        if (timedemo_city_ticks != 0UL) {
            fps10=(TIMEDEMO_PANS*182UL)/timedemo_city_ticks;
            printf("City pan rate     : %lu.%lu pans/sec\n",
                   fps10/10UL,fps10%10UL);
        }
        if (timedemo_hud_ticks != 0UL) {
            fps10=(TIMEDEMO_PANS*182UL)/timedemo_hud_ticks;
            printf("City+HUD rate     : %lu.%lu pans/sec\n",
                   fps10/10UL,fps10%10UL);
        }
        puts("Measured with BIOS ~55ms ticks; no simulation or file edits.");
    }
    return 0;
}
