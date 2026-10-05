/* getcount.s -- libultra's CPU cycle counter read (os/getcount.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: The CPU's Count register (half the CPU clock). */
/* @implements 0x802649F0 tgr osGetCount */

/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl osGetCount
.ent osGetCount
osGetCount:
    mfc0    $2, $9
    jr      $31
    nop
.end osGetCount
