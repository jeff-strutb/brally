/* br_musiccmd.c -- audio.  See br_musiccmd.h. */
#include "br_musiccmd.h"

#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern uint32_t g_0940A4;
void BrExt_10002660(void *);
void BrExt_100025F0(void *);
void BrExt_10072B30(void *, int, int);
void BrExt_10072A90(void *, int, int, int);
void BrExt_10002660(void *p) { (void)p; }
void BrExt_100025F0(void *p) { (void)p; }
void BrExt_10072B30(void *a, int b, int c) { (void)a; (void)b; (void)c; }
void BrExt_10072A90(void *a, int b, int c, int d)
{ (void)a; (void)b; (void)c; (void)d; }
#else
uint32_t g_0940A4;
void BrExt_10002660(void *p);
void BrExt_100025F0(void *p);
void BrExt_10072B30(void *a, int b, int c);
void BrExt_10072A90(void *a, int b, int c, int d);
#endif

/* WHAT IT DOES: send one command to the live music path: Windows CD
 * audio if that mode is on, otherwise the in-process EAR mixer. */
/* @implements 0x100025C0 d3d BrDispatch_100025C0 */
void BrDispatch_100025C0(void *p)
{
    if (g_0940A4 == 1)
        BrExt_10002660(p);
    else
        BrExt_100025F0(p);
}

/* WHAT IT DOES: write a value into a sound table, packing the row index
 * as 2*index. */
/* @implements 0x10072B80 d3d BrWrap_10072B80 */
void BrWrap_10072B80(void *a, int b, int c)
{
    BrExt_10072B30(a, b + b, c);
}

/* WHAT IT DOES: the same table write, with an extra "1" meaning in use. */
/* @implements 0x10072B10 d3d BrWrap_10072B10 */
void BrWrap_10072B10(void *a, int b, int c)
{
    BrExt_10072A90(a, b + b, c, 1);
}

/* WHAT IT DOES: the same table write, with the packed index forced to 1. */
/* @implements 0x10072A70 d3d BrWrap_10072A70 */
void BrWrap_10072A70(void *a, int b, int c)
{
    BrExt_10072A90(a, 1, b, c);
}
