/* n64-cflags: -O1 */
/* pirawdma.c -- libultra's raw cartridge DMA (io/pirawdma.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Start a cartridge DMA once the PI is idle (a read copies
 * cartridge to RDRAM); -1 for an unknown direction. */
/* @implements 0x8026C680 tgr osPiRawStartDma */
s32 osPiRawStartDma(s32 direction, u32 devAddr, void *dramAddr, u32 size)
{
	register u32 stat;

	stat = IO_READ(PI_STATUS_REG);
	while (stat & (PI_STATUS_IO_BUSY | PI_STATUS_DMA_BUSY)) {
		stat = IO_READ(PI_STATUS_REG);
	}
	IO_WRITE(PI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
	IO_WRITE(PI_CART_ADDR_REG, K1_TO_PHYS((u32)osRomBase | devAddr));

	switch (direction) {
	case OS_READ:
		IO_WRITE(PI_WR_LEN_REG, size - 1);
		break;
	case OS_WRITE:
		IO_WRITE(PI_RD_LEN_REG, size - 1);
		break;
	default:
		return -1;
	}
	return 0;
}
