/* br_racebegin.c -- see br_racebegin.h.
 *
 * RESPONSIBILITY: the rules of a race.  The opening of Glide 0x10019A70 and
 * the whole of its one-time arm, 0x10019A70..0x1001AB6F, plus the seven small
 * functions below it in .text that only it reaches.
 *
 * Transcribed from orig/BRGlide.dll.  Every branch carries the address of the
 * instruction it is, so the two can be diffed.
 */
/* Header takes the race-step body as an argument; the original is void and
 * pushes 0x10019A70 / 0x1002C500 as an immediate. */
#define BrRaceEnterOutro BrRaceEnterOutro_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "br_racebegin.h"
#include "slice3_41.h"
#undef BrRaceEnterOutro

#include <stddef.h>
#include <string.h>

#include "br_gamestep.h"   /* BrGameStepSet -- 0x1002E317, already ported */
/* initialised in the original source (restored: the 64-bit core keeps them
 * private to this file; no Glide relocation places them elsewhere) */
int32_t  g_brRaceClockLen    = BR_RACEBEGIN_CLOCK_LEN;

/* ==========================================================================
 * Globals owned ELSEWHERE.  Declared through their owning headers, never
 * redefined here -- the original had one object per address and so must this
 * port (CONVENTIONS.md, "Aliased storage: a link-clean bug").
 *
 *   br_racestep.h   g_pBrRaceCar 0x10AF1208, g_pBrRaceDriver 0x10AF07F8,
 *                   g_brRaceNDriver 0x100B2F00, g_brRaceNCar 0x100B2F04,
 *                   g_brRaceNEntrant 0x100B3858, g_brRaceReplay 0x105CCB88,
 *                   g_brRacePaused 0x105CCB5C, g_brRaceNet 0x10226A48,
 *                   g_brRaceSubstate 0x105CCB94, g_brRaceRules.mode 0x100A9360
 *
 * 0x100A9360 has TWO host objects with storage -- see the banner in
 * br_racebegin.h.  This module reads br_racestep.c's.
 * ========================================================================== */

/* br_carphys.h's 0x104B15E8.  Declared rather than included for the same
 * reason br_racestart.c declares it: including that header drags the tyre
 * model behind a setup routine. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x104B15E8 */

/* br_appstart.h's three config globals, same treatment. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x100B3014 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10226E7C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10226E80 */

/* 0x10B71648, the local player's name.  0x10006250 is handed it verbatim and
 * 0x10019C9F copies it into 0x10AF1350; neither length is bounded in the
 * original (it is `repne scasb` then `rep movsd`).  The port keeps a bounded
 * copy -- a DEVIATION, and the only one in the transcription that changes
 * what the code CAN do rather than what it does. */
#define BR_RACEBEGIN_NAME_MAX 64
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10B71648 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10AF1350 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10AF134C */

/* ==========================================================================
 * The globals this module owns.  Every one was grepped across port/ before
 * being given storage and had no owner anywhere.  Initial values are the
 * shipped image's where .data holds one, and zero where the address is .bss.
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105BC900, .bss  */
/* 64-bit core: declared once, in br_globals.h or its struct's header *//* 0x100A9358 == 5   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* 0x100A935C == -1  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                           /* 0x105CCB90        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                          /* 0x105CCB7C        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                          /* 0x105BCAE4        */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x106EEEFC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x105BC7C0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105BC7C4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x105BC7CC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x105BC7DC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105BC7D4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105BC7D0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x105BC7E4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105BC7E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7D8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105BCAE8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105BC778 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7C8 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100AA044, .data == 1 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105BC8D8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC8E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x11778850 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105BC810 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105CCB9C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BCAE0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x105BC7B8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x105BC760 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x105CCB8C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105CCB98 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x106ED684 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105BC768 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x105BC884 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105BC888 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x106ED6D8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AF2090 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10B1CF10 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AF6724 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x106EA3F4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10226A4C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x106E86C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x106E8720 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AF397C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x11778848 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AF4C00..0x10AF4C0C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A5EA8, .data == 1 */

uint8_t  g_aBrRaceBeginRecHdr[BR_RACEBEGIN_MAXREC][BR_RACEBEGIN_HDR_LEN];

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100A5EB0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                     /* 0x100A5EBC, .data holds a

/* 64-bit core: declared once, in br_globals.h or its struct's header */                      /* 0x106E9A2C */

/* 0x100A6B68's eight-byte -1 prefix.  See the banner: the disassembly reads
 * it as the head of a format string and it is two int32 slots. */
int32_t  g_aBrRaceBeginTexSlot[BR_RACEBEGIN_MAXREC];

/* ==========================================================================
 * The frontier
 * ========================================================================== */

BrRaceBeginOps g_brRaceBeginOps;

static int32_t s_aSkipped[BR_RB_NOPS];
static int32_t s_cRecClamped;
static int32_t s_cReplayPaintSkipped;

static const char *const s_aOpName[BR_RB_NOPS] = {
    "0x1006E280 tick ms",        "0x10008D60 trace (a bare ret)",
    "0x1000CB80",                "*0x10B7352C",
    "*0x118ED1E8",               "*0x100B849C",
    "*0x10B73528",               "0x10031140 BrTrackLoadHandling (PORTED)",
    "0x100311C0 track load",     "0x1006E030",
    "0x100189C0",                "0x1002BF24",
    "0x100353C0",                "0x1006FD50 car model set",
    "0x100609D0",                "0x10005CD0+0x10006400+0x10004E00 net open",
    "0x100060A0 net session",    "0x10006250 net name",
    "0x10009BA0 net count",      "0x100060B0 net next",
    "0x1001C6A0 net add",        "0x10069A80 play movie",
    "0x10063B60",                "0x10063A00",
    "0x10063A40",                "0x10063DD0",
    "0x100627B0",                "0x10062830",
    "0x10061310",                "0x10034870 vector transform",
    "0x100347F0 vector length",  "0x1005C490",
    "0x1006FCE0",                "0x1005E7B0",
    "0x1006FCB0",                "0x10001CF0",
    "0x100BCDD0 car image",      "*0x118ED1C4 texture create",
    "*0x118ED1C0 texture download", "0x1006D280 string",
    "0x10008EF0 fatal",          "0x10030270 blob load",
    "0x1002ECAC",                "0x10033B50",
    "0x1002E13B",                "0x10060E30",
    "0x100181A0 ramp A",         "0x10018230 ramp B",
    "0x10018290 ramp C",         "0x10002C00",
    "0x10002AF0",                "0x10002D30",
    "0x10013E80",                "car+0xE8C equipment record",
    "car+0x29C0 control block",  "0x100BCAB0 difficulty",
    "0x10B71530",                "0x106EEE3C specials list",
    "0x1005E690 AI controller",  "0x10019840",
    "0x10032E40",                "0x1006E3F0",
    "0x1006E360",                "0x1005F530",
    "0x10002570 sqrt",           "0x1006F170"
};

/* (port-only BrRaceBeginSkipped removed) */


/* (port-only BrRaceBeginOpName removed) */


/* (port-only BrRaceBeginReset removed) */


/* (port-only BrRaceBeginRecClamped removed) */


/* (port-only BrRaceBeginReplayPaintSkipped removed) */


/* A hook that is not installed is SKIPPED and COUNTED.  Nothing below ever
 * substitutes a result for one: the two ops with a return value hand back the
 * value that means "it did not happen" (0 ticks, no texture). */
static int have(BrRaceBeginOp op, const void *pfn)
{
    if (pfn != NULL)
        return 1;
    ++s_aSkipped[op];
    return 0;
}

#define OP(name, field)  have((name), (const void *)g_brRaceBeginOps.field)

/* 0x10008D60 is a single `c3` -- CONVENTIONS.md pins it -- so the folded
 * empty functions it stands for really do nothing and omitting the call is
 * EXACT, not a frontier.  It is still counted, because the count is the only
 * evidence the arm reached the point it stands at. */
/* (port-only trace removed) */


/* (port-only ctl_of removed) */


/* (port-only car_at removed) */


/* ==========================================================================
 * 0x10019890 -- the pre-frame colour submissions
 * ========================================================================== */

/* WHAT IT DOES: nudges the screen's colour settings three times around two
 * pieces of drawing work, and does nothing at all while the game is paused.
 * All three colour submissions go to a routine that is empty in this build,
 * so what the shipped game gets out of them is the two calls between them. */
/* @implements 0x10019890 glide BrRaceHudFrame */
/* @n64 0x802003E4 located */
/* Orig always calls 0x10008D60 with five args (add esp,0x14) even though
 * the callee is a bare ret; omitting the call is a port-only fold. */
/* BrExt_10008D60: prototype in br_funcs.h */
/* BrExt_10019840: prototype in br_funcs.h */
/* BrExt_10032E40: prototype in br_funcs.h */
void BrRaceHudFrame(void)
{
    if (g_BrX06909B4 != 0)
        return;
    BrPodNop();
    BrS17DrawGated();
    BrPodNop();
    BrCarTrailStep();
    BrPodNop();
}

/* ==========================================================================
 * 0x10019900 -- into the outro
 * ========================================================================== */

/* WHAT IT DOES: switches the game to the mode that plays the ending, marks
 * which of that mode's three pieces is wanted, and hands the frame over to
 * the race step -- which is the same routine that runs an ordinary race, so
 * the ending plays through the race machinery rather than beside it. */
/* @implements 0x10019900 glide BrRaceEnterOutro */
/* @n64 0x8020068C located */
void BrRaceEnterOutro(void)
{
    /* 0x10019900 / 0x1002C390: push 0x10019A70, then the two stores, then
     * cdecl call 0x1002E317 / 0x10034C66 and add esp,4. */
    g_brRaceRules.mode = 4;
    (*(int32_t *)&g_5bc760) = 2;
    BrGameStepSet(BrRaceStep);
}

/* ==========================================================================
 * 0x10019930 / 0x10019980 -- the cue schedule
 * ========================================================================== */

/* WHAT IT DOES: walks a short list of timed cues and works out when each one
 * should start, placing each start three quarters of the way through the cue
 * before it and then leaving the remaining quarter plus that cue's own gap.
 * What the cues are FOR is not established -- nothing transcribed so far
 * reads the starts back -- so all that can honestly be said is that this is
 * where their timings come from. */
/* @t4-pass 0x10019930 1 2026-09-07 probes 51 bytes 75 insns 31 regions 3 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x10019930 2 2026-09-07 probes 51 bytes 75 insns 31 regions 3 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x10019930 3 2026-09-09 probes 10 bytes 78 insns 32 regions 2 rows 2 census no  (hand, fn.py variants at the mid-bump shape: star/index spellings, orders, decl split, all inert) */
/* @t4-pass 0x10019930 4 2026-09-09 probes 10 bytes 78 insns 32 regions 2 rows 2 census yes  (hand, fn.py variants: casts, guard/store/while forms, all inert; corpus hit at +0x11 -- the do/while-next walk shape confirmed) */
/* @t3 0x10019930 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 78/78 insns 32/32 rows 1+1 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * residue is register colouring plus the cursor-init addend artefact
 * (recomp `mov esi, <g_aBrRaceCue+4>` prints its addend bare, absorbed as
 * the two singletons); identical multiset otherwise after the mid-bump
 * lever (788b247), size- and insn-exact.
 * Do not reopen before the end-grind. */
/* @implements 0x10019930 glide BrRaceCueLayout */
/* @n64 0x802006C8 located */
void BrRaceCueLayout(void)
{
    BrRaceCue *p;
    int32_t  t, len, three;

    /* The original walks the records with an int cursor at record.len
     * (start [esi-4], gap [esi+4], next record's text [esi+0x18]); the
     * text is a pointer here, so the walk is by record. */
    t = g_brRaceCueBase;
    if (g_aBrRaceCue[0].text == 0)
        return;
    p = g_aBrRaceCue;
    do {
        len   = p->len;
        three = (len * 3) / 4;
        t += three;
        p->start = t;
        t += (len - three) + p->gap;
        p++;
    } while (p->text != 0);
}

/* WHAT IT DOES: moves every cue in the list one step earlier. */
/* @implements 0x10019980 glide BrRaceCueRewind */
/* @n64 0x8020072C located */
void BrRaceCueRewind(void)
{
    BrRaceCue *p;

    if (g_aBrRaceCue[0].text == 0)
        return;
    p = g_aBrRaceCue;
    do {
        p->start--;
        p++;
    } while (p->text != 0);
}

/* ==========================================================================
 * 0x100199A0 -- the outro controller
 * ========================================================================== */

/* WHAT IT DOES: the body that drives a car during the ending sequence. It
 * reads how fast the car is actually moving, records that as the car's speed
 * in miles per hour, and then hands the car to the ordinary control chain --
 * so the car is steered by whatever is behind that, and this only keeps the
 * speed readout honest. */
/* @implements 0x100199A0 glide BrRaceCarCtlOutro */
/* BrSqrtF: prototype in br_funcs.h */
/* BrCarCtlChain_1006F170: prototype in br_funcs.h */

void BrRaceCarCtlOutro(BrDriverCar *pCar)
{
    /* No null guard, and no hook table: the original tests +0x730 straight
     * away and calls both bodies directly. The two components that are
     * SQUARED are float locals -- VC5 copies each through an integer
     * register into its own stack slot and then does `fld m; fmul m` -- while
     * the third is read from the member twice and squared on the x87 stack,
     * so it is not a local.
     *
     * The parentheses round the first two products are NOT redundant to the
     * compiler: without them VC5 schedules the z square SECOND instead of
     * last and the function comes out 22 bytes different with an identical
     * instruction multiset. Declaration order decides which of the two
     * copies lands in ecx. */
    if ((*(int32_t *)&pCar->aBody[2].rb.f1B4) != 0) {
        float x = pCar->aBody[0].rb.st.vel.x;
        float y = pCar->aBody[0].rb.st.vel.y;

        pCar->f1030 = BrSqrtF((x * x + y * y) + pCar->aBody[0].rb.st.vel.z * pCar->aBody[0].rb.st.vel.z)
                      * BR_RACEBEGIN_MPS_TO_MPH;
    }

    BrCarStep(pCar);
}

/* ==========================================================================
 * 0x10019A10 / 0x10019A40
 * ========================================================================== */

/* WHAT IT DOES: gives every driver in the field the same reset, one after the
 * other. */
/* @implements 0x10019A10 glide BrRaceDriverReset */
/* @n64 0x8021735C located */
/* Orig is `mov edi, 0x10AF07F8`: the drivers ARE that address, not a
 * pointer stored there.  Slot +0x00 is 1-arg thiscall. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1005f530: prototype in br_funcs.h */
void BrRaceDriverReset(void)
{
    int32_t   n;
    int32_t   i;
    BrDriver *p;

    n = g_brRaceNDriver;
    i = 0;
    if (n <= 0)
        return;
    p = g_aBrRaceDriver;
    do {
        BrDriverAssetsFree(p);
        i++;
        p++;
    } while (i < g_brRaceNDriver);
}

/* WHAT IT DOES: restarts the frame clock. It throws away the running total of
 * elapsed time -- by setting the frame counter so that the very next frame
 * finds it at zero and clears the total rather than adding to it -- and turns
 * the frame limiter back on. */
/* @implements 0x10019A40 glide BrRaceClockReset */
/* Direct calls: thiscall 0x1006E3F0 on the object at 0x105BC858, and the
 * closing 0x1006E360 in tail position (VC5 emits it as a plain jmp). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1006e3f0: prototype in br_funcs.h */
/* FUN_1006e360: prototype in br_funcs.h */
void BrRaceClockReset(void)
{
    br86_timer_restart(&(*(int *)&g_br6806B0));                      /* 0x10019A45 */

    /* 0x10019A4A.  -1, not 0: the clock's own `inc` then `je` is what turns
     * this into "clear the accumulator on the NEXT frame". */
    g_brRaceClockCount   = -1;
    g_brRaceBeginLimitOn = 1;                         /* 0x10019A54 */

    BrTimeUpdate();                                   /* 0x10019A5E, tail jmp */
}

/* ==========================================================================
 * 0x10019A70..0x10019AF8 -- the frame clock and the substate branch
 * ========================================================================== */

/* WHAT IT DOES: the first thing a race frame does. It reads the clock, works
 * out how long the last frame took, keeps the last few of those measurements
 * in a small ring so something else can average them, and adds the time to a
 * running total. Then it answers the one question the rest of the routine is
 * built around: is this the first frame of the race, or not. */
/* (port-only BrRaceStepClock removed) */


/* ==========================================================================
 * The one-time arm's seven mode arms
 * ========================================================================== */

/* 0x10019CFB -- shared by the mode-0 and mode-6 arms when a replay is
 * running.  0x10019D00's `lea` chain is x5, x2+1, <<6, -1, <<4, +1, x8 ==
 * 89992, the same per-car image stride br_racestart.h reads out of
 * "sizeof(UltraCarHeader)=%d". */
/* (port-only begin_replay_reset removed) */


/* 0x10019D39 -- and its non-replay twin. */
/* (port-only begin_clear_ctl removed) */


/* 0x10019CF3 -- where the mode-0, mode-1 and mode-6 arms all land. */
/* (port-only begin_tail_0163 removed) */


/* mode 0 -- 0x10019BC8 */
/* (port-only begin_mode0 removed) */


/* mode 1 -- 0x10019BF1 */
/* (port-only begin_mode1 removed) */


/* mode 6 -- 0x10019C2A, the networked arm */
/* (port-only begin_mode6 removed) */


/* mode 5 -- 0x10019D87 */
/* (port-only begin_mode5 removed) */


/* mode 4 -- 0x10019DC0, the cutscene arm.  This is the READ end of the
 * eight-byte replay header; see the banner for the other end. */
/* (port-only begin_mode4 removed) */


/* mode 2 -- 0x10019F0E, the link/replay arm.  This is the WRITE end of the
 * eight-byte header. */
/* (port-only begin_mode2 removed) */


/* mode 3 and everything above 6 -- 0x1001A1EE */
/* (port-only begin_mode_default removed) */


/* ==========================================================================
 * The common tail, 0x1001A218..0x1001AB6F
 * ========================================================================== */

/* 0x1001A2AE..0x1001A41C -- the track's "specials". */
/* (port-only begin_specials removed) */


/* 0x1001A599..0x1001A65F -- one controller per car. */
/* (port-only begin_controllers removed) */


/* 0x1001A6A0..0x1001A971 -- the car paint textures. */
/* (port-only begin_paint removed) */


/* 0x1001AA5E..0x1001AB6F -- what happens after the substate is raised. */
/* (port-only begin_assets removed) */


/* ==========================================================================
 * 0x10019AFE..0x1001AB6F
 * ========================================================================== */

/* WHAT IT DOES: sets a race up, once, on the frame it starts. It decides how
 * many cars and drivers there are from what kind of race this is -- a full
 * grid, a head-to-head, a link game, a cutscene -- loads the track and its
 * scenery, gives every car its equipment and a driver to control it, builds
 * each car's paint textures, starts the music, rewinds the starting lights
 * and marks itself done so it never runs again. The link and cutscene modes
 * are the two ends of one small record: one writes down what the race was
 * (track, car, four equipment choices, weather) and the other reads it back
 * to reproduce it. */
/* (port-only BrRaceStepBegin removed) */

