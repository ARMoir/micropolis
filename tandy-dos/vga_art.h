/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MICROPOL_VGA_ART_H
#define MICROPOL_VGA_ART_H
#include "video_ops.h"
int vga_art_load(void);
void vga_art_free(void);
const unsigned char *vga_art_palette(void);
void vga_art_begin(unsigned int x,unsigned int y,int dry,int clear);
void vga_art_stamp(unsigned int x,unsigned int y,int newer,unsigned int tile);
void vga_art_scroll(int dx,int dy);
int vga_art_active(int enabled);
unsigned char vga_art_pixel(unsigned int x,unsigned int y,unsigned char ink);
void vga_present_detail_rows(const volatile unsigned char TANDY_FAR *packed,
    volatile unsigned char TANDY_FAR *screen,unsigned char *dirty);
#endif
