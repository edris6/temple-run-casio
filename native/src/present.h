#ifndef TEMPLE_PRESENT_H
#define TEMPLE_PRESENT_H

#include "raster.h"

/* Copies the colours of rows drawn pixels (an even number of rows) to the
 * screen, whose rows follow each other. screen points to the first pixel to
 * replace and is 4-byte aligned. */
void tr_present_rows(const uint32_t *pixels, uint16_t *screen, int rows);
/* Portable form of the same, the reference for the calculator's assembly. */
void tr_present_rows_c(const uint32_t *pixels, uint16_t *screen, int rows);

#endif
