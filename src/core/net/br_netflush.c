/* br_netflush.c -- net.
 *
 * The periodic status broadcast: it only goes out once the number of cars in
 * play agrees with the number of players connected.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

#include "slice4_50.h"

#ifdef BR_MATCHING_BUILD
/* Orig inlines KERNEL32 IAT WaitForSingleObject / ReleaseMutex (FF 15). */
__declspec(dllimport) int __stdcall WaitForSingleObject(void *, unsigned int);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);
#endif

/* XSLICE 0x10005470 -- slice2_12.h. The original reads its two operands from
 * 0x10ACEDB0 and 0x100B36FC; that port takes them as parameters. */
#ifdef BR_MATCHING_BUILD
/* Orig reads 0x10ACEDB0 / 0x100B36FC from inside the callee -- no args. */
extern uint32_t BrEntityCountActive(void);
#else
extern uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords);
#endif
/* XSLICE 0x1000C670 -- slice2_13.h. 0xFFFF is its failure sentinel. */
extern uint32_t BrDPlayGetCurrentPlayers(void);
/* DEVIATION -- slice1_02.h. The original inlines KERNEL32
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h); that header already
 * routes the identical pattern through these two hooks. */
extern void     BrNetMutexLock(void *hMutex);
extern void     BrNetMutexUnlock(void *hMutex);

/* ==========================================================================
 * 6. Network
 * ========================================================================== */

/* 0x100053F0 */
/* WHAT IT DOES: sends a status message out to the other machines in a network
 * game, but only once every player expected has actually turned up -- it
 * compares the number of cars in play against the number of players connected
 * and stays quiet if they disagree. */
/* @implements 0x100053F0 d3d BrNetSendFlush */
void BrNetSendFlush(void)
{
    uint32_t cActive;
    uint32_t flag;

    /* Orig: WaitForSingleObject(h, INFINITE); reload h; load flag; ReleaseMutex(h). */
#ifdef BR_MATCHING_BUILD
    WaitForSingleObject(g_brH221324, 0xffffffffu);
    flag = (uint32_t)g_br22AAA8;
    ReleaseMutex(g_brH221324);
    if (flag == 0) {
        return;
    }
#else
    BrNetMutexLock(g_brH221324);
    BrNetMutexUnlock(g_brH221324);

    if (g_br22AAA8 == 0) {
        return;
    }
#endif

#ifdef BR_MATCHING_BUILD
    cActive = BrEntityCountActive();
#else
    cActive = BrEntityCountActive(g_brPACEDB0, g_br0B36FC);
#endif
    if (cActive != BrDPlayGetCurrentPlayers()) {
        return;
    }

    /* Orig pushes the ADDRESS of g_brP277B40 and of g_brPB4E2E8 (offset,
     * not the pointer those globals hold). */
#ifdef BR_MATCHING_BUILD
    BrNetSend4760(&g_brP277B40, g_br094294, g_br22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48, (char *)&g_brPB4E2E8, 3, 0);
#else
    BrNetSend4760(&g_brP277B40, g_br094294, g_br22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48, g_brPB4E2E8, 3, 0);
#endif
}
