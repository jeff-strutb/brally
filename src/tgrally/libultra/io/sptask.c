/* n64-cflags: -O1 */
/* sptask.c -- libultra's RSP task loading (io/sptask.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define _osVirtualToPhysical(ptr) \
	if (ptr != NULL) { \
		ptr = (void *)osVirtualToPhysical(ptr); \
	}
/* -- end declarations -- */

static OSTask tmp_task;

/* WHAT IT DOES: Copy a task into the static task with every pointer made
 * physical. */
/* @implements 0x80264B60 tgr _VirtualToPhysicalTask */
OSTask *_VirtualToPhysicalTask(OSTask *intp)
{
	OSTask *tp;

	tp = &tmp_task;
	bcopy(intp, tp, sizeof(OSTask));

	_osVirtualToPhysical(tp->t.ucode);
	_osVirtualToPhysical(tp->t.ucode_data);
	_osVirtualToPhysical(tp->t.dram_stack);
	_osVirtualToPhysical(tp->t.output_buff);
	_osVirtualToPhysical(tp->t.output_buff_size);
	_osVirtualToPhysical(tp->t.data_ptr);
	_osVirtualToPhysical(tp->t.yield_data_ptr);
	return tp;
}

/* WHAT IT DOES: Load a task into the RSP: the physical copy of the task
 * (its yield data in place of its data after a yield) into the top of
 * DMEM (a yielded loadable task's microcode address read back from its
 * yield buffer) and the boot microcode into IMEM, with the RSP set to break and its
 * signals cleared. */
/* @implements 0x80264C7C tgr osSpTaskLoad */
void osSpTaskLoad(OSTask *intp)
{
	OSTask *tp;

	tp = _VirtualToPhysicalTask(intp);

	if (tp->t.flags & OS_TASK_YIELDED) {
		tp->t.ucode_data = tp->t.yield_data_ptr;
		tp->t.ucode_data_size = tp->t.yield_data_size;
		intp->t.flags &= ~OS_TASK_YIELDED;
		if (tp->t.flags & OS_TASK_LOADABLE) {
			tp->t.ucode = (u64 *)IO_READ((u32)intp->t.yield_data_ptr + OS_YIELD_DATA_SIZE - 4);
		}
	}
	osWritebackDCache(tp, sizeof(OSTask));
	__osSpSetStatus(SP_CLR_YIELD | SP_CLR_YIELDED | SP_CLR_TASKDONE | SP_SET_INTR_BREAK);
	while (__osSpSetPc(SP_IMEM_START) == -1)
		;
	while (__osSpRawStartDma(OS_WRITE, (SP_IMEM_START - sizeof(*tp)), tp, sizeof(OSTask)) == -1)
		;
	while (__osSpDeviceBusy())
		;
	while (__osSpRawStartDma(OS_WRITE, SP_IMEM_START, tp->t.ucode_boot, tp->t.ucode_boot_size) == -1)
		;
}

/* WHAT IT DOES: Start the loaded task: wait for the RSP's DMA, then clear
 * its halt and single-step. */
/* @implements 0x80264E0C tgr osSpTaskStartGo */
void osSpTaskStartGo(OSTask *tp)
{
	while (__osSpDeviceBusy())
		;
	__osSpSetStatus(SP_SET_INTR_BREAK | SP_CLR_SSTEP | SP_CLR_BROKE | SP_CLR_HALT);
}
