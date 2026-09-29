/* interrupt.s -- libultra's interrupt enable save and restore
 * (os/interrupt.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Turn interrupts off (Status IE), returning whether they
 * were on. */
/* @implements 0x80268540 tgr __osDisableInt */

/* WHAT IT DOES: Turn interrupts back on if the saved state says they
 * were. */
/* @implements 0x80268560 tgr __osRestoreInt */

/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osDisableInt
.ent __osDisableInt
__osDisableInt:
    mfc0    $8, $12
    addiu   $1, $0, -2
    and     $9, $8, $1
    mtc0    $9, $12
    andi    $2, $8, 1
    nop
    jr      $31
    nop
.end __osDisableInt

.globl __osRestoreInt
.ent __osRestoreInt
__osRestoreInt:
    mfc0    $8, $12
    or      $8, $8, $4
    mtc0    $8, $12
    nop
    nop
    jr      $31
    nop
.end __osRestoreInt
