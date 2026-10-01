/* br_netpkt.c -- net.
 *
 * The outgoing side of the wire protocol: opening a packet and stamping it
 * with the tick every receiver orders by.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>

/* 0x100048D0 */
/* WHAT IT DOES: starts a fresh outgoing network packet. It clears the packet
 * object, then under the network lock reads the game clock, remembers that
 * tick in a global so the rest of the send can refer to it, and writes it
 * into the packet as its first three-byte field -- the timestamp every
 * receiver uses to order what arrives. */
/* @implements 0x10004C40 glide BrNetPktStamp */
/* The mutex pair is the raw Win32 import (FF 15), the same lock idiom the
 * rest of the net layer uses; slice1_02.c declared it once for the whole
 * translation unit. */
/* 64-bit core: WaitForSingleObject is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* 0x100037D0 is called with NOTHING pushed and no stack cleanup, so in this
 * TU it is declared with no parameters. slice1_01.h gives it a vestigial
 * `elapsedMs` argument that the body ignores and that generates no code;
 * declaring it here the way the CALL SITE reads keeps that push from
 * appearing. Same symbol either way -- both spellings are cdecl. */
/* BrTicks30FromMs: prototype in br_funcs.h */

/* Both callees are thiscall. BrObjClear takes only `this`, so BR_THISCALL1 is
 * exact for it (spelled __fastcall directly -- this file does not include
 * br_match.h); BrBitStreamWriteU24 has one stack argument as well, which is
 * struct-wrapped so __fastcall cannot claim edx for it -- the same wrapper
 * slice1_09.c defines for its own definition of this function. */
typedef struct { unsigned int v; } BrPktU24Arg;
/* BrObjClear: prototype in br_funcs.h */
void __fastcall   BrBitStreamWriteU24(void *pBs, BrPktU24Arg v); /* 0x1006D000 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10226A64 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1021CE40 */

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
/* @t4-pass 0x1006B080 2 2026-09-07 probes 48 bytes 99 insns 35 regions 2 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B080 3 2026-09-07 probes 48 bytes 99 insns 35 regions 2 rows 4 census yes  (tools/crank.py) */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1006B080.cpp */
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
/* BrCountedTotal: prototype in br_funcs.h */
void __fastcall BrBitStreamWriteU8(void *pBs, BrU8Arg v); /* 0x1006CFA0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* 0x11849E68 */

/* BrNetWriteTag20: prototype in br_funcs.h */

typedef struct { unsigned int v; } BrU32Arg;
void __fastcall BrBitStreamWriteU32(void *pBs, BrU32Arg v); /* 0x1006D050 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: write one player record onto an outgoing bitstream if it
 * still fits in 256 bytes: six field bytes, a 32-bit id, a 24-byte name when
 * the type is 0..2, and a 24-bit extra when the type is 4. Returns 0 if the
 * record would not fit. */
/* @t4-pass 0x1006AEB0 1 2026-09-08 probes 1 bytes 304 insns 0 regions 1 rows 0 census no
 * PARKED T2. Same U8 thiscall wall as BrNetWriteTag20 in this file: MSVC
 * homes each byte arg; orig pushes eax with dirty high bytes. Do not grind. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1006AEB0.cpp */
/* BrNetWritePlayerRec: prototype in br_funcs.h */

/* Hand-matched from disassembly: 0x1006CD80
 * fastcall: pointer in ecx, four fields zeroed then a self-pointer stored at
 * offset 0x10 (= p+0x14), returns this. */

/* WHAT IT DOES: the constructor of the outgoing packet (0x214 bytes) the
 * senders at 0x10004900..0x100051C0 build on the stack: clears the four
 * header fields and aims the write pointer at +0x10 at the packet's own
 * buffer at +0x14.  A C++ constructor, written as __fastcall (this in ecx). */
/* @implements 0x1006CD80 glide FUN_1006cd80 */
int *__fastcall FUN_1006cd80(int *p)
{
  p[2] = 0;   /* 0x08 */
  p[3] = 0;   /* 0x0c */
  p[0] = 0;
  p[1] = 0;   /* 0x04 */
  p[4] = (int)(p + 5);   /* [0x10] = p + 0x14 */
  return p;
}
