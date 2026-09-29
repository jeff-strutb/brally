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

#endif
