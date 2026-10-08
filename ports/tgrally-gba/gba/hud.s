@ hud.s -- the race HUD: racing/racehud.c's BrHudDraw for the one-player view
@ (BrHudDialDraw, BrHudTimesDraw, BrHudLapDraw, the speed) and the text
@ printer under it (drawing/textstate.c: BrTextPrint, BrTextWidth,
@ BrTextEmitString), every retrace, as the GBA's sprites over the 3D.  The
@ numbers are the game's, recorded (tools/hud.py: g_hud_state); each glyph
@ is drawn once by the converter as the RDP draws it, and goes where the
@ printer puts its rectangle: N64 pixels at 3/4 by 2/3.  Later glyphs over
@ earlier ones, as the game's order paints them.  In IWRAM, ARM.
@
@   void hud_init(void)        the sprites' tiles and palette
@   void hud_vblank(void)      from the blank interrupt: this retrace's HUD
        .section .iwram, "ax"
        .arm
        .global hud_init, hud_vblank, s_oam, s_oamn, s_txt, s_oam_ready, s_hud_on, s_k, txt_emit, txt_width, txt_print, put_dec, time_draw, txt_scheme, put_sprite, copy

        .equ    TILE0, 512              @ the first sprite tile in the bitmap modes
        .equ    FACE, 0x2000            @ where things are in sprite VRAM (bytes): glyphs at 0
        .equ    LAMPS, 0x3000
        .equ    NEEDLE, 0x3800
        .equ    G_SCHEME, 0             @ HudGlyph
        .equ    G_SW, 3
        .equ    G_TILE, 4
        .equ    H_SIZE, 26              @ HudState
        .equ    T_HIGHLIGHT, 0          @ the printer's state (D_8028BDC0..)
        .equ    T_ALT, 4
        .equ    T_ALIGN, 8              @ 0 left, 1 right, 2 centre
        .equ    T_CUSTOM, 12
        .equ    T_CENV, 16
        .equ    T_CPRIM, 20
        .equ    T_SIZE, 24
        .equ    T_PRIM, 28              @ while printing
        .equ    T_ENV, 32
        .equ    T_SCHEME, 36

        .section .iwram_bss, "aw", %nobits
        .align  2
s_oam:    .space  128 * 8
s_oamn:   .space  4                     @ the next sprite (from 127 down)
s_txt:    .space  40
s_buf:    .space  32
s_k:      .space  4
s_rand:   .space  4
s_tset:   .space  4                     @ the TextSet in use
s_oam_ready: .space 4                   @ another screen's sprites (the menu's) built: copied at the blank
s_hud_on: .space  4                     @ 1: the race HUD each retrace

        .section .iwram, "ax"
@ ---- the printer ------------------------------------------------------------------
@ r0 = the size's index in the tables (15, 20, 40, 30, 11: hud.py's SIZES) for r0
size_id:
        cmp     r0, #20
        moveq   r0, #1
        bxeq    lr
        cmp     r0, #40
        moveq   r0, #2
        bxeq    lr
        cmp     r0, #30
        moveq   r0, #3
        bxeq    lr
        cmp     r0, #11
        moveq   r0, #4
        movne   r0, #0
        bx      lr

@ the glyphs in use (a TextSet: lut, glyphs, schemes, nschemes, tiles, bytes), their tiles to VRAM
        .global txt_use
txt_use:
        ldr     r1, =s_tset
        str     r0, [r1]
        ldr     r2, [r0, #20]
        ldr     r0, [r0, #16]
        ldr     r1, =0x06014000
        b       copy

@ r0 = BrTextWidth(r0 string, r1 size): each glyph's advance, a space's width (g_txt_adv,
@ g_txt_space: the game's sums for the size)
txt_width:
        stmfd   sp!, {r4-r6, lr}
        mov     r4, r0
        mov     r0, r1
        bl      size_id
        ldr     r5, =g_txt_adv
        add     r5, r5, r0, lsl #6      @ this size's advances
        ldr     r1, =g_txt_space
        ldrb    r6, [r1, r0, lsl #2]    @ and a space's width
        mov     r0, #0                  @ w
1:      ldrb    r1, [r4]
        cmp     r1, #0
        beq     9f
        cmp     r1, #0x21
        blt     2f
        cmp     r1, #0x80
        bge     2f
        cmp     r1, #'%'
        bne     4f
        ldrb    r2, [r4, #1]
        cmp     r2, #0
        beq     4f
        cmp     r2, #'%'
        addeq   r4, r4, #1
        beq     4f
        cmp     r2, #'i'
        cmpne   r2, #'n'
        addeq   r4, r4, #2
        beq     1b
        ldrb    r3, [r4, #2]
        cmp     r3, #0
        addne   r4, r4, #3
        bne     1b
4:      ldrb    r1, [r4]
        ldr     r2, =g_txt_map - 0x21
        ldrb    r1, [r2, r1]
        ldrb    r1, [r5, r1]
        add     r0, r0, r1
        b       3f
2:      add     r0, r0, r6
3:      add     r4, r4, #1
        b       1b
9:      ldmfd   sp!, {r4-r6, lr}
        bx      lr
        .ltorg

@ the colour set of the printer's current colours (T_PRIM, T_ENV, T_ALT): its index, or -1
txt_scheme:
        stmfd   sp!, {r4-r6, lr}
        ldr     r4, =s_txt
        ldr     r0, [r4, #T_PRIM]
        ldr     r1, [r4, #T_ENV]
        ldr     r2, [r4, #T_ALT]
        ldr     r3, =s_tset
        ldr     r3, [r3]
        ldr     r12, [r3, #12]
        ldr     r3, [r3, #8]
        mov     r5, #0
1:      cmp     r5, r12
        mvnge   r5, #0
        bge     2f
        ldmia   r3!, {r6, lr}
        cmp     r6, r0
        cmpeq   lr, r1
        ldr     r6, [r3], #4
        cmpeq   r6, r2
        addne   r5, r5, #1
        bne     1b
2:      str     r5, [r4, #T_SCHEME]
        ldmfd   sp!, {r4-r6, lr}
        bx      lr
        .ltorg

@ r0 (a code letter) -> r0 its primitive colour, r1 its environment colour; -1 if not one
txt_code:
        ldr     r2, =k_codes
1:      ldrb    r3, [r2]
        cmp     r3, #0
        mvneq   r0, #0
        mvneq   r1, #0
        bxeq    lr
        cmp     r3, r0
        addne   r2, r2, #12
        bne     1b
        ldr     r0, [r2, #4]
        ldr     r1, [r2, #8]
        bx      lr
        .ltorg

@ a sprite into the list: r0 attr0, r1 attr1, r2 attr2
put_sprite:
        ldr     r3, =s_oamn
        ldr     r12, [r3]
        cmp     r12, #0
        bxlt    lr
        sub     r12, r12, #1
        str     r12, [r3]
        add     r12, r12, #1
        ldr     r3, =s_oam
        add     r3, r3, r12, lsl #3
        strh    r0, [r3]
        strh    r1, [r3, #2]
        strh    r2, [r3, #4]
        bx      lr
        .ltorg

@ r0 = x * 3 / 4, r1 = y * 2 / 3: N64 pixels to the GBA's (both from r0, r1)
to_gba:
        add     r0, r0, r0, lsl #1
        mov     r0, r0, asr #2
        mov     r1, r1, lsl #1
        ldr     r2, =21846
        mul     r1, r2, r1
        mov     r1, r1, asr #16
        bx      lr
        .ltorg

@ BrTextEmitString(r0 string) at r1 x, r2 y (the printer's size and colours)
txt_emit:
        stmfd   sp!, {r4-r11, lr}
        mov     r4, r0                  @ p
        mov     r5, r1                  @ x
        ldr     r11, =s_txt
        ldr     r6, [r11, #T_SIZE]
        mov     r7, r2
        mov     r0, r6
        bl      size_id
        mov     r9, r0                  @ r9: the size's index
        ldr     r1, =g_txt_space
        add     r1, r1, r0, lsl #2
        ldrb    r10, [r1, #1]           @ r10: a space's advance
        ldrb    r0, [r1, #2]
        sub     r7, r7, r0              @ y -= size * 30 / 40
        ldr     r0, [r11, #T_CUSTOM]    @ the starting colours
        cmp     r0, #0
        ldrne   r0, [r11, #T_CPRIM]
        ldrne   r1, [r11, #T_CENV]
        bne     1f
        ldr     r0, [r11, #T_HIGHLIGHT]
        cmp     r0, #0
        ldrne   r0, =0xFFFF7F
        ldrne   r1, =0xFF7F00
        ldreq   r0, =0xE6E600
        ldreq   r1, =0xC80000
1:      str     r0, [r11, #T_PRIM]
        str     r1, [r11, #T_ENV]
        bl      txt_scheme
.Lchar:
        ldrb    r0, [r4]
        cmp     r0, #0
        beq     .Ldone
        cmp     r0, #' '
        beq     .Lspace
        cmp     r0, #'%'
        bne     .Lglyph
        ldrb    r1, [r4, #1]
        cmp     r1, #0
        beq     .Lglyph
        cmp     r1, #'%'
        addeq   r4, r4, #1
        moveq   r0, r1
        beq     .Lglyph
        cmp     r1, #'i'
        cmpne   r1, #'n'
        addeq   r4, r4, #2
        beq     .Lchar
        ldrb    r2, [r4, #2]
        cmp     r2, #0
        beq     .Lglyph
        mov     r0, r1                  @ a colour code: the primitive's, then the environment's
        bl      txt_code
        cmn     r0, #1
        strne   r0, [r11, #T_PRIM]
        ldrb    r0, [r4, #2]
        bl      txt_code
        cmn     r1, #1
        strne   r1, [r11, #T_ENV]
        bl      txt_scheme
        add     r4, r4, #3
        b       .Lchar
.Lglyph:
        cmp     r0, #0x21
        blt     .Lnext
        cmp     r0, #0x80
        bge     .Lnext
        ldr     r1, =g_txt_map - 0x21
        ldrb    r8, [r1, r0]            @ g
        ldr     r0, [r11, #T_SCHEME]    @ its sprite, if the race draws it
        cmp     r0, #0
        blt     2f
        add     r0, r0, r0, lsl #2      @ (5 sizes a colour set)
        add     r0, r0, r9
        add     r0, r8, r0, lsl #6
        ldr     r3, =s_tset
        ldr     r3, [r3]
        ldr     r1, [r3]
        ldrb    r0, [r1, r0]
        cmp     r0, #0
        beq     2f
        ldr     r1, [r3, #4]
        sub     r1, r1, #8
        add     r3, r1, r0, lsl #3      @ its HudGlyph
        stmfd   sp!, {r3}
        mov     r0, r5
        mov     r1, r7
        bl      to_gba
        ldmfd   sp!, {r3}
        and     r1, r1, #0xFF           @ attr0: y, 4-bit, square
        ldrb    r12, [r3, #G_SW]
        mov     lr, r0, lsl #23
        mov     lr, lr, lsr #23         @ x
        cmp     r12, #32
        orreq   lr, lr, #2 << 14        @ 32x32
        orrne   lr, lr, #1 << 14        @ 16x16
        ldrh    r2, [r3, #G_TILE]
        add     r2, r2, #TILE0
        ldrb    r12, [r3, #G_SCHEME]
        orr     r2, r2, r12, lsl #12    @ its palette bank
        mov     r0, r1
        mov     r1, lr
        bl      put_sprite
2:      ldr     r1, =g_txt_adv          @ x += (w - pad) * size / cell
        add     r1, r1, r9, lsl #6
        ldrb    r0, [r1, r8]
        add     r5, r5, r0
        b       .Lnext
.Lspace:
        add     r5, r5, r10             @ x += size * 12 / 40 + 1
.Lnext:
        add     r4, r4, #1
        b       .Lchar
.Ldone:
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

@ BrTextPrint(r0 string, r1 x, r2 y): aligned as the printer is set
txt_print:
        stmfd   sp!, {r4-r6, lr}
        mov     r4, r0
        mov     r5, r1
        mov     r6, r2
        ldr     r3, =s_txt
        ldr     r0, [r3, #T_ALIGN]
        cmp     r0, #0
        beq     1f
        mov     r0, r4
        ldr     r1, [r3, #T_SIZE]
        bl      txt_width
        ldr     r3, =s_txt
        ldr     r1, [r3, #T_ALIGN]
        cmp     r1, #2
        moveq   r0, r0, asr #1
        sub     r5, r5, r0
1:      mov     r0, r4
        mov     r1, r5
        mov     r2, r6
        bl      txt_emit
        ldmfd   sp!, {r4-r6, lr}
        bx      lr
        .ltorg

@ ---- strings -------------------------------------------------------------------------
@ r0 = the end of the decimal of r1 written at r0 (r1 >= 0)
put_dec:
        stmfd   sp!, {r4-r6, lr}
        mov     r4, r0
        mov     r5, r1
        sub     sp, sp, #12
        mov     r6, #0
        ldr     r2, =0xCCCD             @ / 10 as * 0xCCCD >> 19 (exact below 81920)
1:      mul     r0, r5, r2
        mov     r0, r0, lsr #19
        sub     r1, r5, r0, lsl #3
        sub     r1, r1, r0, lsl #1
        add     r1, r1, #'0'
        strb    r1, [sp, r6]
        add     r6, r6, #1
        movs    r5, r0
        bne     1b
2:      subs    r6, r6, #1
        ldrb    r1, [sp, r6]
        strb    r1, [r4], #1
        bne     2b
        mov     r0, #0
        strb    r0, [r4]
        mov     r0, r4
        add     sp, sp, #12
        ldmfd   sp!, {r4-r6, lr}
        bx      lr

@ r0 = the end of r1 as two digits (%02d) at r0
put_02:
        stmfd   sp!, {r4, r5, lr}
        mov     r4, r0
        mov     r5, r1
        ldr     r0, =0xCCCD
        mul     r0, r1, r0
        mov     r0, r0, lsr #19
        add     r1, r0, #'0'
        strb    r1, [r4], #1
        sub     r1, r5, r0, lsl #3
        sub     r1, r1, r0, lsl #1
        add     r1, r1, #'0'
        strb    r1, [r4], #1
        mov     r0, #0
        strb    r0, [r4]
        mov     r0, r4
        ldmfd   sp!, {r4, r5, lr}
        bx      lr

@ r0 = the end of string r1 copied to r0
put_str:
1:      ldrb    r2, [r1], #1
        strb    r2, [r0], #1
        cmp     r2, #0
        bne     1b
        sub     r0, r0, #1
        bx      lr

@ BrHudTimeDraw(r0 label, r1 the time (HudState's minutes), r2 x, r3 y): the time 15 below its label
time_draw:
        stmfd   sp!, {r4-r7, lr}
        mov     r4, r0
        mov     r5, r1
        mov     r6, r2
        mov     r7, r3
        ldr     r0, =s_buf
        ldr     r1, =k_ww
        bl      put_str
        ldrsh   r1, [r5]
        bl      put_dec
        mov     r1, #'\''
        strb    r1, [r0], #1
        ldrsh   r1, [r5, #2]
        bl      put_02
        mov     r1, #'"'
        strb    r1, [r0], #1
        ldrsh   r1, [r5, #4]
        bl      put_02
        ldr     r0, =s_buf
        mov     r1, r6
        add     r2, r7, #15
        bl      txt_print
        mov     r0, r4
        mov     r1, r6
        mov     r2, r7
        bl      txt_print
        ldmfd   sp!, {r4-r7, lr}
        bx      lr
        .ltorg

@ ---- the HUD ---------------------------------------------------------------------------
hud_init:
        stmfd   sp!, {r4, lr}
        ldr     r0, =g_hud_tset         @ the sprites' tiles and palette
        bl      txt_use
        ldr     r0, =g_dial_face
        ldr     r1, =0x06014000 + FACE
        mov     r2, #64 * 64
        bl      copy
        ldr     r0, =g_dial_lamps
        ldr     r1, =0x06014000 + LAMPS
        ldr     r2, =g_dial_lamp_frames
        ldr     r2, [r2]
        mov     r2, r2, lsl #8
        bl      copy
        ldr     r0, =g_needle
        ldr     r1, =0x06014000 + NEEDLE
        mov     r2, #64 * 64 / 2
        bl      copy
        ldr     r0, =g_hud_pal
        ldr     r1, =0x05000200
        mov     r2, #512
        bl      copy
        ldr     r0, =g_rand_seed
        ldr     r0, [r0]
        ldr     r1, =s_rand
        str     r0, [r1]
        ldmfd   sp!, {r4, lr}
        bx      lr

@ r2 bytes (a multiple of 4) from r0 to r1
copy:
1:      ldr     r3, [r0], #4
        str     r3, [r1], #4
        subs    r2, r2, #4
        bgt     1b
        bx      lr
        .ltorg

hud_vblank:
        stmfd   sp!, {r4-r11, lr}
        ldr     r0, =s_hud_on           @ another screen: its sprites, when built
        ldr     r0, [r0]
        cmp     r0, #0
        bne     7f
        ldr     r4, =s_oam_ready
        ldr     r0, [r4]
        cmp     r0, #0
        beq     .Lhud_done
        ldr     r0, =s_oam
        mov     r1, #0x07000000
        mov     r2, #128 * 8
        bl      copy
        mov     r0, #0
        str     r0, [r4]
        b       .Lhud_done
7:
        ldr     r0, =s_k                @ this retrace's state
        ldr     r1, [r0]
        add     r2, r1, #1
        ldr     r3, =g_hud_frames
        ldr     r3, [r3]
        cmp     r2, r3
        movge   r2, #0
        str     r2, [r0]
        tst     r1, #1                  @ the game draws its HUD every other retrace: so here
        bne     .Lhud_done
        ldr     r4, =g_hud_live         @ the live race's (race.c), else the recorded one
        ldr     r4, [r4]
        cmp     r4, #0
        bne     8f
        ldr     r4, =g_hud_state
        mov     r2, #H_SIZE
        mla     r4, r1, r2, r4          @ r4: the HudState
8:
        ldr     r0, =s_oamn
        mov     r1, #127
        str     r1, [r0]
        ldr     r5, =g_dial             @ BrHudDialDraw: the face, the lamps, the needle
        ldmia   r5, {r6-r11}            @ x y w h lampX lampY
        mov     r0, r6
        mov     r1, r7
        bl      to_gba
        and     r1, r1, #0xFF
        orr     r1, r1, #1 << 13        @ 8-bit
        mov     r0, r0, lsl #23
        mov     r0, r0, lsr #23
        orr     r0, r0, #3 << 14        @ 64x64
        mov     r2, r1
        mov     r1, r0
        mov     r0, r2
        ldr     r2, =TILE0 + FACE / 32
        bl      put_sprite
        add     r0, r6, r10
        add     r1, r7, r11
        bl      to_gba
        and     r1, r1, #0xFF
        orr     r1, r1, #1 << 13
        mov     r0, r0, lsl #23
        mov     r0, r0, lsr #23
        orr     r0, r0, #1 << 14        @ 16x16
        ldrsh   r2, [r4, #22]           @ the lamp frame
        ldr     r3, =g_dial_lamp_frames
        ldr     r3, [r3]
        cmp     r2, r3
        subge   r2, r3, #1
        mov     r2, r2, lsl #3          @ (8 units a frame: 16x16 at 8 bits)
        add     r2, r2, #TILE0 + LAMPS / 32
        mov     r3, r1
        mov     r1, r0
        mov     r0, r3
        bl      put_sprite
        ldrsh   r0, [r4, #24]           @ the needle: rev = a shake (or 64 held) + the revs
        tst     r0, #2
        movne   r8, #64
        bne     1f
        ldr     r1, =s_rand             @ BrRandStep
        ldr     r2, [r1]
        ldr     r3, =16807
        mul     r2, r3, r2
        bic     r2, r2, #0xF8000000
        str     r2, [r1]
        and     r8, r2, #0x7F
1:      ldrsh   r0, [r4, #20]
        add     r8, r8, r0              @ rev
        ldr     r9, [r5, #32]           @ a = rest, less rev * (rest - max) / 8000 when rev > 0
        cmp     r8, #0
        ble     2f
        ldr     r0, [r5, #36]
        sub     r0, r9, r0
        mul     r0, r8, r0
        ldr     r1, =536871             @ 2^32 / 8000
        smull   r2, r3, r0, r1
        sub     r9, r9, r3
2:      ldr     r1, =2608               @ the angle (Q12 radians) in 1024ths of a turn: 1024 / 2pi / 4096 in Q16
        mul     r0, r9, r1
        mov     r0, r0, asr #16
        mov     r0, r0, lsl #22
        mov     r0, r0, lsr #22
        ldr     r1, =g_sin1024
        mov     r2, r0, lsl #1
        ldrsh   r10, [r1, r2]           @ sin
        add     r2, r0, #256
        mov     r2, r2, lsl #22
        mov     r2, r2, lsr #21
        ldrsh   r11, [r1, r2]           @ cos
        ldr     r3, =s_oam              @ its matrix (group 0): u = cos dx - sin dy, v = sin dx + cos dy,
        ldr     r2, =5461               @ dx = gx / 0.75, dy = gy / (2/3); 8.8: pa = cos / 12, pd = cos 3 / 32
        mul     r0, r11, r2
        mov     r0, r0, asr #16
        strh    r0, [r3, #6]            @ pa
        mul     r0, r10, r2
        mov     r0, r0, asr #16
        strh    r0, [r3, #22]           @ pc
        add     r0, r10, r10, lsl #1
        mov     r0, r0, asr #5
        rsb     r0, r0, #0
        strh    r0, [r3, #14]           @ pb
        add     r0, r11, r11, lsl #1
        mov     r0, r0, asr #5
        strh    r0, [r3, #30]           @ pd
        ldr     r0, [r5]                @ its centre, the 128-pixel double-size box around it
        ldr     r1, [r5, #24]
        add     r0, r0, r1
        ldr     r1, [r5, #4]
        ldr     r2, [r5, #28]
        add     r1, r1, r2
        bl      to_gba
        sub     r0, r0, #64
        sub     r1, r1, #64
        and     r1, r1, #0xFF
        orr     r1, r1, #3 << 8         @ affine, double size, 4-bit
        mov     r0, r0, lsl #23
        mov     r0, r0, lsr #23
        orr     r0, r0, #3 << 14        @ 64x64, group 0
        ldr     r2, =g_needle_bank
        ldr     r2, [r2]
        mov     r2, r2, lsl #12
        add     r2, r2, #TILE0 + NEEDLE / 32
        mov     r3, r1
        mov     r1, r0
        mov     r0, r3
        bl      put_sprite
        ldr     r11, =s_txt             @ BrHudTimesDraw (race modes 0, 2; one player)
        ldr     r10, =g_view
        mov     r0, #0
        str     r0, [r11, #T_HIGHLIGHT]
        mov     r0, #1
        str     r0, [r11, #T_ALIGN]
        mov     r0, #15
        str     r0, [r11, #T_SIZE]
        ldr     r7, [r10, #4]
        add     r7, r7, #20             @ y
        ldr     r0, =k_total
        mov     r1, r4
        mov     r2, #296
        mov     r3, r7
        bl      time_draw
        ldrsh   r0, [r4, #12]           @ laps >= laps in the race: the best lap
        ldrsh   r1, [r4, #14]
        cmp     r0, r1
        ldrge   r0, =k_best
        ldrlt   r0, =k_lap
        add     r1, r4, #6
        mov     r2, #296
        add     r3, r7, #30
        bl      time_draw
        ldr     r6, [r10]               @ BrHudLapDraw: x = view x + 16
        add     r6, r6, #16
        ldrsh   r0, [r4, #12]
        ldrsh   r1, [r4, #14]
        cmp     r0, r1
        bge     3f
        mov     r0, #0
        str     r0, [r11, #T_HIGHLIGHT]
        str     r0, [r11, #T_ALIGN]
        mov     r0, #15
        str     r0, [r11, #T_SIZE]
        ldr     r0, =s_buf              @ "%y1LAP n/m"
        ldr     r1, =k_lapof
        bl      put_str
        ldrsh   r1, [r4, #12]
        add     r1, r1, #1
        bl      put_dec
        mov     r1, #'/'
        strb    r1, [r0], #1
        ldrsh   r1, [r4, #14]
        bl      put_dec
        ldr     r0, =s_buf
        mov     r1, r6
        ldr     r2, [r10, #4]
        add     r2, r2, #20
        bl      txt_print
3:      ldr     r7, [r10, #4]           @ the position: y = view bottom - 12, x - 2
        ldr     r0, [r10, #12]
        add     r7, r7, r0
        sub     r7, r7, #12
        sub     r6, r6, #2
        mov     r0, #1
        str     r0, [r11, #T_ALT]
        str     r0, [r11, #T_CUSTOM]
        mov     r0, #0
        str     r0, [r11, #T_ALIGN]
        ldr     r0, =0xFFF07D           @ BrTextSetColours(0xff, 0xf0, 0x7d, 0xff, 0x78, 0)
        str     r0, [r11, #T_CENV]
        ldr     r0, =0xFF7800
        str     r0, [r11, #T_CPRIM]
        ldr     r0, =s_buf
        ldrsh   r1, [r4, #16]
        add     r1, r1, #1
        bl      put_dec
        ldrsh   r0, [r4, #16]           @ its suffix and nudge
        cmp     r0, #0
        ldreq   r8, =k_st
        mvneq   r9, #2
        beq     4f
        cmp     r0, #1
        ldreq   r8, =k_nd
        moveq   r9, #1
        beq     4f
        cmp     r0, #2
        ldreq   r8, =k_rd
        moveq   r9, #0
        ldrne   r8, =k_th
        movne   r9, #1
4:      mov     r0, #40
        str     r0, [r11, #T_SIZE]
        ldr     r0, =s_buf
        mov     r1, #40
        bl      txt_width
        mov     r5, r0                  @ w
        ldr     r0, =s_buf
        sub     r1, r6, #1
        sub     r2, r7, #1
        bl      txt_print
        mov     r0, #20
        str     r0, [r11, #T_SIZE]
        mov     r0, r8
        add     r1, r6, r9
        add     r1, r1, r5
        add     r1, r1, #3
        sub     r2, r7, #15
        bl      txt_print
        mov     r0, #0
        str     r0, [r11, #T_ALT]
        str     r0, [r11, #T_HIGHLIGHT] @ BrHudDraw: the speed
        mov     r0, #1
        str     r0, [r11, #T_ALIGN]
        ldr     r0, =s_buf
        ldr     r1, =k_yw
        bl      put_str
        ldrsh   r1, [r4, #18]
        bl      put_dec
        ldr     r7, [r10, #4]           @ y = view bottom - 4 - the panel
        ldr     r0, [r10, #12]
        add     r7, r7, r0
        sub     r7, r7, #4
        ldr     r0, [r10, #16]
        sub     r7, r7, r0
        ldr     r6, =266
        ldrsh   r0, [r4, #24]
        tst     r0, #1
        subeq   r6, r6, #3              @ kph sits 3 left
        mov     r0, #20
        str     r0, [r11, #T_SIZE]
        ldr     r0, =s_buf
        mov     r1, r6
        sub     r2, r7, #3
        bl      txt_print
        mov     r0, #15
        str     r0, [r11, #T_SIZE]
        mov     r0, #0
        str     r0, [r11, #T_ALIGN]
        ldrsh   r0, [r4, #24]
        tst     r0, #1
        ldrne   r0, =k_mph
        ldreq   r0, =k_kph
        mov     r1, r6
        sub     r2, r7, #3
        bl      txt_print
        ldr     r0, =s_oamn             @ the rest off, and the list to OAM
        ldr     r1, [r0]
        ldr     r2, =s_oam
        mov     r3, #2 << 8
5:      cmp     r1, #0
        blt     6f
        add     r12, r2, r1, lsl #3
        strh    r3, [r12]
        sub     r1, r1, #1
        b       5b
6:      ldr     r0, =s_oam
        mov     r1, #0x07000000
        mov     r2, #128 * 8
        bl      copy
.Lhud_done:
        ldmfd   sp!, {r4-r11, lr}
        bx      lr
        .ltorg

        .section .rodata
k_ww:    .asciz  "%ww"
k_total: .asciz  "%15TOTAL TIME"
k_lap:   .asciz  "%15LAP TIME"
k_best:  .asciz  "%15BEST LAP"
k_lapof: .asciz  "%y1LAP "
k_st:    .asciz  "st"
k_nd:    .asciz  "nd"
k_rd:    .asciz  "rd"
k_th:    .asciz  "th"
k_yw:    .asciz  "%yw"
k_mph:   .asciz  "%wwmph"
k_kph:   .asciz  "%wwkph"
        .align  2
@ the printer's colour codes (BrTextEmitString): a letter, the primitive, the environment
k_codes:
        .word   'r', 0xBE0000, 0xC80000
        .word   'o', 0xCD5F00, 0xCD5F00
        .word   'O', 0xFF7800, 0xFF7800
        .word   'y', 0xFFF500, 0xD2BE00
        .word   'Y', 0xFFFA80, 0xD2C869
        .word   'g', 0x009600, 0x009600
        .word   'b', 0x0000C8, 0x0000C8
        .word   'p', 0xC800C8, 0xC800C8
        .word   '1', 0xFFFFFF, 0xFFFFFF
        .word   'w', 0xFFFFFF, 0xFFFFFF
        .word   '5', 0x808080, 0x808080
        .word   '0', 0x000000, 0x000000
        .word   0
