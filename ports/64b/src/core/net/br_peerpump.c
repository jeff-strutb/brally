/* br_peerpump.c -- net.  The worker thread's message pump, 0x1006AB80.
 * In its own TU for the same reason as br_peerrank.c: it needs the peer
 * table typed as records where br_peer.c declares it as an int.
 */
/* The original is /MD: CRT and KERNEL32 calls go through the import table. */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice1_09.h"   /* br_globals: its objects */
#include <windows.h>
#include <string.h>

/* One networking record, 0x96C bytes (br_peerslot.c pins the stride and the
 * +0x2C status word).  Only the fields this pass touches are named. */
/* BrPeerRec: br_coretypes.h */



#define BR_PEERS 16

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x117A9B88 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x117B3258: each

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x11849F30: the

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the quit event  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the clock       */

/* BrNetWriteTag20: prototype in br_funcs.h */
/* BrNetWritePlayerRec: prototype in br_funcs.h */
/* BrNetWriteRaceOpts: prototype in br_funcs.h */
/* BrSub1006AFA0: prototype in br_funcs.h */

/* WHAT IT DOES: one pass of the network worker thread over all sixteen
 * peers: for each peer whose slot is active it writes the pending outgoing
 * messages -- either the race options (a fresh peer) or, for everyone else,
 * one record per OTHER peer whose state changed since this peer last heard
 * about it, each read under both mutexes.  It then flushes the stream and,
 * if long enough since the last time, re-sends this peer's current entry to
 * every other active peer, stopping early if one refuses.  Every wait also
 * watches the quit event and ends the thread when it fires.  If no peer was
 * active at all, a global retry flag is cleared on the way out. */
/* Byte-exact 2026-09-27.  Source facts that carry it:
 *   - the status tests are plain int `(x & 0x3f) == K`; VC5 keeps the dword
 *     `and` and narrows only the compare (`cmp cl,2`).  A (char) cast narrows
 *     the `and` too.
 *   - st2 is read BEFORE the inlined strcpy, so it lives across it in a stack
 *     slot (frame 0x448) and edi stays free for the cached ReleaseMutex.
 *   - the j body reaches its record through `pj`, read in the order st2, id,
 *     b4, b5, b6: that anchors the walker at +0x30 and inits the view-row
 *     walker before it.
 *   - the tail tests and passes g_aBrPeer71[i] by index and stores through p,
 *     which keeps the outer walker anchored on the +0x2C status word. */
/* @implements 0x1006AB80 glide BrNetPeerPump */
void BrNetPeerPump(void)
{
    DWORD wr;
    int i;
    int j;
    int k;
    int didAny;
    int st;
    int st2;
    int id;
    char changed;
    HANDLE h1[2];
    HANDLE h2[2];
    HANDLE h3[2];
    unsigned char b4;
    unsigned char b5;
    unsigned char b6;
    char name[1024];

    didAny = 0;
    for (i = 0; i < BR_PEERS; i++) {
        h1[0] = (HANDLE)DAT_11849e60;
        h1[1] = g_aBrPeer71[i].hMutex;
        wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
        if (wr == 0) {
            ExitThread(0);
        }
        st = g_aBrPeer71[i].f02C & 0x3f;
        if (st >= 1) {
            didAny = 1;
            if (st == 1) {
                BrNetWriteRaceOpts(g_1826BD0[i], i);
            }
            else {
                for (j = 0; j < BR_PEERS; j++) {
                    BrPeerRec *pj = &g_aBrPeer71[j];
                    h2[0] = (HANDLE)DAT_11849e60;
                    h2[1] = pj->hMutex;
                    wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
                    if (wr == 0) {
                        ExitThread(0);
                    }
                    /* The second wait reuses the SAME pair; only the mutex
                     * slot is replaced. */
                    h2[1] = g_aBr178FEF8[i][j].hMutex;
                    wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
                    if (wr == 0) {
                        ExitThread(0);
                    }
                    st2 = pj->f02C;
                    id = pj->f030;
                    b4 = pj->f034;
                    b5 = pj->f035;
                    b6 = pj->f036;
                    strcpy(name, pj->szName);
                    changed = (g_aBr178FEF8[i][j].f02C != st2);
                    if (changed && (st2 & 0x3f) == 2 && i == j) {
                        g_aBr178FEF8[i][j].f02C = st2;
                        changed = 0;
                    }
                    ReleaseMutex(g_aBr178FEF8[i][j].hMutex);
                    if (changed) {
                        BrNetWritePlayerRec(g_1826BD0[i], j, st2, id,
                                            b4, b5, b6, name,
                                            pj->f004);
                    }
                    ReleaseMutex(pj->hMutex);
                }
            }
            BrNetWriteTag20(g_1826BD0[i], i);
            {
            /* Tested and passed by index, stored through p: all-pointer
             * moves the outer walker's anchor to +0x95C, all-index to +0x964. */
            BrPeerRec *p = &g_aBrPeer71[i];
            if ((unsigned int)DAT_1184c070 >
                    (unsigned int)(g_aBrPeer71[i].f95C + 1000) &&
                DAT_117b3250 == 0 &&
                BrSub1006AFA0(g_1826BD0[i], i, g_aBrPeer71[i].f960, g_aBrPeer71[i].f964) != 0) {
                p->f95C = DAT_1184c070;
                for (k = 0; k < BR_PEERS; k++) {
                    if (i != k) {
                        h3[0] = (HANDLE)DAT_11849e60;
                        h3[1] = g_aBrPeer71[k].hMutex;
                        wr = WaitForMultipleObjects(2, h3, 0, 0xffffffff);
                        if (wr == 0) {
                            ExitThread(0);
                        }
                        if ((g_aBrPeer71[k].f02C & 0x3f) >= 1 &&
                            BrSub1006AFA0(g_1826BD0[i], k,
                                          g_aBrPeer71[k].f960,
                                          g_aBrPeer71[k].f964) == 0) {
                            k = BR_PEERS;
                        }
                        ReleaseMutex(h3[1]);
                    }
                }
            }
            }
        }
        ReleaseMutex(g_aBrPeer71[i].hMutex);
    }
    if (didAny == 0) {
        DAT_117b3250 = 0;
    }
}

