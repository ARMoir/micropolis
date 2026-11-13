/* SPDX-License-Identifier: GPL-3.0-or-later
 * Hardware-independent compact toolbar artwork. Color 0..15 is a
 * native Tandy palette index; 255 means leave existing background.
 */
#ifndef TANDY_TOOL_ART_H
#define TANDY_TOOL_ART_H
#define TOOL_ART_TRANSPARENT 255U

/* Tool key IDs 2=PWR, 3=RES, 4=COM, 5=IND, 7=COAL have pictograms.
 * Road 1, Inspector 0 and Bulldozer 6 retain their known-good art.
 */
int tool_art_has_icon(unsigned int tool);
unsigned int tool_art_pixel(unsigned int tool, unsigned int x,
                            unsigned int y);
#endif
