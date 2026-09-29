/* setsr.s -- libultra's Status register write (os/setsr.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Write the CPU's Status register. */
/* @implements 0x8026B9F0 tgr __osSetSR */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osSetSR
.ent __osSetSR
__osSetSR:
    mtc0    $4, $12
    nop
    jr      $31
    nop
.end __osSetSR
