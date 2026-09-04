/* br_netpkt.c -- net.
 *
 * The outgoing side of the wire protocol: opening a packet and stamping it
 * with the tick every receiver orders by.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

/* 0x100048D0 */
/* WHAT IT DOES: starts a fresh outgoing network packet. It clears the packet
 * object, then under the network lock reads the game clock, remembers that
 * tick in a global so the rest of the send can refer to it, and writes it
 * into the packet as its first three-byte field -- the timestamp every
 * receiver uses to order what arrives. */
/* @implements 0x10004C40 glide BrNetPktStamp */
#ifdef BR_MATCHING_BUILD
/* The mutex pair is the raw Win32 import (FF 15), the same lock idiom the
 * rest of the net layer uses; slice1_02.c declared it once for the whole
 * translation unit. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

/* 0x100037D0 is called with NOTHING pushed and no stack cleanup, so in this
 * TU it is declared with no parameters. slice1_01.h gives it a vestigial
 * `elapsedMs` argument that the body ignores and that generates no code;
 * declaring it here the way the CALL SITE reads keeps that push from
 * appearing. Same symbol either way -- both spellings are cdecl. */
int BrTicks30FromMs(void);

/* Both callees are thiscall. BrObjClear takes only `this`, so BR_THISCALL1 is
 * exact for it (spelled __fastcall directly -- this file does not include
 * br_match.h); BrBitStreamWriteU24 has one stack argument as well, which is
 * struct-wrapped so __fastcall cannot claim edx for it -- the same wrapper
 * slice1_09.c defines for its own definition of this function. */
typedef struct { unsigned int v; } BrPktU24Arg;
void __fastcall   BrObjClear(void *pObj);                        /* 0x1006CDC0 */
void __fastcall   BrBitStreamWriteU24(void *pBs, BrPktU24Arg v); /* 0x1006D000 */

extern void *g_hBrNetMutex;    /* 0x10226A64 */
extern int   g_brNetPktTick;   /* 0x1021CE40 */

void BrNetPktStamp(void *pPkt)
{
    BrPktU24Arg tick;

    BrObjClear(pPkt);
    WaitForSingleObject(g_hBrNetMutex, 0xffffffff);
    g_brNetPktTick = BrTicks30FromMs();
    tick.v = (unsigned int)g_brNetPktTick;
    BrBitStreamWriteU24(pPkt, tick);
    ReleaseMutex(g_hBrNetMutex);
}
#endif
