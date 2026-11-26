#ifndef TEMPLE_PRESENT_H
#define TEMPLE_PRESENT_H

#include "raster.h"

#define TR_SCREEN_WIDTH 396
#define TR_SCREEN_HEIGHT 224

/* Doubles every framebuffer pixel into the middle of the screen, leaving the
 * border untouched. screen holds 396x224 RGB565 pixels, 4-byte aligned. */
void tr_present_scale(const TRFramebuffer *fb, uint16_t *screen);
/* Portable form of the same, the reference for the calculator's assembly. */
void tr_present_scale_c(const TRFramebuffer *fb, uint16_t *screen);

#endif
