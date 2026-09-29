/* writebackdcacheall.s -- libultra's whole data cache write-back
 * (os/writebackdcacheall.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Write back and invalidate the whole 8K data cache, line by
 * line through its index. */
/* @implements 0x802649C0 tgr osWritebackDCacheAll */

/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osWritebackDCacheAll
.ent osWritebackDCacheAll
osWritebackDCacheAll:
    lui     $8, 0x8000
    addiu   $10, $0, 0x2000
    addu    $9, $8, $10
    addiu   $9, $9, -0x10
.L802649D0:
    cache   1, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L802649D0
    addiu   $8, $8, 0x10
    jr      $31
    nop
.end osWritebackDCacheAll
