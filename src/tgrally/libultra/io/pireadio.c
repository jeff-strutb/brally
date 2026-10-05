/* n64-cflags: -O1 */
/* pireadio.c -- libultra's cartridge word read (io/pireadio.c), the
 * version without PI access locking.
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Read one word from the cartridge bus once the PI is idle. */
/* @implements 0x802663A0 tgr osPiReadIo */
s32 osPiReadIo(u32 devAddr, u32 *data)
{
	register u32 stat;

	WAIT_ON_IOBUSY(stat);
	*data = IO_READ((u32)osRomBase | devAddr);
	return 0;
}
