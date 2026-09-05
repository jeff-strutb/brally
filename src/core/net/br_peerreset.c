/* br_peerreset.c -- net.
 *
 * Resetting the networking message tables: every peer record and every one of
 * its message slots is guarded by its own mutex, so clearing the tables means
 * taking each record's mutex in turn.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
#include <windows.h>

/* One networking record: 0x96C bytes, its own mutex at +0, the payload
 * buffer in the middle, and the four-word queue header at the tail. */
typedef struct BrPeerRec {
    HANDLE hMutex;          /* +0x000 */
    int    f04;             /* +0x004 -- the id BrNetPeerMsgCancel matches */
    int    f08;             /* +0x008 -- stamped with the current tick */
    char   pad0c[0x20];     /* +0x00C */
    int    f2c;             /* +0x02C -- state, low 6 bits */
    char   pad30[0x92c];    /* +0x030 */
    int    f95c;            /* +0x95C -- queue header, four words */
    int    f960;
    int    f964;
    int    f968;
} BrPeerRec;

extern int       DAT_117b3250;
extern int       DAT_117b324c;
extern int       DAT_11849e58;
extern BrPeerRec g_aBrPeer71[16];
extern BrPeerRec g_aBr178FEF8[16][16];

/* WHAT IT DOES: reset the whole networking message system. It clears the two
 * global "something is pending" flags, then walks all 16 peers and, for each,
 * all 16 of that peer's message slots. Every record is touched under its own
 * mutex: the four-word queue header at the tail and the state word are zeroed,
 * and each message slot is additionally stamped with the current tick so the
 * timeout logic restarts from now. */
/* @implements 0x1006A330 glide FUN_1006a330 */

void FUN_1006a330(void)
{
    int i;
    int j;

    DAT_117b3250 = 0;
    DAT_11849e58 = 0;
    for (i = 0; i < 16; i++) {
        WaitForSingleObject(g_aBrPeer71[i].hMutex, 0xffffffff);
        g_aBrPeer71[i].f95c = 0;
        g_aBrPeer71[i].f960 = 0;
        g_aBrPeer71[i].f964 = 0;
        g_aBrPeer71[i].f968 = 0;
        g_aBrPeer71[i].f2c = 0;
        ReleaseMutex(g_aBrPeer71[i].hMutex);
        for (j = 0; j < 16; j++) {
            WaitForSingleObject(g_aBr178FEF8[j][i].hMutex, 0xffffffff);
            g_aBr178FEF8[j][i].f08 = DAT_117b324c;
            g_aBr178FEF8[j][i].f95c = 0;
            g_aBr178FEF8[j][i].f960 = 0;
            g_aBr178FEF8[j][i].f964 = 0;
            g_aBr178FEF8[j][i].f968 = 0;
            g_aBr178FEF8[j][i].f2c = 0;
            ReleaseMutex(g_aBr178FEF8[j][i].hMutex);
        }
    }
}

#endif /* BR_MATCHING_BUILD */
