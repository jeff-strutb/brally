/* writebackdcache.s -- libultra's data cache write-back (os/writebackdcache.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Write back and invalidate the data cache lines over a
 * buffer; 8K or more does the whole cache. */
/* @implements 0x8026B7D0 tgr osWritebackDCache */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osWritebackDCache
.ent osWritebackDCache
osWritebackDCache:
    blez    $5, .L8026B818
    nop
    addiu   $11, $0, 0x2000
    sltu    $1, $5, $11
    beqz    $1, .L8026B820
    nop
    move    $8, $4
    addu    $9, $4, $5
    sltu    $1, $8, $9
    beqz    $1, .L8026B818
    nop
    andi    $10, $8, 0xf
    addiu   $9, $9, -0x10
    subu    $8, $8, $10
.L8026B808:
    cache   0x19, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L8026B808
    addiu   $8, $8, 0x10
.L8026B818:
    jr      $31
    nop
.L8026B820:
    lui     $8, 0x8000
    addu    $9, $8, $11
    addiu   $9, $9, -0x10
.L8026B82C:
    cache   1, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L8026B82C
    addiu   $8, $8, 0x10
    jr      $31
    nop
.end osWritebackDCache
