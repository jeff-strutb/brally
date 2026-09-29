/* getsr.s -- libultra's Status register read (os/getsr.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Read the CPU's Status register. */
/* @implements 0x8026BA00 tgr __osGetSR */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osGetSR
.ent __osGetSR
__osGetSR:
    mfc0    $2, $12
    jr      $31
    nop
.end __osGetSR
