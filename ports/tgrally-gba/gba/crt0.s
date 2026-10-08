@ crt0.s -- the cartridge's entry: header space, stacks, copy the IWRAM and
@ EWRAM sections from ROM, clear the BSS, call main.
        .section .crt0, "ax"
        .arm
        .global _start
_start:
        b       boot
        .space  0xC0 - 4                @ the cartridge header (gbalink.py fills it)
boot:
        mov     r0, #0x12               @ IRQ mode stack
        msr     cpsr_c, r0
        ldr     sp, =0x03007FA0
        mov     r0, #0x1F               @ system mode stack (the IRQ's above it: the mixer runs there)
        msr     cpsr_c, r0
        ldr     sp, =0x03007C00
        ldr     r0, =__iwram_lma
        ldr     r1, =__iwram_start
        ldr     r2, =__iwram_end
        bl      copy
        ldr     r0, =__data_lma
        ldr     r1, =__data_start
        ldr     r2, =__data_end
        bl      copy
        mov     r3, #0
        ldr     r1, =__iwram_bss_start
        ldr     r2, =__iwram_bss_end
        bl      zero
        ldr     r1, =__bss_start
        ldr     r2, =__bss_end
        bl      zero
        ldr     r0, =main
        mov     lr, pc
        bx      r0
hang:   b       hang
copy:   cmp     r1, r2
        ldrlo   r3, [r0], #4
        strlo   r3, [r1], #4
        blo     copy
        bx      lr
zero:   cmp     r1, r2
        strlo   r3, [r1], #4
        blo     zero
        bx      lr
        .pool
