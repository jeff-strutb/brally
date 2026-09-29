/* n64-cflags: -O1 */
/* si.c -- libultra's serial interface busy test (io/si.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Whether the SI's DMA or IO read is busy. */
/* @implements 0x8026DBB0 tgr __osSiDeviceBusy */
int __osSiDeviceBusy(void)
{
	register u32 stat = IO_READ(SI_STATUS_REG);

	if (stat & (SI_STATUS_DMA_BUSY | SI_STATUS_RD_BUSY))
		return 1;
	return 0;
}
