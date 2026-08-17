/* br_sfxsrc.h -- the SOUND SOURCE layer: the 25-entry source table, the
 * 15-entry per-channel record, and the two countdown entry points the race
 * step calls.
 *
 * REFERENCE IS orig/BRGlide.dll.  Every address below was checked with
 * tools/whereis.py before a line was written, and every one of them reported
 * `port/ ... NOTHING` for both builds' numbers except where noted.
 *
 * ======================================================================
 * WHY THIS EXISTS
 * ======================================================================
 * br_racestep.c counts a hole called "0x10060E00 / 0x10060DF0 countdown
 * sound", hit exactly four times per race.  Those two addresses are eleven
 * bytes each:
 *
 *     10060DF0  push 0xD ; call 0x10060DB0 ; add esp,4 ; ret
 *     10060E00  push 0xE ; call 0x10060DB0 ; add esp,4 ; ret
 *
 * and 0xD / 0xE are indices into a table of SOUND SOURCES, not group numbers
 * -- except that the table's initialiser makes them equal, which is the one
 * fact that makes the whole chain resolvable.  See BrSfxSrcTableInit.
 *
 * ======================================================================
 * THE CHAIN, WHOLE
 * ======================================================================
 *   0x10060DF0 / 0x10060E00   BrSfxSrcBeep / BrSfxSrcBeep2   (11 B each)
 *     -> 0x10060DB0           BrSfxSrcTrigger                (50 B)
 *          reads g_aBrSfxSrc[i].group / .f0C / .loop and latches i
 *          into 0x10AC67D0, then
 *     -> 0x1006E530           BrSfxSrcPlay                   (34 B)
 *          adds the packed level pair 0x00200020 and channel 3, then
 *     -> 0x1006E4C0           BrSfxSrcStart                  (99 B)
 *          -> 0x1006B530  bind the group's slot to the channel
 *          -> 0x1006B5B0  set the channel voice's loop flag
 *          -> 0x1006B730  set its levels from the packed pair
 *          -> 0x1006B880  derive the 32.32 ratio and START it
 *
 * The last four are the CHANNEL layer, modelled here as BrSfxChan*.  They
 * sit between br_sfx.c (the bank) and slice1_08.c (the voice), and neither
 * of those two files had them: whereis.py reports nothing in port/ for
 * 0x1006B5B0 (D3D 0x10072640) or 0x1006B730 (D3D 0x100727C0), and only
 * br_sfx.c's transcription of 0x1006B880's TAIL (the ratio arithmetic) for
 * 0x1006B880 (D3D 0x10072910).
 *
 * ======================================================================
 * THE STACK TRACE THAT MATTERS
 * ======================================================================
 * 0x1006E530 is four instructions of argument shuffling and it is exactly
 * the trap CONVENTIONS.md warns about.  At 0x1006E542 it reads `[esp+0xC]`
 * -- but two pushes have happened since entry (0x200020 at 0x1006E53C and
 * eax at 0x1006E541), so esp is 8 lower and `[esp+0xC]` names the entry
 * frame's `[esp+4]`, i.e. argument ZERO, not argument two.  Read naively
 * that instruction reassigns a1 to a0's slot and the channel comes out
 * wrong.  Traced: 0x1006E530(a0,a1,a2,a3) -> 0x1006E4C0(a0,a1,a2,a3,0x200020).
 *
 * The same trap sits in 0x1006E4C0 four times over (`[esp+0xC]`, `[esp+0x1C]`,
 * `[esp+0x14]`, `[esp+0x20]` against four different esp values, all naming
 * a0, a4, a1 and a3 respectively).  Each displacement is recorded at its
 * site in the .c.
 *
 * ======================================================================
 * WHAT THE ARGUMENTS ARE, AND HOW THAT WAS PINNED
 * ======================================================================
 * a0 of 0x1006E4C0 is the CHANNEL, not the group.  Three independent
 * confirmations:
 *
 *   1. 0x1006E560 (the silent sibling, packed = 0) is called by 0x10061310
 *      with 0, 2 and 4 and by 0x100612D0 with `2*iCar` -- and br_sfx.h
 *      already establishes that a car owns channel 2*iCar.
 *   2. 0x1006E4C0 passes it as 0x1006B530's SECOND argument, and br_sfx.h
 *      already records 0x1006B530 as `(group, ch)`.
 *   3. 0x10060DB0 passes a literal 3, and the shipped bank marks slot 3 --
 *      and only slot 3 -- for groups 13, 14 and 15, which are beep, beep2
 *      and water.  Channel == slot; see br_sfx.h's SLOT == CHANNEL section.
 *
 * So the countdown plays group 13 (beep) three times and group 14 (beep2)
 * once, both on channel 3, at full centre volume.  That is the 3-2-1-GO.
 *
 * ======================================================================
 * WHAT IS DELIBERATELY NOT HERE -- THE COLLISION AND TAUNT DISPATCH
 * ======================================================================
 * 0x10060F40 and 0x10061470 read the SAME three source fields and drive the
 * per-car engine loops and the one-shots.  0x10061470 (D3D 0x10068400) is
 * 2,565 bytes over a dozen untyped globals and is NOT ported; its OUTPUT --
 * the packed level pair and the engine curve -- already is, in br_sfx.c and
 * br_sfxout.c.  What is missing is the DISPATCH, and it is recorded here in
 * full so the next pass does not have to re-derive it:
 *
 * Seven one-shots, each gated on a per-car IMPACT BYTE and positioned by
 * BrSndPan (0x10060C30, already ported in slice3_41.c).  The packed pair is
 * built exactly as br_sfx.h describes -- `(gainA*vol) << 16 | (gainB*vol)`
 * through _ftol -- and then handed to 0x1006BA60, i.e. BrSndPlaySimple, on
 * slot 1.  Each byte is CLEARED after it fires.
 *
 *   car+0x362  0x100616F4  car-to-car hit.  Fires when the byte is non-zero
 *              (the guard is above, at the block's head).  Three severity
 *              tiers on the SAME byte:
 *                  < 0xAB -> group 17  hit-another-car3   (0x10061705)
 *                  < 0xD5 -> group 16  hit-another-car2   (0x1006170E)
 *                  else   -> group  1  hit-another-car1   (0x10061712)
 *   car+0x363  0x10061726  scenery/wall impact, gated `> 0x7F` (0x10061733),
 *              same three tiers at the same thresholds:
 *                  < 0xAB -> group 19  big-impact3        (0x10061784)
 *                  < 0xD5 -> group 18  big-impact2        (0x1006178D)
 *                  else   -> group  2  big-impact1        (0x10061791)
 *   car+0x36C  0x100617A5  gated `> 0x7F` (0x100617B2), ONE tier:
 *                          group  3  bottom-out           (0x100617F3)
 *   car+0x366  0x10061803  taunt1, group 0x14             (0x10061845)
 *   car+0x367  0x10061853  taunt2, group 0x15             (0x10061895)
 *   ...        taunt3 group 0x16 (0x100618E5) and taunt4 group 0x17
 *              (0x10061935), same shape.
 *
 * The four taunts do NOT go through the tier ladder and they do not use
 * BrSndPan's two GAINS at all -- only its integer distance volume, through
 * a THRESHOLD rather than a clamp: `cmp eax,2 / jle keep / mov eax,0x20`
 * (0x10061833, 0x10061883, 0x100618D3, 0x10061923).  So anything past the
 * nearest two distance steps jumps straight to full scale, and the value is
 * duplicated into both halves of the pair -- a taunt is always dead centre
 * and effectively either near-silent or loud.  Do not "fix" that into a
 * clamp; the three-instruction shape is unambiguous in all four copies.
 *
 * WHY IT IS NOT PORTED HERE.  Nothing in this tree models `car+0x362`: the
 * 0x2B68 record's impact bytes are written by the collision response, and
 * br_collresp.c (which this pass was told not to touch) does not carry them.
 * Transcribing the dispatch against fields that are never written would give
 * a function that links, runs and is silent for ever -- the exact "wrong but
 * plausible" outcome CONVENTIONS.md names as the worst available.  So the
 * thresholds are recorded and the code is not written.
 */
#ifndef BR_SFXSRC_H
#define BR_SFXSRC_H

#include <stdint.h>

#include "br_sfx.h"

/* 0x100B32B0 .. 0x100B3508, stride 24.  The end is pinned by 0x1006136F's
 * own `cmp eax, 0x100B3508`, and 0x100B3508 is the string
 * "MakeEnemyCarColorPanels: ...", so the table cannot run past it. */
#define BR_SFXSRC_COUNT   25

/* 0x10060DF0 and 0x10060E00 push these. */
#define BR_SFXSRC_BEEP    0x0D    /* group 13, "beep.wav"  -- 3, 2, 1  */
#define BR_SFXSRC_BEEP2   0x0E    /* group 14, "beep2.wav" -- GO       */

/* 0x10060DB0 hardcodes this channel, and 0x1006E530 the packed pair.
 * 0x00200020 is (32 << 16) | 32, i.e. both halves at BR_SFX_LEVEL_MAX: full
 * volume, dead centre.  See br_sfx.h for which half is left. */
#define BR_SFXSRC_CHANNEL 3
#define BR_SFXSRC_PACKED  0x00200020u

/* One 24-byte source record, read out of BRGlide.dll's .data at 0x100B32B0.
 *
 * Only `group`, `f0C` and `loop` are read by the countdown chain; `f0C` is
 * carried through 0x1006E530 and 0x1006E4C0 and never used by either, which
 * is preserved rather than optimised away because 0x10061470 reads the same
 * three fields and this table is shared with it.  `pf04`/`pf08` are pointers
 * into a run of 32 dwords at 0x100B52B0 that are all 0x30 in the image, and
 * `f14` is 0x200 for every looping source and 0x100 for the two beeps. */
typedef struct BrSfxSrcDef {
    int32_t  group;      /* +0x00  set to the record's own index at init */
    uint32_t pf04;       /* +0x04  -> 0x100B52B0 + 8*i, image value 0x30 */
    uint32_t pf08;       /* +0x08  -> 0x100B52B4 + 8*i, image value 0x30 */
    int32_t  f0C;        /* +0x0C  passed along and never read           */
    int32_t  loop;       /* +0x10  -1 loop, 0 one-shot                   */
    int32_t  f14;        /* +0x14  0x200 / 0x100 / -1 for record 0       */
} BrSfxSrcDef;

/* The live table.  `group` is ZERO in the image for every record; it is not
 * a constant, it is written at init.  See BrSfxSrcTableInit. */
extern BrSfxSrcDef g_aBrSfxSrc[BR_SFXSRC_COUNT];

/* 0x118EEF40, stride 24, one per channel.  0x1006E4C0 writes +0x00, +0x10
 * and +0x14; 0x1006B880 writes the 64-bit +0x08. */
typedef struct BrSfxChan {
    int32_t  group;      /* +0x00 */
    int32_t  f04;        /* +0x04 -- not written by this chain          */
    int64_t  ratio;      /* +0x08 -- the 32.32 pitch ratio              */
    int32_t  f10;        /* +0x10 -- zeroed by 0x1006E4C0               */
    uint32_t packed;     /* +0x14 -- the level pair                     */
} BrSfxChan;

extern BrSfxChan g_aBrSfxChan[BR_SFX_CHANNELS];

/* 0x1184C080, stride 24 -- a SECOND array with the SAME BrSfxChan layout,
 * holding what has actually been pushed at the voice.  It was missing from
 * this port entirely, so the second half of 0x1006B880's ratio store had
 * nowhere to go.
 *
 * The layout is pinned by field, not assumed: every reference into this block
 * in BRGlide's .text is at base + ch*24 + {0x00, 0x08, 0x0C, 0x14}, and each
 * one is paired with the identical offset off 0x118EEF40 by the frame sync at
 * 0x1006BDD0:
 *
 *   0x1006BE9E  eax = chan[ch].group    ecx = applied[ch].group   (+0x00)
 *   0x1006BEB4  eax:ecx = chan[ch].ratio  edx = applied[ch].ratio (+0x08)
 *   0x1006BEDF  eax = chan[ch].packed   ecx = applied[ch].packed  (+0x14)
 *
 * and each pair gates a re-send: a ratio that already matches is not pushed
 * again (0x1006BEC8/D2 skip the call to 0x1006B5F0).  So this is a dirty
 * check, and it is why the pair BrSfxRatioFromHz / BrSfxHzFromRatio not being
 * an exact inverse does not accumulate -- br_sfx.h says the same from the
 * other end.  +0x04 and +0x10 are never referenced and stay dead here too.
 *
 * NOTHING IN THIS PORT READS IT YET, which is honest rather than an oversight:
 * both readers -- 0x1006B5F0 (set the voice frequency from a ratio, and record
 * it here on success) and 0x1006BDD0 (the per-frame sync above) -- are
 * untranscribed.  BrSfxChanStart is the only writer either binary has on the
 * start path, and it is ported, so the field is written by the thing that
 * writes it in the original and by nothing else. */
extern BrSfxChan g_aBrSfxChanApplied[BR_SFX_CHANNELS];

/* 0x1184C268 -- the voice currently bound to each channel, and 0x1184C1E8 --
 * that channel's base rate in Hz.  Both are indexed by channel and both are
 * written by 0x1006B530. */
extern struct BrSndVoice *g_apBrSfxChanVoice[BR_SFX_CHANNELS];
extern double             g_aBrSfxChanRate[BR_SFX_CHANNELS];

/* 0x10AC67D0 -- the last source index 0x10060DB0 triggered.  slice3_41.h
 * describes the D3D twin of this global (0x10AC6C58 there is a different
 * object; the one it means is "set by 0x10067D40, cleared by
 * BrSndNearestReset") and models no storage for it. */
extern int32_t g_brSfxSrcLast;

/* ------------------------------------------------------------ the table */

/* 0x10061362..0x10061374, the six-instruction loop inside 0x10061310 (D3D
 * 0x100682A0):
 *
 *     ecx = 0; eax = 0x100B32B0;
 *     do { *(int32_t *)eax = ecx; eax += 24; ++ecx; } while (eax < 0x100B3508);
 *
 * So source i's group IS i.  Nothing else in either binary writes that
 * field, which is why the .data image shows zero for all 25 and why reading
 * the image alone makes the countdown look like it plays the engine.
 *
 * Also restores the rest of the table to its .data image, so the function is
 * idempotent and a test can call it twice. */
void BrSfxSrcTableInit(void);

/* ---------------------------------------------------------- the channel */

/* 0x1006B530 (D3D 0x100725C0).  Copy the group's base rate into the
 * channel's rate slot, stop whatever voice the channel already holds, and
 * bind BrSndVoices[group*18 + ch].  Returns 1 when a voice was bound, 0 when
 * the slot is empty -- and 1, without doing anything, when sound is off. */
int BrSfxChanBind(int group, int ch);

/* 0x1006B5B0 (D3D 0x10072640).  Write the bound voice's f18 (the loop flag).
 * Returns 1 on success, 0 when the channel holds no voice, 1 when sound is
 * off.  Does NOT start anything. */
int BrSfxChanSetLoop(int ch, int32_t loop);

/* 0x1006B730 (D3D 0x100727C0).  BrSndVoiceSetLevels on the bound voice, and
 * on success store the pair in the channel record. */
int BrSfxChanSetLevels(int ch, uint32_t packed);

/* 0x1006B880 (D3D 0x10072910).  Look the voice up by (group, ch) -- NOT
 * through the bound pointer -- start it with `loop`, then derive
 *
 *     ratio = (int64)((double)voice->f0C * 2^32 / g_aBrSfxChanRate[ch])
 *
 * store it in the channel record and make that voice the bound one.
 *
 * GOTCHA, preserved: the ratio is computed from the voice's CURRENT f0C
 * AFTER the voice has been started, and the divide is unguarded -- a channel
 * whose rate is still 0 produces an infinity that br_ftol64 turns into the
 * x87 integer indefinite.  br_sfx.h documents the same hole in
 * BrSfxRatioFromHz.
 *
 * Returns 1 on success, 0 when the slot is empty or the start failed. */
int BrSfxChanStart(int group, int ch, int32_t loop);

/* ---------------------------------------------------------- the sources */

/* 0x1006E4C0 (D3D 0x10075260).  The five-argument core.  `f0C` is accepted
 * and ignored, exactly as the original does. */
int BrSfxSrcStart(int ch, int group, int32_t f0C, int32_t loop,
                  uint32_t packed);

/* 0x1006E530 (D3D 0x100752D0).  BrSfxSrcStart(..., 0x00200020). */
int BrSfxSrcPlay(int ch, int group, int32_t f0C, int32_t loop);

/* 0x1006E560.  BrSfxSrcStart(..., 0) -- the SILENT start the engine loops
 * use, so that 0x10061470 can fade them in from nothing. */
int BrSfxSrcPlaySilent(int ch, int group, int32_t f0C, int32_t loop);

/* 0x10060DB0 (D3D 0x10067D40).  Play source `iSrc` on channel 3 at full
 * centre volume and latch the index. */
void BrSfxSrcTrigger(int iSrc);

/* 0x10060DF0 (D3D 0x10067D80) and 0x10060E00 (D3D 0x10067D90). */
void BrSfxSrcBeep(void);
void BrSfxSrcBeep2(void);

/* 0x1006C4BE, the tail of 0x1006C460 (D3D 0x100734F0): `rep stosd` of 15
 * dwords over 0x1184C268.  The teardown clears every channel's bound voice,
 * and it MUST be called whenever the voice table is rebuilt or freed --
 * otherwise a channel keeps a pointer into a bank that no longer exists and
 * the next bind stops a freed voice.  (That is not hypothetical: it is the
 * crash this test suite hit on its first run.)  The per-channel rates and
 * records are cleared with it, which the original does not do separately
 * because they are meaningless without a voice. */
void BrSfxSrcChannelsReset(void);

/* The race step's hook signature (br_racestep.h's `pfnSound`).  0x1001AD9E /
 * 0x1001ADA5:
 *
 *     ++g_brRaceBeep;
 *     if (g_brRaceBeep == 4)  0x10060E00();     GO
 *     else                    0x10060DF0();     3, 2, 1
 *
 * so the argument is the POST-increment value and 4 is the horn. */
void BrSfxSrcRaceCountdown(int iStep);

#endif /* BR_SFXSRC_H */
