# sf.s: single-precision multiply, add and subtract for the R3000 in
# assembly: two normal numbers (and zeros) the short way, rounded to nearest
# even as IEEE 754 does (the bit pattern an FPU gives); anything else
# (denormals, infinities, NaNs, a result that over- or underflows) goes on to
# libgcc's own (__gcc_*), its operands untouched.  softfloat.c's C versions
# of the same are the reference (test/sf_check on the console).
    .set noreorder
    .set noat
    .text

# ---- a * b ------------------------------------------------------------------
    .globl __mulsf3
    .type __mulsf3, @function
__mulsf3:
.ifdef SFCOUNT
    lui     $t0, %hi(sf_count)
    lw      $t1, %lo(sf_count)($t0)
    nop
    addiu   $t1, $t1, 1
    sw      $t1, %lo(sf_count)($t0)
.endif
    srl     $t0, $a0, 23
    andi    $t0, $t0, 0xFF          # ea
    srl     $t1, $a1, 23
    andi    $t1, $t1, 0xFF          # eb
    addiu   $t2, $t0, -1
    sltiu   $t2, $t2, 254
    addiu   $t3, $t1, -1
    sltiu   $t3, $t3, 254
    and     $t2, $t2, $t3
    beqz    $t2, .Lmul_special      # not both normal
    lui     $t4, 0x8000             # (the top bit)
    sll     $t5, $a0, 8
    or      $t5, $t5, $t4           # ma << 8 (1.23 at the top)
    sll     $t6, $a1, 8
    or      $t6, $t6, $t4           # mb << 8
    multu   $t5, $t6                # p = ma * mb << 16: its top bit at 63 or 62
    xor     $t9, $a0, $a1
    srl     $t9, $t9, 31
    sll     $t9, $t9, 31            # the sign
    addu    $t7, $t0, $t1
    addiu   $t7, $t7, -126          # e for a top bit at 63
    mfhi    $t2
    mflo    $t3
    bltz    $t2, 1f                 # the top bit at 63
    nop
    sll     $t2, $t2, 1             # at 62: one left
    srl     $at, $t3, 31
    or      $t2, $t2, $at
    sll     $t3, $t3, 1
    addiu   $t7, $t7, -1
1:  srl     $t8, $t2, 8             # m: 24 bits
    andi    $t2, $t2, 0xFF          # the rounding bits
    sltu    $t3, $zero, $t3         # sticky
    sll     $t2, $t2, 1
    or      $t2, $t2, $t3           # 0x100 is the half
    andi    $at, $t8, 1
    addu    $t2, $t2, $at           # odd: a half rounds up
    sltiu   $t2, $t2, 0x101         # 1: no round
    xori    $t2, $t2, 1
    addu    $t8, $t8, $t2
    srl     $at, $t8, 24            # a carry out: one right
    beqz    $at, 2f
    addiu   $at, $t7, -1            # (delay: e - 1)
    srl     $t8, $t8, 1
    addiu   $t7, $t7, 1
    addiu   $at, $t7, -1
2:  sltiu   $at, $at, 254
    beqz    $at, .Lmul_lib          # over- or underflow
    sll     $t7, $t7, 23
    sll     $t8, $t8, 9
    srl     $t8, $t8, 9             # (the hidden bit off)
    or      $v0, $t9, $t7
    jr      $ra
    or      $v0, $v0, $t8
.Lmul_special:
    # a zero times a finite number is a signed zero
    sll     $t2, $a0, 1
    bnez    $t2, 3f
    li      $t3, 255
    beq     $t1, $t3, .Lmul_lib
    nop
    b       4f
    nop
3:  sll     $t2, $a1, 1
    bnez    $t2, .Lmul_lib
    nop
    beq     $t0, $t3, .Lmul_lib
    nop
4:  xor     $v0, $a0, $a1
    srl     $v0, $v0, 31
    jr      $ra
    sll     $v0, $v0, 31
.Lmul_lib:
    j       __gcc_mulsf3
    nop
    .size __mulsf3, . - __mulsf3

# ---- a - b, a + b ---------------------------------------------------------------
    .globl __subsf3
    .type __subsf3, @function
__subsf3:
    lui     $t9, 0x8000
    b       .Ladd
    xor     $t9, $a1, $t9           # b negated (a1 kept for libgcc)
    .size __subsf3, . - __subsf3

    .globl __addsf3
    .type __addsf3, @function
__addsf3:
    move    $t9, $a1
.Ladd:
.ifdef SFCOUNT
    lui     $t0, %hi(sf_count)
    lw      $t1, %lo(sf_count)+4($t0)
    nop
    addiu   $t1, $t1, 1
    sw      $t1, %lo(sf_count)+4($t0)
.endif
    # t9 = b (negated for a subtract); a0 = a
    srl     $t0, $a0, 23
    andi    $t0, $t0, 0xFF          # ea
    srl     $t1, $t9, 23
    andi    $t1, $t1, 0xFF          # eb
    addiu   $t2, $t0, -1
    sltiu   $t2, $t2, 254
    addiu   $t3, $t1, -1
    sltiu   $t3, $t3, 254
    and     $t2, $t2, $t3
    beqz    $t2, .Ladd_special
    move    $t8, $a0                # (delay) x = a
    # |x| >= |y|
    sll     $t2, $t8, 1
    sll     $t3, $t9, 1
    sltu    $at, $t2, $t3
    beqz    $at, 1f
    move    $t7, $t9                # (delay) y = b
    move    $t8, $t9                # swap: x = b, y = a
    move    $t7, $a0
    move    $at, $t0
    move    $t0, $t1
    move    $t1, $at
1:  # x = t8 (ex t0), y = t7 (ey t1)
    lui     $t4, 0x80
    sll     $t2, $t8, 9
    srl     $t2, $t2, 9
    or      $t2, $t2, $t4
    sll     $t2, $t2, 3             # mx << 3 (guard bits)
    sll     $t3, $t7, 9
    srl     $t3, $t3, 9
    or      $t3, $t3, $t4
    sll     $t3, $t3, 3             # my << 3
    subu    $t5, $t0, $t1           # d = ex - ey
    beqz    $t5, 2f
    sltiu   $at, $t5, 27
    bnez    $at, 3f
    li      $t6, 32
    b       2f
    li      $t3, 1                  # only the sticky bit left
3:  subu    $t6, $t6, $t5
    sllv    $t6, $t3, $t6           # the bits shifted out
    srlv    $t3, $t3, $t5
    sltu    $t6, $zero, $t6
    or      $t3, $t3, $t6           # with the sticky bit
2:  xor     $at, $t8, $t7
    srl     $t6, $t8, 31
    sll     $t6, $t6, 31            # the result's sign: x's
    bltz    $at, .Ladd_diff
    nop
    addu    $t2, $t2, $t3           # m = mx + my
    srl     $at, $t2, 27
    beqz    $at, .Ladd_round
    andi    $at, $t2, 1
    srl     $t2, $t2, 1
    or      $t2, $t2, $at
    b       .Ladd_round
    addiu   $t0, $t0, 1
.Ladd_diff:
    subu    $t2, $t2, $t3           # m = mx - my
    beqz    $t2, .Ladd_zero
    srl     $at, $t2, 26
    bnez    $at, .Ladd_round
    nop
    # normalise: the top bit to 26
    srl     $at, $t2, 11
    bnez    $at, 4f
    nop
    sll     $t2, $t2, 16
    addiu   $t0, $t0, -16
4:  srl     $at, $t2, 19
    bnez    $at, 5f
    nop
    sll     $t2, $t2, 8
    addiu   $t0, $t0, -8
5:  srl     $at, $t2, 23
    bnez    $at, 6f
    nop
    sll     $t2, $t2, 4
    addiu   $t0, $t0, -4
6:  srl     $at, $t2, 25
    bnez    $at, 7f
    nop
    sll     $t2, $t2, 2
    addiu   $t0, $t0, -2
7:  srl     $at, $t2, 26
    bnez    $at, 8f
    nop
    sll     $t2, $t2, 1
    addiu   $t0, $t0, -1
8:  blez    $t0, .Ladd_lib          # an underflow
    nop
.Ladd_round:
    andi    $t3, $t2, 7             # g
    srl     $t2, $t2, 3
    andi    $at, $t2, 1
    addu    $t3, $t3, $at           # 4 rounds up only when odd
    sltiu   $t3, $t3, 5
    xori    $t3, $t3, 1
    addu    $t2, $t2, $t3
    srl     $at, $t2, 24
    beqz    $at, 9f
    nop
    srl     $t2, $t2, 1
    addiu   $t0, $t0, 1
9:  sltiu   $at, $t0, 255
    beqz    $at, .Ladd_lib          # an overflow
    sll     $t0, $t0, 23
    sll     $t2, $t2, 9
    srl     $t2, $t2, 9
    or      $v0, $t6, $t0
    jr      $ra
    or      $v0, $v0, $t2
.Ladd_zero:
    jr      $ra                     # an exact zero is +0 when rounding to nearest
    move    $v0, $zero
.Ladd_special:
    # x + 0 and 0 + y (the other normal)
    sll     $t2, $t9, 1
    bnez    $t2, 1f
    addiu   $t3, $t0, -1
    sltiu   $t3, $t3, 254
    beqz    $t3, .Ladd_lib
    nop
    jr      $ra
    move    $v0, $a0
1:  sll     $t2, $a0, 1
    bnez    $t2, .Ladd_lib
    addiu   $t3, $t1, -1
    sltiu   $t3, $t3, 254
    beqz    $t3, .Ladd_lib
    nop
    jr      $ra
    move    $v0, $t9
.Ladd_lib:
    bne     $t9, $a1, 1f            # (a subtract: b was negated)
    nop
    j       __gcc_addsf3
    nop
1:  j       __gcc_subsf3
    nop
    .size __addsf3, . - __addsf3

.ifdef SFCOUNT
    .data
    .globl sf_count
sf_count: .word 0, 0
.endif
