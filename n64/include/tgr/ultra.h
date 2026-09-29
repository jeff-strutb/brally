/* ultra.h -- the libultra types and internals the ROM's libultra objects
 * need, gathered from libultra's own headers (os.h, osint.h, viint.h and
 * friends).
 */
#ifndef ULTRA_H
#define ULTRA_H

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed long s32;
typedef unsigned long u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#define NULL 0
#define TRUE 1
#define FALSE 0

typedef s32 OSPri;
typedef s32 OSId;
typedef union {
	struct { f32 f_odd; f32 f_even; } f;
	f64 d;
} __OSfp;

typedef struct {
	u64 at, v0, v1, a0, a1, a2, a3;
	u64 t0, t1, t2, t3, t4, t5, t6, t7;
	u64 s0, s1, s2, s3, s4, s5, s6, s7;
	u64 t8, t9, gp, sp, s8, ra;
	u64 lo, hi;
	u32 sr, pc, cause, badvaddr, rcp;
	u32 fpcsr;
	__OSfp fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
	__OSfp fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;

typedef struct OSThread_s {
	struct OSThread_s *next;
	OSPri priority;
	struct OSThread_s **queue;
	struct OSThread_s *tlnext;
	u16 state;
	u16 flags;
	OSId id;
	int fp;
	__OSThreadContext context;
} OSThread;

#define OS_STATE_STOPPED 1
#define OS_STATE_RUNNABLE 2
#define OS_STATE_RUNNING 4
#define OS_STATE_WAITING 8

typedef u32 OSEvent;
typedef void *OSMesg;

typedef struct OSMesgQueue_s {
	OSThread *mtqueue;
	OSThread *fullqueue;
	s32 validCount;
	s32 first;
	s32 msgCount;
	OSMesg *msg;
} OSMesgQueue;

#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1

#define MQ_GET_COUNT(mq) ((mq)->validCount)
#define MQ_IS_EMPTY(mq) (MQ_GET_COUNT(mq) == 0)
#define MQ_IS_FULL(mq) (MQ_GET_COUNT(mq) >= (mq)->msgCount)

typedef struct {
	OSMesgQueue *messageQueue;
	OSMesg message;
} __OSEventState;

extern OSThread *__osRunningThread;
extern OSThread *__osRunQueue;
extern OSThread *__osActiveQueue;
extern OSThread *__osFaultedThread;
extern struct __osThreadTail_s { OSThread *next; OSPri priority; } __osThreadTail;
extern __OSEventState __osEventStateTab[];

u32 __osDisableInt(void);
void __osRestoreInt(u32 im);
void __osEnqueueAndYield(OSThread **queue);
void __osEnqueueThread(OSThread **queue, OSThread *t);
OSThread *__osPopThread(OSThread **queue);
void __osDispatchThread(void);
void osStartThread(OSThread *t);
void __osDequeueThread(OSThread **queue, OSThread *t);
void __osCleanupThread(void);
typedef u32 OSIntMask;
#define OS_IM_ALL 0x003FFF01
#define SR_IMASK 0x0000ff00
#define SR_EXL 0x00000002
#define SR_IE 0x00000001
#define RCP_IMASK 0x003f0000
#define RCP_IMASKSHIFT 16
#define FPCSR_FS 0x01000000
#define FPCSR_EV 0x00000800


/* vi */
typedef struct {
	u32 ctrl;
	u32 width;
	u32 burst;
	u32 vSync;
	u32 hSync;
	u32 leap;
	u32 hStart;
	u32 xScale;
	u32 vCurrent;
} OSViCommonRegs;
typedef struct {
	u32 origin;
	u32 yScale;
	u32 vStart;
	u32 vBurst;
	u32 vIntr;
} OSViFieldRegs;
typedef struct {
	u8 type;
	OSViCommonRegs comRegs;
	OSViFieldRegs fldRegs[2];
} OSViMode;
typedef struct {
	f32 factor;
	u16 offset;
	u32 scale;
} __OSViScale;
typedef struct {
	u16 state;
	u16 retraceCount;
	void *framep;
	OSViMode *modep;
	u32 control;
	OSMesgQueue *msgq;
	OSMesg msg;
	__OSViScale x;
	__OSViScale y;
} __OSViContext;
extern __OSViContext *__osViCurr;
extern __OSViContext *__osViNext;
#define VI_STATE_MODE_UPDATED 0x01
#define VI_STATE_XSCALE_UPDATED 0x02
#define VI_STATE_YSCALE_UPDATED 0x04
#define VI_STATE_CTRL_UPDATED 0x08
#define VI_STATE_BUFFER_UPDATED 0x10
#define VI_STATE_BLACK 0x20
#define VI_STATE_REPEATLINE 0x40
#define VI_STATE_FADE 0x80

/* rcp registers */
#define PHYS_TO_K1(x) ((u32)(x) | 0xA0000000)
#define IO_READ(addr) (*(volatile u32 *)PHYS_TO_K1(addr))
#define IO_WRITE(addr, data) (*(volatile u32 *)PHYS_TO_K1(addr) = (u32)(data))
#define AI_DRAM_ADDR_REG 0x04500000
#define AI_LEN_REG 0x04500004
#define AI_CONTROL_REG 0x04500008
#define AI_STATUS_REG 0x0450000C
#define AI_DACRATE_REG 0x04500010
#define AI_BITRATE_REG 0x04500014
#define AI_CONTROL_DMA_ON 0x01
#define AI_MIN_DAC_RATE 132
#define AI_MAX_BIT_RATE 16
extern s32 osViClock;
u32 osVirtualToPhysical(void *addr);
int __osAiDeviceBusy(void);

#define AI_STATUS_FIFO_FULL 0x80000000
#define K0BASE 0x80000000
#define K1BASE 0xA0000000
#define K2BASE 0xC0000000
#define IS_KSEG0(x) ((u32)(x) >= K0BASE && (u32)(x) < K1BASE)
#define IS_KSEG1(x) ((u32)(x) >= K1BASE && (u32)(x) < K2BASE)
#define K0_TO_PHYS(x) ((u32)(x) & 0x1FFFFFFF)
#define K1_TO_PHYS(x) ((u32)(x) & 0x1FFFFFFF)
u32 __osProbeTLB(void *addr);

/* sp */
typedef struct {
	u32 type;
	u32 flags;
	u64 *ucode_boot;
	u32 ucode_boot_size;
	u64 *ucode;
	u32 ucode_size;
	u64 *ucode_data;
	u32 ucode_data_size;
	u64 *dram_stack;
	u32 dram_stack_size;
	u64 *output_buff;
	u64 *output_buff_size;
	u64 *data_ptr;
	u32 data_size;
	u64 *yield_data_ptr;
	u32 yield_data_size;
} OSTask_t;
typedef union {
	OSTask_t t;
	long long force_structure_alignment;
} OSTask;
#define OS_TASK_YIELDED 0x0001
#define OS_TASK_LOADABLE 0x0004
#define OS_YIELD_DATA_SIZE 0xc00
#define SP_CLR_HALT 0x00001
#define SP_CLR_BROKE 0x00004
#define SP_CLR_SSTEP 0x00020
#define SP_SET_INTR_BREAK 0x00100
#define SP_CLR_SIG0 0x00200
#define SP_CLR_SIG1 0x00800
#define SP_CLR_SIG2 0x02000
#define SP_CLR_YIELD SP_CLR_SIG0
#define SP_CLR_YIELDED SP_CLR_SIG1
#define SP_CLR_TASKDONE SP_CLR_SIG2
#define SP_IMEM_START 0x04001000
#define OS_READ 0
#define OS_WRITE 1
void bcopy(const void *src, void *dst, int len);
void osWritebackDCache(void *vaddr, s32 nbytes);
void __osSpSetStatus(u32 data);
s32 __osSpSetPc(u32 pc);
s32 __osSpRawStartDma(s32 direction, u32 devAddr, void *dramAddr, u32 size);
int __osSpDeviceBusy(void);

/* timers */
typedef u64 OSTime;
typedef struct OSTimer_s {
	struct OSTimer_s *next;
	struct OSTimer_s *prev;
	OSTime interval;
	OSTime value;
	OSMesgQueue *mq;
	OSMesg msg;
} OSTimer;
extern OSTimer *__osTimerList;
OSTime __osInsertTimer(OSTimer *t);
void __osSetTimerIntr(OSTime tim);

/* pi */
typedef struct {
	u16 type;
	u8 pri;
	u8 status;
	OSMesgQueue *retQueue;
} OSIoMesgHdr;
typedef struct {
	OSIoMesgHdr hdr;
	void *dramAddr;
	u32 devAddr;
	u32 size;
	struct OSPiHandle_s *piHandle;
} OSIoMesg;
typedef struct {
	s32 active;
	OSThread *thread;
	OSMesgQueue *cmdQueue;
	OSMesgQueue *evtQueue;
	OSMesgQueue *acsQueue;
	s32 (*dma)(s32, u32, void *, u32);
	s32 (*edma)(struct OSPiHandle_s *, s32, u32, void *, u32);
} OSDevMgr;
s32 osEPiRawStartDma(struct OSPiHandle_s *pihandle, s32 direction, u32 devAddr, void *dramAddr, u32 size);
extern OSDevMgr __osPiDevMgr;
#define OS_MESG_TYPE_DMAREAD 11
#define OS_MESG_TYPE_DMAWRITE 12
#define OS_MESG_PRI_NORMAL 0
#define OS_MESG_PRI_HIGH 1
OSMesgQueue *osPiGetCmdQueue(void);
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void __osPiGetAccess(void);
void __osPiRelAccess(void);
s32 osPiRawReadIo(u32 devAddr, u32 *data);

#define PI_STATUS_REG 0x04600010
#define PI_STATUS_DMA_BUSY 0x01
#define PI_STATUS_IO_BUSY 0x02
#define WAIT_ON_IOBUSY(stat) \
	stat = IO_READ(PI_STATUS_REG); \
	while (stat & (PI_STATUS_IO_BUSY | PI_STATUS_DMA_BUSY)) \
		stat = IO_READ(PI_STATUS_REG);
extern u32 osRomBase;

#define SP_MEM_ADDR_REG 0x04040000
#define SP_DRAM_ADDR_REG 0x04040004
#define SP_RD_LEN_REG 0x04040008
#define SP_WR_LEN_REG 0x0404000C
#define SP_STATUS_REG 0x04040010
#define SP_PC_REG 0x04080000
#define SP_STATUS_HALT 0x0001
#define SP_STATUS_DMA_BUSY 0x0004
#define SP_STATUS_DMA_FULL 0x0008
#define SP_STATUS_IO_FULL 0x0010
u32 osGetCount(void);
void __osSetCompare(u32 v);
extern OSTime __osCurrentTime;
extern u32 __osBaseCounter;
extern u32 __osViIntrCount;
extern u32 __osTimerCounter;

/* si */
#define SI_DRAM_ADDR_REG 0x04800000
#define SI_PIF_ADDR_RD64B_REG 0x04800004
#define SI_PIF_ADDR_WR64B_REG 0x04800010
#define SI_STATUS_REG 0x04800018
#define SI_STATUS_DMA_BUSY 0x0001
#define SI_STATUS_RD_BUSY 0x0002
#define PIF_RAM_START 0x1FC007C0
#define SI_Q_BUF_LEN 1
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 msgCount);
void osInvalDCache(void *vaddr, s32 nbytes);
int __osSiDeviceBusy(void);
void __osSiCreateAccessQueue(void);

/* controllers */
typedef struct {
	u16 button;
	s8 stick_x;
	s8 stick_y;
	u8 errno;
} OSContPad;
typedef struct {
	u16 type;
	u8 status;
	u8 errno;
} OSContStatus;
typedef struct {
	u32 ramarray[15];
	u32 pifstatus;
} OSPifRam;
typedef struct {
	u8 dummy;
	u8 txsize;
	u8 rxsize;
	u8 cmd;
	u16 button;
	s8 stick_x;
	s8 stick_y;
} __OSContReadFormat;
typedef struct {
	u8 align;
	u8 txsize;
	u8 rxsize;
	u8 poll;
	u8 typeh;
	u8 typel;
	u8 status;
	u8 align1;
} __OSContRequesFormat;
#define ARRLEN(x) ((s32)(sizeof(x) / sizeof(x[0])))
#define CHNL_ERR_MASK 0xC0
#define CHNL_ERR(format) (((format).rxsize & CHNL_ERR_MASK) >> 4)
#define CONT_CMD_REQUEST_STATUS 0
#define CONT_CMD_READ_BUTTON 1
#define CONT_CMD_READ_BUTTON_TX 1
#define CONT_CMD_READ_BUTTON_RX 4
#define CONT_CMD_REQUEST_STATUS_TX 1
#define CONT_CMD_REQUEST_STATUS_RX 3
#define CONT_CMD_RESET 0xff
#define CONT_CMD_NOP 0xff
#define CONT_CMD_END 0xfe
#define CONT_CMD_EXE 1
#define MAXCONTROLLERS 4
extern OSPifRam __osContPifRam;
extern u8 __osContLastCmd;
extern u8 __osMaxControllers;
void __osSiGetAccess(void);
void __osSiRelAccess(void);
s32 __osSiRawStartDma(s32 direction, void *dramAddr);

extern u64 osClockRate;
OSTime osGetTime(void);
int osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg);

#define PI_DRAM_ADDR_REG 0x04600000
#define PI_CART_ADDR_REG 0x04600004
#define PI_RD_LEN_REG 0x04600008
#define PI_WR_LEN_REG 0x0460000C
#define PI_Q_BUF_LEN 1
#define OS_EVENT_COUNTER 3
#define OS_EVENT_VI 7
#define OS_EVENT_PI 8
#define OS_MESG_TYPE_VRETRACE 13
#define OS_MESG_TYPE_COUNTER 14
#define OS_PIM_STACKSIZE 4096
#define OS_VIM_STACKSIZE 4096
extern u32 __osPiAccessQueueEnabled;
extern OSMesgQueue __osPiAccessQueue;
void __osPiCreateAccessQueue(void);
void osSetEventMesg(OSEvent event, OSMesgQueue *mq, OSMesg msg);
OSPri osGetThreadPri(OSThread *t);
void osSetThreadPri(OSThread *t, OSPri pri);
void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri p);
void __osDevMgrMain(void *arg);
s32 osPiRawStartDma(s32 direction, u32 devAddr, void *dramAddr, u32 size);
__OSViContext *__osViGetCurrentContext(void);
void __osViInit(void);
void __osViSwapContext(void);
void __osTimerServicesInit(void);
void __osTimerInterrupt(void);

#endif
