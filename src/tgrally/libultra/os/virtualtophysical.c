/* n64-cflags: -O1 */
/* virtualtophysical.c -- libultra's address translation
 * (os/virtualtophysical.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The physical address of a KSEG0 or KSEG1 address (the low
 * 29 bits), else whatever the TLB maps it to. */
/* @implements 0x8026B750 tgr osVirtualToPhysical */
u32 osVirtualToPhysical(void *addr)
{
	if (IS_KSEG0(addr)) {
		return K0_TO_PHYS(addr);
	} else if (IS_KSEG1(addr)) {
		return K1_TO_PHYS(addr);
	} else {
		return __osProbeTLB(addr);
	}
}
