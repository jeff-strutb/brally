/* br_netopen.c -- net: announcing a freshly opened session.
 *
 * 0x10004E00 is the third leg of racebegin's "net open" step (see
 * br_racebegin.c's BR_RB_NETOPEN). It broadcasts the local player's
 * identity through the two packet senders in the C++ lane
 * (0x10004900 / 0x10004AD0) and then waits, up to two seconds at a
 * time, for another node to hand this player a slot number.
 *
 * DAT_1007b264 is the local slot id and is written by the RECEIVE path
 * on another thread -- it is volatile here, and the original's bytes
 * prove it: the value is re-read from memory immediately after every
 * store and at every loop test.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <windows.h>


/* FUN_10004900: prototype in br_funcs.h */
/* FUN_10004ad0: prototype in br_funcs.h */
/* FUN_10004d30: prototype in br_funcs.h */
/* BrSub10075020: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* our slot id; written by receive  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* the DirectPlay object ptr        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* nonzero: we are the host         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* player colour r                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* player colour g                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* player colour b                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* the player name buffer           */

/* WHAT IT DOES: shouts "here I am" onto the network when a session opens
 * and then waits to be given a place in it. Sends the join announcement
 * (name and colours), and if this machine is the host, takes slot zero at
 * once and answers itself with the fuller greeting the other nodes expect.
 * Otherwise it naps in two-second stretches, re-announcing after each one,
 * until the receive thread fills the slot in. Returns whether the first
 * announcement went out at all. */
/* @implements 0x10004E00 glide BrNetOpenAnnounce */
int BrNetOpenAnnounce(void)
{
    int          result;
    int          r;
    unsigned int t0;

    g_id = -1;
    while (g_id == -1) {
        r = BrNetSend4900(&g_brP277B40, g_id,
                         (*(unsigned char *)&g_aBrRaceCar[0].f29AC), (*(unsigned char *)&g_aBrRaceCar[0].f29AD), (*(unsigned char *)&g_aBrRaceCar[0].f29AE),
                         &(*(int *)&g_aBrCfgPlayerName), 0);
        result = r != -1;
        t0 = BrSub10075020();
        if (DAT_10273334 != 0) {
            g_id = 0;        /* the host is always slot 0 */
            BrPalFetch();
            BrNetSend4900(&g_brP277B40, g_id,
                         (*(unsigned char *)&g_aBrRaceCar[0].f29AC), (*(unsigned char *)&g_aBrRaceCar[0].f29AD), (*(unsigned char *)&g_aBrRaceCar[0].f29AE),
                         &(*(int *)&g_aBrCfgPlayerName), 0x10);
            BrNetSend4AD0(&g_brP277B40, g_id, g_226e7c,
                         (*(unsigned char *)&g_aBrRaceCar[0].f29AC), (*(unsigned char *)&g_aBrRaceCar[0].f29AD), (*(unsigned char *)&g_aBrRaceCar[0].f29AE),
                         (*(int *)&g_br277B48), &(*(int *)&g_aBrCfgPlayerName), 2, 0x10);
        }
        /* The inner while's rotated ENTRY test is the original's mid-loop
         * slot check (jump-threaded straight to the epilogue); the deadline
         * `t0 + 2000` is written IN the loop so the invariant add lands in
         * the preheader (`add esi,0x7d0` between entry test and head). */
        while (g_id == -1) {
            if (BrSub10075020() >= t0 + 2000) {
                break;
            }
            Sleep(0);
        }
    }
    return result;
}

