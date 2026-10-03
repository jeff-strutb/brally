/* br_sfx.c -- the sound-effect bank table, filename rule and pitch
 * arithmetic.  See br_sfx.h for the evidence behind every number here.
 *
 * Reference: BRGlide.dll.  D3D addresses in parentheses.
 *
 *   0x1006C010 (0x100730A0)  per-car engine loader -- the three suffixes
 *   0x1006C290 (0x10073320)  bank loader -- the two name tables
 *   0x1006BFD0 (0x10073060)  bank reset  (already ported: BrSndBankReset)
 *   0x1006BFF0 (0x10073080)  bank set-car
 *   0x1006B530 (0x100725C0)  bind group's slot to a channel, copy base rate
 *   0x1006B5F0 (0x10072680)  32.32 ratio -> SetFrequency
 *   0x1006B880 (0x10072910)  start + Hz -> 32.32 ratio
 *   0x1006B6C0 (0x10072750)  float level -> paired-slot SetVolume (this said
 *                            "float Hz -> SetFrequency"; matching the whole
 *                            routine showed its tail reaches the VOLUME
 *                            setter -- see BrSndSetVolumePairF, slice6_77.c)
 *   0x10061470 (0x10068400)  the per-frame car sound driver -- only its
 *                            engine PITCH arithmetic is here (three fragments,
 *                            addresses on each function); the rest of that
 *                            2757-byte function is one-shot triggering over
 *                            globals this module does not model.
 *
 * Nothing in this file opens a file or touches a device.  The platform layer
 * is br_mix.c, which supplies the object slice1_08.c's DirectSound calls go
 * through; this file stays pure so its suite needs neither.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice1_08.h"   /* br_globals: its objects */
#include "br_sfx.h"
#include "br_match.h"    /* BR_STDCALL -- the COM calls below are stdcall */

#include <string.h>

/* ------------------------------------------------------------------------
 * The shipped table, transcribed from .data.
 *
 * Glide 0x100B55F8 and 0x100B5D48; D3D 0x100B5DF0 and 0x100B6540.  All four
 * blocks are byte-identical, which is the second, independent reading that
 * CONVENTIONS.md asks for.
 *
 * Only 115 of the 1872 bytes of each block are non-zero, and they are the 26
 * doubles plus one flag byte per generic row.
 * ---------------------------------------------------------------------- */

#define S1 { 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* marks slot 1 */
#define S3 { 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* marks slot 3 */
#define S0 { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* nothing marked */

const BrSfxGroupDef BrSfxGroups[BR_SFX_GROUPS] = {
    { S0, 11025.0 },   /*  0  engine  "<cc>.wav"                    */
    { S1, 22050.0 },   /*  1  hit-another-car1 | menu front-end5    */
    { S1, 22050.0 },   /*  2  big-impact1      | menu DontQuit      */
    { S1, 22050.0 },   /*  3  bottom-out       | menu Quit          */
    { S1, 11000.0 },   /*  4  s_dirtz          | menu taunt1        */
    { S1, 11000.0 },   /*  5  s_dirt           | menu taunt2        */
    { S1, 11000.0 },   /*  6  s_snow           | menu taunt3        */
    { S1, 11000.0 },   /*  7  s_tarmc          | menu taunt4        */
    { S1, 11000.0 },   /*  8  rn_dirt                               */
    { S1, 30000.0 },   /*  9  rn_dirt   (the table repeats itself)  */
    { S1, 30000.0 },   /* 10  rn_tarm                               */
    { S1, 11000.0 },   /* 11  rn_snow                               */
    { S1, 11000.0 },   /* 12  rn_watr                               */
    { S3, 11000.0 },   /* 13  beep                                  */
    { S3, 11000.0 },   /* 14  beep2                                 */
    { S3, 11000.0 },   /* 15  water                                 */
    { S1, 22050.0 },   /* 16  hit-another-car2                      */
    { S1, 22050.0 },   /* 17  hit-another-car3                      */
    { S1, 22050.0 },   /* 18  big-impact2                           */
    { S1, 22050.0 },   /* 19  big-impact3                           */
    { S1, 22050.0 },   /* 20  taunt1                                */
    { S1, 22050.0 },   /* 21  taunt2                                */
    { S1, 22050.0 },   /* 22  taunt3                                */
    { S1, 22050.0 },   /* 23  taunt4                                */
    { S0, 22050.0 },   /* 24  engine  "<cc>h.wav"                   */
    { S0, 11025.0 }    /* 25  engine  "<cc>r.wav"                   */
};

#undef S1
#undef S3
#undef S0

/* 0x100B7CFC.  Seventeen pointers, the first NULL.  The disc carries three
 * .wav files for each of the sixteen codes. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x100B8140, 26 entries -- the array is exactly BR_SFX_GROUPS long and the
 * menu table starts in the very next dword.  Rows 24..25 are NULL because the
 * engine groups are named per car.
 *
 * Entry 9 really is a second "rn_dirt.wav"; there is no rn_grass or
 * equivalent on the disc, so the duplicate is the shipped behaviour and not a
 * transcription slip. */
static const char *const s_aRaceName[BR_SFX_GROUPS] = {
    NULL,
    "hit-another-car1.wav", "big-impact1.wav", "bottom-out.wav",
    "s_dirtz.wav", "s_dirt.wav", "s_snow.wav", "s_tarmc.wav",
    "rn_dirt.wav", "rn_dirt.wav", "rn_tarm.wav", "rn_snow.wav",
    "rn_watr.wav", "beep.wav", "beep2.wav", "water.wav",
    "hit-another-car2.wav", "hit-another-car3.wav",
    "big-impact2.wav", "big-impact3.wav",
    "taunt1.wav", "taunt2.wav", "taunt3.wav", "taunt4.wav",
    NULL, NULL
};

/* 0x100B81A8, 9 entries, immediately after the race table.  0x100B81D0 --
 * the very next dword -- is the CD track-name table br_audio.h describes, so
 * this one's extent is pinned on both sides. */
#define BR_SFX_MENU_GROUPS 9
static const char *const s_aMenuName[BR_SFX_MENU_GROUPS] = {
    NULL,
    "front-end5.wav", "DontQuit.wav", "Quit.wav",
    "taunt1.wav", "taunt2.wav", "taunt3.wav", "taunt4.wav",
    NULL
};

/* group -> filename suffix, for the three per-car groups. */
/* (port-only engine_suffix removed) */


/* ------------------------------------------------------------------ table */

/* (port-only BrSfxGroupCount removed) */


/* (port-only BrSfxGroupName removed) */


/* (port-only BrSfxGroupBaseRate removed) */


/* (port-only BrSfxGroupSlotUsed removed) */


/* ------------------------------------------------------------- addressing */

/* (port-only BrSfxVoiceIndex removed) */


/* @n64 0x80260B98 located */
/* (port-only BrSfxCarChannel removed) */


/* ------------------------------------------------------------- filenames */

/* strcpy/strcat with a hard bound.  Returns the total length, or -1 if any
 * piece did not fit.  The original builds these in a 0x400-byte stack buffer
 * with no bound at all; the buffer is large enough for every shipped name, so
 * the check is a DEVIATION that cannot change behaviour on real data. */
/* (port-only join3 removed) */


/* (port-only BrSfxCarFileName removed) */


/* (port-only BrSfxGroupFileName removed) */


/* ----------------------------------------------------------------- pitch */

/* MSVC's _ftol, which 0x10074560 is a jump thunk to.  It sets the x87 round
 * control to chop and does a 64-bit fistp, so it truncates toward zero and
 * stores the "integer indefinite" 0x8000000000000000 when the value does not
 * fit -- which includes NaN.
 *
 * The range test is written NEGATED on purpose: an x87 unordered compare sets
 * C0/C3 exactly as "less than" does, so NaN must take the indefinite side.
 * See CONVENTIONS.md. */
/* WHAT IT DOES: the compiler's float-to-integer conversion as the sound code
 * uses it, transcribed because the sound pitch arithmetic depends on how it
 * behaves at the edges. It chops toward zero, and a value too large to
 * convert -- or one that is not a number -- comes out as the processor's
 * "indefinite" value rather than as a clamp. */
/* NOT @implements ANYTHING, and it used to claim 0x1006B880.  That claim was
 * false in both directions: 0x1006B880 is a 201-byte three-argument routine
 * that starts a sound channel (gates on three globals, indexes the voice table
 * `lea eax,[eax+eax*8] / lea ecx,[edi+eax*2]`, calls 0x1006B950, and only then
 * does ratio arithmetic), and it is transcribed in full as BrSfxChanStart in
 * br_sfxsrc.c, which now carries the manifest line.  The _ftol that routine
 * uses is `call 0x10074560`, six bytes of `jmp [0x118F0560]` -- an IMPORT
 * thunk to MSVCRT's _ftol.  So the thing this helper models is not in this
 * binary's code at all and cannot be claimed by any address here. */
/* (port-only br_ftol64 removed) */


/* The 32-bit flavour: the same fistp, of which only the low dword is kept.
 * The low dword of the indefinite value is zero, which is why an overflow
 * here reads as 0 rather than as 0x80000000. */
/* WHAT IT DOES: the 32-bit form of the same conversion, keeping only the
 * bottom half of the result. That is why a value too large to convert reads
 * back as zero here rather than as a huge negative number. */
/* NOT @implements ANYTHING either, and it used to claim 0x1006B6C0.  Same
 * shape of error: 0x1006B6C0 is a 32-byte THREE-argument function ending in a
 * tail call --
 *
 *   1006B6C0  fld dword [esp+0xc]      ; a3
 *   1006B6C4  call 0x10074560          ; _ftol
 *   1006B6C9  mov edx,[esp+4]          ; a1
 *   1006B6D2  lea ecx,[eax+eax]        ; a2 * 2
 *   1006B6D7  call 0x1006B6E0          ; f(a1, a2*2, ftol(a3))
 *
 * -- and this helper is the second instruction of it.
 *
 * UPDATED 2026-09-03: all thirty-two bytes are now matched, as
 * BrSndSetVolumePairF in slice6_77.c beside the FUN_1006b6e0 it tail-calls.
 * The address is claimed there, not here; this helper is still only the
 * truncation and carries no @implements. */
/* (port-only br_ftol32 removed) */


/* (port-only BrSfxRatioFromHz removed) */


/* (port-only BrSfxHzFromRatio removed) */


/* (port-only BrSfxHzFromFloat removed) */


/* ------------------------------------------------------- the engine curve */

/* (port-only BrSfxEngineHz removed) */


/* (port-only BrSfxEngineRatio removed) */


/* (port-only BrSfxEngineHighHz removed) */


/* BrSndBankCarSlot: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: records which car belongs in a given engine-voice slot.
 * Zero means empty; stored codes start at 1. */
/* @implements 0x10073080 d3d BrSndBankSetCar */
void BrSndBankSetCar(int iCar, int iName)
{
    iName++;
    g_0B6540[iCar].iName = iName;
    g_0B6C00[iCar].iName = iName;
    g_0B6C48[iCar].iName = iName;
}

/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The DirectSound buffer behind a voice, seen through the only slot this
 * file uses. The full interface lives in slice1_08.h, which cannot be
 * included here: it types BrSndPDS as BrDSound* where this block needs the
 * plain int the Ghidra bodies were transcribed against. */
typedef struct BrSfxDSBuf BrSfxDSBuf;
typedef struct BrSfxDSBufVtbl {
    void   *aBefore[9];                                          /* +0x00 .. +0x20 */
    int32_t (BR_STDCALL *GetStatus)(BrSfxDSBuf *, uint32_t *);   /* +0x24 */
} BrSfxDSBufVtbl;
struct BrSfxDSBuf { const BrSfxDSBufVtbl *pVtbl; };
/* slice1_08.h's BrSndVoice up to pBuf, field for field (its header cannot
 * be included here, see above): the head holds pointers, so pBuf is not at
 * the original's +0x9C on a 64-bit host */
typedef struct BrSfxVoice {
    void       *pData;
    uint32_t    nDataBytes;
    void       *pFormat;
    uint32_t    f0C;
    int32_t     f10, f14, f18, f1C, f20, f24, f28;
    uint8_t     pad2C[0x70];
    BrSfxDSBuf *pBuf;                                /* +0x9C in the original */
} BrSfxVoice;

/* BrSndVoiceBufIsPlaying: prototype in br_funcs.h */

/* WHAT IT DOES: return whether a sound voice is currently playing. */
/* @implements 0x1006BF50 glide BrSndVoiceIsPlaying */

int BrSndVoiceIsPlaying(int param_1)

{
  int uVar1;

  if ((((*(int *)&DAT_100b51e4[1036]) != 0) && ((((intptr_t)(BrSndPDS))) != 0)) && (BrSndG18290FC != 0)) {
    if ((&(g_apBrSfxChanVoice[0]))[param_1] != 0) {
      uVar1 = BrSndVoiceBufIsPlaying((BrSfxVoice *)(&(g_apBrSfxChanVoice[0]))[param_1]);
      return uVar1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: ask DirectSound whether this voice's buffer is actually
 * playing right now. BrSndVoiceIsPlaying above does the bookkeeping -- sound
 * enabled, device up, slot occupied -- and this does the one device query.
 * A failed query reads as "not playing" rather than propagating an error. */
/* @implements 0x1006BF90 glide BrSndVoiceBufIsPlaying */

int BrSndVoiceBufIsPlaying(BrSfxVoice *pVoice)
{
    uint32_t    status = 0;
    BrSfxDSBuf *pBuf   = pVoice->pBuf;

    if (pBuf->pVtbl->GetStatus(pBuf, &status) == 0)
        return (status & 1) == 1;   /* DSBSTATUS_PLAYING */
    return 0;
}

/* WHAT IT DOES: zero-initialize all entries in the sound-bank state arrays. */
/* @implements 0x1006BFD0 glide BrSndBankClear */

int BrSndBankClear(void)

{
  int i;

  /* the original walks 0x100B6408 and reaches the other two tables at
   * -0x6C0 and +0x48 from the same cursor: fifteen dwords of each */
  for (i = 0; i < 15; i++) {
    ((int32_t *)g_0B6540)[i] = 0;
    ((int32_t *)g_0B6C00)[i] = 0;
    ((int32_t *)g_0B6C48)[i] = 0;
  }
  return;
}

/* BrSndBankSetCar: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSfxCarBankLoad: prototype in br_funcs.h */

/* WHAT IT DOES: initialize the engine-sound bank for a car: set the bank, init the source, play silent. */
/* @implements 0x100612D0 glide BrSfxCarBankInit */
/* @n64 0x80206830 located */

int BrSfxCarBankInit(int param_1,int param_2)

{
  BrSndBankSetCar(param_1,param_2);
  BrSfxCarBankLoad(param_1);
  return BrSfxSrcPlaySilent(param_1 * 2,(DAT_100b32b0[0]),(DAT_100b32bc[0]),(DAT_100b32c0[0]));
}

/* -- the bank loader ---------------------------------------------------- */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B55F8  the voice table, 18 dwords a row */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B5CB8  voice row 24 (engine HIGH)       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B5D00  voice row 25 (engine REV)        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x1184C260  the live group count             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B8140  race-set name table              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B81A8  menu-set name table              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100B7D40  "sfx/" (BossRally.ini SFXDir=)   */
/* BrSndVoiceLoad: prototype in br_funcs.h */

/* WHAT IT DOES: loads a whole sound set into the bank.  Picks the name table
 * and stores the live group count (9 for the menu set, 25 for the race set);
 * for the race set it also walks the 15 engine channels first, loading each
 * marked car's engine trio through the per-car loader and zeroing rows 0, 24
 * and 25 of unmarked channels.  Then for every generic group (rows 1..count-2)
 * it builds "<SFXDir><name>" and loads a voice into each slot the bank marks,
 * zeroing the rest.  Returns 1 when everything loaded (or sound is off), 0
 * when any load failed.
 *
 * RESIDUE (size-exact 459/459, insn-exact 138/138): the engine loop's `i`
 * and its row-24 induction pointer are transposed, esi<->edi, orig i=edi /
 * IV24=esi vs ours i=esi / IV24=edi; every instruction otherwise identical
 * (regnorm 2+2 is the normaliser tripping on reloc'd displacements, the rows
 * sit outside the divergence region).  Whole-loop register transposition,
 * the BrSelLookup / 0x10060F40 class.
 * DEAD (probes w1-w6): rows 24/25 as lockstep ++pointers with the zero arm
 * laid first re-triggers a constant-cache (0 in a reg, 1 in ebx, ppName
 * spilled, 463 B) -- the load arm MUST be the fall-through (`!= 0` first)
 * and the engine loop MUST init i before any row pointer (pV24-first
 * re-triggers the same 463 B shape, w6); i/v declaration order both ways
 * (w2, w3) and head-of-TU placement (w4) are inert; pointer spelling vs
 * indexed spelling of rows 24/25 is codegen-identical once the polarity is
 * right (w5).
 * (thin pre-ledger pass, 7 probes, not counted: w1-w6 above) */
/* @t4-pass 0x1006C290 1 2026-09-09 probes 10 bytes 459 insns 138 regions 2 rows 0 census no  (hand, fn.py variants: literal/guard/index spellings, div-vs-shift, all inert or worse) */
/* @t4-pass 0x1006C290 2 2026-09-09 probes 10 bytes 459 insns 138 regions 2 rows 0 census yes  (hand, fn.py variants: name/decl swaps, store fusion, loop-bound forms, all inert; corpus query at +0x40) */
/* @t3 0x1006C290 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 459/459 insns 138/138 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is the engine loop's i/IV24 esi-edi transposition only (the
 * BrSelLookup class); size- and insn-exact, identical register-blind
 * multiset.  Dead list w1-w6 in the RESIDUE block above plus the two
 * ledger lines.  Do not reopen before the end-grind. */
/* @implements 0x1006C290 glide BrSfxBankLoad */

int BrSfxBankLoad(int iSet)
{
    char **ppName;
    struct BrSndVoice *ok;
    int    row;
    int    cLeft;
    char   buf[1024];
    int    i;
    struct BrSndVoice *v;

    ok = 1;
    if (((*(int *)&DAT_100b51e4[1036]) == 0) || ((((intptr_t)(BrSndPDS))) == 0) || (BrSndG18290FC == 0)) {
        return 1;
    }
    if (iSet == 0) {
        DAT_1184c260 = 9;
        ppName = DAT_100b81a8;
    } else if (iSet == 1) {
        ppName = DAT_100b8140;
        DAT_1184c260 = 0x19;
        for (i = 0; i < 15; i++) {
            if (((int *)g_0B6540)[i] != 0) {
                v = BrSfxCarBankLoad(i / 2);
                if (v == 0)
                    ok = v;
            } else {
                g_aBrSndRow[0].aSlot[i] = 0;
                g_aBrSndRow[24].aSlot[i] = 0;
                g_aBrSndRow[25].aSlot[i] = 0;
            }
        }
    }
    for (row = 1; row < DAT_1184c260 - 1; row++) {
        for (cLeft = 0; cLeft < 15; cLeft++) {
            if (((int *)g_0B6540)[row * 18 + cLeft] != 0) {
                strcpy(buf, g_aBrCfgSfxDir);
                strcat(buf, ppName[row]);
                v = BrSndVoiceLoad(buf);
                g_aBrSndRow[row].aSlot[cLeft] = v;
                if (v == 0)
                    ok = v;
            } else {
                g_aBrSndRow[row].aSlot[cLeft] = 0;
            }
        }
    }
    return ok;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B64A8 ".wav"  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B64A0 "h.wav" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B6498 "r.wav" */

/* WHAT IT DOES: loads (or clears) one car's engine-sound trio.  When sound
 * is off or the bank marks the car's slot empty, it zeroes the car's voice
 * in rows 0, 24 and 25.  Otherwise it builds "<SFXDir><cc>.wav",
 * "<SFXDir><cc>h.wav" and "<SFXDir><cc>r.wav" from the car codes the three
 * bank rows store (code 0 is "no car", so the stored value is car+1 indexing
 * BrSfxCarCode), loads each into its row's voice slot, and returns 0 if any
 * of the three failed, else 1. */
/* @implements 0x1006C010 glide BrSfxCarBankLoad */

int BrSfxCarBankLoad(int iCar)
{
    int  ok;
    char buf[1024];
    int  i2;
    int  code;
    struct BrSndVoice *v;

    i2 = iCar * 2;
    ok = 1;
    if (((*(int *)&DAT_100b51e4[1036]) == 0) || ((((intptr_t)(BrSndPDS))) == 0) || (BrSndG18290FC == 0)
        || (code = ((int *)g_0B6540)[i2]) == 0) {
        g_aBrSndRow[0].aSlot[i2] = 0;
        g_aBrSndRow[24].aSlot[i2] = 0;
        g_aBrSndRow[25].aSlot[i2] = 0;
    } else {
        strcpy(buf, g_aBrCfgSfxDir);
        strcat(buf, BrSfxCarCode[code]);
        strcat(buf, DAT_100b64a8);
        v = BrSndVoiceLoad(buf);
        g_aBrSndRow[0].aSlot[i2] = v;
        if (v == 0)
            ok = 0;
        strcpy(buf, g_aBrCfgSfxDir);
        strcat(buf, BrSfxCarCode[((int *)g_0B6C00)[i2]]);
        strcat(buf, DAT_100b64a0);
        v = BrSndVoiceLoad(buf);
        g_aBrSndRow[24].aSlot[i2] = v;
        if (v == 0)
            ok = 0;
        strcpy(buf, g_aBrCfgSfxDir);
        strcat(buf, BrSfxCarCode[((int *)g_0B6C48)[i2]]);
        strcat(buf, DAT_100b6498);
        v = BrSndVoiceLoad(buf);
        g_aBrSndRow[25].aSlot[i2] = v;
        if (v == 0)
            ok = 0;
    }
    return ok;
}

