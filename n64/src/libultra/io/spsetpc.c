/* n64-cflags: -O1 */
/* spsetpc.c -- libultra's RSP program counter (io/spsetpc.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Set the RSP's PC; -1 unless the RSP is halted. */
/* @implements 0x8026B860 tgr __osSpSetPc */
s32 __osSpSetPc(u32 pc)
{
	register u32 status = IO_READ(SP_STATUS_REG);

	if (!(status & SP_STATUS_HALT)) {
		return -1;
	} else {
		IO_WRITE(SP_PC_REG, pc);
		return 0;
	}
}
