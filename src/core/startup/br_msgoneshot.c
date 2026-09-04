/* br_msgoneshot.c -- startup: release the pending on-screen message one-shot.
 *
 * Filed out of the address batch slice1_02.c; the declarations below are that
 * file's, copied verbatim.  The globals are shared with 0x10005E80
 * BrNetMutexInit, which stays in the batch, so these are declarations only.
 */
#ifdef BR_MATCHING_BUILD

#include <windows.h>

extern int DAT_10226a54;
extern int DAT_10226a28;
extern int DAT_10226a38;
extern unsigned char DAT_1021c9b0;

extern int FUN_1006ba60(int a, int b);
extern unsigned char *DAT_104abb20;
extern int DAT_104abb24;

/* WHAT IT DOES: release the pending input one-shot under the message mutex
 * -- silences its sound, frees the slot, and restores the default message
 * table and level if one was armed. */
/* @implements 0x10006460 glide FUN_10006460 */
/* Mutex-guarded release of one input one-shot: stop the pending sound and
 * reset its slot, and if the arm flag is set restore the default table
 * pointer (&DAT_1021c9b0) and its 3.0f level.  Same lock idiom as BrNetReset. */
void FUN_10006460(void)
{
    WaitForSingleObject((void *)DAT_10226a54, 0xffffffff);
    if (DAT_10226a28 >= 0) {
        FUN_1006ba60(DAT_10226a28, 0x200020);
        DAT_10226a28 = -1;
    }
    if (DAT_10226a38 != 0) {
        DAT_104abb20 = &DAT_1021c9b0;
        DAT_104abb24 = 0x40400000;
        DAT_10226a38 = 0;
    }
    ReleaseMutex((void *)DAT_10226a54);
}

#endif /* BR_MATCHING_BUILD */
