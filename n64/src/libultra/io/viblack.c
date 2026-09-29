/* n64-cflags: -O1 */
/* viblack.c -- libultra's screen blanking (io/viblack.c).
 */

/* -- declarations -- */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct {
	u16 state;
	u16 retraceCount;
} __OSViContext;
extern __OSViContext *__osViNext;
#define VI_STATE_BLACK 0x20
u32 __osDisableInt(void);
void __osRestoreInt(u32 im);
/* -- end declarations -- */

/* WHAT IT DOES: Blank the screen from the next retrace (or show it again),
 * with interrupts off while the next VI state is changed. */
/* @implements 0x80260AB0 tgr osViBlack */
void osViBlack(u8 active)
{
	register u32 saveMask = __osDisableInt();

	if (active)
		__osViNext->state |= VI_STATE_BLACK;
	else
		__osViNext->state &= ~VI_STATE_BLACK;
	__osRestoreInt(saveMask);
}
