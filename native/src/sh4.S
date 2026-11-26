! Hand-written SH-4 loops for the fx-CG50, where most of each frame is spent.
! rows.c and present.c hold the C equivalents; tests/cg_check.c compares them.
! Register use follows the GCC ABI: r8-r14 are saved, r0-r7 and MAC are free.

    .text

! void tr_rows_sh(TRRows *rows)  -- layout and behaviour in rows.h
!
! r14 holds the state. Within a span:
!   r1 pixel   r2 count   r3 z    r4 dz    r5 u     r6 du
!   r7 v       r8 dv      r9 texels (or colour)     r10 v mask  r11 u mask
!   r12 depth  r13 scratch r0 scratch and index

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
#define TEXELS 60
! Beyond the reach of a displacement; addressed through r0.
#define V_MASK 64
#define U_MASK 68
#define COLOR 72
#define FLOOR 76
#define V_SHIFT 80

#define WIDTH 396

! value = (value * r13) >> shift, taking the low 32 bits of the 64-bit result.
! left is 32 - shift, right is -shift.
#define TIMES_DEPTH(value, left, right) \
    dmuls.l r13, value ; \
    sts     mach, value ; \
    sts     macl, r12 ; \
    shld    left, value ; \
    shld    right, r12 ; \
    or      r12, value

#define ADVANCE(value, step) \
    mov.l   @(value, r14), r0 ; \
    mov.l   @(step, r14), r1 ; \
    add     r1, r0 ; \
    mov.l   r0, @(value, r14)

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

.Lrow:
    ! x0 = max((left + 0x7fff) >> 16, 0), x1 = min((right + 0x7fff) >> 16, WIDTH)
    mov.w   .Lhalf, r3
    mov.l   @(LEFT_X, r14), r0
    mov.l   @(RIGHT_X, r14), r2
    add     r3, r0
    add     r3, r2
    swap.w  r0, r0
    swap.w  r2, r2
    exts.w  r0, r0
    exts.w  r2, r2
    cmp/pz  r0
    bt      1f
    mov     #0, r0
1:  mov.w   .Lwidth, r3
    cmp/gt  r3, r2
    bf      2f
    mov     r3, r2
2:  sub     r0, r2              ! count
    cmp/pl  r2
    bt      3f
    bra     .Lnext
    nop

3:  mov     r0, r1
    mov.l   @(LINE, r14), r3
    shll2   r1
    mov.l   @(ZX, r14), r4
    add     r3, r1              ! pixel
    mul.l   r4, r0
    mov.l   @(Z, r14), r3
    mov.l   @(TEXELS, r14), r9
    sts     macl, r5
    tst     r9, r9
    bt/s    .Lflat
    add     r5, r3              ! z

    mov.l   @(UX, r14), r6
    mov.l   @(VX, r14), r8
    mul.l   r6, r0
    mov.l   @(U, r14), r5
    sts     macl, r12
    mul.l   r8, r0
    mov.l   @(V, r14), r7
    add     r12, r5             ! u
    sts     macl, r12
    mov     #FLOOR, r0
    mov.l   @(r0, r14), r13
    tst     r13, r13
    bt/s    .Laffine
    add     r12, r7             ! v

    ! Level surface: scale u/depth and v/depth by the depth of this row.
    mov.l   @r13, r13
    mov     #20, r11
    mov     #-12, r10
    TIMES_DEPTH(r5, r11, r10)
    TIMES_DEPTH(r6, r11, r10)
    mov     #V_SHIFT, r0
    mov.l   @(r0, r14), r10
    mov     #32, r11
    sub     r10, r11
    neg     r10, r10
    TIMES_DEPTH(r7, r11, r10)
    TIMES_DEPTH(r8, r11, r10)

.Laffine:
    mov     #V_MASK, r0
    mov.l   @(r0, r14), r10
    mov     #U_MASK, r0
    mov.l   @(r0, r14), r11

    .align 2
.Lpixel:
    swap.w  r3, r12
    mov.w   @r1, r0             ! stored depth
    exts.w  r12, r12
    cmp/gt  r0, r12
    bf      4f
    swap.w  r7, r0
    swap.w  r5, r13
    and     r10, r0
    and     r11, r13
    or      r13, r0
    add     r0, r0
    mov.w   @(r0, r9), r0
    mov.w   r12, @r1
    mov.w   r0, @(2, r1)
4:  add     r4, r3
    add     r6, r5
    dt      r2
    add     r8, r7
    bf/s    .Lpixel
    add     #4, r1
    bra     .Lnext
    nop

.Lflat:
    mov     #COLOR, r0
    mov.l   @(r0, r14), r9
    .align 2
.Lflat_pixel:
    swap.w  r3, r12
    mov.w   @r1, r0
    exts.w  r12, r12
    cmp/gt  r0, r12
    bf      5f
    mov     r9, r0
    mov.w   r12, @r1
    mov.w   r0, @(2, r1)
5:  dt      r2
    add     r4, r3
    bf/s    .Lflat_pixel
    add     #4, r1

.Lnext:
    ADVANCE(LEFT_X, LEFT_STEP)
    ADVANCE(RIGHT_X, RIGHT_STEP)
    ADVANCE(Z, ZY)
    ADVANCE(U, UY)
    ADVANCE(V, VY)
    mov.l   @(LINE, r14), r0
    mov.w   .Lstride, r1
    add     r1, r0
    mov.l   r0, @(LINE, r14)
    mov     #FLOOR, r0
    mov.l   @(r0, r14), r1
    tst     r1, r1
    bt      6f
    add     #4, r1
    mov.l   r1, @(r0, r14)
6:  mov.l   @(ROWS, r14), r1
    dt      r1
    mov.l   r1, @(ROWS, r14)
    bt      7f
    bra     .Lrow
    nop

7:  mov.l   @r15+, r14
    mov.l   @r15+, r13
    mov.l   @r15+, r12
    mov.l   @r15+, r11
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    rts
    mov.l   @r15+, r8

.Lhalf:     .short 0x7fff
.Lwidth:    .short WIDTH
.Lstride:   .short WIDTH * 4
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
