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

/* One entry of the record's +0xD0 table: only its leading time is read. */
typedef struct BrPeerSub {
    float         time;              /* +0x00  BR_K_00077BF0 = none yet    */
    unsigned char pad04[0xA0 - 0x04];
} BrPeerSub;

/* One networking record, 0x96C bytes (br_peerslot.c pins the stride and
 * the +0x2C status word).  Only the fields this pass touches are named. */
typedef struct BrPeerRec {
    void         *hMutex;            /* +0x000 */
    int           f004;              /* +0x004  the peer's DirectPlay id  */
    unsigned char pad008[0x24];
    int           f02C;              /* +0x02C  status; low 6 bits = state */
    unsigned char pad030[0x0D0 - 0x030];
    BrPeerSub     aSub[7];           /* +0x0D0                            */
    unsigned char pad530[0x558 - 0x530];
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
typedef struct BrPeerRankEnt {
    int   idx;                           /* peer index */
    float score;
} BrPeerRankEnt;
extern BrPeerRankEnt g_aBrPeerRank[BR_PEERS];  /* 0x11849EB0 */
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
#endif /* BR_MATCHING_BUILD */
