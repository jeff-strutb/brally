/* invalicache.s -- libultra's instruction cache invalidate (os/invalicache.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Invalidate the instruction cache lines over a buffer; 16K
 * or more invalidates the whole cache. */
/* @implements 0x8026BAC0 tgr osInvalICache */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osInvalICache
.ent osInvalICache
osInvalICache:
    blez    $5, .L8026BB08
    nop
    addiu   $11, $0, 0x4000
    sltu    $1, $5, $11
    beqz    $1, .L8026BB10
    nop
    move    $8, $4
    addu    $9, $4, $5
    sltu    $1, $8, $9
    beqz    $1, .L8026BB08
    nop
    andi    $10, $8, 0x1f
    addiu   $9, $9, -0x20
    subu    $8, $8, $10
.L8026BAF8:
    cache   0x10, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L8026BAF8
    addiu   $8, $8, 0x20
.L8026BB08:
    jr      $31
    nop
.L8026BB10:
    lui     $8, 0x8000
    addu    $9, $8, $11
    addiu   $9, $9, -0x20
.L8026BB1C:
    cache   0, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L8026BB1C
    addiu   $8, $8, 0x20
    jr      $31
    nop
.end osInvalICache
