#include "present.h"

#define LEFT ((TR_SCREEN_WIDTH - TR_WIDTH * 2) / 2)
#define TOP ((TR_SCREEN_HEIGHT - TR_HEIGHT * 2) / 2)

#ifdef __sh__
/* In sh4.S, which assumes these sizes. */
void tr_present_sh(const uint32_t *pixels, uint32_t *row);
_Static_assert(TR_WIDTH == 192 && TR_HEIGHT == 108 && TR_SCREEN_WIDTH == 396,
    "sh4.S must match the frame and screen sizes");
#endif

/* Two identical pixels written at once halve the writes to video RAM. */
typedef uint32_t __attribute__((__may_alias__)) PixelPair;
_Static_assert(LEFT % 2 == 0 && TR_SCREEN_WIDTH % 2 == 0, "VRAM rows must align pixel pairs");

void tr_present_scale_c(const TRFramebuffer *fb, uint16_t *screen)
{
    const uint32_t *source = fb->pixels;
    for (int y = 0; y < TR_HEIGHT; y++) {
        PixelPair *row = (PixelPair *)(screen + (y * 2 + TOP) * TR_SCREEN_WIDTH + LEFT);
        PixelPair *end = row + TR_WIDTH;
        do {
#define DOUBLE(i) { uint32_t pixel = source[i]; \
            uint32_t pair = pixel << 16 | (pixel & 0xffffu); \
            row[i] = pair; row[i + TR_SCREEN_WIDTH / 2] = pair; }
            DOUBLE(0) DOUBLE(1) DOUBLE(2) DOUBLE(3)
#undef DOUBLE
            source += 4; row += 4;
        } while (row != end);
    }
}

void tr_present_scale(const TRFramebuffer *fb, uint16_t *screen)
{
#ifdef __sh__
    tr_present_sh(fb->pixels,
        (uint32_t *)(void *)(screen + TOP * TR_SCREEN_WIDTH + LEFT));
#else
    tr_present_scale_c(fb, screen);
#endif
}
