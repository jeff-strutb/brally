/* br_netflush.c -- net.
 *
 * The periodic status broadcast: it only goes out once the number of cars in
 * play agrees with the number of players connected.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdio.h>
#include <string.h>

#include "slice4_50.h"

/* Orig inlines KERNEL32 IAT WaitForSingleObject / ReleaseMutex (FF 15). */
/* 64-bit core: WaitForSingleObject is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* XSLICE 0x10005470 -- slice2_12.h. The original reads its two operands from
 * 0x10ACEDB0 and 0x100B36FC; that port takes them as parameters. */
/* Orig reads 0x10ACEDB0 / 0x100B36FC from inside the callee -- no args. */
/* 64-bit core: declared once, by its definition's header */
/* XSLICE 0x1000C670 -- slice2_13.h. 0xFFFF is its failure sentinel. */
/* BrDPlayGetCurrentPlayers: prototype in br_funcs.h */
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
    WaitForSingleObject(g_brH221324, 0xffffffffu);
    flag = (uint32_t)(*(int32_t *)&DAT_102265d8);
    ReleaseMutex(g_brH221324);
    if (flag == 0) {
        return;
    }

    cActive = BrEntityCountActive();
    if (cActive != BrDPlayGetCurrentPlayers()) {
        return;
    }

    /* Orig pushes the ADDRESS of g_brP277B40 and of g_brPB4E2E8 (offset,
     * not the pointer those globals hold). */
    BrNetSend4AD0(&g_brP277B40, g_id, (*(int32_t *)&g_226e7c),
                  (*(uint8_t (*)[3])((char *)&g_aBrRaceCar + 0x29AC)) /* BR_LP64_BYTE_VIEW */[0], (*(uint8_t (*)[3])((char *)&g_aBrRaceCar + 0x29AC)) /* BR_LP64_BYTE_VIEW */[1], (*(uint8_t (*)[3])((char *)&g_aBrRaceCar + 0x29AC)) /* BR_LP64_BYTE_VIEW */[2],
                  g_br277B48, (char *)&g_aBrCfgPlayerName, 3, 0);
}
