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

/* 0x1006B080 */
/* WHAT IT DOES: appends a nibble-packed table field to an outgoing packet,
 * but only if nine more bytes still fit in the 256-byte buffer. It writes a
 * tag byte (the field kind OR'd with 0x20), then walks an eight-entry global
 * table writing one byte per entry -- each entry's high field in the top
 * nibble, its low field in the bottom -- and reports success. If the field
 * would not fit it writes nothing and reports failure, so a half-written
 * field can never go out. */
/* @implements 0x1006B080 glide BrNetWriteTag20 */
/* RESIDUE (2026-09-06): +4 insns / +16 B, REGNORM 4+0. The C body is
 * complete and correct; the wall is the SAME construct C cannot spell that
 * parks its family (BrNetWriteTagC0 / BrNetWriteRaceOpts in ghidra_batch.c):
 * the original pushes each byte argument with its upper three bytes still
 * dirty (`mov al,[..]; or/build al; push eax`), which MSVC only emits when the
 * thiscall callee's PARAMETER IS A BYTE TYPE (register-eligible, so it never
 * homes). Every C wrapper -- 1-byte struct, 4-byte union via .b, padded struct
 * -- homes the partial write first (`mov [slot],B; mov R,[slot]; push R`).
 * This function makes TWO such calls (the tag byte and the loop byte), so it
 * inherits the wall twice: 2x (home+reload). PROBED AND DEAD upstream; routes
 * to the C++ TU lane, do not grind in C.
 * @t4-pass 0x1006B080 1 2026-09-06 probes 1 bytes 83 insns 31 regions 2 rows 4 census no */
/* BrBitStreamWriteU8 (0x1006CFA0) is thiscall with one byte stack argument;
 * the byte is struct/union-wrapped so __fastcall cannot claim edx for it --
 * the same wrapper the rest of this cluster uses. BrCountedTotal (0x1006D180)
 * is thiscall on the stream, taking only `this`. */
typedef union { unsigned char b; unsigned int u; } BrU8Arg;
int  __fastcall BrCountedTotal(void *pBs);              /* 0x1006D180 */
void __fastcall BrBitStreamWriteU8(void *pBs, BrU8Arg v); /* 0x1006CFA0 */
extern unsigned char DAT_11849e68[];                    /* 0x11849E68 */

int BrNetWriteTag20(void *pThis, unsigned char kind)
{
    BrU8Arg b;
    unsigned char *p;

    if (BrCountedTotal(pThis) + 9 <= 0x100) {
        b.b = (unsigned char)(kind | 0x20);
        BrBitStreamWriteU8(pThis, b);
        p = DAT_11849e68;
        do {
            b.b = (unsigned char)((p[4] << 4) | p[0]);
            BrBitStreamWriteU8(pThis, b);
            p += 8;
        } while ((int)p < (int)&DAT_11849e68[0x40]);
        return 1;
    }
    return 0;
}
#endif
