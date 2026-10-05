/* invaldcache.s -- libultra's data cache invalidate (os/invaldcache.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Invalidate the data cache lines over a buffer (writing
 * back the partial lines at either end first); 8K or more invalidates the
 * whole cache. */
/* @implements 0x802662E0 tgr osInvalDCache */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osInvalDCache
.ent osInvalDCache
osInvalDCache:
    blez    $5, .L80266360
    nop
    addiu   $11, $0, 0x2000
    sltu    $1, $5, $11
    beqz    $1, .L80266368
    nop
    move    $8, $4
    addu    $9, $4, $5
    sltu    $1, $8, $9
    beqz    $1, .L80266360
    nop
    andi    $10, $8, 0xf
    beqz    $10, .L80266330
    addiu   $9, $9, -0x10
    subu    $8, $8, $10
    cache   0x15, 0($8)
    sltu    $1, $8, $9
    beqz    $1, .L80266360
    nop
    addiu   $8, $8, 0x10
.L80266330:
    andi    $10, $9, 0xf
    beqz    $10, .L80266350
    nop
    subu    $9, $9, $10
    cache   0x15, 0x10($9)
    sltu    $1, $9, $8
    bnez    $1, .L80266360
    nop
.L80266350:
    cache   0x11, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L80266350
    addiu   $8, $8, 0x10
.L80266360:
    jr      $31
    nop
.L80266368:
    lui     $8, 0x8000
    addu    $9, $8, $11
    addiu   $9, $9, -0x10
.L80266374:
    cache   1, 0($8)
    sltu    $1, $8, $9
    bnez    $1, .L80266374
    addiu   $8, $8, 0x10
    jr      $31
    nop
.end osInvalDCache
