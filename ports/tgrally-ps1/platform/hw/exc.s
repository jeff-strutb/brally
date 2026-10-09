# exc.s: the CPU's exception vector, taken over from the BIOS.  Interrupts
# (the VBlank, the DMA, the CD, the SPU) go to ps1_irq in C; any other
# exception is a fault: ps1_fault prints where and stops.  Not reentrant:
# the handler runs with interrupts off.
    .set noreorder
    .set noat
    .text

# copied to 0x80000080 by ps1_exc_install
    .global ps1_exc_stub
ps1_exc_stub:
    la      $k0, ps1_exc_handler
    jr      $k0
    nop
    .global ps1_exc_stub_end
ps1_exc_stub_end:

    .global ps1_exc_handler
ps1_exc_handler:
    la      $k0, exc_frame
    sw      $at,   0($k0)
    sw      $v0,   4($k0)
    sw      $v1,   8($k0)
    sw      $a0,  12($k0)
    sw      $a1,  16($k0)
    sw      $a2,  20($k0)
    sw      $a3,  24($k0)
    sw      $t0,  28($k0)
    sw      $t1,  32($k0)
    sw      $t2,  36($k0)
    sw      $t3,  40($k0)
    sw      $t4,  44($k0)
    sw      $t5,  48($k0)
    sw      $t6,  52($k0)
    sw      $t7,  56($k0)
    sw      $t8,  60($k0)
    sw      $t9,  64($k0)
    sw      $ra,  68($k0)
    sw      $sp,  72($k0)
    sw      $gp,  76($k0)
    sw      $s0,  80($k0)
    sw      $s1,  84($k0)
    sw      $s2,  88($k0)
    sw      $s3,  92($k0)
    sw      $s4,  96($k0)
    sw      $s5, 100($k0)
    sw      $s6, 104($k0)
    sw      $s7, 108($k0)
    sw      $fp, 112($k0)
    mfhi    $t0
    mflo    $t1
    sw      $t0, 116($k0)
    sw      $t1, 120($k0)
    mfc0    $t2, $14                # EPC
    nop
    sw      $t2, 124($k0)
    la      $gp, _gp
    la      $sp, exc_stack_top
    mfc0    $a0, $13                # Cause
    nop
    andi    $t0, $a0, 0x7C
    bnez    $t0, 9f                 # not an interrupt: a fault
    nop
    # a COP2 (GTE) instruction at EPC was already executed when the
    # interrupt came: step past it on the way back
    lw      $t1, 0($t2)
    li      $t3, 0x4A000000
    srl     $t4, $t1, 25
    sll     $t4, $t4, 25
    bne     $t4, $t3, 1f
    nop
    addiu   $t2, $t2, 4
    sw      $t2, 124($k0)
1:  jal     ps1_irq
    nop
    la      $k0, exc_frame
    lw      $t0, 116($k0)
    lw      $t1, 120($k0)
    mthi    $t0
    mtlo    $t1
    lw      $at,   0($k0)
    lw      $v0,   4($k0)
    lw      $v1,   8($k0)
    lw      $a0,  12($k0)
    lw      $a1,  16($k0)
    lw      $a2,  20($k0)
    lw      $a3,  24($k0)
    lw      $t0,  28($k0)
    lw      $t1,  32($k0)
    lw      $t2,  36($k0)
    lw      $t3,  40($k0)
    lw      $t4,  44($k0)
    lw      $t5,  48($k0)
    lw      $t6,  52($k0)
    lw      $t7,  56($k0)
    lw      $t8,  60($k0)
    lw      $t9,  64($k0)
    lw      $ra,  68($k0)
    lw      $sp,  72($k0)
    lw      $gp,  76($k0)
    lw      $k0, 124($k0)
    nop
    jr      $k0
    rfe

9:  mfc0    $a1, $14                # EPC
    mfc0    $a2, $8                 # BadVAddr
    la      $a3, exc_frame
    jal     ps1_fault
    nop
8:  b       8b
    nop

    .bss
    .align  3
    .global exc_frame
exc_frame:
    .space  128
    .align  3
    .space  0x1000
exc_stack_top:
