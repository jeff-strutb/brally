/* n64-cflags: -O1 */
/* sirawdma.c -- libultra's PIF RAM DMA (io/sirawdma.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: DMA the 64-byte PIF RAM block to or from RDRAM (writing
 * the cache back first, or invalidating it after); -1 while the SI is
 * busy. */
/* @implements 0x802694D0 tgr __osSiRawStartDma */
s32 __osSiRawStartDma(s32 direction, void *dramAddr)
{
	if (__osSiDeviceBusy())
		return -1;

	if (direction == OS_WRITE)
		osWritebackDCache(dramAddr, 64);

	IO_WRITE(SI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));

	if (direction == OS_READ)
		IO_WRITE(SI_PIF_ADDR_RD64B_REG, PIF_RAM_START);
	else
		IO_WRITE(SI_PIF_ADDR_WR64B_REG, PIF_RAM_START);

	if (direction == OS_READ)
		osInvalDCache(dramAddr, 64);
	return 0;
}
