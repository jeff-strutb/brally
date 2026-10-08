@ front.s -- the front end, main.c's (built from C with FRONT_C) to the bit:
@ the camera cell's visibility list walked, each triangle tested against the
@ distances the game drew it at, its vertices through the camera (once a cell),
@ culled, clipped at the near plane, halved in clip space where deep, and
@ put in the depth buckets.  In IWRAM, ARM.
@
@   void render_visible(const Frame *f, int *ncells, int *nverts)
@
@ PV: X Y W sx sy u v (words).  Tri: a b c col (halves), cx cy (halves),
@ dlo dhi (bytes), tex (half), uv[6] (halves).  Cell: vstart (word), nv
@ (half), dlo dhi (bytes), tstart, nt (words).  Poly: (x y u v)[3] (words),
@ col next tex pad (halves).
        .section .iwram_bss, "aw", %nobits
        .align  2
        .global s_vis_list
s_vis_list: .space 4                    @ render_visible's list, when the frame makes its own

        .section .ovl_draw, "ax"
        .arm
        .global render_visible, project, emit, persp, cut, tri

        .equ    SW, 160
        .equ    SH, 128
        .equ    NEAR, 128               @ the near plane, W in 1/256 units
        .equ    GMAX, 3                 @ an edge's halvings at most
        .ifndef DEEP
        .equ    DEEP, 0                 @ halved when the far end is more than 1 + 2^DEEP times as deep
        .endif
        .ifndef MINEDGE
        .equ    MINEDGE, 256            @ and the edge longer than this (sixteenths of a pixel)
        .endif
        .equ    MAXPOLY, 2400
        .equ    NBUCKET, 256
        .ifndef DOTA
        .equ    DOTA, 2048              @ a triangle under 4 square pixels is a dot
        .endif

@ COUNT builds: g_fcount[k] += 1 (main.c), the flags and registers kept
        .macro  CNT k
        .ifdef  COUNT
        stmfd   sp!, {r0, r1}
        ldr     r0, =g_fcount
        ldr     r1, [r0, #\k * 4]
        add     r1, r1, #1
        str     r1, [r0, #\k * 4]
        ldmfd   sp!, {r0, r1}
        .endif
        .endm

@ project: r0 = PV; sx, sy from X, Y, W.  Clobbers r1-r3, r12.
project:
        ldr     r1, [r0, #8]
        movs    r12, r1, lsr #20        @ W under 2^20: the shift that brings it under 4096 from a table
        ldreq   r3, =s_rsh
        ldrbeq  r12, [r3, r1, lsr #12]
        moveq   r1, r1, lsr r12
        beq     2f
        mov     r12, #0                 @ (further: halved until under)
1:      cmp     r1, #4096
        movge   r1, r1, asr #1
        addge   r12, r12, #1
        bge     1b
2:      ldr     r3, =s_rec
        ldr     r3, [r3, r1, lsl #2]
        mov     r3, r3, lsr r12         @ 1 / W
        ldr     r1, [r0]
        smull   r12, r2, r1, r3
        mov     r12, r12, lsr #20
        orr     r12, r12, r2, lsl #12
        add     r12, r12, #SW / 2 * 16
        mov     r2, #0x40000            @ kept within 2^18 sixteenths
        cmp     r12, r2
        movgt   r12, r2
        cmn     r12, r2
        rsblt   r12, r2, #0
        str     r12, [r0, #12]
        ldr     r1, [r0, #4]
        smull   r12, r2, r1, r3
        mov     r12, r12, lsr #20
        orr     r12, r12, r2, lsl #12
        add     r12, r12, #SH / 2 * 16
        mov     r2, #0x40000
        cmp     r12, r2
        movgt   r12, r2
        cmn     r12, r2
        rsblt   r12, r2, #0
        str     r12, [r0, #16]
        bx      lr

@ emit: r0 a, r1 b, r2 c (PV); into the buckets if it faces us and is on the
@ screen.  Colour and texture from s_ct.  AAPCS.
emit:
        CNT     3
        stmfd   sp!, {r4-r11, lr}
        ldr     r3, [r0, #12]           @ a
        ldr     r4, [r0, #16]
        ldr     r5, [r1, #12]           @ b
        ldr     r6, [r1, #16]
        ldr     r7, [r2, #12]           @ c
        ldr     r8, [r2, #16]
        sub     r9, r5, r3
        sub     r10, r8, r4
        smull   r11, r12, r9, r10       @ (b - a) x (c - a)
        sub     r9, r4, r6              @ -(by - ay)
        sub     r10, r7, r3
        smlal   r11, r12, r9, r10
        cmp     r12, #0
        bge     9f                      @ facing away, or edge on
        cmp     r3, r5                  @ x: r9 min, r10 max
        movlt   r9, r3
        movlt   r10, r5
        movge   r9, r5
        movge   r10, r3
        cmp     r7, r9
        movlt   r9, r7
        cmp     r7, r10
        movgt   r10, r7
        cmp     r10, #0
        blt     9f
        cmp     r9, #SW * 16
        bge     9f
        cmp     r4, r6                  @ y
        movlt   r9, r4
        movlt   r10, r6
        movge   r9, r6
        movge   r10, r4
        cmp     r8, r9
        movlt   r9, r8
        cmp     r8, r10
        movgt   r10, r8
        cmp     r10, #0
        blt     9f
        cmp     r9, #SH * 16
        bge     9f
        add     r9, r9, #7              @ a row centre within it?
        add     r10, r10, #7
        mov     r9, r9, asr #4
        cmp     r9, r10, asr #4
        bge     9f
        cmp     r3, r5                  @ a column centre (a sixteenth's grace)?
        movlt   r9, r3
        movlt   r10, r5
        movge   r9, r5
        movge   r10, r3
        cmp     r7, r9
        movlt   r9, r7
        cmp     r7, r10
        movgt   r10, r7
        add     r9, r9, #6
        add     r10, r10, #8
        mov     r9, r9, asr #4
        cmp     r9, r10, asr #4
        bge     9f
        cmn     r12, #1                 @ under DOTA (twice the area, 1/256 pixels): a dot
        bne     1f
        cmn     r11, #DOTA
        bgt     .Ldot
1:      ldr     r11, =s_npoly
        ldr     r12, [r11]
        ldr     r3, =MAXPOLY
        cmp     r12, r3
        bge     9f
        ldr     r3, [r0, #8]            @ the depth key: (Wa + Wb + Wc) >> 8, at most 255
        ldr     r4, [r1, #8]
        add     r3, r3, r4
        ldr     r4, [r2, #8]
        add     r3, r3, r4
        mov     r3, r3, asr #8
        cmp     r3, #NBUCKET - 1
        movgt   r3, #NBUCKET - 1
        mov     r4, r12, lsl #4
        sub     r4, r4, r12, lsl #1     @ 14 n
        ldr     lr, =s_poly
        add     lr, lr, r4, lsl #2      @ the poly
        add     r0, r0, #12             @ the corners: sx sy u v
        ldmia   r0, {r4-r7}
        stmia   lr!, {r4-r7}
        add     r1, r1, #12
        ldmia   r1, {r4-r7}
        stmia   lr!, {r4-r7}
        add     r2, r2, #12
        ldmia   r2, {r4-r7}
        stmia   lr!, {r4-r7}
        ldr     r4, =s_bucket
        add     r4, r4, r3, lsl #1
        ldrh    r5, [r4]                @ next: the bucket's head
        ldr     r6, =s_ct
        ldr     r6, [r6]                @ col | tex << 16
        mov     r7, r6, lsl #16
        mov     r7, r7, lsr #16
        orr     r7, r7, r5, lsl #16
        mov     r6, r6, lsr #16
        str     r7, [lr]
        str     r6, [lr, #4]
        add     r12, r12, #1
        strh    r12, [r4]
        str     r12, [r11]
9:      ldmfd   sp!, {r4-r11, lr}
        bx      lr
@ a triangle too small to set up: its colour at the pixel under its middle, in its bucket as
@ a Poly with tex 0xFFFE (raster.s draw), x and y in its first two words
.Ldot:
        ldr     r11, =s_npoly
        ldr     r12, [r11]
        ldr     r9, =MAXPOLY
        cmp     r12, r9
        bge     9b
        add     r9, r3, r5              @ the middle: the corners' sum / 3 / 16
        add     r9, r9, r7
        add     r10, r4, r6
        add     r10, r10, r8
        ldr     lr, =0x5556
        mul     r9, lr, r9
        mul     r10, lr, r10
        mov     r9, r9, asr #20
        mov     r10, r10, asr #20
        cmp     r9, #0
        movlt   r9, #0
        cmp     r9, #SW - 1
        movgt   r9, #SW - 1
        cmp     r10, #0
        movlt   r10, #0
        cmp     r10, #SH - 1
        movgt   r10, #SH - 1
        ldr     r3, [r0, #8]            @ the depth key, as for a poly
        ldr     r4, [r1, #8]
        add     r3, r3, r4
        ldr     r4, [r2, #8]
        add     r3, r3, r4
        mov     r3, r3, asr #8
        cmp     r3, #NBUCKET - 1
        movgt   r3, #NBUCKET - 1
        mov     r4, r12, lsl #4
        sub     r4, r4, r12, lsl #1     @ 14 n
        ldr     lr, =s_poly
        add     lr, lr, r4, lsl #2
        str     r9, [lr]
        str     r10, [lr, #4]
        ldr     r4, =s_bucket
        add     r4, r4, r3, lsl #1
        ldrh    r5, [r4]
        ldr     r6, =s_ct
        ldrh    r6, [r6]                @ the colour
        orr     r6, r6, r5, lsl #16     @ col | next << 16
        str     r6, [lr, #48]
        ldr     r6, =0xFFFE
        strh    r6, [lr, #52]
        add     r12, r12, #1
        strh    r12, [r4]
        str     r12, [r11]
        b       9b
        .ltorg

@ halve: r8 |= bit when edge p-q is to be halved (main.c's halve: fewer than
@ GMAX halvings so far, its far end more than twice as deep as its near end,
@ over 16 pixels long).  g: the edge's halvings (a register).
        .macro  HALVE p, q, g, bit
        cmp     \g, #GMAX
        bhs     1f
        ldr     r0, [\p, #8]
        ldr     r1, [\q, #8]
        subs    r2, r0, r1
        rsblt   r2, r2, #0              @ hi - lo
        cmp     r0, r1
        movgt   r0, r1                  @ lo
        cmp     r2, r0, lsl #DEEP
        ble     1f
        ldr     r0, [\p, #12]
        ldr     r1, [\q, #12]
        subs    r0, r0, r1
        rsblt   r0, r0, #0
        ldr     r1, [\p, #16]
        ldr     r2, [\q, #16]
        subs    r1, r1, r2
        rsblt   r1, r1, #0
        add     r0, r0, r1
        cmp     r0, #MINEDGE
        orrgt   r8, r8, #\bit
1:
        .endm

@ the clip-space midpoint of p-q into d, projected
        .macro  MID p, q, d
        ldr     r0, [\p]
        ldr     r1, [\q]
        mov     r0, r0, asr #1
        add     r0, r0, r1, asr #1
        str     r0, [\d]
        ldr     r0, [\p, #4]
        ldr     r1, [\q, #4]
        mov     r0, r0, asr #1
        add     r0, r0, r1, asr #1
        str     r0, [\d, #4]
        ldr     r0, [\p, #8]
        ldr     r1, [\q, #8]
        add     r0, r0, r1
        mov     r0, r0, asr #1
        str     r0, [\d, #8]
        ldr     r0, [\p, #20]
        ldr     r1, [\q, #20]
        add     r0, r0, r1
        mov     r0, r0, asr #1
        str     r0, [\d, #20]
        ldr     r0, [\p, #24]
        ldr     r1, [\q, #24]
        add     r0, r0, r1
        mov     r0, r0, asr #1
        str     r0, [\d, #24]
        mov     r0, \d
        bl      project
        .endm

@ persp: r0 a, r1 b, r2 c (PV), r3 g (the edges' halvings: a-b bits 0-2,
@ b-c 3-5, c-a 6-8).  AAPCS.
persp:
        stmfd   sp!, {r4, r5}
        .irp    off, 12, 16             @ off the screen, and so every piece of it?
        ldr     r4, [r0, #\off]
        ldr     r5, [r1, #\off]
        ldr     r12, [r2, #\off]
        .if     \off == 12
        cmp     r4, #SW * 16
        cmpge   r5, #SW * 16
        cmpge   r12, #SW * 16
        .else
        cmp     r4, #SH * 16
        cmpge   r5, #SH * 16
        cmpge   r12, #SH * 16
        .endif
        bge     8f
        and     r4, r4, r5
        ands    r4, r4, r12
        bmi     8f
        .endr
                                        @ furthest at most twice the nearest: no edge
        ldr     r4, [r0, #8]            @ would be halved, so the triangle as it is
        mov     r5, r4
        ldr     r12, [r1, #8]
        cmp     r12, r4
        movlt   r4, r12
        cmp     r12, r5
        movgt   r5, r12
        ldr     r12, [r2, #8]
        cmp     r12, r4
        movlt   r4, r12
        cmp     r12, r5
        movgt   r5, r12
        sub     r5, r5, r4
        cmp     r5, r4, lsl #DEEP
        ldmfd   sp!, {r4, r5}
        ble     emit
        b       7f
8:      ldmfd   sp!, {r4, r5}
        bx      lr
7:
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #84             @ the midpoints
        mov     r4, r0
        mov     r5, r1
        mov     r6, r2
        mov     r7, r3
        mov     r8, #0
        and     r9, r7, #7
        mov     r10, r7, lsr #3
        and     r10, r10, #7
        mov     r11, r7, lsr #6
        HALVE   r4, r5, r9, 1
        HALVE   r5, r6, r10, 2
        HALVE   r6, r4, r11, 4
        cmp     r8, #0
        bne     2f
        mov     r0, r4
        mov     r1, r5
        mov     r2, r6
        bl      emit
        b       9f
2:      cmp     r8, #1                  @ turned so the halved edges come first
        cmpne   r8, #3
        cmpne   r8, #7
        beq     3f
        mov     r0, r4
        mov     r4, r5
        mov     r5, r6
        mov     r6, r0
        mov     r0, r7, lsr #3
        orr     r0, r0, r7, lsl #6
        mov     r7, r0, lsl #23
        mov     r7, r7, lsr #23
        mov     r0, r8, lsr #1
        orr     r0, r0, r8, lsl #2
        and     r8, r0, #7
        b       2b
3:      and     r9, r7, #7              @ each edge's halvings + 1
        add     r9, r9, #1
        mov     r10, r7, lsr #3
        and     r10, r10, #7
        add     r10, r10, #1
        mov     r11, r7, lsr #6
        add     r11, r11, #1
        mov     r7, r9                  @ gi: the most halved edge cut's
        cmp     r8, #1
        beq     4f
        cmp     r10, r7
        movgt   r7, r10
        cmp     r8, #3
        beq     4f
        cmp     r11, r7
        movgt   r7, r11
4:      MID     r4, r5, sp
        cmp     r8, #1
        bne     5f
        mov     r0, r4                  @ a-b: two
        mov     r1, sp
        mov     r2, r6
        sub     r3, r11, #1
        mov     r3, r3, lsl #6
        orr     r3, r3, r7, lsl #3
        orr     r3, r3, r9
        bl      persp
        mov     r0, sp
        mov     r1, r5
        mov     r2, r6
        sub     r3, r10, #1
        orr     r3, r9, r3, lsl #3
        orr     r3, r3, r7, lsl #6
        bl      persp
        b       9f
5:      add     r12, sp, #28
        MID     r5, r6, r12
        cmp     r8, #3
        bne     6f
        mov     r0, sp                  @ a-b and b-c: three
        mov     r1, r5
        add     r2, sp, #28
        orr     r3, r9, r10, lsl #3
        orr     r3, r3, r7, lsl #6
        bl      persp
        mov     r0, r4
        mov     r1, sp
        add     r2, sp, #28
        orr     r3, r9, r7, lsl #3
        orr     r3, r3, r7, lsl #6
        bl      persp
        mov     r0, r4
        add     r1, sp, #28
        mov     r2, r6
        sub     r3, r11, #1
        mov     r3, r3, lsl #6
        orr     r3, r3, r10, lsl #3
        orr     r3, r3, r7
        bl      persp
        b       9f
6:      add     r12, sp, #56            @ all three: four
        MID     r6, r4, r12
        mov     r0, r4
        mov     r1, sp
        add     r2, sp, #56
        orr     r3, r9, r7, lsl #3
        orr     r3, r3, r11, lsl #6
        bl      persp
        mov     r0, sp
        mov     r1, r5
        add     r2, sp, #28
        orr     r3, r9, r10, lsl #3
        orr     r3, r3, r7, lsl #6
        bl      persp
        add     r0, sp, #56
        add     r1, sp, #28
        mov     r2, r6
        orr     r3, r7, r10, lsl #3
        orr     r3, r3, r11, lsl #6
        bl      persp
        mov     r0, sp
        add     r1, sp, #28
        add     r2, sp, #56
        orr     r3, r7, r7, lsl #3
        orr     r3, r3, r7, lsl #6
        bl      persp
9:      add     sp, sp, #84
        ldmfd   sp!, {r4-r11, lr}
        bx      lr

@ cut: r0 a, r1 b (PV, the near plane between them), r2 o: where edge a-b
@ meets the plane, from its nearer end either way round.  AAPCS.
cut:
        stmfd   sp!, {r4-r7, lr}
        ldr     r3, [r0, #8]
        ldr     r12, [r1, #8]
        cmp     r3, r12
        movgt   r4, r0
        movgt   r0, r1
        movgt   r1, r4
        movgt   r4, r3
        movgt   r3, r12
        movgt   r12, r4
        sub     r12, r12, r3            @ d = Wb - Wa, > 0
        mov     r4, #0
1:      cmp     r12, #4096
        movge   r12, r12, asr #1
        addge   r4, r4, #1
        bge     1b
        ldr     r5, =s_rec
        ldr     r5, [r5, r12, lsl #2]
        mov     r5, r5, lsr r4
        rsb     r3, r3, #NEAR
        smull   r4, r6, r3, r5
        mov     r4, r4, lsr #8
        orr     r4, r4, r6, lsl #24     @ t, Q16
        .irp    off, 0, 4, 20, 24
        ldr     r3, [r0, #\off]
        ldr     r5, [r1, #\off]
        sub     r5, r5, r3
        smull   r6, r7, r5, r4
        mov     r6, r6, lsr #16
        orr     r6, r6, r7, lsl #16
        add     r3, r3, r6
        str     r3, [r2, #\off]
        .endr
        mov     r3, #NEAR
        str     r3, [r2, #8]
        mov     r0, r2
        bl      project
        ldmfd   sp!, {r4-r7, lr}
        bx      lr
        .ltorg

@ copy a corner (7 words) from s to d, its u, v from the triangle (r7)
        .macro  CORNER s, d, k
        ldmia   \s, {r0-r3, r9}
        ldrsh   r10, [r7, #16 + \k * 4]
        ldrsh   r11, [r7, #18 + \k * 4]
        stmia   \d, {r0-r3, r9-r11}
        .endm

@ copy a PV, d = s
        .macro  COPYPV s, d
        ldmia   \s, {r0-r3, r9-r11}
        stmia   \d, {r0-r3, r9-r11}
        .endm

@ tri: r0-r2 the corners (PV), r3 the Tri.  AAPCS.
@ frame: c[3] at 0, q[4] at 84
tri:
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #196
        mov     r4, r0
        mov     r5, r1
        mov     r6, r2
        mov     r7, r3
        ldr     r0, [r4, #8]
        ldr     r1, [r5, #8]
        ldr     r2, [r6, #8]
        mov     r8, #0                  @ which are in front of the near plane
        cmp     r0, #NEAR
        orrge   r8, r8, #1
        cmp     r1, #NEAR
        orrge   r8, r8, #2
        cmp     r2, #NEAR
        orrge   r8, r8, #4
        cmp     r8, #0
        beq     9f
        cmp     r8, #7
        bne     1f
        ldr     r0, [r4, #12]           @ all: facing away, or off the screen?
        ldr     r1, [r4, #16]
        ldr     r2, [r5, #12]
        sub     r2, r2, r0
        ldr     r3, [r5, #16]
        sub     r3, r1, r3              @ -(y1 - y0)
        ldr     r9, [r6, #12]
        sub     r9, r9, r0
        ldr     r10, [r6, #16]
        sub     r10, r10, r1
        smull   r11, r12, r2, r10
        smlal   r11, r12, r3, r9
        cmp     r12, #0
        bge     9f
        ldr     r0, [r4, #12]
        ldr     r1, [r5, #12]
        ldr     r2, [r6, #12]
        and     r3, r0, r1
        and     r3, r3, r2
        cmp     r3, #0
        blt     9f
        cmp     r0, #SW * 16
        cmpge   r1, #SW * 16
        cmpge   r2, #SW * 16
        bge     9f
        ldr     r0, [r4, #16]
        ldr     r1, [r5, #16]
        ldr     r2, [r6, #16]
        and     r3, r0, r1
        and     r3, r3, r2
        cmp     r3, #0
        blt     9f
        cmp     r0, #SH * 16
        cmpge   r1, #SH * 16
        cmpge   r2, #SH * 16
        bge     9f
1:      ldrh    r0, [r7, #6]            @ colour and texture, for emit
        ldrh    r1, [r7, #14]
        orr     r0, r0, r1, lsl #16
        ldr     r1, =s_ct
        str     r0, [r1]
        cmp     r8, #7
        bne     3f
        add     r12, r7, #16            @ all in front: this triangle's u, v into the cell's
        ldmia   r12, {r9-r11}           @ corners themselves (each triangle sets them as it
        mov     r12, r9, lsl #16        @ uses them), and on with those
        mov     r12, r12, asr #16
        mov     r9, r9, asr #16
        str     r12, [r4, #20]
        str     r9, [r4, #24]
        mov     r12, r10, lsl #16
        mov     r12, r12, asr #16
        mov     r10, r10, asr #16
        str     r12, [r5, #20]
        str     r10, [r5, #24]
        mov     r12, r11, lsl #16
        mov     r12, r12, asr #16
        mov     r11, r11, asr #16
        str     r12, [r6, #20]
        str     r11, [r6, #24]
        mov     r0, r4
        mov     r1, r5
        mov     r2, r6
        mov     r3, #0
        ldr     r12, =s_ct
        ldr     r12, [r12]
        mvn     lr, #0
        cmp     lr, r12, lsr #16
        beq     4f
        bl      persp
        b       9f
4:      bl      emit
        b       9f
3:      mov     r12, sp                 @ some behind: the corners copied, to be clipped
        CORNER  r4, r12, 0
        add     r12, sp, #28
        CORNER  r5, r12, 1
        add     r12, sp, #56
        CORNER  r6, r12, 2
        ldr     r1, =s_ct
        ldr     r1, [r1]
        mvn     r0, #0
        cmp     r0, r1, lsr #16
        moveq   r7, #0                  @ r7: textured
        movne   r7, #1
2:      add     r4, sp, #84             @ the polygon in front of the plane: q[r4..]
        mov     r5, r4
        .irp    i, 0, 1, 2
        .if     \i == 2
        .set    j, 0
        .else
        .set    j, \i + 1
        .endif
        tst     r8, #1 << \i
        beq     3f
        add     r6, sp, #\i * 28
        COPYPV  r6, r5
        add     r5, r5, #28
3:      and     r0, r8, #(1 << \i) | (1 << j)
        cmp     r0, #0                  @ the plane between this corner and the next?
        beq     4f
        cmp     r0, #(1 << \i) | (1 << j)
        beq     4f
        add     r0, sp, #\i * 28
        add     r1, sp, #j * 28
        mov     r2, r5
        bl      cut
        add     r5, r5, #28
4:
        .endr
        mov     r0, r4
        add     r1, r4, #28
        add     r2, r4, #56
        mov     r3, #0
        cmp     r7, #0
        blne    persp
        cmp     r7, #0
        bleq    emit
        sub     r0, r5, r4
        cmp     r0, #112
        bne     9f
        mov     r0, r4
        add     r1, r4, #56
        add     r2, r4, #84
        mov     r3, #0
        cmp     r7, #0
        blne    persp
        cmp     r7, #0
        bleq    emit
9:      add     sp, sp, #196
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ the vertex whose index is in r0 through the camera, once a cell: r0 = its PV.
@ r7 the cell's vertices, r8 the stamp; clobbers r1-r3, r9, r10, r12.
        .macro  VERTEX
        ldr     r1, =s_tag
        add     r1, r1, r0, lsl #1
        ldrh    r2, [r1]
        add     r3, r7, r0, lsl #3
        rsb     r0, r0, r0, lsl #3
        ldr     r12, =s_pv
        add     r0, r12, r0, lsl #2
        cmp     r2, r8
        beq     8f
        strh    r8, [r1]
        ldrsh   r1, [r3]
        ldrsh   r2, [r3, #2]
        ldrsh   r3, [r3, #4]
        .irp    row, 0, 1, 2
        ldr     r9, [sp, #\row * 16]
        smull   r10, r12, r9, r1
        ldr     r9, [sp, #\row * 16 + 4]
        smlal   r10, r12, r9, r2
        ldr     r9, [sp, #\row * 16 + 8]
        smlal   r10, r12, r9, r3
        ldr     r9, [sp, #\row * 16 + 12]
        mov     r10, r10, lsr #12
        orr     r10, r10, r12, lsl #20
        add     r10, r10, r9
        str     r10, [r0, #\row * 4]
        .endr
        cmp     r10, #NEAR
        blge    project
        ldr     r1, [sp, #RV_NV]
        add     r1, r1, #1
        str     r1, [sp, #RV_NV]
8:
        .endm

        .equ    RV_M, 0                 @ the camera's 12 words
        .equ    RV_PX, 48
        .equ    RV_PY, 52
        .equ    RV_PNC, 56
        .equ    RV_PNV, 60
        .equ    RV_NC, 64
        .equ    RV_NV, 68
        .equ    RV_PA, 72
        .equ    RV_PB, 76
        .equ    RV_CAM, 80              @ the camera (1/8 units), then the screen's edges as planes
        .equ    RV_EDGE, 92
        .equ    RV_ALL, 156             @ a cell of all its triangles: the list's place after it, else 0
        .equ    RV_FRAME, 160

render_visible:
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #RV_FRAME
        ldmia   r0!, {r3-r8}
        stmia   sp, {r3-r8}
        ldmia   r0!, {r3-r8}
        add     r9, sp, #24
        stmia   r9, {r3-r8}
        add     r3, r0, #8              @ the camera and the edges: 19 words
        add     r9, sp, #RV_CAM
        mov     r10, #19
1:      ldr     r11, [r3], #4
        str     r11, [r9], #4
        subs    r10, r10, #1
        bne     1b
        ldrsh   r9, [r0]                @ px, py
        ldrsh   r10, [r0, #2]
        str     r9, [sp, #RV_PX]
        str     r10, [sp, #RV_PY]
        str     r1, [sp, #RV_PNC]
        str     r2, [sp, #RV_PNV]
        mov     r0, #0
        str     r0, [sp, #RV_NC]
        str     r0, [sp, #RV_NV]
        ldr     r0, =s_vis_list         @ the frame's own list (the live race: race.c), else the
        ldr     r4, [r0]                @ recorded race's for the camera's cell
        cmp     r4, #0
        beq     1f
        ldr     r8, =s_stamp
        ldrh    r8, [r8]
        b       .Lcell
1:      ldr     r0, =g_org              @ the camera's 32-unit cell
        ldmia   r0, {r1, r2}
        add     r1, r1, r9
        mov     r1, r1, asr #5
        ldr     r0, =g_pvs_x0
        ldr     r0, [r0]
        sub     r1, r1, r0
        add     r2, r2, r10
        mov     r2, r2, asr #5
        ldr     r0, =g_pvs_y0
        ldr     r0, [r0]
        sub     r2, r2, r0
        ldr     r0, =g_pvs_w
        ldr     r0, [r0]
        cmp     r1, r0
        bhs     .Ldone
        ldr     r3, =g_pvs_h
        ldr     r3, [r3]
        cmp     r2, r3
        bhs     .Ldone
        mla     r3, r2, r0, r1
        ldr     r0, =g_pvs_at
        ldr     r3, [r0, r3, lsl #2]
        cmn     r3, #1
        beq     .Ldone
        ldr     r4, =g_pvs
        add     r4, r4, r3, lsl #1      @ r4: the list
        ldr     r8, =s_stamp
        ldrh    r8, [r8]
@ r4 the list, r5 triangles left in the cell, r6 its triangles, r7 its vertices, r8 the stamp
.Lcell:
        ldrh    r0, [r4], #2
        mvn     r1, #0
        cmp     r0, r1, lsr #16
        beq     .Lcells_done
        ldrh    r5, [r4], #2
        mov     r1, #0                  @ the count's top bit: all the cell's triangles, in order
        tst     r5, #0x8000             @ (their numbers from s_iota)
        bicne   r5, r5, #0x8000
        movne   r1, r4
        ldrne   r4, =s_iota
        str     r1, [sp, #RV_ALL]
        ldr     r1, =g_cells            @ (24 bytes a cell)
        add     r0, r0, r0, lsl #1
        add     r1, r1, r0, lsl #3
        ldrsh   r2, [r1, #16]           @ wholly past an edge of the screen? its sphere against each
        ldrsh   r3, [r1, #18]
        ldrsh   r9, [r1, #20]
        ldrsh   r10, [r1, #22]
        add     r10, r10, #2
        add     r12, sp, #RV_EDGE
        .rept   4
        ldmia   r12!, {r0, r11, lr}
        mul     r0, r2, r0
        mla     r0, r3, r11, r0
        mla     r0, r9, lr, r0
        ldr     r11, [r12], #4
        add     r11, r11, r10
        rsb     r11, r11, #0
        cmp     r0, r11, lsl #12
        blt     .Lcull
        .endr
        ldr     r2, [r1]                @ vstart
        ldr     r3, [r1, #8]            @ tstart
        ldr     r7, =g_verts
        add     r7, r7, r2, lsl #3
        ldr     r6, =g_tris             @ (40 bytes a triangle)
        add     r3, r3, r3, lsl #2
        add     r6, r6, r3, lsl #3
        add     r8, r8, #1
        mov     r8, r8, lsl #16
        mov     r8, r8, lsr #16
        ldr     r0, [sp, #RV_NC]
        add     r0, r0, #1
        str     r0, [sp, #RV_NC]
        cmp     r5, #0
        beq     .Lcell_end
.Ltri:
        CNT     0
        ldrh    r0, [r4], #2
        add     r0, r0, r0, lsl #2
        add     r11, r6, r0, lsl #3     @ the triangle
        add     r12, r11, #8            @ drawn at this distance? (cx cy, dlo dhi: one load)
        ldmia   r12, {r0, r1}
        mov     r2, r0, lsl #16
        mov     r2, r2, asr #16
        mov     r0, r0, asr #16
        ldr     r3, [sp, #RV_PX]
        sub     r2, r2, r3
        ldr     r3, [sp, #RV_PY]
        sub     r0, r0, r3
        mul     r3, r2, r2
        mla     r3, r0, r0, r3          @ e2
        and     r0, r1, #0xFF
        mul     r2, r0, r0
        cmp     r3, r2, lsl #6
        blt     .Lnext
        mov     r0, r1, lsr #8
        and     r0, r0, #0xFF
        mul     r2, r0, r0
        cmp     r3, r2, lsl #6
        bgt     .Lnext
        CNT     1
        add     r12, r11, #28           @ the camera clearly behind its plane: facing away
        ldmia   r12, {r0, r1, r2}       @ n0 n1, n2, d
        mov     r3, r0, lsl #16
        mov     r3, r3, asr #16
        ldr     r9, [sp, #RV_CAM]
        mul     r10, r3, r9
        mov     r0, r0, asr #16
        ldr     r9, [sp, #RV_CAM + 4]
        mla     r10, r0, r9, r10
        mov     r1, r1, lsl #16
        mov     r1, r1, asr #16
        ldr     r9, [sp, #RV_CAM + 8]
        mla     r10, r1, r9, r10
        sub     r10, r10, r2
        cmn     r10, #8192
        blt     .Lnext
        CNT     2
        ldrh    r0, [r11]
        VERTEX
        str     r0, [sp, #RV_PA]
        ldrh    r0, [r11, #2]
        VERTEX
        str     r0, [sp, #RV_PB]
        ldrh    r0, [r11, #4]
        VERTEX
        mov     r2, r0
        ldr     r0, [sp, #RV_PA]
        ldr     r1, [sp, #RV_PB]
        mov     r3, r11
        .ifdef  COUNT
        stmfd   sp!, {r0-r3}
        mov     r0, #0x04000000
        add     r0, r0, #0x100
        ldrh    r0, [r0, #8]
        ldr     r1, =g_fcount + 32
        str     r0, [r1, #4]
        ldmfd   sp!, {r0-r3}
        bl      tri
        mov     r0, #0x04000000
        add     r0, r0, #0x100
        ldrh    r0, [r0, #8]
        ldr     r1, =g_fcount + 32
        ldr     r2, [r1, #4]
        sub     r0, r0, r2
        mov     r0, r0, lsl #16
        ldr     r2, [r1]
        add     r2, r2, r0, lsr #16
        str     r2, [r1]
        .else
        bl      tri
        .endif
.Lnext:
        subs    r5, r5, #1
        bne     .Ltri
.Lcell_end:
        ldr     r0, [sp, #RV_ALL]       @ back in the list after an all-triangles cell
        cmp     r0, #0
        movne   r4, r0
        b       .Lcell
.Lcull:
        ldr     r0, [sp, #RV_ALL]
        cmp     r0, #0
        addeq   r4, r4, r5, lsl #1      @ past its triangles in the list
        movne   r4, r0
        b       .Lcell
.Lcells_done:
        ldr     r0, =s_stamp
        strh    r8, [r0]
.Ldone:
        ldr     r0, [sp, #RV_PNC]
        ldr     r1, [r0]
        ldr     r2, [sp, #RV_NC]
        add     r1, r1, r2
        str     r1, [r0]
        ldr     r0, [sp, #RV_PNV]
        ldr     r1, [r0]
        ldr     r2, [sp, #RV_NV]
        add     r1, r1, r2
        str     r1, [r0]
        add     sp, sp, #RV_FRAME
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ 0, 1, 2 ..: an all-triangles cell's numbers (race.c's lists)
        .section .rodata
        .align  1
        .global s_iota
s_iota:
        .set    i, 0
        .rept   256
        .hword  i
        .set    i, i + 1
        .endr
