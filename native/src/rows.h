#ifndef TEMPLE_ROWS_H
#define TEMPLE_ROWS_H

/* The rasterizer's hot loops: an attribute's setup, and a run of rows between
 * two edges. The SH versions are hand-written (sh4.S); the C versions define
 * what they must do. */
#include <stddef.h>
#include <stdint.h>

/* A quantity that is linear in screen space, all modulo 2^32: its value at
 * column 0 of a row, and its change per column and per row. */
typedef struct { uint32_t value, dx, dy; } TRLinear;

/* What the attributes of one triangle share. */
typedef struct {
    int32_t xb, xc, yc, yb; /* reciprocal of the area times edge components */
    int32_t shift;          /* 12 to 43 */
    /* From vertex a to the centre of its pixel, in sixteenths of a pixel. */
    int32_t fraction_x, fraction_y;
    uint32_t anchor_x;      /* that pixel's column */
    uint32_t rows;          /* from its row to the first row to draw */
} TRPlane;

/* Solves for one attribute. out->value holds it at vertex a, and db and dc are
 * its differences to b and c; out then describes it from the first row. sh4.S
 * depends on the layouts of TRPlane and TRLinear. */
void tr_plane_c(const TRPlane *plane, TRLinear *out, int32_t db, int32_t dc);
#ifdef __sh__
void tr_plane_sh(const TRPlane *plane, TRLinear *out, int32_t db, int32_t dc);
#define tr_plane tr_plane_sh
#else
#define tr_plane tr_plane_c
#endif

/* sh4.S depends on this layout. All arithmetic wraps modulo 2^32. */
typedef struct {
    uint32_t *line;               /* first pixel of the current row */
    uint32_t rows;                /* at least one */
    /* Edges at the row's centre: 16.16 pixels plus 0x7fff, so that the high
     * half is the first column whose centre is not left of the edge. */
    uint32_t left_x, left_step;
    uint32_t right_x, right_step;
    TRLinear z;                   /* depth in the high half */
    TRLinear u, v;                /* 16.16; unused without texels */
    /* For a level surface, the view depth of each row, and u and v then hold
     * u/depth and v/depth in texels. Otherwise NULL, and u and v hold byte
     * offsets: twice the texel, with v also times the texture width. */
    const int32_t *floor_depth;
    const uint16_t *texels;       /* NULL fills with color */
    /* offset = (v>>16 & v_mask) | (u>>16 & u_mask), in bytes */
    uint32_t v_mask, u_mask;
    uint32_t color;
    uint32_t v_shift;             /* 12 - log2(texture width), if floor_depth */
} TRRows;

/* Draws rows->rows rows, covering the pixels whose centres lie in
 * left <= x < right and passing the depth test, and advances the state it uses
 * to the row after the last one. width is TR_WIDTH. */
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
