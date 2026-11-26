#ifndef TEMPLE_ROWS_H
#define TEMPLE_ROWS_H

/* The rasterizer's inner loops: a run of rows between two edges. The SH
 * version is hand-written (sh4.S); the C version defines what it must do. */
#include <stddef.h>
#include <stdint.h>

/* sh4.S depends on this layout. All arithmetic wraps modulo 2^32. */
typedef struct {
    uint32_t *line;               /* first pixel of the current row */
    uint32_t rows;                /* at least one */
    uint32_t left_x, left_step;   /* edges, 16.16 pixels at the row centre */
    uint32_t right_x, right_step;
    uint32_t z, zx, zy;           /* depth at column 0, per column, per row */
    uint32_t u, ux, uy;           /* 16.16 texels, likewise */
    uint32_t v, vx, vy;           /* texels times the texture width */
    const uint16_t *texels;       /* NULL fills with color */
    uint32_t v_mask, u_mask;      /* index = (v>>16 & v_mask) | (u>>16 & u_mask) */
    uint32_t color;
    /* For a level surface, the view depth of each row. u and v then hold
     * u/depth and v/depth, without the texture width factor on v. */
    const int32_t *floor_depth;
    uint32_t v_shift;             /* 12 - log2(texture width), if floor_depth */
} TRRows;

/* Draws rows->rows rows, covering the pixels whose centres lie in
 * left <= x < right and passing the depth test, and advances the state to the
 * row after the last one. width is TR_WIDTH. */
void tr_rows_c(TRRows *rows);
#ifdef __sh__
void tr_rows_sh(TRRows *rows);
#define tr_rows tr_rows_sh
#else
#define tr_rows tr_rows_c
#endif

/* Fills lines blocks of eight words, at least one, starting at a 32-byte
 * boundary. The SH version avoids reading memory it is about to overwrite. */
void tr_fill_c(uint32_t *pixels, uint32_t lines, uint32_t value);
#ifdef __sh__
void tr_fill_sh(uint32_t *pixels, uint32_t lines, uint32_t value);
#define tr_fill tr_fill_sh
#else
#define tr_fill tr_fill_c
#endif

#endif
