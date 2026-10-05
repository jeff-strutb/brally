/* probetlb.s -- libultra's TLB lookup (os/probetlb.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: The physical address the TLB maps a virtual address to
 * (using the even or odd page of the matching entry and the page mask), or
 * -1 when no valid entry maps it. */
/* @implements 0x8026E600 tgr __osProbeTLB */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osProbeTLB
.ent __osProbeTLB
__osProbeTLB:
    mfc0    $8, $10
    andi    $9, $8, 0xff
    addiu   $1, $0, -0x2000
    and     $10, $4, $1
    or      $9, $9, $10
    mtc0    $9, $10
    nop
    nop
    nop
    tlbp
    nop
    nop
    mfc0    $11, $0
    lui     $1, 0x8000
    and     $11, $11, $1
    bnez    $11, .L8026E6A8
    nop
    tlbr
    nop
    nop
    nop
    mfc0    $11, $5
    addi    $11, $11, 0x2000
    srl     $11, $11, 1
    and     $12, $11, $4
    bnez    $12, .L8026E678
    addi    $11, $11, -1
    mfc0    $2, $2
    b       .L8026E67C
    nop
.L8026E678:
    mfc0    $2, $3
.L8026E67C:
    andi    $13, $2, 2
    beqz    $13, .L8026E6A8
    nop
    lui     $1, 0x3fff
    ori     $1, $1, 0xffc0
    and     $2, $2, $1
    sll     $2, $2, 6
    and     $13, $4, $11
    add     $2, $2, $13
    b       .L8026E6AC
    nop
.L8026E6A8:
    addiu   $2, $0, -1
.L8026E6AC:
    mtc0    $8, $10
    jr      $31
    nop
.end __osProbeTLB
