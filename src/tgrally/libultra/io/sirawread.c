/* n64-cflags: -O1 */
/* sirawread.c -- libultra's raw PIF word read (io/sirawread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Read a word over the SI once it is idle; -1 while busy. */
/* @implements 0x8026BA20 tgr __osSiRawReadIo */
s32 __osSiRawReadIo(u32 devAddr, u32 *data)
{
	if (__osSiDeviceBusy())
		return -1;
	*data = IO_READ(devAddr);
	return 0;
}
