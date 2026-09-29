/* n64-cflags: -O1 */
/* sp.c -- libultra's RSP busy test (io/sp.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Whether the RSP's DMA or IO is busy or full. */
/* @implements 0x8026B930 tgr __osSpDeviceBusy */
int __osSpDeviceBusy(void)
{
	register u32 stat = IO_READ(SP_STATUS_REG);

	if (stat & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL | SP_STATUS_IO_FULL))
		return 1;
	return 0;
}
