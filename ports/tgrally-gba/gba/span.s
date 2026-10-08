@ span.s -- small pieces in IWRAM, ARM: the screen fill, and the vertical blank
@ interrupt.
        .section .iwram, "ax"
        .arm

@   void fill(uint32_t *dst, uint32_t word, int n)   n words, a multiple of 8
        .global fill
fill:
        stmfd   sp!, {r4-r9}
        mov     r3, r1
        mov     r4, r1
        mov     r5, r1
        mov     r6, r1
        mov     r7, r1
        mov     r8, r1
        mov     r9, r1
        mov     r12, r1
1:      stmia   r0!, {r3-r9, r12}
        subs    r2, r2, #8
        bgt     1b
        ldmfd   sp!, {r4-r9}
        bx      lr

@ the vertical blank interrupt (through the BIOS's dispatcher, 0x03007FFC):
@ show the finished page main.c asked for in s_flip (its DISPCNT) with its wipe's window
@ (s_winh: WIN0H), then 0
        .global irq_vblank, s_winh, s_press
irq_vblank:
        ldr     r1, =s_vbl
        ldr     r2, [r1]
        add     r2, r2, #1
        str     r2, [r1]
        mov     r0, #0x04000000
        add     r3, r0, #0x130          @ the pad, every retrace: the buttons pressed since the last
        ldrh    r3, [r3]                @ one, latched in s_press until the frame takes them
        ldr     r1, =s_press
        ldr     r2, [r1, #4]            @ (last retrace's)
        str     r3, [r1, #4]
        bic     r2, r2, r3              @ up then, down now (KEYINPUT is active low)
        ldr     r3, [r1]
        orr     r3, r3, r2
        str     r3, [r1]
        ldr     r1, =s_flip
        ldr     r2, [r1]
        cmp     r2, #0
        beq     1f
        strh    r2, [r0]
        mov     r2, #0
        str     r2, [r1]
        ldr     r1, =s_winh
        ldr     r2, [r1]
        strh    r2, [r0, #0x40]
1:
        add     r3, r0, #0x200
        mov     r2, #1
        strh    r2, [r3, #2]            @ IF: the blank answered
        stmfd   sp!, {lr}
        bl      snd_vblank              @ the sound first: its buffers swap at the blank, to the sample (sound.s)
        bl      hud_vblank              @ the HUD's sprites, in the blank (hud.s)
        ldmfd   sp!, {lr}
        bx      lr
        .ltorg

        .section .iwram_bss, "aw", %nobits
        .align  2
s_winh: .space  4
s_press: .space 8                       @ the presses latched; the pad last retrace
