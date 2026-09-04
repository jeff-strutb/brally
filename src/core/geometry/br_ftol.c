/* br_ftol.c -- float to whole number, the game's own second name for it.
 *
 * RESPONSIBILITY: geometry -- the arithmetic that moves positions around.
 * This is the truncation the span and grid code rounds its coordinates with.
 *
 * Moved here out of src/core/slice6_74.c (an address batch, not a module).
 */
#include <stdint.h>

#include "slice2_21.h"   /* the BrFtolArg prototype */

/* 0x1002B920.  Verified by disassembly rather than taken on trust: the entire
 * body is `fld dword [esp+4]` then `jmp 0x1007C8A0`, nine bytes, so this is
 * __ftol reached through a stack argument instead of the x87 stack and is
 * behaviourally identical to br_crt.h's BrFtolTrunc. Out-of-range therefore
 * gives 0, per br_crt.c and CONVENTIONS (and NOT 0x80000000, per br_crt.h's
 * mistaken comment). 12 call sites. */
/* WHAT IT DOES: turns a fractional number into a whole one by throwing away
 * the fraction. Another second name for an existing routine; a number too big
 * to fit comes back as zero. */
/* @implements 0x1002B920 d3d BrFtolArg */
int32_t BrFtolArg(float f)
{
    return (int32_t)f;
}
