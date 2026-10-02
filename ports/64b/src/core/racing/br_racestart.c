/* br_racestart.c -- see br_racestart.h.
 *
 * RESPONSIBILITY: the rules of a race.  Glide 0x100628B0 (D3D 0x10069840),
 * the last thing state 3 does before the game starts running.
 */
#include "br_race.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include "br_racestart.h"

#include <stddef.h>

/* ==========================================================================
 * Globals owned ELSEWHERE.  Declared, never defined here -- the original had
 * one object per address and so must this port (CONVENTIONS.md, "Aliased
 * storage: a link-clean bug").  Each is the symbol its owning header already
 * publishes, at the address the listing reads.
 * ========================================================================== */

/* br_racestep.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x10226A44 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100B2F04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B3858 */

/* br_carphys.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x104B15E8 */

/* br_appstart.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100A9360 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10226E80 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x1007B320 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x1007B324 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B328 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x1007B32C */

/* ==========================================================================
 * Globals this module owns.  All eleven were grepped across port/ first and
 * had no owner anywhere.  .bss in the original, so zero at load.
 * ========================================================================== */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

static int32_t s_aSkipped[BR_RACESTART_NSTEPS];
static int32_t s_cSpun;

/* (port-only hooked removed) */


/* ==========================================================================
 * Byte-wise stores into the equipment record.
 *
 * The record is written to disc verbatim (br_save.h: it IS the save file's
 * 0x200-byte payload, at file offset 0x008), so its byte order is externally
 * visible.  CONVENTIONS.md forbids overlaying a struct on such an image, and
 * the original is x86, so these go out little-endian a byte at a time.
 * ========================================================================== */
/* (port-only put_u32le removed) */


/* (port-only put_u16le removed) */


/* ==========================================================================
 * 0x1002F6C0 -- eleven bytes, one store.
 * ========================================================================== */
/* WHAT IT DOES: sets one race global to a fixed value. What that value
 * controls is not established here. */
/* @implements 0x1002F6C0 glide BrRaceSub1002F6C0 */
/* @n64 0x80242940 located */
void BrRaceSub1002F6C0(void)
{
    g_brRace6EECC8 = BR_RACESTART_6EECC8_VALUE;
}

/* The null step itself: glide 0x10008D60 / D3D 0x10008B80, a one-byte `ret`.
 * The original does not receive this step as an argument -- it pushes the
 * stub's address as an IMMEDIATE (`push 0x10008D60`), so the address has to
 * come from a real function here too, not from a parameter. */
/* BrRaceNullStep: the original function is BrPodNop */

/* ==========================================================================
 * 0x10062850 -- twenty-three bytes.
 *
 * `mov eax,[esp+4]` then `mov [0x100B3858],eax`, then `push 0x10008D60` and
 * a cdecl call to 0x1002E317.  So the entrant count and the null step are
 * one operation in the original, and 0x100628B0 then repeats BOTH of them
 * separately a few instructions later.  Both repeats are preserved.
 *
 * The original takes ONE argument.  The push is an immediate, not a reload of
 * a second stack slot, and 0x1006294F calls it with a single argument (edi).
 * `pfnNullStep` therefore has no counterpart in the original and is ignored;
 * under cdecl an unused trailing parameter costs the callee nothing, so the
 * body is byte-identical to the one-argument original.  The parameter is
 * still in the signature only because br_racestart.h publishes it.
 * ========================================================================== */
/* WHAT IT DOES: records how many cars are in the race and, in the same
 * breath, installs the do-nothing frame step -- so the game stops doing per-
 * frame work while the race is being set up. The two really are one
 * operation in the original, and the routine that calls it repeats both a
 * moment later. */
/* @implements 0x10062850 glide BrRaceEntrantCountSet */
/* @n64 0x80210F4C located */
void BrRaceEntrantCountSet(int32_t n)
{
    g_brRaceNEntrant = n;
    BrGameStepSet(BrPodNop);
}

/* ==========================================================================
 * Glide 0x100628B0 / D3D 0x10069840
 * ========================================================================== */
/* (port-only BrRaceStart removed) */


/* (port-only BrRaceStartSkipped removed) */


/* (port-only BrRaceStartSpun removed) */


/* (port-only BrRaceStartResetForTest removed) */


/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1005c560: prototype in br_funcs.h */
/* BrEntInit_1002F680: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1002db88: prototype in br_funcs.h */
/* FUN_100627b0: prototype in br_funcs.h */

/* WHAT IT DOES: set difficulty flags from a 0-4 level selector (AI, rubber-banding, etc.). */
/* @implements 0x100627B0 glide BrRaceDifficultySet */

int BrRaceDifficultySet(int param_1)

{
  (*(int *)((char *)&g_aBrEntRecs + 0x80)) = 0;
  (*(int *)((char *)&g_aBrEntRecs + 0x84)) = 0;
  (*(int *)((char *)&g_aBrEntRecs + 0x7C)) = 0;
  switch(param_1) {
  case 0:
    (*(int *)((char *)&g_aBrEntRecs + 0x78)) = 0;
    return;
  case 1:
    (*(int *)((char *)&g_aBrEntRecs + 0x78)) = 1;
    return;
  case 2:
    (*(int *)((char *)&g_aBrEntRecs + 0x78)) = 1;
    (*(int *)((char *)&g_aBrEntRecs + 0x84)) = 1;
    return;
  case 3:
    (*(int *)((char *)&g_aBrEntRecs + 0x78)) = 1;
    (*(int *)((char *)&g_aBrEntRecs + 0x80)) = 1;
    return;
  case 4:
    (*(int *)((char *)&g_aBrEntRecs + 0x78)) = 1;
    (*(int *)((char *)&g_aBrEntRecs + 0x7C)) = 1;
  }
  return;
}

/* WHAT IT DOES: set difficulty and apply it to the current race state. */
/* @implements 0x10062830 glide BrRaceDifficultyApply */

int BrRaceDifficultyApply(int param_1)

{
  BrRaceDifficultySet(param_1);
  BrEntGfxRebindAll();
  return;
}

/* WHAT IT DOES: walk the 16 entity slots (base 0x10AF1208, stride 0x2B68) in step with
 * their pad blocks (base 0x106ED708, stride 0x15C), running 0x1005C560 on each entity and
 * BrEntInit on each pad block. Pointer compare is SIGNED (jl). */
/* @implements 0x10062870 glide BrEntSlotsReset */
/* @n64 0x80200154 located */

void BrEntSlotsReset(void)

{
  int i;

  for (i = 0; i < 16; i++) {
    BrRaceCarReset(&g_aBrRaceCar[i]);
    BrEntInit(&g_aBrEnts[i]);
  }
}

/* 0x100628B0 -- the state-3 race start, transcribed. Its port body is
 * BrRaceStart above; this is the original's shape. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100703a0: prototype in br_funcs.h */
/* FUN_1002dec3: prototype in br_funcs.h */
/* BrNop_1002E334: prototype in br_funcs.h */
/* BrNop_1002E2E3: prototype in br_funcs.h */
/* BrNop_1002E136: prototype in br_funcs.h */
/* BrRaceStep_10019A70: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* FUN_10062850: prototype in br_funcs.h */
/* BrPodNop: prototype in br_funcs.h */
/* BrSaveLoad: prototype in br_funcs.h */
/* BrUiVolumeApply: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrNop_1002E32F: prototype in br_funcs.h */

/* WHAT IT DOES: the last thing state 3 does before the race runs. Copies
 * the two pending race words in, runs the per-race resets, sets the tick
 * flag for mode 4, blanks three equipment slots for modes 1 and 6, installs
 * the null step then the real race step, resets the car/entrant counts and
 * the weather, spins the two save-load phases until each reports done,
 * applies the volume, picks the selection for mode 0, copies the four
 * configured car options into the equipment record and reports three
 * structure sizes to the (empty) trace hook. */
/* @implements 0x100628B0 glide BrGlRaceStart */

void BrGlRaceStart(void)

{
  g_brRace6EC760 = (*(int32_t *)&g_brItemIconCount);
  g_brRace6E9A34 = g_brRaceB71A6C;
  BrSub100770C0();
  (*(int *)&g_brRaceTick) = (unsigned int)((*(int *)&g_brRaceRules.mode) == 4);
  g_brRace18EEED8 = 0;
  (*(int *)&DAT_105ccb68[6]) = 0;
  FUN_1002dec3();
  BrNop_1002E334();
  BrNop_1002E2E3();
  BrEntSlotsReset();
  BrNop_1002E136();
  BrCursorPairSet(0);
  if (((*(int *)&g_brRaceRules.mode) == 1) || ((*(int *)&g_brRaceRules.mode) == 6)) {
    *(unsigned short *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0xf2) = 0xffff;
    *(unsigned short *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0xf0) = 0xffff;
    *(unsigned short *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0xf4) = 0xffff;
  }
  BrRaceEntrantCountSet(1);
  BrGameStepSet(BrPodNop);
  (*(int32_t *)((char *)&g_aBrEntRecs + 0xAC)) = 0;
  g_brRaceB71288 = 0;
  (*(int *)&g_BrCarCount) = 2;
  (*(int *)&g_brRaceNEntrant) = 1;
  (*(int32_t *)&DAT_104b15e8) = 1;
  DAT_104abb20 = 0;
  (*(float *)&g_brRace4ABB24) = 0.0f;
  BrGameStepSet(BrRaceStep);
  (*(int *)&g_brRace5BC8D8) = 8;
  BrRaceSub1002F6C0();
  BrPodNop();
  while (BrSaveLoad(0,1) == 0) {
  }
  BrRaceSub1002F6C0();
  BrPodNop();
  while (BrSaveLoad(2,1) == 0) {
  }
  BrUiVolumeApply();
  if ((*(int *)&g_brRaceRules.mode) == 0) {
    BrSelLookup();
    (*(int *)&g_brRaceNEntrant) = 1;
  }
  else if ((*(int *)&g_brRaceRules.mode) == 2) {
    (*(int *)&g_brRaceNEntrant) = 1;
  }
  (*(int32_t *)&DAT_104b15e8) = g_226e80;
  *(int *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0xf8) = g_7b320;
  *(int *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0xfc) = g_7b324;
  *(int *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0x104) = g_7b328;
  *(int *)((((intptr_t)(g_aBrRaceCar[0].pEquip))) + 0x100) = g_7b32c;
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrNop_1002E32F();
  return;
}

