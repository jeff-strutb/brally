/* n64-cflags: -O1 */
/* epirawread.c -- libultra's raw word read through a PI handle
 * (io/epirawread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Read a word from a device on the PI bus once the PI is
 * idle. */
/* @implements 0x8026E780 tgr __osEPiRawReadIo */
s32 __osEPiRawReadIo(OSPiHandle *pihandle, u32 devAddr, u32 *data)
{
	register u32 stat;

	WAIT_ON_IOBUSY(stat);
	*data = IO_READ(pihandle->baseAddress | devAddr);
	return 0;
}
