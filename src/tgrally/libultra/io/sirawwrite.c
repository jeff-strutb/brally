/* n64-cflags: -O1 */
/* sirawwrite.c -- libultra's raw PIF word write (io/sirawwrite.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Write a word over the SI once it is idle; -1 while busy. */
/* @implements 0x8026BA70 tgr __osSiRawWriteIo */
s32 __osSiRawWriteIo(u32 devAddr, u32 data)
{
	if (__osSiDeviceBusy())
		return -1;
	IO_WRITE(devAddr, data);
	return 0;
}
