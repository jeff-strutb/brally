/* br_peerrank.c -- net.  The worker thread's peer ranking pass, 0x1006A7E0
 * (D3D 0x10071870).  Its siblings -- the wait loop 0x1006A650 and the mutex
 * bring-up -- are in br_peer.c; this one is in its own TU because it needs
 * the peer table typed as records where br_peer.c declares it as an int.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT and KERNEL32 calls go through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>

/* One networking record, 0x96C bytes (br_peerslot.c pins the stride and
 * the +0x2C status word).  Only the fields this pass touches are named. */
typedef struct BrPeerRec {
    void         *hMutex;            /* +0x000 */
    int           f004;              /* +0x004  the peer's DirectPlay id  */
    unsigned char pad008[0x24];
    int           f02C;              /* +0x02C  status; low 6 bits = state */
    unsigned char pad030[0x0D0 - 0x030];
    unsigned char aSub[0x558 - 0x0D0];   /* +0x0D0  0xA0-byte entries      */
    int           f558;              /* +0x558  index into aSub           */
    unsigned char pad55C[0x968 - 0x55C];
    unsigned int  f968;              /* +0x968  pending count             */
} BrPeerRec;

typedef char br_peerrank_stride[(sizeof(BrPeerRec) == 0x96C) ? 1 : -1];

#define BR_PEERS 16

extern BrPeerRec g_aBrPeer71[BR_PEERS];   /* 0x117A9B88 */
extern void     *DAT_11849e60;            /* the quit event               */
extern int       DAT_11849e58;            /* next message sequence number */
extern int      *g_brSlot4098;            /* 0x10AC4098 the session record */
extern float     g_aBrPeerRank[BR_PEERS][2];   /* 0x11849EB0: {index bits, score} */
extern int       g_aBrPeerOrder[BR_PEERS];     /* 0x11849E68: rank by peer */
extern int  FUN_100371f0(int *pSess, int idPeer, int seq);   /* 0x100371F0 */
extern int  BrSub10071B60(const void *pA, const void *pB);   /* 0x1006AAD0, the comparator */

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
/* PARKED T2 2026-09-09 at 763/748 B, 221/222 insns, register-blind 16+17,
 * 250 positional diffs.  The first two passes are the original's shape
 * (biased induction pointer, event-then-mutex handle pair, record pointer
 * as a declaration initialiser in the sorted-list loop with the sequence
 * number re-loaded after the status store); the residue is (1) n/i are
 * coloured ebx/ebp the other way round in the first pass, (2) the third
 * loop's induction pointer is biased to the RECORD BASE where the original
 * biases it to the status word (+0x2C: `[esi]` is f02C, the mutex is
 * `[esi-0x2C]`), and (3) that loop's i/pS/pI are coloured edi/ebp/ebx in
 * the original and ebp/ebx/edi here.  The first loop's bias (+0x968, the
 * pending count) came out right from index-form source, so the pick is
 * source-driven; what drives it is not yet known.
 * Levers that landed (keep): block scoping of the two passes (frame 0x20),
 * event-first handle pairs, `i` initialised before the cursors, the
 * pointer initialiser in the sorted loop.  Dead probes (fn.py, all inert):
 * pointer formed after the handle read; pointer from the pair inline;
 * pointer via char arithmetic (worse); st at block scope; the masked state
 * as a named local; n block-local in both passes. */
/* @implements 0x1006A7E0 glide BrNetPeerRank */
void BrNetPeerRank(void)
{
    int n;
    int k;

    /* Block-scoped so the two passes' locals share frame slots: the first
     * pass's handle pair and the second pass's status word live in the same
     * dword, as the original's [esp+0x20] says. */
    {
        HANDLE ah2[2];
        int    i;
        float *pS;

        n  = 0;
        i  = BR_PEERS - 1;
        pS = &g_aBrPeerRank[0][1];
        for (; i >= 0; i--) {
            ah2[0] = DAT_11849e60;
            ah2[1] = g_aBrPeer71[i].hMutex;
            if (WaitForMultipleObjects(2, ah2, 0, INFINITE) == 0)
                ExitThread(0);
            if ((g_aBrPeer71[i].f02C & 0x3f) >= 2 && (g_aBrPeer71[i].f02C & 0x3f) < 5 &&
                g_aBrPeer71[i].f968 != 0) {
                ((int *)pS)[-1] = i;
                n++;
                *pS = (float)g_aBrPeer71[i].f968;
                pS += 2;
            }
            ReleaseMutex(g_aBrPeer71[i].hMutex);
        }

        if (n != 0) {
            qsort(g_aBrPeerRank, n, 8, BrSub10071B60);
            /* This loop reaches the record through a POINTER (the mutex
             * handle alone is read in index form, before the pointer is
             * formed): the original re-loads the sequence number after the
             * status store because a store through a pointer may alias it,
             * where the index-form loops do not. */
            for (k = n - 1; k >= 0; k--) {
                int        idx = *(int *)&g_aBrPeerRank[k][0];
                BrPeerRec *p = &g_aBrPeer71[idx];

                ah2[0] = DAT_11849e60;
                ah2[1] = p->hMutex;
                if (WaitForMultipleObjects(2, ah2, 0, INFINITE) == 0)
                    ExitThread(0);
                if ((p->f02C & 0x3f) >= 2 && (p->f02C & 0x3f) < 5 &&
                    p->f968 != 0) {
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
        int   *pI;
        float *pS;
        int    i;
        float *pD;

        i  = BR_PEERS - 1;
        n  = 0;
        pD = &g_aBrPeerRank[BR_PEERS - 1][1];
        pS = &g_aBrPeerRank[0][1];
        pI = (int *)&g_aBrPeerRank[0][0];
        for (; i >= 0; i--) {
            int st;

            ah[0] = DAT_11849e60;
            ah[1] = g_aBrPeer71[i].hMutex;
            if (WaitForMultipleObjects(2, ah, 0, INFINITE) == 0)
                ExitThread(0);
            if ((g_aBrPeer71[i].f02C & 0x3f) >= 2 && (g_aBrPeer71[i].f02C & 0x3f) < 5 &&
                *(float *)&g_aBrPeer71[i].aSub[g_aBrPeer71[i].f558 * 0xA0] >= BR_K_00077BF0) {
                FUN_100371f0(g_brSlot4098, g_aBrPeer71[i].f004, DAT_11849e58);
                g_aBrPeer71[i].f02C = DAT_11849e58 + 5;
                DAT_11849e58 = DAT_11849e58 + 1;
            }
            st = g_aBrPeer71[i].f02C;
            if ((st & 0x3f) >= 5) {
                *pI = i;
                pI += 2;
                n++;
                *pS = BR_K_00077BF4 - (float)st;
                pS += 2;
            } else if ((st & 0x3f) >= 2) {
                *pI = i;
                pI += 2;
                n++;
                *pS = *(float *)&g_aBrPeer71[i].aSub[g_aBrPeer71[i].f558 * 0xA0];
                pS += 2;
            } else {
                ((int *)pD)[-1] = i;
                *pD = BR_K_00077BF8 - (float)i;
                pD -= 2;
            }
            ReleaseMutex(g_aBrPeer71[i].hMutex);
        }

        qsort(g_aBrPeerRank, n, 8, BrSub10071B60);
        k = 0;
        for (pI = (int *)&g_aBrPeerRank[0][0]; pI < (int *)&g_aBrPeerRank[BR_PEERS][0]; pI += 2) {
            g_aBrPeerOrder[*pI] = k;
            k++;
        }
    }
}
#endif /* BR_MATCHING_BUILD */
