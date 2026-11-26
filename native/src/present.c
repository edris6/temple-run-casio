#include "present.h"

#ifdef __sh__
/* In sh4.S, which handles eight pixels at a time. */
void tr_present_sh(const uint32_t *pixels, uint32_t *screen, uint32_t groups);
_Static_assert(TR_WIDTH * 2 % 8 == 0, "an even number of rows must be whole groups");
#endif

void tr_present_rows_c(const uint32_t *pixels, uint16_t *screen, int rows)
{
    for (int i = 0; i < rows * TR_WIDTH; i++) screen[i] = tr_pixel_color(pixels[i]);
}

void tr_present_rows(const uint32_t *pixels, uint16_t *screen, int rows)
{
#ifdef __sh__
    tr_present_sh(pixels, (uint32_t *)(void *)screen, (uint32_t)(rows * TR_WIDTH / 8));
#else
    tr_present_rows_c(pixels, screen, rows);
#endif
}
