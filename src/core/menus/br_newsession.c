/* br_newsession.c -- menus: the "new session" global reset (0x1003E680 D3D,
 * 0x10037C90 Glide), which puts every setup-screen choice back to its default
 * before a new game.
 *
 * Filed out of slice6_73.c, whose preamble it keeps below so the compiler's
 * view of the body is unchanged.  Matching arm only: the port body stays in
 * slice6_73.c, which models these globals as fields of g_br73 (and whose
 * BR_HOST_LINK renaming keeps slice6_70 the port owner).
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "slice6_73.h"


/* WHAT IT DOES: wipes the slate for a new game -- every choice the player
 * could have made on the setup screens goes back to its default, the "1 of 1"
 * counters are rebuilt, and three large blocks of session state are cleared.
 * It prints the same number into both counter strings, because the value that
 * ought to have made the second one different was zeroed moments before. */
/* @t4-pass 0x10037C90 1 2026-09-26 probes 300 bytes 290 insns 62 regions 2 rows 3 census yes  (hand, slice6_73.c: the DEAD 2026-09-26 campaign above -- every constant type/spelling, puns, helpers, C++ lane, SP3, TU position; micro-measured the VC5 zero-promotion rule) */
/* @t4-pass 0x10037C90 2 2026-09-27 probes 16 bytes 290 insns 62 regions 2 rows 3 census yes  (hand, br_newsession.c after the refile: struct/array/short[4] views of the floats, TU-defined and static definitions, (x&0), 0/1, x*0, inline returning 0, and constants that round to 0.0f without being 0 (1e-50, 1e-50f, 1e-46f, -0.0f, 0.0L) -- all one zero web.  Census: a whole-binary scan for overlapping pure zero-store webs finds the same class in 0x10063DD0 BrCollRespReset (floats in edx, ints in ecx; certified T3), so the original compiler saw float 0 and int 0 as distinct constants and no C spelling found yet reproduces that) */
/* @t3 0x10037C90 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 290/289 insns 62/63 rows 2+1 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * RESIDUE: one register-allocation fork -- the original zeroes the three
 * float fields through a second zero register (xor ecx,ecx) beside the ebx
 * zero web; every spelling joins one web (DEAD lists above; the same class
 * is certified in 0x10063DD0).  A3 pairs it by the promoted-zero rule.
 * Do not reopen before the end-grind (project rule 12). */
/* @implements 0x1003E680 d3d BrSub1003E680 */
/* Matching arm: loose Glide globals in the original's store order, the
 * imported sprintf cached in esi, memset as rep stosd.  The three float
 * fields (0x10AC40F8, 0x10AC40FC, 0x10AC5C20) are zeroed through a SECOND
 * zero register (ecx) in the original; every spelling tried (0.0f, 0.0,
 * (float)0, int 0, pointer/unsigned types, a named float local) folds them
 * into the ebx zero or an immediate.  RESIDUE: that one `xor ecx,ecx` web
 * (290/289 B, register-blind 1+2).  Note 0x10AC40F8 is a short[4] elsewhere
 * (BrSeasonApply, BrItemSetNumWord), stored here as two dwords.
 * DEAD 2026-09-26 (~300 compiles): double/int64/short/char/pointer/enum-like
 * types, NULL, memset(4) and memset(8) (8 gives its own zero reg but pins
 * both stores together at the top), `*(int *)&` puns, inline helpers taking
 * or returning the zero, named zero locals of every type, C++ lane, SP3
 * compiler, TU headers/padding.  Micro-measured VC5 rule: 0 is promoted to a
 * caller-saved reg at >=2 zero STORES in a call-free region, to a
 * callee-saved one at >=4 spanning a call; compares do not count and float
 * 0.0f stores never promote.  No 4-byte store type makes a second web.
 * DEAD 2026-09-26 (26 more + micro-tests): volatile float/int/unsigned/long
 * globals, copies from the first float (fb = fa), chained fa = fb = fc,
 * static const float/int zeros, pointer stores.  Micro-tests: 23 constant
 * spellings and 14 source shapes all join ONE zero web; only a union or a
 * static const array yields extra registers, and those are copies/loads, not
 * an xor.  A3 now pairs the reg-zero stores with the immediate (t3.py
 * promoted-zero rule), so this is a T3 candidate once refiled. */
extern float DAT_10ac40f8, DAT_10ac40fc, DAT_10ac5c20;
extern int DAT_100abde8, DAT_10ac5d58, DAT_10ac5d5c, DAT_10ac5d60;
extern int DAT_100abdec, DAT_100abdf0, DAT_100abdf4, DAT_100abdf8;
extern int DAT_10ac5d68, DAT_10ac5d6c, DAT_10ac5bf8, DAT_10ac5bfc;
extern int DAT_10ac5c04, DAT_10ac5c08, DAT_10ac5c0c;
extern unsigned char DAT_10ac5c10;
extern int DAT_10ac5c14, DAT_10ac5c18, DAT_10ac5a40, DAT_10ac5c1c;
extern int DAT_10ac5c28, DAT_10ac5bf4;
extern char DAT_10ac5870[], DAT_10ac46a0[];
extern int DAT_10ac5a48[0x53], DAT_10ac4c60[0x53], DAT_1021c650[0x46];
extern unsigned short DAT_10ac5b38;
extern int BrPairBufReset(void);            /* 0x10037870 */
extern void BrSub10037B20(void);            /* 0x10037B20 */
void BrSub1003E680(void)
{
    DAT_10ac40f8 = 0.0f;
    DAT_100abde8 = 2;
    DAT_10ac5d58 = 0;
    DAT_10ac5d5c = 0;
    DAT_10ac5d60 = 0;
    DAT_100abdec = 1;
    DAT_100abdf0 = 1;
    DAT_100abdf4 = 1;
    DAT_100abdf8 = 3;
    DAT_10ac5d68 = 0;
    DAT_10ac5d6c = 0;
    DAT_10ac5bf8 = 0;
    DAT_10ac5bfc = 0;
    DAT_10ac5c04 = 0;
    DAT_10ac5c08 = 0;
    DAT_10ac5c0c = 0;
    DAT_10ac5c10 = 0;
    DAT_10ac5c14 = 0;
    DAT_10ac5c18 = 0;
    DAT_10ac5a40 = 0;
    DAT_10ac40fc = 0.0f;
    DAT_10ac5c1c = 0;
    DAT_10ac5c20 = 0.0f;
    DAT_10ac5c28 = 0;
    DAT_10ac5bf4 = 0;
    sprintf(DAT_10ac5870, "%d", 1);
    sprintf(DAT_10ac46a0, "%d", DAT_10ac5bfc + 1);
    BrPairBufReset();
    DAT_10ac5bf4 = 0;
    memset(DAT_10ac5a48, 0, sizeof DAT_10ac5a48);
    memset(DAT_10ac4c60, 0, sizeof DAT_10ac4c60);
    memset(DAT_1021c650, 0, sizeof DAT_1021c650);
    DAT_10ac5b38 = 0x102;
    DAT_1021c650[0] = -1;
    BrSub10037B20();
}
