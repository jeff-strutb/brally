/* n64-cflags: -O1 */
/* epirawdma.c -- libultra's raw DMA through a PI handle
 * (io/epirawdma.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define PI_DOMAIN1 0
#define PI_BSD_DOM1_LAT_REG 0x04600014
#define PI_BSD_DOM1_PWD_REG 0x04600018
#define PI_BSD_DOM1_PGS_REG 0x0460001C
#define PI_BSD_DOM1_RLS_REG 0x04600020
#define PI_BSD_DOM2_LAT_REG 0x04600024
#define PI_BSD_DOM2_PWD_REG 0x04600028
#define PI_BSD_DOM2_PGS_REG 0x0460002C
#define PI_BSD_DOM2_RLS_REG 0x04600030
extern OSPiHandle *__osCurrentHandle[2];
#define UPDATE_REG(reg, var) \
	if (cHandle->var != pihandle->var) \
		IO_WRITE(reg, pihandle->var);
#define EPI_SYNC(pihandle, stat, domain) \
	WAIT_ON_IOBUSY(stat) \
	domain = pihandle->domain; \
	if (__osCurrentHandle[domain] != pihandle) { \
		OSPiHandle *cHandle = __osCurrentHandle[domain]; \
		if (domain == PI_DOMAIN1) { \
			UPDATE_REG(PI_BSD_DOM1_LAT_REG, latency); \
			UPDATE_REG(PI_BSD_DOM1_PGS_REG, pageSize); \
			UPDATE_REG(PI_BSD_DOM1_RLS_REG, relDuration); \
			UPDATE_REG(PI_BSD_DOM1_PWD_REG, pulse); \
		} else { \
			UPDATE_REG(PI_BSD_DOM2_LAT_REG, latency); \
			UPDATE_REG(PI_BSD_DOM2_PGS_REG, pageSize); \
			UPDATE_REG(PI_BSD_DOM2_RLS_REG, relDuration); \
			UPDATE_REG(PI_BSD_DOM2_PWD_REG, pulse); \
		} \
		__osCurrentHandle[domain] = pihandle; \
	}
/* -- end declarations -- */

/* WHAT IT DOES: Start a DMA with a device on the PI bus once the PI is
 * idle, first switching the bus domain's timing registers to the handle's
 * where they differ when the domain was last used through another handle;
 * -1 for an unknown direction. */
/* @implements 0x8026C760 tgr osEPiRawStartDma */
s32 osEPiRawStartDma(OSPiHandle *pihandle, s32 direction, u32 devAddr, void *dramAddr, u32 size)
{
	u32 stat;
	u32 domain;

	EPI_SYNC(pihandle, stat, domain);
	IO_WRITE(PI_DRAM_ADDR_REG, osVirtualToPhysical(dramAddr));
	IO_WRITE(PI_CART_ADDR_REG, K1_TO_PHYS(pihandle->baseAddress | devAddr));

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
