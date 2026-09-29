/* n64-cflags: -O1 */
/* sprawdma.c -- libultra's RSP DMA (io/sprawdma.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Start a DMA between RDRAM and the RSP's memory; -1 while
 * the RSP's DMA is busy. */
/* @implements 0x8026B8A0 tgr __osSpRawStartDma */
s32 __osSpRawStartDma(s32 direction, u32 devAddr, void *dramAddr, u32 size)
{
	if (__osSpDeviceBusy())
		return -1;

	IO_WRITE(SP_MEM_ADDR_REG, devAddr);
	IO_WRITE(SP_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));

	if (direction == OS_READ) {
		IO_WRITE(SP_WR_LEN_REG, size - 1);
	} else {
		IO_WRITE(SP_RD_LEN_REG, size - 1);
	}
	return 0;
}
