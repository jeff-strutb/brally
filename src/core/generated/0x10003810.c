/* Matching TU for 0x10003810 -- net clock sync: record a ping, apply an ack. */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

typedef struct {
    int key;         /* what the ack quotes back */
    int tSent;       /* local time the ping went out */
} BrPingSlot;

extern BrPingSlot g_aBrPing[8];                /* 0x102265E0 .. 0x10226620 */
extern int        g_brPingHead;                /* 0x10226A68 */
extern unsigned   g_brPingBestRtt;             /* 0x10226A6C */
extern int        g_brClockOffset;             /* 0x1021C908 */

void BrTimeUpdate(void);                       /* 0x1006E360 */
int  BrGetTimerState(void);                    /* 0x1006E350 */

/* WHAT IT DOES: the net clock-sync ring.  An incoming ack that beat (or
 * tied) the best round-trip so far is matched against the eight outstanding
 * pings by key, and the matching ping sets the local-to-remote clock offset
 * from the remote's timestamp plus half the round-trip.  Then the new ping
 * (key, send time) is written into the ring at the head, which wraps at 8. */
/* @implements 0x10003810 glide BrNetPingSync */
void BrNetPingSync(int key, int tSent, int ackKey, unsigned rtt)
{
    BrPingSlot *p;

    if (rtt <= g_brPingBestRtt && rtt != 0) {
        for (p = g_aBrPing; (int)p < (int)&g_aBrPing[8]; p++) {
            if (p->key == ackKey) {
                int tRemote = (rtt >> 1) - p->tSent + tSent + p->key;
                BrTimeUpdate();
                g_brClockOffset = BrGetTimerState() - tRemote;
                g_brPingBestRtt = rtt;
            }
        }
    }
    g_aBrPing[g_brPingHead].key   = key;
    g_aBrPing[g_brPingHead].tSent = tSent;
    g_brPingHead++;
    if (g_brPingHead >= 8)
        g_brPingHead = 0;
}

#endif /* BR_MATCHING_BUILD */
