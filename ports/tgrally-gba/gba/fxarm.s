@ fxarm.s -- the simulation's 32.32 number (sim/fx.h) on the ARM7: multiply, divide and
@ square root in IWRAM.
@
@   fx fx_mul(fx a, fx b)    (a b) >> 32 (rounded down, as the host builds)
@   fx fx_div(fx a, fx b)    (a << 32) / b without a divide: the divisor's top 32 bits, their
@                            reciprocal from a table and two Newton steps (some 30 good bits,
@                            more than a float's 24), times a; 0 divisor: +-max
@   udiv                     r0 = r0 / r1, unsigned (0 when r1 is 0), clobbers r1-r3: the
@                            same reciprocal, the quotient made exact by its remainder
@   __aeabi_memcpy4, 8       a copy between word-aligned addresses, four words at a time
@   fx fx_sqrt(fx a)         sqrt(a << 32): the top 64 bits of a << 32, normalised by an even
@                            shift, a bitwise root (32 good bits)
        .section .iwram, "ax"
        .arm
        .global fx_mul, fx_div, fx_sqrt, udiv, __aeabi_memcpy4, __aeabi_memcpy8

@ r1:r0 = |r1:r0|, r1:r0 negative: the flags' N on entry
        .macro  ABS64 lo, hi
        cmp     \hi, #0
        bge     1f
        rsbs    \lo, \lo, #0
        rsc     \hi, \hi, #0
1:
        .endm

fx_mul:
        stmfd   sp!, {r4}               @ the product's bits 32..95, two's complement (the host's
        umull   r12, r4, r0, r2         @ (a b) >> 32): unsigned al bl, its high word
        mov     r12, #0
        umlal   r4, r12, r1, r2         @ + ah bl
        umlal   r4, r12, r0, r3         @ + al bh
        mla     r12, r1, r3, r12        @ + ah bh << 32
        cmp     r1, #0                  @ the signed words: a negative ah is ah - 2^32, so less bl
        sublt   r12, r12, r2
        cmp     r3, #0                  @ and less al for a negative bh
        sublt   r12, r12, r0
        mov     r0, r4
        mov     r1, r12
        ldmfd   sp!, {r4}
        bx      lr

__aeabi_memcpy4:
__aeabi_memcpy8:
        stmfd   sp!, {r4, r5}
        subs    r2, r2, #16
        bcc     2f
1:      ldmia   r1!, {r3-r5, r12}
        stmia   r0!, {r3-r5, r12}
        subs    r2, r2, #16
        bcs     1b
2:      adds    r2, r2, #12             @ the words left
        bmi     4f
3:      ldr     r3, [r1], #4
        str     r3, [r0], #4
        subs    r2, r2, #4
        bpl     3b
4:      adds    r2, r2, #4              @ the bytes left
        beq     6f
5:      ldrb    r3, [r1], #1
        strb    r3, [r0], #1
        subs    r2, r2, #1
        bne     5b
6:      ldmfd   sp!, {r4, r5}
        bx      lr

@ \t = the leading zeros of \r (\r != 0), \r shifted left by them; clobbers lr
        .macro  CLZ32 r, t
        mov     \t, #0
        movs    lr, \r, lsr #16
        moveq   \r, \r, lsl #16
        addeq   \t, \t, #16
        movs    lr, \r, lsr #24
        moveq   \r, \r, lsl #8
        addeq   \t, \t, #8
        movs    lr, \r, lsr #28
        moveq   \r, \r, lsl #4
        addeq   \t, \t, #4
        movs    lr, \r, lsr #30
        moveq   \r, \r, lsl #2
        addeq   \t, \t, #2
        movs    lr, \r, lsr #31
        moveq   \r, \r, lsl #1
        addeq   \t, \t, #1
        .endm

fx_div:
        stmfd   sp!, {r4-r9, lr}
        eor     r8, r1, r3              @ the quotient's sign
        ABS64   r0, r1
        ABS64   r2, r3
        orrs    r4, r2, r3
        beq     .Ldiv0
        orrs    r4, r0, r1
        beq     .Ldone0
        mov     r6, #0                  @ k: the divisor's leading zeros (64 bits)
        cmp     r3, #0
        moveq   r3, r2
        moveq   r2, #0
        addeq   r6, r6, #32
        mov     r4, r3
        CLZ32   r4, r12
        add     r6, r6, r12
        rsb     r4, r12, #32
        cmp     r12, #0
        movne   r3, r3, lsl r12
        orrne   r3, r3, r2, lsr r4      @ d: the divisor's top 32 bits (bit 31 set), D = d / 2^32
        ldr     r4, =s_rcp - 128 * 4    @ r: 1 / D in Q31, from the top byte, then Newton twice:
        mov     r5, r3, lsr #24         @ r = r (2 - D r)  (multiplies only)
        ldr     r4, [r4, r5, lsl #2]
        .rept   2
        umull   r5, r7, r3, r4          @ D r, Q63
        rsbs    r5, r5, #0              @ 2 - D r, Q63 (its high word Q31)
        rsc     r7, r7, #0
        umull   r5, r9, r4, r7          @ r (2 - D r), Q62
        mov     r4, r9, lsl #1
        orr     r4, r4, r5, lsr #31
        .endr
        cmp     r4, #0
        mvneq   r4, #0                  @ (exactly 2: the largest below it)
        umull   r2, r3, r0, r4          @ |a| r: 96 bits in r9:r3:r2
        umull   r5, r9, r1, r4
        adds    r3, r3, r5
        adc     r9, r9, #0
        rsbs    r6, r6, #63             @ the quotient: |a| r >> (63 - k)
        bmi     .Lbig
        cmp     r6, #32
        bge     .Lhi
        cmp     r6, #0                  @ s < 32: (r9:r3:r2) >> s, the top word must go
        beq     .Ls0
        rsb     r7, r6, #32
        movs    r5, r9, lsr r6
        bne     .Lbig
        mov     r0, r2, lsr r6
        orr     r0, r0, r3, lsl r7
        mov     r1, r3, lsr r6
        orr     r1, r1, r9, lsl r7
        b       .Lsign
.Ls0:   cmp     r9, #0
        bne     .Lbig
        mov     r0, r2
        mov     r1, r3
        b       .Lsign
.Lhi:   sub     r6, r6, #32             @ s >= 32: (r9:r3) >> (s - 32)
        cmp     r6, #0
        moveq   r0, r3
        moveq   r1, r9
        beq     .Lsign
        rsb     r7, r6, #32
        mov     r0, r3, lsr r6
        orr     r0, r0, r9, lsl r7
        mov     r1, r9, lsr r6
.Lsign:
        cmp     r1, #0                  @ past the largest: out of range
        blt     .Lbig
        cmp     r8, #0
        bge     .Ldone
        rsbs    r0, r0, #0
        rsc     r1, r1, #0
.Ldone:
        ldmfd   sp!, {r4-r9, lr}
        bx      lr
.Lbig:
.Ldiv0:
        mvn     r0, #0                  @ out of range: the largest of the sign
        mvn     r1, #0x80000000
        cmp     r8, #0
        bge     .Ldone
        mov     r0, #0
        mov     r1, #0x80000000
        b       .Ldone
.Ldone0:
        mov     r0, #0
        mov     r1, #0
        b       .Ldone

udiv:
        cmp     r1, #0
        moveq   r0, #0
        bxeq    lr
        stmfd   sp!, {r4-r6, r12, lr}
        mov     r6, r1                  @ d
        mov     r3, r1
        CLZ32   r3, r12                 @ r3 = d << s (bit 31 set), r12 = s
        ldr     r4, =s_rcp - 128 * 4    @ r = 2^63 / (d << s), as fx_div makes it
        mov     r5, r3, lsr #24
        ldr     r4, [r4, r5, lsl #2]
        .rept   2
        umull   r5, r1, r3, r4
        rsbs    r5, r5, #0
        rsc     r1, r1, #0
        umull   r5, r2, r4, r1
        mov     r4, r2, lsl #1
        orr     r4, r4, r5, lsr #31
        .endr
        cmp     r4, #0
        mvneq   r4, #0
        umull   r5, r1, r0, r4          @ q = (n r) >> (63 - s): within a few of n / d
        rsb     r12, r12, #31
        mov     r1, r1, lsr r12
1:      umull   r2, r3, r1, r6          @ q d > n: one less
        cmp     r3, #0
        bne     2f
        cmp     r2, r0
        bls     3f
2:      sub     r1, r1, #1
        b       1b
3:      sub     r2, r0, r2              @ the remainder at least d: one more
4:      cmp     r2, r6
        subhs   r2, r2, r6
        addhs   r1, r1, #1
        bhs     4b
        mov     r0, r1
        ldmfd   sp!, {r4-r6, r12, lr}
        bx      lr
        .ltorg

@ 1 / D for D = (i + 0.5) / 256, i = 128 .. 255, in Q31 (2^40 / (2 i + 1)): fx_div's first guess
        .align  2
s_rcp:
        .word   0xFF00FF00, 0xFD08E550, 0xFB188565, 0xF92FB221, 0xF74E3FC2, 0xF57403D5, 0xF3A0D52C, 0xF1D48BCE
        .word   0xF00F00F0, 0xEE500EE5, 0xEC979118, 0xEAE56403, 0xE939651F, 0xE79372E2, 0xE5F36CB0, 0xE45932D7
        .word   0xE2C4A688, 0xE135A9C9, 0xDFAC1F74, 0xDE27EB2C, 0xDCA8F158, 0xDB2F171D, 0xD9BA4256, 0xD84A598E
        .word   0xD6DF43FC, 0xD578E97C, 0xD4173289, 0xD2BA083B, 0xD161543E, 0xD00D00D0, 0xCEBCF8BB, 0xCD712752
        .word   0xCC29786C, 0xCAE5D85F, 0xC9A633FC, 0xC86A7890, 0xC73293D7, 0xC5FE7403, 0xC4CE07B0, 0xC3A13DE6
        .word   0xC2780613, 0xC152500C, 0xC0300C03, 0xBF112A8A, 0xBDF59C91, 0xBCDD535D, 0xBBC8408C, 0xBAB65610
        .word   0xB9A7862A, 0xB89BC36C, 0xB79300B7, 0xB68D3134, 0xB58A4855, 0xB48A39D4, 0xB38CF9B0, 0xB2927C29
        .word   0xB19AB5C4, 0xB0A59B41, 0xAFB321A1, 0xAEC33E1F, 0xADD5E632, 0xACEB0F89, 0xAC02B00A, 0xAB1CBDD3
        .word   0xAA392F35, 0xA957FAB5, 0xA8791708, 0xA79C7B16, 0xA6C21DF6, 0xA5E9F6ED, 0xA513FD6B, 0xA4402910
        .word   0xA36E71A2, 0xA29ECF16, 0xA1D13985, 0xA105A932, 0xA03C1688, 0x9F747A15, 0x9EAECC8D, 0x9DEB06C9
        .word   0x9D2921C3, 0x9C69169B, 0x9BAADE8E, 0x9AEE72FC, 0x9A33CD67, 0x997AE76B, 0x98C3BAC7, 0x980E4156
        .word   0x975A750F, 0x96A85009, 0x95F7CC72, 0x9548E497, 0x949B92DD, 0x93EFD1C5, 0x93459BE6, 0x929CEBF4
        .word   0x91F5BCB8, 0x91500915, 0x90ABCC02, 0x90090090, 0x8F67A1E3, 0x8EC7AB39, 0x8E2917E0, 0x8D8BE33F
        .word   0x8CF008CF, 0x8C55841C, 0x8BBC50C8, 0x8B246A87, 0x8A8DCD1F, 0x89F87469, 0x89645C4F, 0x88D180CD
        .word   0x883FDDF0, 0x87AF6FD5, 0x872032AC, 0x869222B1, 0x86053C34, 0x85797B91, 0x84EEDD35, 0x84655D9B
        .word   0x83DCF94D, 0x8355ACE3, 0x82CF7503, 0x824A4E60, 0x81C635BC, 0x814327E3, 0x80C121B2, 0x80402010

@ sqrt(a << 32): m = a << s with s even and bit 63 or 62 set; root of m (32 bits) by the
@ bitwise method; the result shifted by 16 - s / 2
fx_sqrt:
        stmfd   sp!, {r4-r8, lr}
        cmp     r1, #0
        blt     .Lsq0
        orrs    r4, r0, r1
        beq     .Lsq0
        mov     r6, #0
        cmp     r1, #0
        moveq   r1, r0
        moveq   r0, #0
        addeq   r6, r6, #32
        mov     r4, r1
        CLZ32   r4, r12
        bic     r12, r12, #1            @ an even shift
        add     r6, r6, r12
        rsb     r4, r12, #32
        cmp     r12, #0
        movne   r1, r1, lsl r12
        orrne   r1, r1, r0, lsr r4
        movne   r0, r0, lsl r12
        mov     r2, #0                  @ the root
        mov     r3, #0                  @ the remainder (hi:lo up to 34 bits: r3 and r7)
        mov     r7, #0
        mov     r8, #32
1:      mov     r7, r7, lsl #2          @ remainder << 2 | the next two bits of m
        orr     r7, r7, r3, lsr #30
        mov     r3, r3, lsl #2
        orr     r3, r3, r1, lsr #30
        mov     r1, r1, lsl #2
        orr     r1, r1, r0, lsr #30
        mov     r0, r0, lsl #2
        mov     r4, r2, lsl #2          @ trial: root << 2 | 1 (34 bits: r5:r4)
        mov     r5, r2, lsr #30
        orr     r4, r4, #1
        subs    r12, r3, r4
        sbcs    lr, r7, r5
        movcs   r3, r12
        movcs   r7, lr
        adc     r2, r2, r2              @ root << 1 | the bit
        subs    r8, r8, #1
        bne     1b
        mov     r0, r2                  @ root of m (32 bits); a << 32 = m << (32 - s): shift by
        rsb     r6, r6, #32             @ (32 - s) / 2
        movs    r6, r6, asr #1
        bmi     2f
        rsb     r4, r6, #32
        mov     r1, r0, lsr r4
        mov     r0, r0, lsl r6
        cmp     r6, #0
        moveq   r1, #0
        b       3f
2:      rsb     r6, r6, #0
        mov     r0, r0, lsr r6
        mov     r1, #0
3:      ldmfd   sp!, {r4-r8, lr}
        bx      lr
.Lsq0:
        mov     r0, #0
        mov     r1, #0
        ldmfd   sp!, {r4-r8, lr}
        bx      lr
