/* setcompare.s -- libultra's timer compare write (os/setcompare.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Set the CPU's Compare register (the counter interrupt
 * fires when Count reaches it). */
/* @implements 0x8026E6C0 tgr __osSetCompare */

/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osSetCompare
.ent __osSetCompare
__osSetCompare:
    mtc0    $4, $11
    jr      $31
    nop
.end __osSetCompare
