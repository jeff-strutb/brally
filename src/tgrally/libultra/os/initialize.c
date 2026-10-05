/* n64-cflags: -O1 */
/* initialize.c -- libultra's boot-time setup (os/initialize.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
typedef struct {
	unsigned int inst1, inst2, inst3, inst4;
} __osExceptionVector;
extern __osExceptionVector __osExceptionPreamble[];
extern s32 osResetType;
extern u8 osAppNMIBuffer[];
#define OS_APP_NMI_BUFSIZE 64
#define UT_VEC 0x80000000
#define XUT_VEC 0x80000080
#define ECC_VEC 0x80000100
#define E_VEC 0x80000180
#define PIF_RAM_END 0x1FC007FF
#define SR_CU1 0x20000000
#define OS_CLOCK_RATE 62500000LL
#define VI_NTSC_CLOCK 48681812
#define VI_PAL_CLOCK 49656530
#define VI_MPAL_CLOCK 48628316
u32 __osGetSR(void);
void __osSetSR(u32 v);
u32 __osSetFpcCsr(u32 v);
s32 __osSiRawReadIo(u32 devAddr, u32 *data);
s32 __osSiRawWriteIo(u32 devAddr, u32 data);
void osInvalICache(void *vaddr, s32 nbytes);
void osMapTLBRdb(void);
s32 osPiReadIo(u32 devAddr, u32 *data);
/* -- end declarations -- */

u64 osClockRate = OS_CLOCK_RATE;
s32 osViClock = VI_NTSC_CLOCK;
u32 __osShutdown = 0;
u32 __OSGlobalIntMask = OS_IM_ALL;
u32 __osFinalrom;

/* WHAT IT DOES: Set up the CPU and system at boot: FPU on, PIF told the
 * game has started, the exception preamble copied to the four exception
 * vectors, the debugger window mapped, the CPU clock read from the
 * cartridge header (three quarters of it counted), the NMI buffer cleared
 * after a cold reset, and the VI clock for the TV standard. */
/* @implements 0x80265D90 tgr osInitialize */
void osInitialize(void)
{
	u32 pifdata;
	u32 clock = 0;

	__osFinalrom = TRUE;
	__osSetSR(__osGetSR() | SR_CU1);
	__osSetFpcCsr(FPCSR_FS | FPCSR_EV);
	while (__osSiRawReadIo(PIF_RAM_END - 3, &pifdata))
		;
	while (__osSiRawWriteIo(PIF_RAM_END - 3, pifdata | 8))
		;
	*(__osExceptionVector *)UT_VEC = *__osExceptionPreamble;
	*(__osExceptionVector *)XUT_VEC = *__osExceptionPreamble;
	*(__osExceptionVector *)ECC_VEC = *__osExceptionPreamble;
	*(__osExceptionVector *)E_VEC = *__osExceptionPreamble;
	osWritebackDCache((void *)UT_VEC, E_VEC - UT_VEC + sizeof(__osExceptionVector));
	osInvalICache((void *)UT_VEC, E_VEC - UT_VEC + sizeof(__osExceptionVector));
	osMapTLBRdb();
	osPiReadIo(4, &clock);
	clock &= ~0xf;
	if (clock != 0)
		osClockRate = clock;
	osClockRate = osClockRate * 3 / 4;
	if (osResetType == 0)
		bzero(osAppNMIBuffer, OS_APP_NMI_BUFSIZE);
	if (osTvType == OS_TV_PAL)
		osViClock = VI_PAL_CLOCK;
	else if (osTvType == OS_TV_MPAL)
		osViClock = VI_MPAL_CLOCK;
	else
		osViClock = VI_NTSC_CLOCK;
}
