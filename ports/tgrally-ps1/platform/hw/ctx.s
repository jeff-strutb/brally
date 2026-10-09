# ctx.s: switching between the game's threads.  The N64's threads ran one at
# a time, switched only inside OS calls (os/thread.c keeps that model), so a
# switch is a plain call: save the callee-saved registers on this side,
# restore the other side's, return there.
#
#   void ps1_ctx_switch(Ps1Ctx *from, Ps1Ctx *to)
#   Ps1Ctx: s0-s7, fp, gp, sp, ra (12 words)
    .set noreorder
    .text
    .global ps1_ctx_switch
    .type ps1_ctx_switch, @function
ps1_ctx_switch:
    sw      $s0,  0($a0)
    sw      $s1,  4($a0)
    sw      $s2,  8($a0)
    sw      $s3, 12($a0)
    sw      $s4, 16($a0)
    sw      $s5, 20($a0)
    sw      $s6, 24($a0)
    sw      $s7, 28($a0)
    sw      $fp, 32($a0)
    sw      $gp, 36($a0)
    sw      $sp, 40($a0)
    sw      $ra, 44($a0)
    lw      $ra, 44($a1)            # first: a load's value is not there for the next instruction
    lw      $s0,  0($a1)
    lw      $s1,  4($a1)
    lw      $s2,  8($a1)
    lw      $s3, 12($a1)
    lw      $s4, 16($a1)
    lw      $s5, 20($a1)
    lw      $s6, 24($a1)
    lw      $s7, 28($a1)
    lw      $fp, 32($a1)
    lw      $gp, 36($a1)
    lw      $sp, 40($a1)
    jr      $ra
    nop

# a new thread's first switch lands here: s0 the function, s1 its argument
    .global ps1_ctx_entry
    .type ps1_ctx_entry, @function
    .weak   host_thread_exit
ps1_ctx_entry:
    jalr    $s0
    move    $a0, $s1
    jal     host_thread_exit
    nop
1:  b       1b
    nop

# the instruction cache, after code was written (the exception vector):
# isolate the cache and invalidate every line, from uncached code
    .global ps1_flush_icache
    .type ps1_flush_icache, @function
ps1_flush_icache:
    la      $t0, 1f
    lui     $t1, 0xA000
    or      $t0, $t0, $t1
    jr      $t0
    nop
1:  mfc0    $t3, $12
    nop
    li      $t1, 0x00010000             # SR.IsC
    mtc0    $t1, $12
    nop
    li      $t0, 0xFFFE0130             # the cache control: tag test, I-cache
    li      $t1, 0x00000804
    sw      $t1, 0($t0)
    li      $t2, 0
    li      $t4, 0x1000
2:  sw      $zero, 0($t2)
    addiu   $t2, $t2, 16
    bne     $t2, $t4, 2b
    nop
    li      $t1, 0x0001E988             # the cache control's normal value
    sw      $t1, 0($t0)
    mtc0    $t3, $12
    nop
    jr      $ra
    nop
