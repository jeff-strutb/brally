/* br_peerrank.c -- net.  The worker thread's peer ranking pass, 0x1006A7E0
 * (D3D 0x10071870).  Its siblings -- the wait loop 0x1006A650 and the mutex
 * bring-up -- are in br_peer.c; this one is in its own TU because it needs
 * the peer table typed as records where br_peer.c declares it as an int.
 */
/* The original is /MD: CRT and KERNEL32 calls go through the import table. */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include <windows.h>
#include <stdlib.h>

/* One entry of the record's +0xD0 table: only its leading time is read. */
/* BrPeerSub: br_coretypes.h */

/* One networking record, 0x96C bytes (br_peerslot.c pins the stride and
 * the +0x2C status word).  Only the fields this pass touches are named. */
/* BrPeerRec: br_coretypes.h */



#define BR_PEERS 16

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x117A9B88 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the quit event               */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* next message sequence number */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* 0x10AC4098 the session record */
/* BrPeerRankEnt: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x11849EB0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x11849E68: rank by peer */
/* FUN_100371f0: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */   /* 0x1006AAD0, the comparator */

#define BR_K_00077BF0   4188888.0f      /* the "no time yet" sentinel */
#define BR_K_00077BF4   10000000.0f
#define BR_K_00077BF8  (-10000000.0f)

/* WHAT IT DOES: rank the sixteen peers for message scheduling.  First pass:
 * under each peer's mutex, every peer in states 2..4 with pending traffic is
 * listed with its pending count as the score; the list is sorted and, from
 * the top, each such peer is sent its next message and moved on.  Second
 * pass: every peer is re-examined; those in states 2..4 whose current entry
 * has a time recorded are sent a message too; then every peer is scored --
 * states 5 and up by their status word, 2..4 by that entry's time, and the
 * idle ones pushed to the bottom -- the list is sorted again, and each
 * peer's position in it is written to the order table.  A signalled quit
 * event ends the thread from inside either wait. */
/* Byte-exact 2026-09-27, transcribed fresh from the asm in index form (the
 * old draft walked the rank table with raw float/int cursors).  Source facts:
 *   - the rank table is an array of {idx, score} records indexed by the
 *     running count n (and, for idle peers, by j counting down from 15);
 *   - the two passes are separate blocks, each with its own handle pair:
 *     pass 2's pair sits above pass 1's and its status word reuses that slot;
 *   - pass 2 reads the state tests, the entry time and the call/store through
 *     p = &g_aBrPeer71[i], but waits, reads st and releases by index: that
 *     anchors its walker on the +0x2C status word. */
/* @implements 0x1006A7E0 glide BrNetPeerRank */
void BrNetPeerRank(void)
{
    int i;
    int k;
    int n;
    int j;

    {
        HANDLE ah[2];

        n = 0;
        for (i = BR_PEERS - 1; i >= 0; i--) {
            ah[0] = DAT_11849e60;
            ah[1] = g_aBrPeer71[i].hMutex;
            if (WaitForMultipleObjects(2, ah, 0, INFINITE) == 0)
                ExitThread(0);
            if ((g_aBrPeer71[i].f02C & 0x3f) >= 2 && (g_aBrPeer71[i].f02C & 0x3f) < 5 &&
                g_aBrPeer71[i].f968 != 0) {
                g_aBrPeerRank[n].idx = i;
                g_aBrPeerRank[n].score = (float)g_aBrPeer71[i].f968;
                n++;
            }
            ReleaseMutex(g_aBrPeer71[i].hMutex);
        }

        if (n != 0) {
            qsort(g_aBrPeerRank, n, 8, BrSub10071B60);
            for (k = n - 1; k >= 0; k--) {
                BrPeerRec *p = &g_aBrPeer71[g_aBrPeerRank[k].idx];

                ah[0] = DAT_11849e60;
                ah[1] = p->hMutex;
                if (WaitForMultipleObjects(2, ah, 0, INFINITE) == 0)
                    ExitThread(0);
                if ((p->f02C & 0x3f) >= 2 && (p->f02C & 0x3f) < 5 && p->f968 != 0) {
                    FUN_100371f0(g_brSlot4098, p->f004, DAT_11849e58);
                    p->f02C = DAT_11849e58 + 5;
                    DAT_11849e58 = DAT_11849e58 + 1;
                }
                ReleaseMutex(p->hMutex);
            }
        }
    }
    {
        HANDLE ah[2];
        int st;
        BrPeerRec *p;

        n = 0;
        j = BR_PEERS - 1;
        for (i = BR_PEERS - 1; i >= 0; i--) {
            p = &g_aBrPeer71[i];
            ah[0] = DAT_11849e60;
            ah[1] = g_aBrPeer71[i].hMutex;
            if (WaitForMultipleObjects(2, ah, 0, INFINITE) == 0)
                ExitThread(0);
            if ((p->f02C & 0x3f) >= 2 && (p->f02C & 0x3f) < 5 &&
                p->aSub[p->f558].time >= BR_K_00077BF0) {
                FUN_100371f0(g_brSlot4098, p->f004, DAT_11849e58);
                p->f02C = DAT_11849e58 + 5;
                DAT_11849e58 = DAT_11849e58 + 1;
            }
            st = g_aBrPeer71[i].f02C;
            if ((st & 0x3f) >= 5) {
                g_aBrPeerRank[n].idx = i;
                g_aBrPeerRank[n].score = BR_K_00077BF4 - (float)st;
                n++;
            } else if ((st & 0x3f) >= 2) {
                g_aBrPeerRank[n].idx = i;
                g_aBrPeerRank[n].score = p->aSub[p->f558].time;
                n++;
            } else {
                g_aBrPeerRank[j].idx = i;
                g_aBrPeerRank[j].score = BR_K_00077BF8 - (float)i;
                j--;
            }
            ReleaseMutex(g_aBrPeer71[i].hMutex);
        }
    }
    qsort(g_aBrPeerRank, n, 8, BrSub10071B60);
    for (k = 0; k < BR_PEERS; k++)
        g_aBrPeerOrder[g_aBrPeerRank[k].idx] = k;
}
