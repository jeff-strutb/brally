/* setfpccsr.s -- libultra's FPU control write (os/setfpccsr.s).
 */
/* n64-cflags: -O1 -mips3 -32 */

/* WHAT IT DOES: Set the FPU's control and status register, returning its
 * old value. */
/* @implements 0x8026BA10 tgr __osSetFpcCsr */
/* registers by number: IDO's as has no names */
.set noreorder
.set noat

.text

.globl __osSetFpcCsr
.ent __osSetFpcCsr
__osSetFpcCsr:
    cfc1    $2, $31
    ctc1    $4, $31
    jr      $31
    nop
.end __osSetFpcCsr
