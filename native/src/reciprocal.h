/* Reciprocals without a divide instruction (the SH4AL-DSP has none). */
#ifndef TEMPLE_RECIPROCAL_H
#define TEMPLE_RECIPROCAL_H

#include <stdint.h>

/* 2^31 / m for the middle of each of 256 ranges of m in [2^15, 2^16). */
extern uint32_t tr_reciprocal_table[256];

/* For 0 < d < 2^31, returns r so that 1/d is about r * 2^shift / 2^47, with a
 * relative error near 2^-15 after one Newton step. shift counts d's leading
 * zero bits. */
static inline uint32_t tr_reciprocal(uint32_t d, int *shift)
{
    int s = 0;
    if (d < 1u << 16) { d <<= 16; s = 16; }
    if (d < 1u << 24) { d <<= 8; s += 8; }
    if (d < 1u << 28) { d <<= 4; s += 4; }
    if (d < 1u << 30) { d <<= 2; s += 2; }
    if (d < 1u << 31) { d <<= 1; s += 1; }
    uint32_t m = d >> 16;
    uint32_t r = tr_reciprocal_table[(m >> 7) - 256];
    int32_t error = (int32_t)(0x80000000u - m * r);
    r = (uint32_t)((int32_t)r + (((int32_t)r * (error >> 8)) >> 23));
    *shift = s;
    return r;
}

#endif
