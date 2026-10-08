@ menu.s -- the menu's icons (BrModelDraw as the menu draws it): menu.c's
@ icon_draw makes the frame's matrix and light; here every vertex goes to
@ the screen and is lit, and every triangle facing the camera goes to the
@ race's buckets (raster.s draws them) with its texture at its brightness.
@ In IWRAM, ARM.
@
@   void icon_xform(const MVert *v, int n, const int32_t *k, MV *out)
@     k: the X, Y and W rows (the model's x, y, z coefficients and a constant, Q12),
@        the light's way (Q12), its level, the ambient's, s_rec
@     out: x, y (Q4 pixels), w (W >> 10), the light 0..255
@   void icon_tris(const MTri *t, int n, const MV *mv, int front)
        .section .ovl_draw, "ax"
        .arm
        .global icon_xform, icon_tris

        .equ    SW, 160
        .equ    SH, 128
        .equ    NBUCKET, 256
        .equ    MAXPOLY, 2400
        .equ    K_L, 48
        .equ    K_DIR, 60
        .equ    K_AMB, 64
        .equ    K_REC, 68

icon_xform:
        stmfd   sp!, {r4-r11, lr}
        cmp     r1, #0
        beq     9f
1:      ldrsh   r4, [r0]                @ x, y, z
        ldrsh   r5, [r0, #2]
        ldrsh   r6, [r0, #4]
        ldmia   r2, {r7-r10}            @ X
        mla     r11, r7, r4, r10
        mla     r11, r8, r5, r11
        mla     r11, r9, r6, r11
        add     r12, r2, #16            @ Y
        ldmia   r12, {r7-r10}
        mla     r12, r7, r4, r10
        mla     r12, r8, r5, r12
        mla     r12, r9, r6, r12
        add     lr, r2, #32             @ W
        ldmia   lr, {r7-r10}
        mla     lr, r7, r4, r10
        mla     lr, r8, r5, lr
        mla     lr, r9, r6, lr
        mov     r6, lr, asr #10         @ w, within 1..4095
        cmp     r6, #1
        movlt   r6, #1
        cmp     r6, #4096
        movge   r6, #4096
        subge   r6, r6, #1
        ldr     r7, [r2, #K_REC]
        ldr     r7, [r7, r6, lsl #2]    @ 2^24 / w: 2^34 / W
        smull   r8, r9, r11, r7         @ x = X / W, Q4: (X r) >> 30
        mov     r4, r9, lsl #2
        orr     r4, r4, r8, lsr #30
        smull   r8, r9, r12, r7         @ y
        mov     r5, r9, lsl #2
        orr     r5, r5, r8, lsr #30
        ldrsb   r8, [r0, #6]            @ the light: the ambient, and the light's level times n . l
        ldrsb   r9, [r0, #7]            @ where it faces it (n 127 long, l Q12)
        ldrsb   r10, [r0, #8]
        add     r7, r2, #K_L
        ldmia   r7, {r11, r12, lr}
        mul     r7, r8, r11
        mla     r7, r9, r12, r7
        mla     r7, r10, lr, r7
        ldr     r8, [r2, #K_DIR]
        cmp     r7, #0
        mulgt   r10, r7, r8
        movgt   r10, r10, asr #19
        movle   r10, #0
        ldr     r8, [r2, #K_AMB]
        add     r7, r10, r8
        cmp     r7, #255
        movgt   r7, #255
        stmia   r3!, {r4-r7}
        add     r0, r0, #10
        subs    r1, r1, #1
        bne     1b
9:      ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ the triangles: those facing the camera (or drawn both ways round) and on the screen, each a
@ Poly in the bucket of its depth; its texture the slot's at the brightness of its light (the
@ three corners' mean, or its part's fixed grey)
icon_tris:
        stmfd   sp!, {r4-r11, lr}
        sub     sp, sp, #8
        str     r3, [sp]
        cmp     r1, #0
        beq     .Ldone
.Ltri:
        ldrh    r3, [r0]                @ the corners
        add     r4, r2, r3, lsl #4
        ldrh    r3, [r0, #2]
        add     r5, r2, r3, lsl #4
        ldrh    r3, [r0, #4]
        add     r6, r2, r3, lsl #4
        ldmia   r4, {r7, r8}
        ldmia   r5, {r9, r10}
        ldmia   r6, {r11, r12}
        sub     r9, r9, r7              @ its winding: (b - a) x (c - a)
        sub     r12, r12, r8
        mul     lr, r9, r12
        sub     r11, r11, r7
        sub     r10, r10, r8
        mul     r3, r11, r10
        subs    lr, lr, r3
        beq     .Lnext
        ldrb    r3, [r0, #6]
        tst     r3, #1                  @ (MT_TWO: either way)
        bne     1f
        ldr     r3, [sp]
        cmp     r3, #0
        rsblt   lr, lr, #0
        cmp     lr, #0
        ble     .Lnext
1:      add     r9, r9, r7              @ on the screen
        add     r11, r11, r7
        add     r10, r10, r8
        add     r12, r12, r8
        mov     r3, r7
        cmp     r9, r3
        movlt   r3, r9
        cmp     r11, r3
        movlt   r3, r11
        cmp     r3, #SW * 16
        bge     .Lnext
        mov     r3, r7
        cmp     r9, r3
        movgt   r3, r9
        cmp     r11, r3
        movgt   r3, r11
        cmp     r3, #0
        blt     .Lnext
        mov     r3, r8
        cmp     r10, r3
        movlt   r3, r10
        cmp     r12, r3
        movlt   r3, r12
        cmp     r3, #SH * 16
        bge     .Lnext
        mov     r3, r8
        cmp     r10, r3
        movgt   r3, r10
        cmp     r12, r3
        movgt   r3, r12
        cmp     r3, #0
        blt     .Lnext
        ldr     r3, =s_npoly
        ldr     lr, [r3]
        cmp     lr, #MAXPOLY
        bge     .Lnext
        ldr     r3, =s_poly             @ the Poly: s_poly + 56 n
        add     r3, r3, lr, lsl #6
        sub     r3, r3, lr, lsl #3
        str     r7, [r3]
        str     r8, [r3, #4]
        str     r9, [r3, #16]
        str     r10, [r3, #20]
        str     r11, [r3, #32]
        str     r12, [r3, #36]
        ldrsh   r7, [r0, #12]           @ u, v
        ldrsh   r8, [r0, #14]
        ldrsh   r9, [r0, #16]
        ldrsh   r10, [r0, #18]
        ldrsh   r11, [r0, #20]
        ldrsh   r12, [r0, #22]
        str     r7, [r3, #8]
        str     r8, [r3, #12]
        str     r9, [r3, #24]
        str     r10, [r3, #28]
        str     r11, [r3, #40]
        str     r12, [r3, #44]
        ldrb    r7, [r0, #6]            @ the light: its part's grey, or the corners' mean
        tst     r7, #4
        ldrbne  r7, [r0, #7]
        bne     2f
        ldr     r7, [r4, #12]
        ldr     r8, [r5, #12]
        add     r7, r7, r8
        ldr     r8, [r6, #12]
        add     r7, r7, r8
        mov     r8, #85
        mul     r9, r7, r8
        mov     r7, r9, lsr #8
2:      mov     r7, r7, lsl #3          @ its level, 1..8: (light 8 + 128) >> 8
        add     r7, r7, #128
        mov     r7, r7, asr #8
        cmp     r7, #1
        movlt   r7, #1
        cmp     r7, #8
        movgt   r7, #8
        ldrh    r8, [r0, #8]            @ g_menu_slot[slot][level - 1]
        ldr     r9, =g_menu_slot
        add     r9, r9, r8, lsl #4
        add     r9, r9, r7, lsl #1
        ldrh    r9, [r9, #-2]
        strh    r9, [r3, #52]
        mov     r9, #0
        strh    r9, [r3, #48]
        ldr     r7, [r4, #8]            @ its bucket: W / 3 in half units, about the icon's
        ldr     r8, [r5, #8]
        add     r7, r7, r8
        ldr     r8, [r6, #8]
        add     r7, r7, r8
        mov     r8, #85
        mul     r9, r7, r8
        mov     r7, r9, asr #9
        sub     r7, r7, #400
        sub     r7, r7, #14
        cmp     r7, #0
        movlt   r7, #0
        cmp     r7, #NBUCKET - 1
        movgt   r7, #NBUCKET - 1
        ldr     r8, =s_bucket
        add     r8, r8, r7, lsl #1
        ldrh    r9, [r8]
        strh    r9, [r3, #50]
        add     lr, lr, #1
        strh    lr, [r8]
        ldr     r9, =s_npoly
        str     lr, [r9]
.Lnext:
        add     r0, r0, #24
        subs    r1, r1, #1
        bne     .Ltri
.Ldone:
        add     sp, sp, #8
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg
