/* bzero.s -- libultra's memory clear (libc/bzero.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Clear len bytes: the unaligned head with one swl, then 32
 * bytes at a time, then words, then bytes. */
/* @implements 0x8026BBA0 tgr bzero */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl bzero
.ent bzero
bzero:
    slti    $1, $5, 0xc
    bnez    $1, .L8026BC1C
    negu    $3, $4
    andi    $3, $3, 3
    beqz    $3, .L8026BBC0
    subu    $5, $5, $3
    swl     $0, 0($4)
    addu    $4, $4, $3
.L8026BBC0:
    addiu   $1, $0, -0x20
    and     $7, $5, $1
    beqz    $7, .L8026BBFC
    subu    $5, $5, $7
    addu    $7, $7, $4
.L8026BBD4:
    addiu   $4, $4, 0x20
    sw      $0, -0x20($4)
    sw      $0, -0x1c($4)
    sw      $0, -0x18($4)
    sw      $0, -0x14($4)
    sw      $0, -0x10($4)
    sw      $0, -0xc($4)
    sw      $0, -8($4)
    bne     $4, $7, .L8026BBD4
    sw      $0, -4($4)
.L8026BBFC:
    addiu   $1, $0, -4
    and     $7, $5, $1
    beqz    $7, .L8026BC1C
    subu    $5, $5, $7
    addu    $7, $7, $4
.L8026BC10:
    addiu   $4, $4, 4
    bne     $4, $7, .L8026BC10
    sw      $0, -4($4)
.L8026BC1C:
    blez    $5, .L8026BC34
    nop
    addu    $5, $5, $4
.L8026BC28:
    addiu   $4, $4, 1
    bne     $4, $5, .L8026BC28
    sb      $0, -1($4)
.L8026BC34:
    jr      $31
    nop
.end bzero
