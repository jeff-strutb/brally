/* br_sndvolume.c -- audio: applying a voice's volume to DirectSound.
 *
 * BrSndVoiceApplyVolume scales the voice's stored level by the master volume
 * and hands it to IDirectSoundBuffer::SetVolume (DSBVOLUME_MIN when the
 * master volume is zero).  Matching build only, as in the batch.
 *
 * Filed out of the address batch slice6_76.c, whose preamble is carried
 * verbatim below.
 */

#include <stddef.h>
#include <stdint.h>

#include "slice6_76.h"

/* ==========================================================================
 * 0. Cross-module declarations (see the banner)
 * ========================================================================== */

/* slice1_05.h -- 0x1002F900.  slice2_15.h calls the same address with its own
 * name for the command pair; both structs are {uint32_t w0, w1;}. */
struct BrGfxWords;
struct BrGfxCmd;
extern void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                                int a0,  int b0,  int c0,  int d0,
                                int Aa0, int Ab0, int Ac0, int Ad0,
                                int a1,  int b1,  int c1,  int d1,
                                int Aa1, int Ab1, int Ac1, int Ad1);

/* slice5_61.h -- 0x10042AF0 and 0x10060E90. */
extern void    BrGfx42AF0_1(void *p0);
extern int32_t BrTimeNow(void);

/* slice2_15.h / slice5_62.h -- 0x10069490, an adapter over br_pool.c. */
struct BrMat4;
extern struct BrMat4 *BrSub_10069490(void);

/* slice3_41.h -- 0x10069530. */
extern void *BrPool32Alloc(void);

/* slice5_63.h -- 0x1003E310. */
extern void BrSub1003E310(void);

/* slice4_53.h -- 0x1006A4A0. */
extern void BrSub1006A4A0(void *pThis, void *pArg);

/* slice1_10.h -- 0x10079550; slice3_45.h owns the one instance. */
struct BrFfb;
extern void BrFfbShutdown(struct BrFfb *pFfb);
extern struct BrFfb g_brFfb;

/* slice2_25.h -- 0x100443E0 and 0x10044280.  Both return int; both callers
 * (slice2_26.c) declare void and ignore it. */
struct BrGameObj;
extern int BrOptOpen2950A(struct BrGameObj *pUnused);
extern int BrOptOpen2950B(struct BrGameObj *pUnused);

/* slice4_50.h -- 0x10043BF0. */
extern void BrSub10043BF0(struct BrGameObj *p);

/* slice1_08.h -- 0x10072550, and the three "is sound usable" gates. */
struct BrSndVoice;
extern int32_t BrSndVoiceStop(struct BrSndVoice *pVoice);
extern int32_t   BrSndG0B5DE8;    /* 0x100B5DE8 */
struct BrDSound;
extern struct BrDSound *BrSndPDS; /* 0x118290F8 */
extern void     *BrSndG18290FC;   /* 0x118290FC */

/* slice1_08.h / slice3_40.h -- 0x100BBAE0, a BYTE master volume. */
extern uint8_t BrSndMasterVolume;

/* slice3_40.h -- 0x100BBAD8, and the two ten-entry level tables. */
extern uint8_t BrG_0BBAD8;
extern const int32_t BrOptLevelATable[10];   /* 0x100ADF68 */
extern const int32_t BrOptLevelBTable[10];   /* 0x100ADF90 */

/* slice2_25.h / slice3_40.h -- the two slider positions. */
extern int32_t g_brB4E708;   /* 0x10B4E708 */
extern int32_t g_brB4E70C;   /* 0x10B4E70C */

/* slice2_18.h -- 0x106C65E4, the hi-res flag: non-zero doubles every rect. */
extern int32_t BrG_6C65E4;

/* slice2_20.h -- 0x100B8C90.  br_data.c defines it as 1. */
extern int g_i0B8C90;

/* slice4_50.h:250 -- 0x10094294, the local slot / palette index.  slice4_50.c
 * OWNS the storage; this packet only reads it.  See the note in section 1. */
extern int32_t g_br094294;

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_11849e64;
int FUN_1006a650();
int FUN_1006a7e0();
int FUN_1006aaf0();
int FUN_1006ab80();
int FUN_1006b0e0();
extern unsigned int DAT_11849ea8;
extern unsigned int DAT_1184c070;
extern int g_brP277B40;
int BrDelta_100713A0();
/* Body in src/core/racing/br_secondtick.c; the starter below spawns it. */
void BrSecondTickLoop(void);
#include <windows.h>
extern int DAT_11849e60;
extern int DAT_1184c078;
extern int DAT_1184c07c;
void BrSndVoiceApplyFreq(int);
void BrSndVoiceApplyPan(int);

/* BrSndVoiceSetPan (0x1006B5B0) stays in ghidra_batch.c — context-sensitive codegen. */

typedef void (__stdcall *dsbuf_fn2)(int, int);

typedef int (__stdcall *dsbuf_fn1)(int);

/* WHAT IT DOES: push the voice's stored volume to DirectSound
 * (IDirectSoundBuffer::SetVolume, vtable +0x3c).  The stored level is scaled
 * by the global master level 0..255 and mapped onto DirectSound's
 * hundredths-of-a-decibel scale by (level - 400) * 10.  A master level of 0
 * short-circuits to DSBVOLUME_MIN (-10000) rather than computing silence.
 *
 * RESIDUE (T3a, parked): 84 vs 79 bytes, regnorm 3+1.  The arithmetic, the
 * unsigned /255 reciprocal, the branch polarity and both call sites are
 * already identical; the whole gap is allocation:
 *   - orig loads the parameter ONCE into ecx above `test al,al`; we load it
 *     per arm (+4 B), which costs a second callee-saved register (push/pop
 *     edi, +2 B) because the vtable fetch lands before `sub edx,0x190`
 *     instead of after it, while orig reuses edx for the vtable;
 *   - orig `mov eax,0xffffd8f0 / push eax`, we `push 0xffffd8f0` (-1 B).
 * DO NOT RE-RUN THESE -- six spellings, all BYTE-IDENTICAL output:
 *   (1) `dsbuf_fn2 fn = ...` assigned before the call, sibling style;
 *   (2) the call written inline with no local at all;
 *   (3) a `static __inline` two-arg helper called from both arms;
 *   (4) `int vol;` declared above the if and assigned in both arms;
 *   (5) a named `pBuf` local assigned AFTER the value (the "name the
 *       pointer" lever) with the call through `*pBuf`;
 *   (6) the multiply written master-first instead of level-first;
 *   (7) 2026-09-04: a struct-typed parameter (`level` at +0x14, `pBuf` at
 *       +0x9c, fields instead of `param_1 + N` arithmetic) -- byte-identical
 *       too, so the operand-kind lever that closed 0x1000EAF0's wheel
 *       pointer does not reach a single-use parameter.
 * The next lever has to come from outside the statement spelling. */
/* @t4-pass 0x1006B440 1 2026-09-07 probes 58 bytes 81 insns 30 regions 1 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 2 2026-09-07 probes 59 bytes 81 insns 30 regions 1 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 3 2026-09-10 probes 60 bytes 81 insns 30 regions 1 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 4 2026-09-10 probes 60 bytes 81 insns 30 regions 1 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 5 2026-09-10 probes 60 bytes 81 insns 30 regions 1 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 6 2026-09-10 probes 59 bytes 81 insns 30 regions 1 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 7 2026-09-10 probes 40 bytes 84 insns 31 regions 2 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x1006B440 8 2026-09-10 probes 40 bytes 84 insns 31 regions 2 rows 4 census yes  (tools/crank.py) */
/* WHAT IT DOES: sets how loud one playing sound is, by handing DirectSound
 * the voice's own level scaled by the game's master volume -- and jumping
 * straight to full silence when the master volume is zero. */
/* @t3 0x1006B440 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 84/79 insns 31/29 rows 1+3 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 8 zero-movement 7 8
 * Residue and dead list in the dossier above: the -10000 constant reaches the
 * stack through a register on one side and an immediate on the other, and the
 * rest is the argument-home allocation.  Eight counted passes, the last two
 * zero-movement, corpus a MISS at the first divergence.  Do not reopen before
 * the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1006B440 glide BrSndVoiceApplyVolume */

void BrSndVoiceApplyVolume(int param_1)

{
  int       vol;
  dsbuf_fn2 fn;

  if (BrSndMasterVolume != 0) {
    vol = ((*(unsigned int *)(param_1 + 0x14) * BrSndMasterVolume) / 0xff - 400) * 10;
    fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x3c);
    fn(*(int *)(param_1 + 0x9c), vol);
    return;
  }
  vol = -10000;
  fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x3c);
  fn(*(int *)(param_1 + 0x9c), vol);
  return;
}

#endif /* BR_MATCHING_BUILD */
