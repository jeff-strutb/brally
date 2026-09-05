/* Auto-generated from Ghidra decompilation — 0x10005330 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
int BrNetSlotGetF02C(int);                       /* 0x10004D80 */
int BrNetSend4AD0(void *dest, int a1, int a2, unsigned char r,
                  unsigned char g, unsigned char b, int a6,
                  char *text, unsigned char a8, unsigned char a9);  /* 0x10004AD0 */
extern HANDLE DAT_10226a34;          /* g_brH22AF04, the tick mutex        */
extern int DAT_10226624;             /* g_br22AAF4, the tick counter       */
extern int DAT_10226a50;
extern int DAT_10226a48;             /* g_brRaceNet                        */
extern int DAT_10226a44;
extern int DAT_105ccb88;             /* g_brFlag6909E0                     */
extern int DAT_10af21b0;
extern int DAT_100bcbe8;             /* g_br0BD3E0                         */
extern int DAT_1007b264;             /* g_br094294, the local player slot  */
extern int DAT_10226e7c;             /* g_br22B34C                         */
extern int DAT_10273330;             /* g_br277B48                         */
extern unsigned char DAT_10af3bb4;   /* g_brAD0854: the player colour r,g,b */
extern unsigned char DAT_10af3bb5;
extern unsigned char DAT_10af3bb6;
extern char DAT_10b71648[];          /* g_brPB4E2E8, the player name       */
extern char DAT_10273328[];          /* g_brP277B40, the send target       */

/* RESIDUE (2026-09-04): 1 byte, orig+0x8b. The original emits
 * `and al,0xbf; add esp,4; or al,0x80` -- the two masks are NOT folded.
 * VC5 folds (x & 0xbf) | 0x80 into (x & 0x3f) | 0x80 for every spelling
 * tried: expression form, ~0x40, -65, -128, unsigned constants, a cast to
 * (unsigned char)/(char) between the two ops, an int or unsigned char
 * local with the ops as separate statements, a volatile local, a union
 * with two bitfield writes (b6=0; b7=1), the callee declared to return
 * char/unsigned char/short, the parameter declared int (folds to
 * `and eax,0x3f`). `+ 0x80` keeps 0xbf but emits `add`; `^ 0x80` emits
 * `xor`. The sibling 0x10005400 (BrCdAudioTick, ghidra_batch.c) has the
 * same wall: ITS original is `and al,0x7f; or al,0x40`, also un-folded --
 * the note there saying the original folds is backwards. Source spelling
 * of the flag-byte update is unknown; the corpus has no member emitting
 * and-imm8/or-imm8 on a call result. `>= 27` (not `> 26`) fixed the other
 * two bytes. */
/* WHAT IT DOES: one tick of the network "still here" beacon. Under the tick
 * mutex, advance the counter when it is running and, every 27th tick, set
 * the resend flag and wrap it. Then, if the counter is running, a net race
 * is up, the deactivate flag is clear and the local frame count has not yet
 * reached the limit, broadcast the local player's slot, colour and name with
 * bit 7 set and bit 6 cleared in the slot's flag byte. */
/* @implements 0x10005330 glide BrNetBeaconTick */
void BrNetBeaconTick(void)
{
  int n;

  WaitForSingleObject(DAT_10226a34, INFINITE);
  if (DAT_10226624 != 0) {
    DAT_10226624++;
    if (DAT_10226624 >= 27) {
      DAT_10226a50 = 1;
      DAT_10226624 = 0;
    }
  }
  n = DAT_10226624;
  ReleaseMutex(DAT_10226a34);
  if (n != 0 && DAT_10226a48 != 0 && DAT_10226a44 != 0 && DAT_105ccb88 == 0
      && DAT_10af21b0 < DAT_100bcbe8) {
    BrNetSend4AD0(DAT_10273328, DAT_1007b264, DAT_10226e7c, DAT_10af3bb4,
                  DAT_10af3bb5, DAT_10af3bb6, DAT_10273330, DAT_10b71648,
                  (BrNetSlotGetF02C(DAT_1007b264) & 0xbf) | 0x80, 0);
  }
}


#endif /* BR_MATCHING_BUILD */
