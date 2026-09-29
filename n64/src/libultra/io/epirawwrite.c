/* n64-cflags: -O1 */
/* epirawwrite.c -- libultra's raw word write through a PI handle
 * (io/epirawwrite.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Write a word to a device on the PI bus once the PI is
 * idle. */
/* @implements 0x8026E730 tgr __osEPiRawWriteIo */
s32 __osEPiRawWriteIo(OSPiHandle *pihandle, u32 devAddr, u32 data)
{
	register u32 stat;

	WAIT_ON_IOBUSY(stat);
	IO_WRITE(pihandle->baseAddress | devAddr, data);
	return 0;
}
