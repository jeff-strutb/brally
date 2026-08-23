/* br_racestart.c -- see br_racestart.h.
 *
 * RESPONSIBILITY: the rules of a race.  Glide 0x100628B0 (D3D 0x10069840),
 * the last thing state 3 does before the game starts running.
 */
#include "br_racestart.h"

#include <stddef.h>

/* ==========================================================================
 * Globals owned ELSEWHERE.  Declared, never defined here -- the original had
 * one object per address and so must this port (CONVENTIONS.md, "Aliased
 * storage: a link-clean bug").  Each is the symbol its owning header already
 * publishes, at the address the listing reads.
 * ========================================================================== */

/* br_racestep.h */
extern int32_t g_brRaceTick;         /* 0x10226A44 */
extern int32_t g_brRaceNCar;         /* 0x100B2F04 */
extern int32_t g_brRaceNEntrant;     /* 0x100B3858 */

/* br_carphys.h */
extern int32_t g_brCarPhysWeather;   /* 0x104B15E8 */

/* br_appstart.h */
extern int32_t g_brCfgGameMode;         /* 0x100A9360 */
extern int32_t g_brCfgChosenWeather;    /* 0x10226E80 */
extern int32_t g_brCfgHandlingType;     /* 0x1007B320 */
extern int32_t g_brCfgTransmission;     /* 0x1007B324 */
extern int32_t g_brCfgSuspensionType;   /* 0x1007B328 */
extern int32_t g_brCfgTireType;         /* 0x1007B32C */

/* ==========================================================================
 * Globals this module owns.  All eleven were grepped across port/ first and
 * had no owner anywhere.  .bss in the original, so zero at load.
 * ========================================================================== */
int32_t  g_brRace6EC760;
int32_t  g_brRace6E9A34;
int32_t  g_brRaceB71A68;
int32_t  g_brRaceB71A6C;
int32_t  g_brRace18EEED8;
int32_t  g_brRace5CCB80;
int32_t  g_brRace6ED6DC;
int32_t  g_brRaceB71288;
int32_t  g_brRace4ABB20;
int32_t  g_brRace4ABB24;
int32_t  g_brRace5BC8D8;
uint32_t g_brRace6EECC8;

static int32_t s_aSkipped[BR_RACESTART_NSTEPS];
static int32_t s_cSpun;

static int hooked(BrRaceStartStep step, int fHooked)
{
    if (fHooked) {
        return 1;
    }
    ++s_aSkipped[step];
    return 0;
}

/* ==========================================================================
 * Byte-wise stores into the equipment record.
 *
 * The record is written to disc verbatim (br_save.h: it IS the save file's
 * 0x200-byte payload, at file offset 0x008), so its byte order is externally
 * visible.  CONVENTIONS.md forbids overlaying a struct on such an image, and
 * the original is x86, so these go out little-endian a byte at a time.
 * ========================================================================== */
static void put_u32le(unsigned char *pb, int32_t off, uint32_t v)
{
    pb[off + 0] = (unsigned char)(v & 0xFFu);
    pb[off + 1] = (unsigned char)((v >> 8) & 0xFFu);
    pb[off + 2] = (unsigned char)((v >> 16) & 0xFFu);
    pb[off + 3] = (unsigned char)((v >> 24) & 0xFFu);
}

static void put_u16le(unsigned char *pb, int32_t off, uint32_t v)
{
    pb[off + 0] = (unsigned char)(v & 0xFFu);
    pb[off + 1] = (unsigned char)((v >> 8) & 0xFFu);
}

/* ==========================================================================
 * 0x1002F6C0 -- eleven bytes, one store.
 * ========================================================================== */
/* WHAT IT DOES: sets one race global to a fixed value. What that value
 * controls is not established here. */
/* @implements 0x1002F6C0 glide BrRaceSub1002F6C0 */
void BrRaceSub1002F6C0(void)
{
    g_brRace6EECC8 = BR_RACESTART_6EECC8_VALUE;
}

/* The null step itself: glide 0x10008D60 / D3D 0x10008B80, a one-byte `ret`.
 * The original does not receive this step as an argument -- it pushes the
 * stub's address as an IMMEDIATE (`push 0x10008D60`), so the address has to
 * come from a real function here too, not from a parameter. */
static void BrRaceNullStep(void)
{
}

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
void BrRaceEntrantCountSet(int32_t n, BrGameStepFn pfnNullStep)
{
    (void)pfnNullStep;
    g_brRaceNEntrant = n;
    BrGameStepSet(BrRaceNullStep);
}

/* ==========================================================================
 * Glide 0x100628B0 / D3D 0x10069840
 * ========================================================================== */
void BrRaceStart(const BrRaceStartOps *pOps,
                 BrGameStepFn pfnRaceStep, BrGameStepFn pfnNullStep)
{
    int32_t        mode;
    unsigned char *pRec;
    int            i;

    if (pOps == NULL) {
        return;
    }

    /* 0x100628B0 / 0x100628BD.  Two plain copies. */
    g_brRace6EC760 = g_brRaceB71A68;
    g_brRace6E9A34 = g_brRaceB71A6C;

    /* 0x100628C8. */
    if (hooked(BR_RACESTART_100703A0, pOps->pfn100703A0 != NULL)) {
        pOps->pfn100703A0(pOps->pUser);
    }

    /* 0x100628CD .. 0x100628DC.  `cmp eax,4 ; sete dl` with edx zeroed, so
     * the result is strictly 0 or 1 -- not the mode value.  br_racestep.h
     * cites this site independently. */
    mode = g_brCfgGameMode;
    g_brRaceTick = (mode == BR_RACESTART_TICK_MODE) ? 1 : 0;

    /* 0x100628E2 / 0x100628E8.  Both from the zeroed esi. */
    g_brRace18EEED8 = 0;
    g_brRace5CCB80 = 0;

    /* 0x100628EE / 0x100628F3 / 0x100628F8.  The last two are empty. */
    if (hooked(BR_RACESTART_1002DEC3, pOps->pfn1002DEC3 != NULL)) {
        pOps->pfn1002DEC3(pOps->pUser);
    }
    if (hooked(BR_RACESTART_1002E334, pOps->pfn1002E334 != NULL)) {
        pOps->pfn1002E334(pOps->pUser);
    }
    if (hooked(BR_RACESTART_1002E2E3, pOps->pfn1002E2E3 != NULL)) {
        pOps->pfn1002E2E3(pOps->pUser);
    }

    /* 0x100628FD.  Ten car records at 0x10AF1208 (stride 0x2B68, the
     * `g_pBrRaceCar` br_racestep.h names) and ten 0x15C records at
     * 0x106ED708 -- the count is arithmetic, 0x2B680 / 0x2B68. */
    if (hooked(BR_RACESTART_10062870, pOps->pfn10062870 != NULL)) {
        pOps->pfn10062870(pOps->pUser);
    }

    /* 0x10062902.  Empty. */
    if (hooked(BR_RACESTART_1002E136, pOps->pfn1002E136 != NULL)) {
        pOps->pfn1002E136(pOps->pUser);
    }

    /* 0x10062908.  One cdecl argument, the zeroed esi. */
    if (hooked(BR_RACESTART_CURSORPAIR, pOps->pfnCursorPairSet != NULL)) {
        pOps->pfnCursorPairSet(pOps->pUser, 0);
    }

    /* 0x1006290D.  The mode is RELOADED from the global here; nothing above
     * writes it, so the value is the same, but the reload is what makes the
     * two tests independent readings in the listing. */
    mode = g_brCfgGameMode;

    /* 0x1006291A .. 0x1006294F.  Three 16-bit stores of 0xFFFF, and the
     * ORDER is +0xF2, +0xF0, +0xF4 -- not ascending.  The record pointer is
     * reloaded before each in the original. */
    if (mode == BR_RACESTART_FFFF_MODE_A || mode == BR_RACESTART_FFFF_MODE_B) {
        if (hooked(BR_RACESTART_EQUIPRECORD, pOps->pfnEquipRecord != NULL)) {
            pRec = pOps->pfnEquipRecord(pOps->pUser);
            if (pRec != NULL) {
                put_u16le(pRec, BR_RACESTART_OFF_FFFF_1, 0xFFFFu);
                put_u16le(pRec, BR_RACESTART_OFF_FFFF_2, 0xFFFFu);
                put_u16le(pRec, BR_RACESTART_OFF_FFFF_3, 0xFFFFu);
            }
        }
    }

    /* 0x1006294F.  One argument, edi == 1. */
    BrRaceEntrantCountSet(BR_RACESTART_NENTRANT, pfnNullStep);

    /* 0x10062958.  The SAME null step installed again, immediately.  A
     * redundant store in the original, kept because removing it would be a
     * claim about the original that this port has no evidence for. */
    BrGameStepSet(pfnNullStep);

    /* 0x10062965 .. 0x10062992.  Note g_brCarPhysWeather is set to 1 here
     * and overwritten from the config below; both writes are the
     * original's. */
    g_brRace6ED6DC = 0;
    g_brRaceB71288 = 0;
    g_brRaceNCar = BR_RACESTART_NCAR;
    g_brRaceNEntrant = BR_RACESTART_NENTRANT;
    g_brCarPhysWeather = BR_RACESTART_WEATHER_INIT;
    g_brRace4ABB20 = 0;
    g_brRace4ABB24 = 0;

    /* 0x1006299C.  The race step goes in.  br_gamestep.h: the race is not a
     * phase, it is whatever this slot holds. */
    BrGameStepSet(pfnRaceStep);

    /* 0x100629A4. */
    g_brRace5BC8D8 = BR_RACESTART_5BC8D8;

    /* 0x100629AE .. 0x100629D2, then 0x100629D4 .. 0x100629FA.  Two
     * identical blocks differing only in the phase number.  Each is
     * `0x1002F6C0() ; trace() ; while (!load(phase, 1)) ;` and the compiler
     * rotated the loop, which does not change how many times it runs. */
    for (i = 0; i < 2; i++) {
        int32_t phase = (i == 0) ? BR_RACESTART_LOAD_PHASE_A
                                 : BR_RACESTART_LOAD_PHASE_B;
        int     spins;

        BrRaceSub1002F6C0();

        /* 0x100629B3 / 0x100629D9: `call 0x10008D60` with NO arguments and
         * no cleanup, unlike the four at the tail.  A fifth folded empty
         * function; recorded through the same hook with zeros so the call
         * SITE is visible without inventing arguments it does not have. */
        if (hooked(BR_RACESTART_TRACE, pOps->pfnTrace != NULL)) {
            pOps->pfnTrace(pOps->pUser, 0u, 0u);
        }

        /* DEVIATION: bounded.  See BR_RACESTART_LOAD_SPINS. */
        for (spins = 0; spins < BR_RACESTART_LOAD_SPINS; spins++) {
            int32_t done = 0;
            if (hooked(BR_RACESTART_LOADSTEP, pOps->pfnLoadStep != NULL)) {
                done = pOps->pfnLoadStep(pOps->pUser, phase,
                                         BR_RACESTART_LOAD_ARG2);
            }
            /* `test al,al` -- only the LOW BYTE decides.  A callee returning
             * 0x100 would be "not done" in the original. */
            if ((done & 0xFF) != 0) {
                break;
            }
        }
        if (spins >= BR_RACESTART_LOAD_SPINS) {
            ++s_cSpun;
        }
    }

    /* 0x100629FC. */
    if (hooked(BR_RACESTART_10059E00, pOps->pfn10059E00 != NULL)) {
        pOps->pfn10059E00(pOps->pUser);
    }

    /* 0x10062A01 .. 0x10062A1A.  Mode 0 falls THROUGH into the mode-2 arm's
     * store, so 0 and 2 share it and every other mode skips it. */
    mode = g_brCfgGameMode;
    if (mode == 0) {
        if (hooked(BR_RACESTART_SELLOOKUP, pOps->pfnSelLookup != NULL)) {
            pOps->pfnSelLookup(pOps->pUser);
        }
    }
    if (mode == 0 || mode == 2) {
        g_brRaceNEntrant = BR_RACESTART_NENTRANT;
    }

    /* 0x10062A1C .. 0x10062A77. */
    g_brCarPhysWeather = g_brCfgChosenWeather;

    if (hooked(BR_RACESTART_EQUIPRECORD, pOps->pfnEquipRecord != NULL)) {
        pRec = pOps->pfnEquipRecord(pOps->pUser);
        if (pRec != NULL) {
            /* DEVIATION: the original reloads [0x10AF2094] before each of
             * the four stores.  Nothing between them can change it -- the
             * intervening instructions are loads of .data constants and
             * stores to the record -- so the pointer is fetched once here.
             *
             * The last two are CROSSED and that is not a slip: 0x1007B328
             * (SuspensionType) goes to +0x104 and 0x1007B32C (TireType) to
             * +0x100, per 0x10062A5B/0x10062A60 and 0x10062A6B/0x10062A71. */
            put_u32le(pRec, BR_RACESTART_OFF_HANDLING,
                      (uint32_t)g_brCfgHandlingType);
            put_u32le(pRec, BR_RACESTART_OFF_TRANSMISSION,
                      (uint32_t)g_brCfgTransmission);
            put_u32le(pRec, BR_RACESTART_OFF_SUSPENSION,
                      (uint32_t)g_brCfgSuspensionType);
            put_u32le(pRec, BR_RACESTART_OFF_TIRE,
                      (uint32_t)g_brCfgTireType);
        }
    }

    /* 0x10062A77 .. 0x10062AAD.  Four calls, all folded onto the one-byte
     * `ret`.  The argument pairs are the only evidence of what they were. */
    if (hooked(BR_RACESTART_TRACE, pOps->pfnTrace != NULL)) {
        pOps->pfnTrace(pOps->pUser, BR_RACESTART_TRACE_A1_1,
                       BR_RACESTART_TRACE_A2_1);
    }
    if (hooked(BR_RACESTART_TRACE, pOps->pfnTrace != NULL)) {
        pOps->pfnTrace(pOps->pUser, BR_RACESTART_TRACE_A1_2,
                       BR_RACESTART_TRACE_A2_2);
    }
    if (hooked(BR_RACESTART_TRACE, pOps->pfnTrace != NULL)) {
        pOps->pfnTrace(pOps->pUser, BR_RACESTART_TRACE_A1_3,
                       BR_RACESTART_TRACE_A2_3);
    }
    if (hooked(BR_RACESTART_TRACE, pOps->pfnTrace != NULL)) {
        pOps->pfnTrace(pOps->pUser, BR_RACESTART_TRACE_A1_4,
                       BR_RACESTART_TRACE_A2_4);
    }

    /* 0x10062AB5.  Empty. */
    if (hooked(BR_RACESTART_1002E32F, pOps->pfn1002E32F != NULL)) {
        pOps->pfn1002E32F(pOps->pUser);
    }
}

int32_t BrRaceStartSkipped(BrRaceStartStep step)
{
    if (step < 0 || step >= BR_RACESTART_NSTEPS) {
        return 0;
    }
    return s_aSkipped[step];
}

int32_t BrRaceStartSpun(void)
{
    return s_cSpun;
}

void BrRaceStartResetForTest(void)
{
    int i;
    for (i = 0; i < BR_RACESTART_NSTEPS; i++) {
        s_aSkipped[i] = 0;
    }
    s_cSpun = 0;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int BrG_6C661C;
extern int BrG_6C6624;
extern int DAT_106ed6a8;
extern int DAT_106ed6b0;
int FUN_1002db88();
int FUN_100627b0();

/* WHAT IT DOES: set difficulty flags from a 0-4 level selector (AI, rubber-banding, etc.). */
/* @implements 0x100627B0 glide BrRaceDifficultySet */

int BrRaceDifficultySet(int param_1)

{
  DAT_106ed6b0 = 0;
  BrG_6C6624 = 0;
  BrG_6C661C = 0;
  switch(param_1) {
  case 0:
    DAT_106ed6a8 = 0;
    return;
  case 1:
    DAT_106ed6a8 = 1;
    return;
  case 2:
    DAT_106ed6a8 = 1;
    BrG_6C6624 = 1;
    return;
  case 3:
    DAT_106ed6a8 = 1;
    DAT_106ed6b0 = 1;
    return;
  case 4:
    DAT_106ed6a8 = 1;
    BrG_6C661C = 1;
  }
  return;
}

/* WHAT IT DOES: set difficulty and apply it to the current race state. */
/* @implements 0x10062830 glide BrRaceDifficultyApply */

int BrRaceDifficultyApply(int param_1)

{
  FUN_100627b0(param_1);
  FUN_1002db88();
  return;
}

#endif /* BR_MATCHING_BUILD */
