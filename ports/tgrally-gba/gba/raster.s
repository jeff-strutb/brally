@ raster.s -- the textured triangle, main.c's raster_tex (built from C with
@ RASTER_C) to the bit: the same values, computed the same way.  In IWRAM, ARM.
@
@   void raster_tex(const Poly *p)
@   Poly: (x y u v)[3] (words), col next tex pad (halves)
@   Tex: data (word), wm2 hm7 (halves), wbits hbits alpha pad (bytes); its rows 64 texels apart
        .section .ovl_draw, "ax"
        .arm
        .global raster_tex

        .equ    SW, 160
        .equ    SH, 128
        .equ    NBUCKET, 256
        .equ    F_X, 0                  @ x0 x1 x2, sorted by y
        .equ    F_Y, 12                 @ y0 y1 y2
        .equ    F_U0, 24
        .equ    F_V0, 28
        .equ    F_PAR, 32               @ dudx dvdx, the texture, wmask << 1, hmask << 7
        .equ    F_DUDY, 60
        .equ    F_DVDY, 64
        .equ    F_SS, 68                @ the short edge's slope
        .equ    F_S02, 72               @ the long edge's
        .equ    F_ROWEND, 76
        .equ    F_CU, 80
        .equ    F_CV, 84
        .equ    F_HALF, 88              @ the half's edge: F_X + 0 or + 4
        .equ    F_ALPHA, 92
        .equ    F_RDUV, 96              @ a row's steps: uv, xs, xo (the row loop's ldm)
        .equ    F_RSS, 100
        .equ    F_RS02, 104
        .equ    F_UV0, 108              @ u, v packed at pixel (0, 0)
        .equ    F_ODD, 112              @ ROWS2: the row's odd last pixel
        .equ    FRAME, 116

@ lr = ((a b - c d) >> r4, its low word) times r7, >> 8 (r10 = 32 - r4)
        .macro  GRAD a, b, c, d
        smull   r0, r1, \a, \b
        rsb     lr, \c, #0
        smlal   r0, r1, lr, \d
        mov     r0, r0, lsr r4
        orr     r0, r0, r1, lsl r10
        smull   lr, r1, r0, r7
        mov     lr, lr, lsr #8
        orr     lr, lr, r1, lsl #24
        .endm

@ d = 1 / w (w >= 1, clobbered) as main.c's recip: w halved until under 4096
        .macro  RECIP d, w, t, rec
        mov     \t, #0
1:      cmp     \w, #4096
        movge   \w, \w, asr #1
        addge   \t, \t, #1
        bge     1b
        ldr     \d, [\rec, \w, lsl #2]
        mov     \d, \d, lsr \t
        .endm

@ COUNT builds: g_fcount[k] += reg (main.c), the flags and registers kept
        .macro  CNTADD k, reg                   @ (reg not r2 or r3)
        .ifdef  COUNT
        stmfd   sp!, {r2, r3}
        mrs     r2, cpsr
        stmfd   sp!, {r2}
        ldr     r2, =g_fcount
        ldr     r3, [r2, #\k * 4]
        add     r3, r3, \reg
        str     r3, [r2, #\k * 4]
        ldmfd   sp!, {r2}
        msr     cpsr_f, r2
        ldmfd   sp!, {r2, r3}
        .endif
        .endm

@ swap two registers when the flags say gt
        .macro  SWAPGT a, b
        movgt   r0, \a
        movgt   \a, \b
        movgt   \b, r0
        .endm

@ u and v packed in one word, each 6.10 (u the high half): the texel is
@ (uv >> 26) & wmask across, (uv >> 10) & hmask down, rows 64 apart
@ r12 = the texel at r2 (uv), which steps by r4; r6 the texture, r7 wmask << 1, r8 hmask << 7
        .macro  TEXEL
        and     r12, r7, r2, lsr #25
        and     lr, r8, r2, lsr #3
        orr     r12, r12, lr
        ldrh    r12, [r6, r12]
        add     r2, r2, r4
        .endm

@ the texel to the pixel at r0, which steps on (alpha 1: a clear one left undrawn; 2: half
@ over the pixel, the texel already halved by the converter)
        .macro  PUT alpha
        .if     \alpha == 2
        tst     r12, #0x8000
        ldrheq  lr, [r0]
        moveq   lr, lr, lsr #1
        biceq   lr, lr, #0x10
        biceq   lr, lr, #0x200
        addeq   r12, r12, lr
        strheq  r12, [r0]
        add     r0, r0, #2
        nop
        nop
        nop
        .elseif \alpha
        tst     r12, #0x8000
        strheq  r12, [r0]
        add     r0, r0, #2
        .else
        strh    r12, [r0], #2
        .endif
        .endm

@ the texel at r2 to two pixels at r0 (word aligned) in one store, r2 stepped twice (alpha 1: a
@ clear texel leaves both undrawn); 8 instructions (alpha 1: 9)
        .macro  PAIR alpha
        and     r12, r7, r2, lsr #25
        and     lr, r8, r2, lsr #3
        orr     r12, r12, lr
        ldrh    r12, [r6, r12]
        add     r2, r2, r4, lsl #1
        .if     \alpha
        tst     r12, #0x8000
        orreq   r12, r12, r12, lsl #16
        streq   r12, [r0]
        add     r0, r0, #4
        .else
        orr     r12, r12, r12, lsl #16
        str     r12, [r0], #4
        nop
        .endif
        .endm

@ the half's rows as ROWS does them, a texel to each pair of pixels (half the fetches): an odd
@ first pixel alone, then pairs in words, an odd last pixel alone (its count kept in r0's bit 0)
        .macro  ROWS2 alpha
1:      .ifdef  COUNT
        stmfd   sp!, {r0, r1}
        ldr     r0, =g_fcount
        ldr     r1, [r0, #24]
        add     r1, r1, #1
        str     r1, [r0, #24]
        ldmfd   sp!, {r0, r1}
        .endif
        cmp     r9, r10
        movlt   r0, r9, asr #16
        movlt   r1, r10, asr #16
        movge   r0, r10, asr #16
        movge   r1, r9, asr #16
        cmp     r0, #0
        movlt   r0, #0
        cmp     r1, #SW
        movgt   r1, #SW
        subs    r1, r1, r0
        ble     3f
        CNTADD  4, r1
        mla     r2, r4, r0, r3
        add     r0, r11, r0, lsl #1
        tst     r0, #2                  @ an odd first pixel: alone
        beq     4f
        TEXEL
        PUT     \alpha
        subs    r1, r1, #1
        ble     3f
4:      and     r12, r1, #1             @ an odd last pixel: noted
        str     r12, [sp, #F_ODD]
        movs    r1, r1, lsr #1          @ the pairs
        beq     6f
        and     r12, r1, #3             @ four pairs a pass, entered part way in for the rest
        rsb     r12, r12, #4
        and     r12, r12, #3
        add     r1, r1, #3
        mov     r1, r1, lsr #2
        .if     \alpha
        add     r12, r12, r12, lsl #3
        add     pc, pc, r12, lsl #2     @ (9 instructions a pair)
        .else
        add     pc, pc, r12, lsl #5     @ (8 instructions a pair)
        .endif
        nop
5:      .rept   4
        PAIR    \alpha
        .endr
        subs    r1, r1, #1
        bne     5b
6:      ldr     r12, [sp, #F_ODD]
        cmp     r12, #0
        beq     3f
        TEXEL
        PUT     \alpha
3:      add     r12, sp, #F_RDUV
        ldmia   r12, {r0, r1, r12}      @ the uv, xs, xo steps
        add     r3, r3, r0
        add     r9, r9, r1
        add     r10, r10, r12
        add     r11, r11, #SW * 2
        cmp     r11, r5
        bne     1b
        .endm

@ the half's rows: r9 xs, r10 xo (Q16 pixels, biased), r11 the row, r3 uv at its pixel 0,
@ r5 the rows' end; the steps in the frame (F_RDUV..)
        .macro  ROWS alpha
1:      cmp     r9, r10
        movlt   r0, r9, asr #16
        movlt   r1, r10, asr #16
        movge   r0, r10, asr #16
        movge   r1, r9, asr #16
        cmp     r0, #0
        movlt   r0, #0
        cmp     r1, #SW
        movgt   r1, #SW
        subs    r1, r1, r0
        ble     3f
        mla     r2, r4, r0, r3
        add     r0, r11, r0, lsl #1
        and     r12, r1, #7             @ eight texels a pass, entered part way in for the rest
        rsb     r12, r12, #8
        and     r12, r12, #7
        add     r1, r1, #7
        mov     r1, r1, lsr #3
        .if     \alpha == 2
        add     pc, pc, r12, lsl #6     @ (16 instructions a texel)
        .elseif \alpha
        add     pc, pc, r12, lsl #5     @ (8 instructions a texel)
        .else
        add     r12, r12, r12, lsl #1
        add     pc, pc, r12, lsl #3     @ (6 instructions a texel)
        .endif
        nop
2:      .rept   8
        TEXEL
        PUT     \alpha
        .endr
        subs    r1, r1, #1
        bne     2b
3:      add     r12, sp, #F_RDUV
        ldmia   r12, {r0, r1, r12}      @ the uv, xs, xo steps
        add     r3, r3, r0
        add     r9, r9, r1
        add     r10, r10, r12
        add     r11, r11, #SW * 2
        cmp     r11, r5
        bne     1b
        .endm

raster_tex:
        .ifdef  COUNT
        stmfd   sp!, {r0, r1}
        ldr     r0, =g_fcount
        ldr     r1, [r0, #20]
        add     r1, r1, #1
        str     r1, [r0, #20]
        ldmfd   sp!, {r0, r1}
        .endif
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #FRAME
        ldrh    lr, [r0, #52]           @ the texture
        ldmia   r0!, {r1, r4, r7, r10}   @ x0 y0 u0 v0
        ldmia   r0!, {r2, r5, r8, r11}   @ x1 y1 u1 v1
        ldmia   r0, {r3, r6, r9, r12}    @ x2 y2 u2 v2
        cmp     r4, r5                  @ sorted by y: 0-1, 1-2, 0-1
        SWAPGT  r1, r2
        SWAPGT  r4, r5
        SWAPGT  r7, r8
        SWAPGT  r10, r11
        cmp     r5, r6
        SWAPGT  r2, r3
        SWAPGT  r5, r6
        SWAPGT  r8, r9
        SWAPGT  r11, r12
        cmp     r4, r5
        SWAPGT  r1, r2
        SWAPGT  r4, r5
        SWAPGT  r7, r8
        SWAPGT  r10, r11
        subs    r0, r6, r4
        ble     .Lret                   @ under a sixteenth of a row high
        stmia   sp, {r1-r6}
        str     r7, [sp, #F_U0]
        str     r10, [sp, #F_V0]
        sub     r2, r2, r1              @ dx1
        sub     r3, r3, r1              @ dx2
        sub     r5, r5, r4              @ dy1
        sub     r6, r6, r4              @ dy2
        sub     r8, r8, r7              @ du1
        sub     r9, r9, r7              @ du2
        sub     r11, r11, r10           @ dv1
        sub     r12, r12, r10           @ dv2
        ldr     r0, =s_textab           @ the span's texture (12 bytes a Tex) in the screen's table
        ldr     r0, [r0]
        add     lr, lr, lr, lsl #1
        add     r0, r0, lr, lsl #2
        ldmia   r0, {r1, r4, r7}        @ data; wmask << 1 | hmask << 7 << 16; wbits, hbits, alpha
        str     r1, [sp, #F_PAR + 8]
        mov     r1, r4, lsl #16
        mov     r1, r1, lsr #16
        str     r1, [sp, #F_PAR + 12]
        mov     r4, r4, lsr #16
        str     r4, [sp, #F_PAR + 16]
        mov     r7, r7, lsr #16
        and     r7, r7, #0xFF
        str     r7, [sp, #F_ALPHA]
        smull   r0, r1, r2, r6          @ den = dx1 dy2 - dx2 dy1
        rsb     lr, r3, #0
        smlal   r0, r1, lr, r5
        orrs    lr, r0, r1
        beq     .Lret
        mov     r10, r1
        cmp     r1, #0
        bge     1f
        rsbs    r0, r0, #0
        rsc     r1, r1, #0
1:      cmp     r1, #0                  @ sh: |den|'s bit length - 12, at least 0
        movne   r4, #32
        movne   lr, r1
        moveq   r4, #0
        moveq   lr, r0
        cmp     lr, #0x10000
        addhs   r4, r4, #16
        movhs   lr, lr, lsr #16
        cmp     lr, #0x100
        addhs   r4, r4, #8
        movhs   lr, lr, lsr #8
        cmp     lr, #0x10
        addhs   r4, r4, #4
        movhs   lr, lr, lsr #4
        cmp     lr, #4
        addhs   r4, r4, #2
        movhs   lr, lr, lsr #2
        cmp     lr, #2
        addhs   r4, r4, #1
        movhs   lr, lr, lsr #1
        add     r4, r4, lr
        subs    r4, r4, #12
        movlt   r4, #0
        rsb     r7, r4, #32
        mov     r0, r0, lsr r4
        orr     r0, r0, r1, lsl r7
        ldr     r1, =s_rec
        ldr     r7, [r1, r0, lsl #2]
        cmp     r10, #0
        rsblt   r7, r7, #0              @ 1 / den, signed
        rsb     r10, r4, #32
        GRAD    r8, r6, r9, r5          @ dudx = (du1 dy2 - du2 dy1) / den
        str     lr, [sp, #F_PAR]
        GRAD    r11, r6, r12, r5
        str     lr, [sp, #F_PAR + 4]
        GRAD    r9, r2, r8, r3          @ dudy = (du2 dx1 - du1 dx2) / den
        str     lr, [sp, #F_DUDY]
        GRAD    r12, r2, r11, r3
        str     lr, [sp, #F_DVDY]
        ldr     r12, =s_rec             @ the long edge's slope
        RECIP   r0, r6, r1, r12
        smull   r1, r4, r3, r0
        mov     r1, r1, lsr #8
        orr     r1, r1, r4, lsl #24
        str     r1, [sp, #F_S02]
        ldr     r0, [sp, #F_X]          @ u, v at pixel (0, 0)
        ldr     r1, [sp, #F_Y]
        rsb     r0, r0, #8
        rsb     r1, r1, #8
        ldr     r4, [sp, #F_PAR]
        ldr     r5, [sp, #F_DUDY]
        smull   r7, r8, r4, r0
        smlal   r7, r8, r5, r1
        mov     r7, r7, lsr #4
        orr     r7, r7, r8, lsl #28
        ldr     r4, [sp, #F_U0]
        add     r7, r7, r4, lsl #12
        str     r7, [sp, #F_CU]
        ldr     r4, [sp, #F_PAR + 4]
        ldr     r5, [sp, #F_DVDY]
        smull   r7, r8, r4, r0
        smlal   r7, r8, r5, r1
        mov     r7, r7, lsr #4
        orr     r7, r7, r8, lsl #28
        ldr     r4, [sp, #F_V0]
        add     r7, r7, r4, lsl #12
        str     r7, [sp, #F_CV]
        ldr     r0, [sp, #F_PAR]        @ u, v packed: the steps and the start
        ldr     r1, [sp, #F_PAR + 4]
        mov     r0, r0, asr #6
        mov     r1, r1, asr #6
        add     r4, r1, r0, lsl #16     @ r4: across, kept
        ldr     r0, [sp, #F_DUDY]
        ldr     r1, [sp, #F_DVDY]
        mov     r0, r0, asr #6
        mov     r1, r1, asr #6
        add     r0, r1, r0, lsl #16
        str     r0, [sp, #F_RDUV]
        ldr     r0, [sp, #F_CU]
        ldr     r1, [sp, #F_CV]
        mov     r0, r0, asr #6
        mov     r1, r1, asr #6
        add     r0, r1, r0, lsl #16
        str     r0, [sp, #F_UV0]
        ldr     r0, [sp, #F_S02]
        str     r0, [sp, #F_RS02]
        add     r0, sp, #F_PAR + 8
        ldmia   r0, {r6-r8}             @ kept: the texture, wmask << 1, hmask << 7
        str     sp, [sp, #F_HALF]
.Lhalf:
        ldr     r0, [sp, #F_HALF]
        ldr     r1, [r0, #F_Y]          @ ya
        ldr     r2, [r0, #F_Y + 4]      @ yb
        subs    r3, r2, r1
        ble     .Lnext_half
        ldr     r12, =s_rec
        RECIP   r9, r3, lr, r12
        ldr     r10, [r0]               @ xa
        ldr     r3, [r0, #4]            @ xb
        sub     r3, r3, r10
        smull   r11, lr, r3, r9
        mov     r11, r11, lsr #8
        orr     r11, r11, lr, lsl #24   @ the short edge's slope
        str     r11, [sp, #F_RSS]
        add     r12, r1, #7
        mov     r12, r12, asr #4        @ the rows whose centre is in [ya, yb): yy..
        add     r2, r2, #7
        mov     r2, r2, asr #4          @ ..ye
        cmp     r12, #0
        movlt   r12, #0
        cmp     r2, #SH
        movgt   r2, #SH
        cmp     r12, r2
        bge     .Lnext_half
        mov     r3, r12, lsl #4
        add     r3, r3, #8              @ the first row's centre
        sub     r0, r3, r1
        smull   r9, lr, r11, r0
        mov     r9, r9, lsr #4
        orr     r9, r9, lr, lsl #28
        add     r9, r9, r10, lsl #12    @ xs
        ldr     r1, [sp, #F_Y]
        sub     r0, r3, r1
        ldr     r11, [sp, #F_S02]
        smull   r10, lr, r11, r0
        mov     r10, r10, lsr #4
        orr     r10, r10, lr, lsl #28
        ldr     r1, [sp, #F_X]
        add     r10, r10, r1, lsl #12   @ xo
        add     r9, r9, #0x8000         @ both less a half plus 0xFFFF: a column is x >> 16
        sub     r9, r9, #1
        add     r10, r10, #0x8000
        sub     r10, r10, #1
        ldr     r11, =s_back
        ldr     r11, [r11]
        add     r5, r11, r2, lsl #8
        add     r5, r5, r2, lsl #6      @ r5: + 320 ye, the rows' end
        add     r11, r11, r12, lsl #8
        add     r11, r11, r12, lsl #6   @ the row: + 320 yy
        ldr     r0, [sp, #F_RDUV]
        ldr     r1, [sp, #F_UV0]
        mla     r3, r0, r12, r1         @ uv at the row's pixel 0
        ldr     r0, [sp, #F_ALPHA]
        cmp     r0, #0
        bne     .Lalpha
        ROWS2   0
        b       .Lnext_half
.Lalpha:
        cmp     r0, #2
        beq     .Lblend
        ROWS2   1
        b       .Lnext_half
.Lblend:
        ROWS    2
.Lnext_half:
        ldr     r0, [sp, #F_HALF]
        add     r0, r0, #4
        str     r0, [sp, #F_HALF]
        sub     r0, r0, sp
        cmp     r0, #8
        blt     .Lhalf
.Lret:
        add     sp, sp, #FRAME
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@   void raster(const Poly *p)
@ a flat triangle (main.c's raster with RASTER_C): its colour across the
@ pixels whose centres it covers.  The same frame layout as raster_tex.
        .global raster
raster:
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #FRAME
        ldrh    r4, [r0, #48]           @ the colour, twice over
        orr     r4, r4, r4, lsl #16
        mov     r5, r4
        mov     r6, r4
        mov     r7, r4
        ldr     r1, [r0]                @ x0 y0, x1 y1, x2 y2
        ldr     r8, [r0, #4]
        ldr     r2, [r0, #16]
        ldr     r9, [r0, #20]
        ldr     r3, [r0, #32]
        ldr     r10, [r0, #36]
        cmp     r8, r9                  @ sorted by y: 0-1, 0-2, 1-2
        SWAPGT  r1, r2
        SWAPGT  r8, r9
        cmp     r8, r10
        SWAPGT  r1, r3
        SWAPGT  r8, r10
        cmp     r9, r10
        SWAPGT  r2, r3
        SWAPGT  r9, r10
        subs    r0, r10, r8
        ble     .Lf_ret
        stmia   sp, {r1-r3}
        add     r0, sp, #F_Y
        stmia   r0, {r8-r10}
        ldr     r12, =s_rec             @ the long edge's slope
        sub     r10, r10, r8
        RECIP   r0, r10, r11, r12
        sub     r3, r3, r1
        smull   r1, r11, r3, r0
        mov     r1, r1, lsr #8
        orr     r1, r1, r11, lsl #24
        str     r1, [sp, #F_S02]
        str     sp, [sp, #F_HALF]
.Lf_half:
        ldr     r0, [sp, #F_HALF]
        ldr     r1, [r0, #F_Y]
        ldr     r2, [r0, #F_Y + 4]
        subs    r3, r2, r1
        ble     .Lf_next_half
        ldr     r12, =s_rec
        RECIP   r9, r3, lr, r12
        ldr     r10, [r0]
        ldr     r3, [r0, #4]
        sub     r3, r3, r10
        smull   r11, lr, r3, r9
        mov     r11, r11, lsr #8
        orr     r11, r11, lr, lsl #24
        str     r11, [sp, #F_SS]
        add     r12, r1, #7
        mov     r12, r12, asr #4
        add     r2, r2, #7
        mov     r2, r2, asr #4
        cmp     r12, #0
        movlt   r12, #0
        cmp     r2, #SH
        movgt   r2, #SH
        cmp     r12, r2
        bge     .Lf_next_half
        mov     r3, r12, lsl #4
        add     r3, r3, #8
        sub     r0, r3, r1
        smull   r9, lr, r11, r0
        mov     r9, r9, lsr #4
        orr     r9, r9, lr, lsl #28
        add     r9, r9, r10, lsl #12    @ xs
        ldr     r1, [sp, #F_Y]
        sub     r0, r3, r1
        ldr     r11, [sp, #F_S02]
        smull   r10, lr, r11, r0
        mov     r10, r10, lsr #4
        orr     r10, r10, lr, lsl #28
        ldr     r1, [sp, #F_X]
        add     r10, r10, r1, lsl #12   @ xo
        add     r9, r9, #0x8000
        sub     r9, r9, #1
        add     r10, r10, #0x8000
        sub     r10, r10, #1
        ldr     r11, =s_back
        ldr     r11, [r11]
        add     r8, r11, r2, lsl #8
        add     r8, r8, r2, lsl #6      @ r8: the rows' end
        add     r11, r11, r12, lsl #8
        add     r11, r11, r12, lsl #6
        ldr     r12, [sp, #F_SS]
        ldr     lr, [sp, #F_S02]
@ r9 xs, r10 xo, r11 the row, r12 ss, lr s02
1:      cmp     r9, r10
        movlt   r0, r9, asr #16
        movlt   r1, r10, asr #16
        movge   r0, r10, asr #16
        movge   r1, r9, asr #16
        cmp     r0, #0
        movlt   r0, #0
        cmp     r1, #SW
        movgt   r1, #SW
        subs    r1, r1, r0
        ble     6f
        add     r0, r11, r0, lsl #1
        tst     r0, #2
        beq     2f
        strh    r4, [r0], #2
        subs    r1, r1, #1
        beq     6f
2:      subs    r1, r1, #8
        blt     4f
3:      stmia   r0!, {r4-r7}
        subs    r1, r1, #8
        bge     3b
4:      add     r1, r1, #8
        movs    r2, r1, lsr #1
        beq     5f
41:     str     r4, [r0], #4
        subs    r2, r2, #1
        bne     41b
5:      tst     r1, #1
        strhne  r4, [r0]
6:      add     r9, r9, r12
        add     r10, r10, lr
        add     r11, r11, #SW * 2
        cmp     r11, r8
        bne     1b
.Lf_next_half:
        ldr     r0, [sp, #F_HALF]
        add     r0, r0, #4
        str     r0, [sp, #F_HALF]
        sub     r0, r0, sp
        cmp     r0, #8
        blt     .Lf_half
.Lf_ret:
        add     sp, sp, #FRAME
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@   int draw(void)
@ the buckets, furthest first, each its list: the triangles drawn
        .global draw
draw:
        stmfd   sp!, {r4-r6, lr}
        mov     r6, #0
        ldr     r4, =s_bucket + (NBUCKET - 1) * 2
        ldr     r5, =s_bucket
1:      ldrh    r0, [r4], #-2
2:      cmp     r0, #0
        beq     4f
        sub     r0, r0, #1              @ the poly: s_poly + 56 (k - 1)
        rsb     r0, r0, r0, lsl #3
        ldr     r1, =s_poly
        add     r0, r1, r0, lsl #3
        stmfd   sp!, {r0}
        ldrh    r1, [r0, #52]
        mvn     r2, #0
        cmp     r1, r2, lsr #16
        bne     3f
        bl      raster
        b       31f
32:     ldmia   r0, {r1, r2}            @ a dot (front.s): its colour at x, y
        ldr     r3, =s_back
        ldr     r3, [r3]
        add     r3, r3, r1, lsl #1
        add     r2, r2, r2, lsl #2      @ + 320 y
        add     r3, r3, r2, lsl #6
        ldrh    r1, [r0, #48]
        strh    r1, [r3]
        b       31f
3:      sub     r2, r2, #1 << 16        @ (0xFFFE)
        cmp     r1, r2, lsr #16
        beq     32b
        bl      raster_tex
31:     ldmfd   sp!, {r0}
        add     r6, r6, #1
        ldrh    r0, [r0, #50]           @ next
        b       2b
4:      cmp     r4, r5
        bhs     1b
        mov     r0, r6
        ldmfd   sp!, {r4-r6, lr}
        bx      lr
        .ltorg
