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

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <windows.h>

#ifdef BR_MATCHING_BUILD

int  FUN_10004900(void *, int, unsigned char, unsigned char, unsigned char,
                  void *, int);
void FUN_10004ad0(void *, int, int, unsigned char, unsigned char,
                  unsigned char, int, void *, int, int);
void FUN_10004d30(void);             /* clears the peer/slot tables      */
unsigned int BrSub10075020(void);    /* 0x1006E280  millisecond clock    */

extern volatile int DAT_1007b264;    /* our slot id; written by receive  */
extern int DAT_10273328;             /* the DirectPlay object ptr        */
extern int DAT_10273330;
extern int DAT_10273334;             /* nonzero: we are the host         */
extern int DAT_10226e7c;
extern unsigned char DAT_10af3bb4;   /* player colour r                  */
extern unsigned char DAT_10af3bb5;   /* player colour g                  */
extern unsigned char DAT_10af3bb6;   /* player colour b                  */
extern int DAT_10b71648;             /* the player name buffer           */

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

    DAT_1007b264 = -1;
    while (DAT_1007b264 == -1) {
        r = FUN_10004900(&DAT_10273328, DAT_1007b264,
                         DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6,
                         &DAT_10b71648, 0);
        result = r != -1;
        t0 = BrSub10075020();
        if (DAT_10273334 != 0) {
            DAT_1007b264 = 0;        /* the host is always slot 0 */
            FUN_10004d30();
            FUN_10004900(&DAT_10273328, DAT_1007b264,
                         DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6,
                         &DAT_10b71648, 0x10);
            FUN_10004ad0(&DAT_10273328, DAT_1007b264, DAT_10226e7c,
                         DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6,
                         DAT_10273330, &DAT_10b71648, 2, 0x10);
        }
        /* The inner while's rotated ENTRY test is the original's mid-loop
         * slot check (jump-threaded straight to the epilogue); the deadline
         * `t0 + 2000` is written IN the loop so the invariant add lands in
         * the preheader (`add esi,0x7d0` between entry test and head). */
        while (DAT_1007b264 == -1) {
            if (BrSub10075020() >= t0 + 2000) {
                break;
            }
            Sleep(0);
        }
    }
    return result;
}

#endif /* BR_MATCHING_BUILD */
