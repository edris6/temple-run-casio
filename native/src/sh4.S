! Hand-written SH-4 loops for the fx-CG50, where most of each frame is spent.
! rows.c and present.c hold the C equivalents; tests/cg_check.c compares them.
! Register use follows the GCC ABI: r8-r14 are saved, r0-r7 and MAC are free.

    .text

! void tr_plane_sh(const TRPlane *plane, TRLinear *out, int32_t db, int32_t dc)
! layout and behaviour in rows.h
!
! r4 plane, r5 out, r6 db, r7 dc. The numerators of the two gradients are
! r1:r0 (per column) and r3:r2 (per row).

! r0 = the low word of (hi:r0) >> shift for shift below 32, or zero if that
! loses significant bits. r7 = -shift, r6 = 32 - shift.
#define GRADIENT(hi, lo) \
    mov     hi, r8 ; \
    shad    r7, r8 ; \
    shld    r7, lo ; \
    shld    r6, hi ; \
    or      hi, lo ; \
    mov     lo, hi ; \
    shll    hi ; \
    subc    hi, hi ; \
    cmp/eq  hi, r8 ; \
    bt      1f ; \
    mov     #0, lo ; \
1:

    .align 5
    .global _tr_plane_sh
    .type _tr_plane_sh, @function
_tr_plane_sh:
    mov.l   r8, @-r15
    mov.l   @r4+, r0            ! xb
    dmuls.l r6, r0
    mov.l   @r4+, r2            ! xc
    sts     macl, r0
    sts     mach, r1
    dmuls.l r7, r2
    mov.l   @r4+, r8            ! yc
    sts     macl, r2
    sts     mach, r3
    clrt
    subc    r2, r0
    subc    r3, r1
    dmuls.l r7, r8
    mov.l   @r4+, r7            ! yb
    sts     macl, r2
    sts     mach, r3
    dmuls.l r6, r7
    mov.l   @r4+, r6            ! shift
    sts     macl, r7
    sts     mach, r8
    clrt
    subc    r7, r2
    subc    r8, r3
    neg     r6, r7
    mov     #31, r8
    cmp/gt  r8, r6
    mov     r7, r6
    bt/s    .Lwide
    add     #32, r6             ! 32 - shift

    GRADIENT(r1, r0)
    GRADIENT(r3, r2)
    bra     .Lanchor
    nop

.Lwide:                         ! shift is 32 or more: only the high words count
    shad    r6, r1
    mov     r1, r0
    shad    r6, r3
    mov     r3, r2

.Lanchor:
    mov.l   r0, @(4, r5)
    mov.l   r2, @(8, r5)
    mov     r5, r1
    clrmac
    add     #4, r1
    mac.l   @r4+, @r1+          ! per_x * fraction_x
    mac.l   @r4+, @r1+          ! per_y * fraction_y
    mov.l   @r4+, r3            ! anchor_x
    mov     #-4, r8
    sts     macl, r6
    sts     mach, r7
    shld    r8, r6
    mov     #28, r8
    shld    r8, r7
    or      r7, r6              ! the sum >> 4
    mul.l   r3, r0
    mov.l   @r5, r1
    add     r1, r6
    mov.l   @r4, r3             ! rows
    sts     macl, r1
    sub     r1, r6
    mul.l   r3, r2
    mov.l   @r15+, r8
    sts     macl, r1
    add     r1, r6
    rts
    mov.l   r6, @r5
    .size _tr_plane_sh, . - _tr_plane_sh


! value = (value * r13) >> shift, taking the low 32 bits of the 64-bit result.
! left is 32 - shift, right is -shift.
#define TIMES_DEPTH(value, left, right) \
    dmuls.l r13, value ; \
    sts     mach, value ; \
    sts     macl, r12 ; \
    shld    left, value ; \
    shld    right, r12 ; \
    or      r12, value

! void tr_rows_sh(TRRows *rows)  -- layout and behaviour in rows.h
!
! r14 holds the state. Within a span:
!   r1 pixel   r2 count   r3 z    r4 dz    r5 u     r6 du
!   r7 v       r8 dv      r9 texels (or colour)     r10 v mask  r11 u mask
!   r12 depth  r13 scratch r0 scratch and offset
! r4 and r9 stay loaded for all rows, as do r6, r8, r10 and r11 in the loop
! for walls. There are three row loops: flat, walls and level surfaces.

#define LINE 0
#define ROWS 4
#define LEFT_X 8
#define LEFT_STEP 12
#define RIGHT_X 16
#define RIGHT_STEP 20
#define Z 24
#define ZX 28
#define ZY 32
#define U 36
#define UX 40
#define UY 44
#define V 48
#define VX 52
#define VY 56
#define FLOOR 60
! Beyond the reach of a displacement; addressed through r0.
#define TEXELS 64
#define V_MASK 68
#define U_MASK 72
#define COLOR 76
#define V_SHIFT 80

#define WIDTH 396

#define LOAD(offset, reg) \
    mov     offset, r0 ; \
    mov.l   @(r0, r14), reg

! r0 = max(left >> 16, 0), r2 = min(right >> 16, WIDTH) - r0. Leaves the row
! if that is empty; otherwise r1 = first pixel and r3 = its depth.
#define SPAN(empty, width) \
    mov.l   @(LEFT_X, r14), r0 ; \
    mov.l   @(RIGHT_X, r14), r2 ; \
    mov.w   width, r3 ; \
    swap.w  r0, r0 ; \
    swap.w  r2, r2 ; \
    exts.w  r0, r0 ; \
    exts.w  r2, r2 ; \
    cmp/pz  r0 ; \
    bt      1f ; \
    mov     #0, r0 ; \
1:  cmp/gt  r3, r2 ; \
    bf      2f ; \
    mov     r3, r2 ; \
2:  sub     r0, r2 ; \
    cmp/pl  r2 ; \
    bf      empty ; \
    mov.l   @(LINE, r14), r1 ; \
    mov     r0, r3 ; \
    shll2   r3 ; \
    mul.l   r4, r0 ; \
    add     r3, r1 ; \
    mov.l   @(Z, r14), r3 ; \
    sts     macl, r12 ; \
    add     r12, r3

! r5 = u and r7 = v at column r0, given their steps in r6 and r8.
#define TEXTURE_START \
    mul.l   r6, r0 ; \
    mov.l   @(U, r14), r5 ; \
    sts     macl, r12 ; \
    mul.l   r8, r0 ; \
    mov.l   @(V, r14), r7 ; \
    add     r12, r5 ; \
    sts     macl, r12 ; \
    add     r12, r7

#define TEXTURED_PIXELS \
3:  swap.w  r3, r12 ; \
    mov.w   @r1, r0 ;           /* stored depth */ \
    exts.w  r12, r12 ; \
    cmp/gt  r0, r12 ; \
    bf      4f ; \
    swap.w  r7, r0 ; \
    swap.w  r5, r13 ; \
    and     r10, r0 ; \
    and     r11, r13 ; \
    or      r13, r0 ; \
    mov.w   @(r0, r9), r0 ; \
    mov.w   r12, @r1 ; \
    mov.w   r0, @(2, r1) ; \
4:  add     r4, r3 ; \
    add     r6, r5 ; \
    dt      r2 ; \
    add     r8, r7 ; \
    bf/s    3b ; \
    add     #4, r1

#define ADVANCE(value, step) \
    mov.l   @(value, r14), r0 ; \
    mov.l   @(step, r14), r1 ; \
    add     r1, r0 ; \
    mov.l   r0, @(value, r14)

! Moves edges, depth and line down a row; leaves T set after the last one.
#define NEXT_ROW(stride) \
    ADVANCE(LEFT_X, LEFT_STEP) ; \
    ADVANCE(RIGHT_X, RIGHT_STEP) ; \
    ADVANCE(Z, ZY) ; \
    mov.l   @(LINE, r14), r0 ; \
    mov.w   stride, r1 ; \
    add     r1, r0 ; \
    mov.l   r0, @(LINE, r14) ; \
    mov.l   @(ROWS, r14), r0 ; \
    dt      r0 ; \
    mov.l   r0, @(ROWS, r14)

    .align 5
    .global _tr_rows_sh
    .type _tr_rows_sh, @function
_tr_rows_sh:
    mov.l   r8, @-r15
    mov.l   r9, @-r15
    mov.l   r10, @-r15
    mov.l   r11, @-r15
    mov.l   r12, @-r15
    mov.l   r13, @-r15
    mov.l   r14, @-r15
    mov     r4, r14
    mov.l   @(ZX, r14), r4
    LOAD(#TEXELS, r9)
    tst     r9, r9
    bt      .Lflat
    LOAD(#V_MASK, r10)
    LOAD(#U_MASK, r11)
    mov.l   @(FLOOR, r14), r0
    tst     r0, r0
    bt      .Lwall
    bra     .Lfloor
    nop
.Lwall:
    mov.l   @(UX, r14), r6
    mov.l   @(VX, r14), r8

    .align 2
.Lwall_row:
    SPAN(.Lwall_next, .Lwwidth)
    TEXTURE_START
    TEXTURED_PIXELS
.Lwall_next:
    ADVANCE(U, UY)
    ADVANCE(V, VY)
    NEXT_ROW(.Lwstride)
    bf      .Lwall_row
    bra     .Ldone
    nop
.Lwwidth:   .short WIDTH
.Lwstride:  .short WIDTH * 4

.Lflat:
    LOAD(#COLOR, r9)
    .align 2
.Lflat_row:
    SPAN(.Lflat_next, .Lfwidth)
3:  swap.w  r3, r12
    mov.w   @r1, r0
    exts.w  r12, r12
    cmp/gt  r0, r12
    bf      4f
    mov     r9, r0
    mov.w   r12, @r1
    mov.w   r0, @(2, r1)
4:  dt      r2
    add     r4, r3
    bf/s    3b
    add     #4, r1
.Lflat_next:
    NEXT_ROW(.Lfstride)
    bf      .Lflat_row
    bra     .Ldone
    nop
.Lfwidth:   .short WIDTH
.Lfstride:  .short WIDTH * 4

.Lfloor:
    .align 2
.Lfloor_row:
    SPAN(.Lfloor_next, .Llwidth)
    mov.l   @(UX, r14), r6
    mov.l   @(VX, r14), r8
    TEXTURE_START
    ! Level surface: scale u/depth and v/depth by the depth of this row, then
    ! double them into byte offsets.
    mov.l   @(FLOOR, r14), r13
    mov.l   @r13, r13
    mov     #20, r11
    mov     #-12, r10
    TIMES_DEPTH(r5, r11, r10)
    TIMES_DEPTH(r6, r11, r10)
    LOAD(#V_SHIFT, r10)
    mov     #32, r11
    sub     r10, r11
    neg     r10, r10
    TIMES_DEPTH(r7, r11, r10)
    TIMES_DEPTH(r8, r11, r10)
    add     r5, r5
    add     r6, r6
    add     r7, r7
    add     r8, r8
    LOAD(#V_MASK, r10)
    LOAD(#U_MASK, r11)
    TEXTURED_PIXELS
.Lfloor_next:
    ADVANCE(U, UY)
    ADVANCE(V, VY)
    mov.l   @(FLOOR, r14), r0
    add     #4, r0
    mov.l   r0, @(FLOOR, r14)
    NEXT_ROW(.Llstride)
    bt      .Ldone
    bra     .Lfloor_row
    nop
.Llwidth:   .short WIDTH
.Llstride:  .short WIDTH * 4

.Ldone:
    mov.l   @r15+, r14
    mov.l   @r15+, r13
    mov.l   @r15+, r12
    mov.l   @r15+, r11
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    rts
    mov.l   @r15+, r8
    .size _tr_rows_sh, . - _tr_rows_sh


! void tr_fill_sh(uint32_t *pixels, uint32_t lines, uint32_t value)
! Fills whole 32-byte cache lines. MOVCA.L claims a line without reading it
! from memory; all eight of its words are then written.

    .align 5
    .global _tr_fill_sh
    .type _tr_fill_sh, @function
_tr_fill_sh:
    mov     r6, r0
1:  movca.l r0, @r4
    mov.l   r6, @(4, r4)
    mov.l   r6, @(8, r4)
    mov.l   r6, @(12, r4)
    mov.l   r6, @(16, r4)
    mov.l   r6, @(20, r4)
    mov.l   r6, @(24, r4)
    mov.l   r6, @(28, r4)
    dt      r5
    bf/s    1b
    add     #32, r4
    rts
    nop
    .size _tr_fill_sh, . - _tr_fill_sh


! void tr_present_sh(const uint32_t *pixels, uint32_t *screen, uint32_t groups)
! Packs the colour halves (the low words) of 8 * groups framebuffer words into
! video RAM, two pixels in each 32-bit write.

#define PACK(offset) \
    mov.l   @r4+, r1 ; \
    mov.l   @r4+, r2 ; \
    swap.w  r2, r2 ; \
    xtrct   r1, r2 ; \
    mov.l   r2, @(offset, r5)

    .align 5
    .global _tr_present_sh
    .type _tr_present_sh, @function
_tr_present_sh:
    .align 2
1:  PACK(0)
    PACK(4)
    PACK(8)
    PACK(12)
    dt      r6
    bf/s    1b
    add     #16, r5
    rts
    nop
    .size _tr_present_sh, . - _tr_present_sh
