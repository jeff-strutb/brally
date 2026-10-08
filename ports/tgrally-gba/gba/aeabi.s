@ aeabi.s -- run-time helpers the compiler calls for the simulation's 64-bit numbers in
@ Thumb code (which has no 64-bit multiply or shift of its own)
@
@   __aeabi_lmul    r1:r0 * r3:r2, the low 64 bits
@   __aeabi_llsl    r1:r0 << r2
@   __aeabi_llsr    r1:r0 >> r2 (logical)
@   __aeabi_lasr    r1:r0 >> r2 (arithmetic)
        .text
        .arm
        .global __aeabi_lmul, __aeabi_llsl, __aeabi_llsr, __aeabi_lasr

__aeabi_lmul:
        mul     r3, r0, r3              @ al bh + ah bl into the high word
        mla     r3, r1, r2, r3
        umull   r0, r1, r2, r0          @ + al bl
        add     r1, r1, r3
        bx      lr

__aeabi_llsl:
        subs    r3, r2, #32             @ n >= 32: the low word moves up
        movpl   r1, r0, lsl r3
        movpl   r0, #0
        bxpl    lr
        rsb     r3, r2, #32
        mov     r1, r1, lsl r2
        orr     r1, r1, r0, lsr r3
        mov     r0, r0, lsl r2
        bx      lr

__aeabi_llsr:
        subs    r3, r2, #32
        movpl   r0, r1, lsr r3
        movpl   r1, #0
        bxpl    lr
        rsb     r3, r2, #32
        mov     r0, r0, lsr r2
        orr     r0, r0, r1, lsl r3
        mov     r1, r1, lsr r2
        bx      lr

__aeabi_lasr:
        subs    r3, r2, #32
        movpl   r0, r1, asr r3
        movpl   r1, r1, asr #31
        bxpl    lr
        rsb     r3, r2, #32
        mov     r0, r0, lsr r2
        orr     r0, r0, r1, lsl r3
        mov     r1, r1, asr r2
        bx      lr
