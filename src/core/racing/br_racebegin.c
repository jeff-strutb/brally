/* br_racebegin.c -- see br_racebegin.h.
 *
 * RESPONSIBILITY: the rules of a race.  The opening of Glide 0x10019A70 and
 * the whole of its one-time arm, 0x10019A70..0x1001AB6F, plus the seven small
 * functions below it in .text that only it reaches.
 *
 * Transcribed from orig/BRGlide.dll.  Every branch carries the address of the
 * instruction it is, so the two can be diffed.
 */
#ifdef BR_MATCHING_BUILD
/* Header takes the race-step body as an argument; the original is void and
 * pushes 0x10019A70 / 0x1002C500 as an immediate. */
#define BrRaceEnterOutro BrRaceEnterOutro_port
#endif
#include "br_racebegin.h"
#ifdef BR_MATCHING_BUILD
#undef BrRaceEnterOutro
#endif

#include <stddef.h>
#include <string.h>

#include "br_gamestep.h"   /* BrGameStepSet -- 0x1002E317, already ported */

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
extern int32_t g_brCarPhysWeather;      /* 0x104B15E8 */

/* br_appstart.h's three config globals, same treatment. */
extern int32_t g_brCfgChosenTrack;      /* 0x100B3014 */
extern int32_t g_brCfgChosenCar;        /* 0x10226E7C */
extern int32_t g_brCfgChosenWeather;    /* 0x10226E80 */

/* 0x10B71648, the local player's name.  0x10006250 is handed it verbatim and
 * 0x10019C9F copies it into 0x10AF1350; neither length is bounded in the
 * original (it is `repne scasb` then `rep movsd`).  The port keeps a bounded
 * copy -- a DEVIATION, and the only one in the transcription that changes
 * what the code CAN do rather than what it does. */
#define BR_RACEBEGIN_NAME_MAX 64
char g_aBrRaceBeginName[BR_RACEBEGIN_NAME_MAX];      /* 0x10B71648 */
char g_aBrRaceBeginNameCopy[BR_RACEBEGIN_NAME_MAX];  /* 0x10AF1350 */
void *g_pBrRaceBeginSession;                         /* 0x10AF134C */

/* ==========================================================================
 * The globals this module owns.  Every one was grepped across port/ before
 * being given storage and had no owner anywhere.  Initial values are the
 * shipped image's where .data holds one, and zero where the address is .bss.
 * ========================================================================== */

uint32_t g_aBrRaceClockRing[BR_RACEBEGIN_CLOCK_MAX];  /* 0x105BC900, .bss  */
int32_t  g_brRaceClockLen    = BR_RACEBEGIN_CLOCK_LEN;/* 0x100A9358 == 5   */
int32_t  g_brRaceClockCursor = -1;                    /* 0x100A935C == -1  */
uint32_t g_brRaceClockLast;                           /* 0x105CCB90        */
uint32_t g_brRaceClockAccum;                          /* 0x105CCB7C        */
int32_t  g_brRaceClockCount;                          /* 0x105BCAE4        */

int32_t  g_brRaceBeginSpecialsN;    /* 0x106EEEFC */
int32_t  g_brRaceBeginAirplane;     /* 0x105BC7C0 */
int32_t  g_brRaceBeginAirTrigger;   /* 0x105BC7C4 */
int32_t  g_brRaceBeginAirArmed;     /* 0x105BC7CC */
int32_t  g_brRaceBeginPathLeft;     /* 0x105BC7DC */
int32_t  g_brRaceBeginPathRight;    /* 0x105BC7E0 */
int32_t  g_brRaceBeginPathLen;      /* 0x105BC7D4 */
int32_t  g_brRaceBeginPathIdx;      /* 0x105BC7D0 */
float    g_brRaceBeginPathT;        /* 0x105BC7E4 */
float    g_brRaceBeginPathSeg;      /* 0x105BC7E8 */
float    g_brRaceBeginPathScale;    /* 0x105BC7D8 */
int32_t  g_brRaceBeginFxCount;      /* 0x105BCAE8 */
int32_t  g_aBrRaceBeginFx[8];       /* 0x105BC778 */
int32_t  g_brRaceBeginCamMinus1;    /* 0x105BC7C8 */

int32_t  g_brRaceBeginNTexSet = 1;  /* 0x100AA044, .data == 1 */
int32_t  g_brRaceBeginRecLen;       /* 0x105BC8D8 */
uint8_t  g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_LEN];    /* 0x105BC8E0 */
uint8_t  g_aBrRaceBeginRecIn[BR_RACEBEGIN_HDR_LEN];  /* 0x11778850 */
int32_t  g_brRaceBeginBestCar;      /* 0x105BC810 */
int32_t  g_brRaceBeginIntroSide;    /* 0x105CCB9C */
int32_t  g_brRaceBeginMovieDone;    /* 0x105BCAE0 */
int32_t  g_brRaceBeginRecArmed;     /* 0x105BC7B8 */
int32_t  g_brRaceBeginStage;        /* 0x105BC760 */
int32_t  g_brRaceBeginFade;         /* 0x105CCB8C */
int32_t  g_brRaceBeginRamped;       /* 0x105CCB98 */
int32_t  g_brRaceBeginActive;       /* 0x106ED684 */
int32_t  g_brRaceBeginSeqIdx;       /* 0x105BC768 */
float    g_brRaceBeginSeqT;         /* 0x105BC884 */
int32_t  g_brRaceBeginSeq888;       /* 0x105BC888 */
int32_t  g_brRaceBeginDrawn;        /* 0x106ED6D8 */
int32_t  g_brRaceBeginLights;       /* 0x10AF2090 */
int32_t  g_brRaceBeginB1CF10;       /* 0x10B1CF10 */
int32_t  g_brRaceBeginAF6724;       /* 0x10AF6724 */
int32_t  g_brRaceBeginDifficulty;   /* 0x106EA3F4 */
int32_t  g_brRaceBegin226A4C;       /* 0x10226A4C */
int32_t  g_brRaceBegin6E86C8;       /* 0x106E86C8 */
int32_t  g_brRaceBegin6E8720;       /* 0x106E8720 */
float    g_brRaceBeginAF397C;       /* 0x10AF397C */
int32_t  g_brRaceBeginMirrorOff;    /* 0x11778848 */
int32_t  g_aBrRaceBeginLink[4];     /* 0x10AF4C00..0x10AF4C0C */
int32_t  g_brRaceBeginLimitOn = 1;  /* 0x100A5EA8, .data == 1 */

uint8_t  g_aBrRaceBeginRecHdr[BR_RACEBEGIN_MAXREC][BR_RACEBEGIN_HDR_LEN];

BrRaceCue g_aBrRaceCue[BR_RACEBEGIN_CUE_MAX];   /* 0x100A5EB0 */
int32_t   g_brRaceCueArmed;                     /* 0x100A5EBC, .data holds a
                                                 * pointer, i.e. non-zero    */
int32_t   g_brRaceCueBase;                      /* 0x106E9A2C */

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

int32_t BrRaceBeginSkipped(BrRaceBeginOp op)
{
    if ((int)op < 0 || (int)op >= BR_RB_NOPS)
        return 0;
    return s_aSkipped[op];
}

const char *BrRaceBeginOpName(BrRaceBeginOp op)
{
    if ((int)op < 0 || (int)op >= BR_RB_NOPS)
        return "(no such op)";
    return s_aOpName[op];
}

void BrRaceBeginReset(void)
{
    int i;
    for (i = 0; i < BR_RB_NOPS; ++i)
        s_aSkipped[i] = 0;
    s_cRecClamped = 0;
    s_cReplayPaintSkipped = 0;
}

int32_t BrRaceBeginRecClamped(void)
{
    return s_cRecClamped;
}

int32_t BrRaceBeginReplayPaintSkipped(void)
{
    return s_cReplayPaintSkipped;
}

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
static void trace(void)
{
    if (g_brRaceBeginOps.pfnTrace != NULL)
        g_brRaceBeginOps.pfnTrace();
    else
        ++s_aSkipped[BR_RB_TRACE];
}

static BrRaceCtl *ctl_of(BrDriverCar *pCar)
{
    if (pCar == NULL)
        return NULL;
    if (pCar->pCtl != NULL)
        return pCar->pCtl;
    if (OP(BR_RB_CTL, pfnCtl))
        return g_brRaceBeginOps.pfnCtl(pCar);
    return NULL;
}

static BrDriverCar *car_at(int32_t i)
{
    if (g_pBrRaceCar == NULL || i < 0)
        return NULL;
    return &g_pBrRaceCar[i];
}

/* ==========================================================================
 * 0x10019890 -- the pre-frame colour submissions
 * ========================================================================== */

/* WHAT IT DOES: nudges the screen's colour settings three times around two
 * pieces of drawing work, and does nothing at all while the game is paused.
 * All three colour submissions go to a routine that is empty in this build,
 * so what the shipped game gets out of them is the two calls between them. */
/* @implements 0x10019890 glide BrRaceHudFrame */
void BrRaceHudFrame(void)
{
    if (g_brRacePaused != 0)                          /* 0x10019897 */
        return;

    /* 0x10019899: (0, 0x80, 0x80, 0xF0, 0xFF) */
    trace();
    if (OP(BR_RB_10019840, pfn10019840))              /* 0x100198B7 */
        g_brRaceBeginOps.pfn10019840();
    /* 0x100198BC: (0, 0, 0, 0xC0, 0xFF) */
    trace();
    if (OP(BR_RB_10032E40, pfn10032E40))              /* 0x100198D4 */
        g_brRaceBeginOps.pfn10032E40();
    /* 0x100198D9: (0, 0, 0x82, 0, 0xFF) */
    trace();
}

/* ==========================================================================
 * 0x10019900 -- into the outro
 * ========================================================================== */

/* WHAT IT DOES: switches the game to the mode that plays the ending, marks
 * which of that mode's three pieces is wanted, and hands the frame over to
 * the race step -- which is the same routine that runs an ordinary race, so
 * the ending plays through the race machinery rather than beside it. */
/* @implements 0x10019900 glide BrRaceEnterOutro */
#ifdef BR_MATCHING_BUILD
void BrRaceEnterOutro(void)
{
    /* 0x10019900 / 0x1002C390: push 0x10019A70, then the two stores, then
     * cdecl call 0x1002E317 / 0x10034C66 and add esp,4. */
    g_brRaceRules.mode = 4;
    g_brRaceBeginStage = 2;
    BrGameStepSet(BrRaceStepFrame);
}
#else
void BrRaceEnterOutro(void (*pfnRaceStep)(void))
{
    /* 0x10019900 pushes 0x10019A70 -- the race step itself -- BEFORE the two
     * stores.  A host cannot name an original address, so the body goes in as
     * an argument; the order of the two stores against the install is what is
     * preserved, and nothing runs a frame between them. */
    g_brRaceRules.mode  = 4;                          /* 0x10019905 */
    g_brRaceBeginStage  = 2;                          /* 0x1001990F */
    BrGameStepSet(pfnRaceStep);                       /* 0x10019919 */
}
#endif

/* ==========================================================================
 * 0x10019930 / 0x10019980 -- the cue schedule
 * ========================================================================== */

/* WHAT IT DOES: walks a short list of timed cues and works out when each one
 * should start, placing each start three quarters of the way through the cue
 * before it and then leaving the remaining quarter plus that cue's own gap.
 * What the cues are FOR is not established -- nothing transcribed so far
 * reads the starts back -- so all that can honestly be said is that this is
 * where their timings come from. */
/* @implements 0x10019930 glide BrRaceCueLayout */
void BrRaceCueLayout(void)
{
    int32_t t;
    int     i;

    if (g_brRaceCueArmed == 0)                        /* 0x1001993E */
        return;

    /* 0x10019936: the running position starts at 0x106E9A2C and is never
     * stored back -- the global is read-only here. */
    t = g_brRaceCueBase;

    for (i = 0; i < BR_RACEBEGIN_CUE_MAX; ++i) {
        int32_t len   = g_aBrRaceCue[i].len;          /* 0x10019946 */
        int32_t three = (len * 3) / 4;                /* 0x1001994B..0x10019954:
                                                       * `cdq / and edx,3 / add
                                                       * / sar 2` is signed
                                                       * division truncating
                                                       * toward zero          */

        t += three;                                   /* 0x10019957 */
        g_aBrRaceCue[i].start = t;                    /* 0x10019959 */

        /* 0x1001995C re-reads the SAME field it just used and recomputes the
         * same quotient; the difference is what is left of the cue. */
        t += (len - three) + g_aBrRaceCue[i].gap;     /* 0x1001996B..0x10019975 */

        /* 0x10019972 `mov eax,[esi+8]` -- after the cursor has advanced, so
         * the terminator read is the NEXT record's +0x0C, not this one's. */
        if (i + 1 >= BR_RACEBEGIN_CUE_MAX)
            break;                                    /* DEVIATION: the
                                                       * original has no bound */
        if (g_aBrRaceCue[i + 1].next == 0)            /* 0x10019977 */
            break;
    }
}

/* WHAT IT DOES: moves every cue in the list one step earlier. */
/* @implements 0x10019980 glide BrRaceCueRewind */
void BrRaceCueRewind(void)
{
    int i;

    if (g_brRaceCueArmed == 0)                        /* 0x10019987 */
        return;

    for (i = 0; i < BR_RACEBEGIN_CUE_MAX; ++i) {
        g_aBrRaceCue[i].start -= 1;                   /* 0x1001998E..0x10019994 */
        if (i + 1 >= BR_RACEBEGIN_CUE_MAX)
            break;                                    /* DEVIATION, as above  */
        if (g_aBrRaceCue[i + 1].next == 0)            /* 0x10019997 */
            break;
    }
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
void BrRaceCarCtlOutro(BrDriverCar *pCar)
{
    if (pCar == NULL)
        return;

    if (pCar->f730 != 0) {                            /* 0x100199AC */
        /* 0x100199BC..0x100199E7.  The three components are loaded in the
         * order +0x1EC, +0x1E8, +0x1F0 and the products are summed
         * (y*y + x*x) + z*z -- st(2) first, then st(1).  Each product and
         * each sum lives in an x87 register, which CONVENTIONS.md pins at
         * 53-bit, so they are `double` exactly.
         *
         * 0x100199E7 `fstp dword ptr [esp]` SPILLS the sum to a 32-bit slot
         * before the square root is called, so the port rounds there too --
         * widening past that point would be the same bug pointing the other
         * way. */
        double x = (double)pCar->f1E8.y;   /* +0x1EC */
        double y = (double)pCar->f1E8.x;   /* +0x1E8 */
        double z = (double)pCar->f1E8.z;   /* +0x1F0 */
        float  sum = (float)((y * y + x * x) + z * z);
        float  len = 0.0f;

        if (OP(BR_RB_SQRT, pfnSqrt))                  /* 0x100199EC */
            len = g_brRaceBeginOps.pfnSqrt(sum);

        /* 0x100199F1 multiplies in a register and 0x100199FA spills to
         * car+0x1030, so the product rounds once, at the store. */
        pCar->f1030 = (float)((double)len * BR_RACEBEGIN_MPS_TO_MPH);
    }

    if (OP(BR_RB_1006F170, pfn1006F170))              /* 0x10019A02 */
        g_brRaceBeginOps.pfn1006F170(pCar);
}

/* ==========================================================================
 * 0x10019A10 / 0x10019A40
 * ========================================================================== */

/* WHAT IT DOES: gives every driver in the field the same reset, one after the
 * other. */
/* @implements 0x10019A10 glide BrRaceDriverReset */
void BrRaceDriverReset(void)
{
    int32_t i;

    for (i = 0; i < g_brRaceNDriver; ++i) {           /* 0x10019A18/0x10019A35 */
        if (g_pBrRaceDriver == NULL)
            break;
        if (OP(BR_RB_1005F530, pfn1005F530))          /* 0x10019A24 */
            g_brRaceBeginOps.pfn1005F530(&g_pBrRaceDriver[i]);
    }
}

/* WHAT IT DOES: restarts the frame clock. It throws away the running total of
 * elapsed time -- by setting the frame counter so that the very next frame
 * finds it at zero and clears the total rather than adding to it -- and turns
 * the frame limiter back on. */
/* @implements 0x10019A40 glide BrRaceClockReset */
void BrRaceClockReset(void)
{
    if (OP(BR_RB_1006E3F0, pfn1006E3F0))              /* 0x10019A45 */
        g_brRaceBeginOps.pfn1006E3F0();

    /* 0x10019A4A.  -1, not 0: the clock's own `inc` then `je` is what turns
     * this into "clear the accumulator on the NEXT frame". */
    g_brRaceClockCount   = -1;
    g_brRaceBeginLimitOn = 1;                         /* 0x10019A54 */

    if (OP(BR_RB_1006E360, pfn1006E360))              /* 0x10019A5E, a tail
                                                       * call, so it is the
                                                       * last thing that
                                                       * happens either way   */
        g_brRaceBeginOps.pfn1006E360();
}

/* ==========================================================================
 * 0x10019A70..0x10019AF8 -- the frame clock and the substate branch
 * ========================================================================== */

/* WHAT IT DOES: the first thing a race frame does. It reads the clock, works
 * out how long the last frame took, keeps the last few of those measurements
 * in a small ring so something else can average them, and adds the time to a
 * running total. Then it answers the one question the rest of the routine is
 * built around: is this the first frame of the race, or not. */
int BrRaceStepClock(void)
{
    uint32_t now = 0;
    uint32_t dt;
    int32_t  i, len;

    if (OP(BR_RB_TICKMS, pfnTickMs))                  /* 0x10019A77 */
        now = g_brRaceBeginOps.pfnTickMs();

    /* 0x10019A82/0x10019A90: the delta is taken against the PREVIOUS stamp
     * and the stamp is replaced in the same breath.  Unsigned, so a wrap of
     * the millisecond counter still yields the right difference. */
    dt = now - g_brRaceClockLast;
    g_brRaceClockLast = now;                          /* 0x10019A84 */

    i   = g_brRaceClockCursor;                        /* 0x10019A89 */
    len = g_brRaceClockLen;                           /* 0x10019A92 */

    if (i < 0) {                                      /* 0x10019A9A `jge` */
        i = 0;                                        /* 0x10019A9C */
        if (len > 0) {                                /* 0x10019AA0 `jle` */
            int32_t k;
            int32_t n = len;
            if (n > BR_RACEBEGIN_CLOCK_MAX)
                n = BR_RACEBEGIN_CLOCK_MAX;           /* DEVIATION: bounded  */
            /* 0x10019AAB `rep stosd` -- DWORDS, not bytes.  A cursor of -1
             * is what the shipped image ships, so the first frame of the
             * process primes every slot with one measurement instead of
             * leaving the ring full of zeroes. */
            for (k = 0; k < n; ++k)
                g_aBrRaceClockRing[k] = dt;
            i = len;                                  /* 0x10019AAD */
        }
    }

    /* 0x10019AB5: `inc` then `je` -- the accumulator is cleared on the frame
     * the counter reaches zero, which is the frame AFTER BrRaceClockReset
     * set it to -1. */
    ++g_brRaceClockCount;
    if (g_brRaceClockCount == 0)
        g_brRaceClockAccum = 0;                       /* 0x10019AC6 */
    else
        g_brRaceClockAccum += dt;                     /* 0x10019ABE */

    ++i;                                              /* 0x10019ACC */
    if (i >= len)                                     /* 0x10019ACD `jl`     */
        i = 0;                                        /* 0x10019AD6 */
    g_brRaceClockCursor = i;
    if (i >= 0 && i < BR_RACEBEGIN_CLOCK_MAX)         /* DEVIATION: bounded  */
        g_aBrRaceClockRing[i] = dt;                   /* 0x10019ADD */

    /* 0x10019AE4 / 0x10019AF8.  Non-zero substate takes the per-frame arm. */
    return (g_brRaceSubstate == 0);
}

/* ==========================================================================
 * The one-time arm's seven mode arms
 * ========================================================================== */

/* 0x10019CFB -- shared by the mode-0 and mode-6 arms when a replay is
 * running.  0x10019D00's `lea` chain is x5, x2+1, <<6, -1, <<4, +1, x8 ==
 * 89992, the same per-car image stride br_racestart.h reads out of
 * "sizeof(UltraCarHeader)=%d". */
static void begin_replay_reset(void)
{
    if (OP(BR_RB_100609D0, pfn100609D0)) {            /* 0x10019D1B */
        uint8_t *pBase = NULL;
        if (OP(BR_RB_CARIMAGE, pfnCarImage))
            pBase = g_brRaceBeginOps.pfnCarImage(g_brRaceBeginBestCar);
        /* The original forms 0x102066C8 - 89992*bestCar, i.e. it counts DOWN
         * from the end of the image block.  The port asks for the slot base
         * and lets the host decide where that is; the multiplier is the same
         * number and is asserted separately. */
        g_brRaceBeginOps.pfn100609D0(pBase);
    }

    /* 0x10019D23: 0x118EEF50..0x118EF0B8, stride 0x18, the first two dwords
     * of each cleared.  That block is render state (br_racestep.c already
     * names 0x118EEF48's neighbours as such) and has no host model, so it is
     * recorded here and not modelled. */
}

/* 0x10019D39 -- and its non-replay twin. */
static void begin_clear_ctl(void)
{
    int32_t i;

    for (i = 0; i < g_brRaceNEntrant; ++i) {          /* 0x10019D40/0x10019D6C */
        BrRaceCtl *p = ctl_of(car_at(i));
        if (p == NULL)
            continue;
        p->pHdr    = NULL;                            /* 0x10019D51 */
        p->apRec[0] = NULL;                           /* 0x10019D5A */
        if (BR_RACEBEGIN_MAXREC > 1)
            p->apRec[1] = NULL;                       /* 0x10019D63 */
    }
}

/* 0x10019CF3 -- where the mode-0, mode-1 and mode-6 arms all land. */
static void begin_tail_0163(void)
{
    if (g_brRaceReplay != 0) {                        /* 0x10019CF9 */
        begin_replay_reset();
    } else {
        begin_clear_ctl();
    }
    /* 0x10019D70: the flag is set to "not replaying", which on this path is
     * the same test that was just taken. */
    g_brRaceBeginFade = (g_brRaceReplay == 0);
}

/* mode 0 -- 0x10019BC8 */
static void begin_mode0(void)
{
    g_brRaceNDriver = 0x14;                           /* 0x10019BD3, 20 */
    g_brRaceNCar    = 3;                              /* 0x10019BDD */
    if (OP(BR_RB_CARMODELSET, pfnCarModelSet))        /* 0x10019BE7 */
        g_brRaceBeginOps.pfnCarModelSet(car_at(0), g_brCfgChosenCar);
    begin_tail_0163();
}

/* mode 1 -- 0x10019BF1 */
static void begin_mode1(void)
{
    g_brRaceNDriver = 2;                              /* 0x10019BF7 */
    g_brRaceNCar    = 2;                              /* 0x10019C03 */
    if (OP(BR_RB_CARMODELSET, pfnCarModelSet)) {
        g_brRaceBeginOps.pfnCarModelSet(car_at(0), g_brCfgChosenCar); /* 0x10019C09 */
        /* 0x10019C1A pushes 0x10AF3D70 == 0x10AF1208 + 0x2B68 == car[1]. */
        g_brRaceBeginOps.pfnCarModelSet(car_at(1), g_brCfgChosenCar);
    }
    g_brRaceBeginAF6724 = 0;                          /* 0x10019C1F */
    begin_tail_0163();
}

/* mode 6 -- 0x10019C2A, the networked arm */
static void begin_mode6(void)
{
    if (g_brRaceReplay != 0) {                        /* 0x10019C30 */
        begin_replay_reset();
        g_brRaceBeginFade = (g_brRaceReplay == 0);
        return;
    }

    g_brRaceNEntrant = 1;                             /* 0x10019C3B */
    /* 0x10019C41: the count is 1 when 0x10226A4C is CLEAR and 0 when it is
     * set -- the `je` goes to the pair of ones. */
    if (g_brRaceBegin226A4C == 0) {
        g_brRaceNDriver = 1;                          /* 0x10019C53 */
        g_brRaceNCar    = 1;
    } else {
        g_brRaceNDriver = 0;                          /* 0x10019C45 */
        g_brRaceNCar    = 0;
    }

    if (OP(BR_RB_CARMODELSET, pfnCarModelSet))        /* 0x10019C6A */
        g_brRaceBeginOps.pfnCarModelSet(car_at(0), g_brCfgChosenCar);

    if (g_brRaceNet != 0) {                           /* 0x10019C6F */
        if (OP(BR_RB_NETOPEN, pfnNetOpen))            /* 0x10019C77..0x10019C86 */
            g_brRaceBeginOps.pfnNetOpen();
        if (OP(BR_RB_NETSESSION, pfnNetSession))
            g_pBrRaceBeginSession = g_brRaceBeginOps.pfnNetSession();

        /* 0x10019C90..0x10019CB7: an inlined strcpy of the name at
         * 0x10B71648 into 0x10AF1350, unbounded in the original (`repne
         * scasb` then `rep movsd`).  DEVIATION: bounded here. */
        {
            size_t n = strlen(g_aBrRaceBeginName);
            if (n >= sizeof(g_aBrRaceBeginNameCopy))
                n = sizeof(g_aBrRaceBeginNameCopy) - 1u;
            memcpy(g_aBrRaceBeginNameCopy, g_aBrRaceBeginName, n);
            g_aBrRaceBeginNameCopy[n] = '\0';
        }

        /* 0x10019C9F pushes the NAME and 0x10019CBE pushes the SESSION, so
         * cdecl order is (session, name). */
        if (OP(BR_RB_NETNAME, pfnNetName))            /* 0x10019CBF */
            g_brRaceBeginOps.pfnNetName(g_pBrRaceBeginSession,
                                        g_aBrRaceBeginName);
    }

    /* 0x10019CC7.  The compare is `jae`/`jb`, i.e. UNSIGNED, against the
     * player count -- so a negative car count would read as enormous and the
     * loop would not run.  Both spellings are the original's. */
    if (OP(BR_RB_NETCOUNT, pfnNetCount)) {
        uint32_t cPlayers = g_brRaceBeginOps.pfnNetCount();
        while ((uint32_t)g_brRaceNCar < cPlayers) {
            int32_t iPlayer = -1;
            if (OP(BR_RB_NETNEXT, pfnNetNext))        /* 0x10019CD4 */
                iPlayer = g_brRaceBeginOps.pfnNetNext();
            if (iPlayer >= 0) {                       /* 0x10019CD9 */
                if (OP(BR_RB_NETADD, pfnNetAdd))      /* 0x10019CDE */
                    g_brRaceBeginOps.pfnNetAdd(iPlayer);
            }
            cPlayers = g_brRaceBeginOps.pfnNetCount(); /* 0x10019CE6 */
            /* Without a hook for pfnNetAdd the count cannot move and the
             * original would spin here too -- so the port bounds it rather
             * than reproducing a hang, which is what CONVENTIONS.md asks of
             * a loop whose termination is the thing under test. */
            if (g_brRaceBeginOps.pfnNetAdd == NULL)
                break;
        }
    }

    begin_tail_0163();
}

/* mode 5 -- 0x10019D87 */
static void begin_mode5(void)
{
    g_brRaceNEntrant    = 1;                          /* 0x10019D87 */
    g_brRaceNDriver     = 1;                          /* 0x10019D8D */
    g_brRaceNCar        = 1;                          /* 0x10019D93 */
    g_brRaceBeginFade   = 0;                          /* 0x10019D99 */
    g_brRaceReplay      = 0;                          /* 0x10019D9F */
    g_brRaceBeginLights = 0;                          /* 0x10019DA5 */
    /* 0x10019DAB stores the address 0x10AF3988 into 0x10AF393C.  Both are
     * render-side objects with no host model; the store is recorded and the
     * port keeps only the fact that the slot became non-NULL. */
    g_brRaceBeginB1CF10 = 0;                          /* 0x10019DB5 */
}

/* mode 4 -- 0x10019DC0, the cutscene arm.  This is the READ end of the
 * eight-byte replay header; see the banner for the other end. */
static void begin_mode4(void)
{
    BrDriverCar *pCar = car_at(0);
    BrRaceCtl   *pCtl = ctl_of(pCar);

    g_brCarPhysWeather  = 1;                          /* 0x10019DC6 */
    g_brRaceNEntrant    = 1;                          /* 0x10019DCC */
    g_brRaceNDriver     = 1;                          /* 0x10019DD2 */
    g_brRaceNCar        = 1;                          /* 0x10019DD8 */
    g_brRaceBeginFade   = 0;                          /* 0x10019DDE */
    g_brRaceBeginLights = 0;                          /* 0x10019DE4 */

    if (pCtl != NULL)
        pCtl->pHdr = g_aBrRaceBeginRec;               /* 0x10019DEA */

    /* 0x10019DF1: `sub eax,0 / je / dec / je / dec / jne`, i.e. a three-way
     * on the stage with everything else falling past the movie entirely. */
    if (g_brRaceBeginStage == 0) {                    /* 0x10019E25 */
        const char *psz = (g_brRaceBeginIntroSide == 0)
                          ? "RallyIntro1.dat"         /* 0x10019E2F */
                          : "RallyIntro2.dat";        /* 0x10019E36 */
        if (OP(BR_RB_PLAYMOVIE, pfnPlayMovie))
            g_brRaceBeginOps.pfnPlayMovie(psz, 0);
        /* 0x10019E48: the side alternates, 0 and 1. */
        ++g_brRaceBeginIntroSide;
        if (g_brRaceBeginIntroSide > 1)               /* 0x10019E50 `jle` */
            g_brRaceBeginIntroSide = 0;
        goto played;
    }
    if (g_brRaceBeginStage == 1) {                    /* 0x10019E15 */
        if (OP(BR_RB_PLAYMOVIE, pfnPlayMovie))
            g_brRaceBeginOps.pfnPlayMovie("RallyCredits.dat", 0);
        goto played;
    }
    if (g_brRaceBeginStage == 2) {                    /* 0x10019E00 */
        BrRaceCueLayout();
        if (OP(BR_RB_PLAYMOVIE, pfnPlayMovie))
            g_brRaceBeginOps.pfnPlayMovie("RallyOutro.dat", 0);
        goto played;
    }
    goto after;                                       /* 0x10019E63 */

played:
    if (OP(BR_RB_10063B60, pfn10063B60))              /* 0x10019E58 */
        g_brRaceBeginOps.pfn10063B60();
    g_brRaceBeginMovieDone = 0;                       /* 0x10019E5D */

after:
    if (pCtl != NULL) {
        pCtl->apRec[0] = NULL;                        /* 0x10019E69 */
        if (BR_RACEBEGIN_MAXREC > 1)
            pCtl->apRec[1] = NULL;                    /* 0x10019E71 */
    }

    /* 0x10019E79: three bytes at 0x10AF3BB4/B5/B6 == car+0x29AC/AD/AE set to
     * 0xFF.  No host model; recorded, not invented. */

    if (pCtl != NULL && pCtl->pHdr != NULL) {
        const uint8_t *h = pCtl->pHdr;

        g_brCfgChosenTrack = (int8_t)h[BR_RACEBEGIN_HDR_TRACK];    /* 0x10019E94 */

        if (pCar != NULL)
            pCar->f29A4 = (int8_t)h[BR_RACEBEGIN_HDR_CARMODEL];    /* 0x10019EA7 */
        if (OP(BR_RB_CARMODELSET, pfnCarModelSet))                 /* 0x10019EAC */
            g_brRaceBeginOps.pfnCarModelSet(pCar,
                (int8_t)h[BR_RACEBEGIN_HDR_CARMODEL]);

        if (pCar != NULL) {
            pCar->fE98 = (int8_t)h[BR_RACEBEGIN_HDR_HANDLING];     /* 0x10019EBD */
            pCar->fE9C = (int8_t)h[BR_RACEBEGIN_HDR_TRANS];        /* 0x10019ECA */
            pCar->fE90 = (int8_t)h[BR_RACEBEGIN_HDR_TIRE];         /* 0x10019ED7 */
            pCar->fE94 = (int8_t)h[BR_RACEBEGIN_HDR_SUSP];         /* 0x10019EE4 */
        }
        pCtl->b25 = h[BR_RACEBEGIN_HDR_EQUIP5];                    /* 0x10019EF0 */

        g_brCfgChosenWeather = (int8_t)h[BR_RACEBEGIN_HDR_WEATHER];/* 0x10019EFF */
        g_brCarPhysWeather   = g_brCfgChosenWeather;               /* 0x10019F04 */
    }
}

/* mode 2 -- 0x10019F0E, the link/replay arm.  This is the WRITE end of the
 * eight-byte header. */
static void begin_mode2(void)
{
    int32_t      iSlot = g_brRaceNEntrant;
    BrDriverCar *pSlot;
    BrRaceCtl   *pCtl;
    int32_t      i;

    if (OP(BR_RB_CARMODELSET, pfnCarModelSet))        /* 0x10019F1A */
        g_brRaceBeginOps.pfnCarModelSet(car_at(0), g_brCfgChosenCar);

    g_brRaceBeginFade   = 0;                          /* 0x10019F2A */
    g_brRaceBeginRecArmed = 1;                        /* 0x10019F30 */
    g_brRaceNDriver     = iSlot + 1;                  /* 0x10019F39 */
    g_brRaceNCar        = iSlot + 1;                  /* 0x10019F3E */

    pSlot = car_at(iSlot);
    pCtl  = ctl_of(pSlot);

    /* 0x10019F5C: the slot's header points at 0x11778850, which is a
     * DIFFERENT object from the source at 0x105BC8E0 the next block copies
     * out of.  Keeping them apart matters: the copy is what makes the
     * acceptance tests below read the INCOMING record rather than re-read
     * the buffer they came from. */
    if (pCtl != NULL)
        pCtl->pHdr = g_aBrRaceBeginRecIn;

    trace();                                          /* 0x10019F6E */

    /* 0x10019FA8: `rep movsd`/`rep movsb` of [0x105BC8D8] BYTES from
     * 0x105BC8E0 into the slot's header. */
    if (pCtl != NULL && pCtl->pHdr != NULL) {
        size_t n = (size_t)((g_brRaceBeginRecLen < 0) ? 0 : g_brRaceBeginRecLen);
        if (n > sizeof(g_aBrRaceBeginRec))
            n = sizeof(g_aBrRaceBeginRec);            /* DEVIATION: bounded  */
        memcpy(pCtl->pHdr, g_aBrRaceBeginRec, n);
    }

    if (pCtl != NULL && pCtl->pHdr != NULL && pSlot != NULL)
        pSlot->f29A4 = (int8_t)pCtl->pHdr[BR_RACEBEGIN_HDR_CARMODEL]; /* 0x10019FD9 */
    if (pCtl != NULL && pCtl->pHdr != NULL) {
        if (OP(BR_RB_CARMODELSET, pfnCarModelSet))    /* 0x10019FE6 */
            g_brRaceBeginOps.pfnCarModelSet(pSlot,
                (int8_t)pCtl->pHdr[BR_RACEBEGIN_HDR_CARMODEL]);
    }
    if (OP(BR_RB_10063B60, pfn10063B60))              /* 0x10019FEB */
        g_brRaceBeginOps.pfn10063B60();

    /* 0x1001A018: the two acceptance tests -- the track and the weather. */
    if (pCtl != NULL && pCtl->pHdr != NULL &&
        (int8_t)pCtl->pHdr[BR_RACEBEGIN_HDR_TRACK] == g_brCfgChosenTrack &&
        (int8_t)pCtl->pHdr[BR_RACEBEGIN_HDR_WEATHER] == g_brCfgChosenWeather) {
        trace();                                      /* 0x1001A031 "TRACK=" */
        pCtl->f48 = BR_RACEBEGIN_REC_LEN0;            /* 0x1001A058 */
        trace();                                      /* 0x1001A06B "PLAYLIMIT" */
        pCtl->f4C = g_brRaceBeginRecLen;              /* 0x1001A097 */
    } else {
        trace();                                      /* 0x1001A0A3 "WRONG TRACK" */
        if (pCtl != NULL)
            pCtl->pHdr = NULL;                        /* 0x1001A0CA */
        g_brRaceNCar    = 1;                          /* 0x1001A0CD */
        g_brRaceNDriver = 1;                          /* 0x1001A0D3 */
    }

    /* 0x1001A0E8.  Per-entrant recording buffers.  The three arrays overlap
     * in the original at index 2 and up -- see br_racebegin.h -- so the port
     * clamps and counts instead. */
    for (i = 0; i < g_brRaceNEntrant; ++i) {          /* 0x1001A0E0/0x1001A1E2 */
        BrDriverCar *pCar  = car_at(i);
        BrRaceCtl   *pC    = ctl_of(pCar);
        uint8_t     *pRec;
        uint8_t     *pEq   = NULL;

        if (i >= BR_RACEBEGIN_MAXREC) {
            ++s_cRecClamped;
            continue;
        }
        if (pC == NULL)
            continue;

        pRec = g_aBrRaceBeginRecHdr[i];
        pC->apRec[i] = pRec;                          /* 0x1001A104 */

        pRec[BR_RACEBEGIN_HDR_TRACK] = (uint8_t)g_brCfgChosenTrack;  /* 0x1001A11F */
        /* 0x1001A127 reads car+0x29A8 -- the APPLIED model, not the
         * requested one at +0x29A4 that the reader writes. */
        pRec[BR_RACEBEGIN_HDR_CARMODEL] =
            (pCar != NULL) ? (uint8_t)pCar->f29A8 : 0u;              /* 0x1001A131 */

        if (pCar != NULL) {
            pEq = pCar->pEquip;
            if (pEq == NULL && OP(BR_RB_EQUIPRECORD, pfnEquipRecord))
                pEq = g_brRaceBeginOps.pfnEquipRecord(pCar);
        }
        if (pEq != NULL) {
            pRec[BR_RACEBEGIN_HDR_HANDLING] = pEq[BR_RACEBEGIN_EQ_HANDLING]; /* 0x1001A14A */
            pRec[BR_RACEBEGIN_HDR_TRANS]    = pEq[BR_RACEBEGIN_EQ_TRANS];    /* 0x1001A163 */
            pRec[BR_RACEBEGIN_HDR_TIRE]     = pEq[BR_RACEBEGIN_EQ_TIRE];     /* 0x1001A17C */
            pRec[BR_RACEBEGIN_HDR_SUSP]     = pEq[BR_RACEBEGIN_EQ_SUSP];     /* 0x1001A195 */
            pRec[BR_RACEBEGIN_HDR_EQUIP5]   = pEq[BR_RACEBEGIN_EQ_FIFTH];    /* 0x1001A1AE */
        }
        pRec[BR_RACEBEGIN_HDR_WEATHER] = (uint8_t)g_brCfgChosenWeather;      /* 0x1001A1C1 */

        pC->aLen[i] = BR_RACEBEGIN_REC_LEN0;          /* 0x1001A1CA */
        pC->aCap[i] = BR_RACEBEGIN_REC_CAP;           /* 0x1001A1D8 */
    }
}

/* mode 3 and everything above 6 -- 0x1001A1EE */
static void begin_mode_default(void)
{
    if (OP(BR_RB_CARMODELSET, pfnCarModelSet))        /* 0x1001A1F9 */
        g_brRaceBeginOps.pfnCarModelSet(car_at(0), g_brCfgChosenCar);
    g_brRaceBeginFade = 0;                            /* 0x1001A203 */
    g_brRaceNDriver   = g_brRaceNEntrant;             /* 0x1001A209 */
    g_brRaceNCar      = g_brRaceNEntrant;             /* 0x1001A20E */
}

/* ==========================================================================
 * The common tail, 0x1001A218..0x1001AB6F
 * ========================================================================== */

/* 0x1001A2AE..0x1001A41C -- the track's "specials". */
static void begin_specials(void)
{
    int32_t i;

    g_brRaceBeginAirArmed   = 0;                      /* 0x1001A2B4 */
    g_brRaceBeginAirTrigger = 0;                      /* 0x1001A2C0 */
    g_brRaceBeginAirplane   = 0;                      /* 0x1001A2C6 */
    g_brRaceBeginPathRight  = 0;                      /* 0x1001A2CC */
    g_brRaceBeginPathLeft   = 0;                      /* 0x1001A2D2 */
    g_brRaceBeginPathT      = 0.0f;                   /* 0x1001A2D8 */
    g_brRaceBeginPathSeg    = 0.0f;                   /* 0x1001A2E2 */
    g_brRaceBeginPathIdx    = 0;                      /* 0x1001A2EC */
    g_brRaceBeginPathLen    = 0;                      /* 0x1001A2F2 */
    trace();                                          /* 0x1001A2F8 "specials" */
    g_brRaceBeginFxCount    = 0;                      /* 0x1001A309 */

    /* 0x1001A31A: the object list at 0x106EEE3C, stride 0xC, is walked and
     * its +0x08 byte selects one of five arms through the table at
     * 0x1001C664.  Kinds 3..7; anything else is skipped.  The list itself
     * comes from a hook -- it lives in the loaded track, which this module
     * does not own -- and the five arms name five globals nothing else in
     * this tree does:
     *
     *   kind 3  0x1001A353  the airplane's object index    -> 0x105BC7C0
     *   kind 4  0x1001A32D  the left path, and its LENGTH  -> 0x105BC7DC,
     *                                                         0x105BC7D4
     *   kind 5  0x1001A344  the right path                -> 0x105BC7E0
     *   kind 6  0x1001A362  the airplane's trigger        -> 0x105BC7C4
     *   kind 7  0x1001A371  a waterfall, appended to the
     *                       list at 0x105BC778             -> 0x105BCAE8
     */
    for (i = 0; i < g_brRaceBeginSpecialsN; ++i) {
        BrRaceSpecial e;
        int32_t       k;

        e.f00 = 0; e.f04 = 0; e.kind = 0;
        if (!OP(BR_RB_SPECIAL, pfnSpecial))
            continue;
        if (!g_brRaceBeginOps.pfnSpecial(i, &e))
            continue;

        /* 0x1001A31E `add eax,-3` then 0x1001A324 `cmp eax,4 / ja` -- the
         * UNSIGNED compare is what makes kinds below 3 skip too. */
        k = e.kind - 3;
        if ((uint32_t)k > 4u)
            continue;                                 /* 0x1001A395 */

        switch (e.kind) {
        case BR_RACEBEGIN_SPECIAL_PATHLEFT:           /* 0x1001A32D */
            g_brRaceBeginPathLen  = e.f04;
            g_brRaceBeginPathLeft = e.f00;
            break;
        case BR_RACEBEGIN_SPECIAL_PATHRIGHT:          /* 0x1001A344 */
            g_brRaceBeginPathRight = e.f00;
            break;
        case BR_RACEBEGIN_SPECIAL_AIRPLANE:           /* 0x1001A353 */
            g_brRaceBeginAirplane = e.f00;
            break;
        case BR_RACEBEGIN_SPECIAL_TRIGGER:            /* 0x1001A362 */
            g_brRaceBeginAirTrigger = e.f00;
            break;
        default:                                      /* 0x1001A371 */
            /* The append is UNBOUNDED in the original -- 0x1001A37F writes
             * through 0x105BC778 + 4*count with no test.  DEVIATION: the
             * port stops writing past its array but still counts, so the
             * count matches the original's. */
            if (g_brRaceBeginFxCount >= 0 &&
                g_brRaceBeginFxCount < (int32_t)(sizeof(g_aBrRaceBeginFx) /
                                                 sizeof(g_aBrRaceBeginFx[0])))
                g_aBrRaceBeginFx[g_brRaceBeginFxCount] = e.f00;
            ++g_brRaceBeginFxCount;                   /* 0x1001A386 */
            break;
        }
        trace();                                      /* 0x1001A38D */
    }

    /* 0x1001A3A6.  The airplane needs its trigger AND both paths; any one
     * missing takes it out. */
    if (g_brRaceBeginAirTrigger == 0 ||
        g_brRaceBeginPathLeft   == 0 ||
        g_brRaceBeginPathRight  == 0) {
        g_brRaceBeginAirplane = 0;                    /* 0x1001A3C0 */
        return;                                       /* 0x1001A3CE, via eax == 0 */
    }
    if (g_brRaceBeginAirplane == 0)                   /* 0x1001A3CC */
        return;

    {
        /* 0x1001A3D3: the vector is seeded to (1, 0, 0).
         *
         * 0x1001A3F6 `lea edx,[esp+0x2C]` and 0x1001A3FB `lea eax,[esp+0x30]`
         * are separated by ONE push, so BOTH name esp+0x2C -- the transform
         * is IN PLACE.  See the banner; this is the ESP-relative trap that
         * has shipped as a bug twice in this tree, running the other way. */
        float v[3];
        v[0] = 1.0f; v[1] = 0.0f; v[2] = 0.0f;

        if (OP(BR_RB_VECXFORM, pfnVecXform))          /* 0x1001A401 */
            g_brRaceBeginOps.pfnVecXform(v, v, NULL);
        if (OP(BR_RB_VECLEN, pfnVecLen))              /* 0x1001A40E */
            g_brRaceBeginPathScale = g_brRaceBeginOps.pfnVecLen(v);
    }
}

/* 0x1001A599..0x1001A65F -- one controller per car. */
static void begin_controllers(void)
{
    int32_t i;

    g_brRaceBeginPathIdx = 0;                         /* 0x1001A5A2, 0x105CCB60
                                                       * shares this arm's
                                                       * zeroing; it is a
                                                       * different address and
                                                       * is not modelled      */

    for (i = 0; i < g_brRaceNCar; ++i) {              /* 0x1001A5A0/0x1001A657 */
        BrDriverCar *pCar = car_at(i);
        int32_t      mode = g_brRaceRules.mode;

        if (pCar == NULL)
            continue;

        pCar->f29B0 = BR_RACEBEGIN_FULL;              /* 0x1001A5C7 */

        if (i < g_brRaceNEntrant) {                   /* 0x1001A5C5 */
            pCar->pfnControl = g_brRaceBeginOps.pfnCtlHuman;   /* 0x1001A5CF */
            if (g_brRaceBeginOps.pfnCtlHuman == NULL)
                ++s_aSkipped[BR_RB_CTLHUMAN];
            pCar->b29AF = 0u;                         /* 0x1001A612 */
        } else if (mode == 2) {                       /* 0x1001A5D7 */
            pCar->pfnControl = g_brRaceBeginOps.pfnCtlHuman;   /* 0x1001A5DC */
            if (g_brRaceBeginOps.pfnCtlHuman == NULL)
                ++s_aSkipped[BR_RB_CTLHUMAN];
            /* 0x1001A5E2 `mov [esi+0x1AA7], al` -- al is the low byte of the
             * MODE, which on this arm is 2.  So the byte br_racestep.c reads
             * as "the recovery state" starts at 2 for a link opponent, and
             * that is what puts it straight into the +0x29B0 bleed. */
            pCar->b29AF = (uint8_t)mode;
            pCar->f29B0 = BR_RACEBEGIN_LINK_RECOVER;  /* 0x1001A5E8 */
            /* NOTE: this arm does NOT clear +0x29AF afterwards -- it jumps
             * past 0x1001A612 to 0x1001A619. */
        } else if (mode == 6) {                       /* 0x1001A5F4 */
            pCar->pfnControl = BrRaceCarCtlOutro;     /* 0x1001A5F9 */
            pCar->b29AF = 0u;                         /* 0x1001A612 */
        } else {
            if (OP(BR_RB_1005C490, pfn1005C490))      /* 0x1001A607 */
                g_brRaceBeginOps.pfn1005C490(pCar);
            pCar->pfnControl = g_brRaceBeginOps.pfnCtlAi;      /* 0x1001A60C */
            if (g_brRaceBeginOps.pfnCtlAi == NULL)
                ++s_aSkipped[BR_RB_CTLAI];
            pCar->b29AF = 0u;                         /* 0x1001A612 */
        }

        if (pCar->fE88 == 0) {                        /* 0x1001A619 */
            if (OP(BR_RB_1006FCE0, pfn1006FCE0))      /* 0x1001A62C */
                g_brRaceBeginOps.pfn1006FCE0(pCar, i, pCar->f29A8);
        }
        if (OP(BR_RB_1005E7B0, pfn1005E7B0))          /* 0x1001A637 */
            g_brRaceBeginOps.pfn1005E7B0(pCar);

        pCar->fF7C  = 0;                              /* 0x1001A641 */
        pCar->fFFC  = 0;                              /* 0x1001A644 */
        pCar->f1004 = 0;                              /* 0x1001A64A */
    }

    /* 0x1001A65F.  An EMPTY field takes this arm -- and note it reads the
     * count AFTER the loop, so a loop that ran leaves the same value. */
    if (g_brRaceNCar == 0) {
        BrDriverCar *pCar = car_at(0);
        /* 0x1001A663 `rep stosd` with ecx == 0x57E2 == 22498 DWORDS ==
         * 89992 bytes -- exactly one car image slot.  DWORDS, not bytes. */
        if (OP(BR_RB_CARIMAGE, pfnCarImage)) {
            uint8_t *p = g_brRaceBeginOps.pfnCarImage(0);
            if (p != NULL)
                memset(p, 0, (size_t)BR_RACEBEGIN_CARIMAGE_STRIDE);
        }
        if (OP(BR_RB_1006FCB0, pfn1006FCB0))          /* 0x1001A675 */
            g_brRaceBeginOps.pfn1006FCB0(pCar, 0);
        if (OP(BR_RB_1005E7B0, pfn1005E7B0))          /* 0x1001A67F */
            g_brRaceBeginOps.pfn1005E7B0(pCar);
        if (OP(BR_RB_10001CF0, pfn10001CF0))          /* 0x1001A689 */
            g_brRaceBeginOps.pfn10001CF0(pCar);
        /* 0x1001A68E: `fld / fsub -1.0 / fstp`, i.e. += 1. */
        g_brRaceBeginAF397C =
            (float)((double)g_brRaceBeginAF397C - BR_RACEBEGIN_AF397C_STEP);
    }
}

/* 0x1001A6A0..0x1001A971 -- the car paint textures. */
static void begin_paint(void)
{
    int32_t k;

    for (k = 0; k < g_brRaceBeginNTexSet; ++k) {      /* 0x1001A6AD/0x1001A96B */
        uint8_t *pImg = NULL;
        uint8_t *pHdr;
        int32_t  hdrOff;
        int32_t  w, h, mw, mh, rows;
        int32_t  m;

        /* 0x1001A6C9 reads the slot index out of 0x106E86C8 + 0x58*k - 4 and
         * multiplies by 89992 -- the same `lea` chain as 0x10019D00. */
        if (OP(BR_RB_CARIMAGE, pfnCarImage))
            pImg = g_brRaceBeginOps.pfnCarImage(g_brRaceBegin6E86C8);
        if (pImg == NULL) {
            /* 0x1001A94B still runs on this path in the original; the
             * handle slots are reset whatever the image was. */
            if (k < BR_RACEBEGIN_MAXREC)
                g_aBrRaceBeginTexSlot[k] = BR_RACEBEGIN_TEXSLOT_NONE;
            continue;
        }

        /* 0x1001A6E1: the weather picks which of the two headers inside the
         * image the textures are described by. */
        hdrOff = (g_brCarPhysWeather == BR_RACEBEGIN_WEATHER_FAR)
                 ? BR_RACEBEGIN_HDR_OFF_FAR            /* 0x1001A6EB */
                 : BR_RACEBEGIN_HDR_OFF_NEAR;          /* 0x1001A6F3 */
        pHdr = pImg + hdrOff;

        w  = (int8_t)pImg[0xE4];                      /* 0x1001A6F9 */
        h  = (int8_t)pImg[0xE5];                      /* 0x1001A700 */
        mw = (int8_t)pImg[0xE8];                      /* 0x1001A7F0 */
        mh = (int8_t)pImg[0xE9];                      /* 0x1001A7F7 */
        rows = (int8_t)pImg[0xD8] + 2;                /* 0x1001A84E */

        if (OP(BR_RB_TEXCREATE, pfnTexCreate)) {      /* 0x1001A726 */
            (void)g_brRaceBeginOps.pfnTexCreate(pImg + 0x500, pHdr,
                                                w, h, w,
                                                1, 2, 0, 0, 1, 1, 0, 0, 0, 0);
        }

        /* 0x1001A72E: byte 0xDB selects whether a mip chain is built, and
         * which of two edits is made to each level's header word. */
        if (pImg[0xDB] == 1u || pImg[0xDB] == 2u) {
            uint8_t *pWord = pHdr + 0x1FE;            /* 0x1001A750 */

            for (m = 0; m < BR_RACEBEGIN_TEX_LEVELS; ++m) {  /* 0x1001A756 */
                if (pImg[0xDB] == 1u) {
                    /* 0x1001A768..0x1001A78A.  The word is read, two fields
                     * are extracted, recombined, and stored BYTE-SWAPPED --
                     * `mov cl,ah / mov ch,al` puts the low byte high.  The
                     * image is N64 data, so the swap is the endianness. */
                    uint32_t wrd = (uint32_t)pWord[0] |
                                   ((uint32_t)pWord[1] << 8);
                    uint32_t v   = ((((wrd & 0x3000u) | 0x0800u) >> 11) |
                                    ((wrd & 0x00C6u) << 5)) & 0xFFFFu;
                    uint32_t out = ((v & 0xFFu) << 8) | ((v >> 8) & 0xFFu);
                    pWord[0] = (uint8_t)(out & 0xFFu);
                    pWord[1] = (uint8_t)((out >> 8) & 0xFFu);
                } else {
                    /* 0x1001A793 `and word ptr [edi], 0xFEFF`. */
                    pWord[1] = (uint8_t)(pWord[1] & 0xFEu);
                }

                if (OP(BR_RB_TEXCREATE, pfnTexCreate)) {      /* 0x1001A7C5 */
                    (void)g_brRaceBeginOps.pfnTexCreate(pImg + 0x500, pHdr,
                                                        w, h, w,
                                                        1, 2, 0, 0,
                                                        1, 1, 0, 0, 0, 0);
                }
                pWord -= 2;                           /* 0x1001A7D8 */
            }
        }

        /* 0x1001A82F: the mirror texture, from w*h bytes into the pixels. */
        if (OP(BR_RB_TEXCREATE, pfnTexCreate)) {
            (void)g_brRaceBeginOps.pfnTexCreate(pImg + 0x500 + (size_t)(w * h),
                                                pHdr, mw, mh, mw,
                                                1, 2, 0, 0, 1, 1, 0, 0, 1, 0);
        }

        /* 0x1001A888: the source rows are moved to a scratch area 0x8000
         * bytes above the image's own base, ready to be downloaded from. */
        {
            int32_t  n   = rows * mw * mh;
            uint8_t *pDst = pImg - n + BR_RACEBEGIN_PAINT_BYTES;
            uint8_t *pSrc = pImg + 0x500 + (size_t)(w * h);
            if (n > 0)
                memmove(pDst, pSrc, (size_t)n);

            /* 0x1001A8AB.  One download per row, walking a cursor through
             * the pixels and another through the scratch. */
            {
                uint8_t *pCur = pImg + 0x500;
                uint8_t *pOut = pDst;
                for (m = 0; m < rows; ++m) {
                    int32_t got = 0;
                    if (OP(BR_RB_TEXDOWNLOAD, pfnTexDownload))  /* 0x1001A8C3 */
                        got = g_brRaceBeginOps.pfnTexDownload(pCur, pOut,
                                                              pHdr, mw, mh,
                                                              mw, 1, 2);
                    pCur += got;                      /* 0x1001A8C9 */
                    /* 0x1001A8D0 -- car+0x1004 keeps the LAST row's size. */
                    { BrDriverCar *pc = car_at(0);
                      if (pc != NULL) pc->f1004 = got; }

                    /* 0x1001A8CE `ja`, unsigned: the pixel cursor must not
                     * pass the scratch. */
                    if ((uintptr_t)pCur > (uintptr_t)pOut) {
                        if (OP(BR_RB_STRING, pfnString) &&
                            OP(BR_RB_FATAL, pfnFatal))
                            g_brRaceBeginOps.pfnFatal(
                                g_brRaceBeginOps.pfnString(
                                    BR_RACEBEGIN_MSG_OVERRUN));
                    }
                    pOut += mw * mh;                  /* 0x1001A901 */
                    /* 0x1001A90B `jle`, SIGNED, against 0x8000. */
                    if ((ptrdiff_t)(pCur - pImg) > BR_RACEBEGIN_PAINT_BYTES) {
                        if (OP(BR_RB_STRING, pfnString) &&
                            OP(BR_RB_FATAL, pfnFatal))
                            g_brRaceBeginOps.pfnFatal(
                                g_brRaceBeginOps.pfnString(
                                    BR_RACEBEGIN_MSG_TOOBIG));
                    }
                }
            }
        }

        /* 0x1001A94B.  See the banner: the cursor starts inside 0x100A6B68's
         * eight-byte -1 prefix, so this is "reset the handle slots". */
        if (k < BR_RACEBEGIN_MAXREC)
            g_aBrRaceBeginTexSlot[k] = BR_RACEBEGIN_TEXSLOT_NONE;
        else
            ++s_cRecClamped;   /* DEVIATION: a third set would write over the
                                * format string that follows the prefix      */
    }
}

/* 0x1001AA5E..0x1001AB6F -- what happens after the substate is raised. */
static void begin_assets(void)
{
    if (g_brRaceReplay == 0) {                        /* 0x1001AA64 */
        /* 0x1001AA66: 0x105BCAEC is seeded with 0x105BCAF8, the destination,
         * and the loader is given (dst, name, buf). */
        if (OP(BR_RB_BLOBLOAD, pfnBlobLoad))          /* 0x1001AA7F */
            g_brRaceBeginOps.pfnBlobLoad(NULL, "misc\\modelLights.blob", NULL);
        if (OP(BR_RB_1002ECAC, pfn1002ECAC))          /* 0x1001AA8E */
            g_brRaceBeginOps.pfn1002ECAC(NULL);
    }

    if (OP(BR_RB_10033B50, pfn10033B50))              /* 0x1001AA96 */
        g_brRaceBeginOps.pfn10033B50();
    if (OP(BR_RB_10063DD0, pfn10063DD0))              /* 0x1001AA9B */
        g_brRaceBeginOps.pfn10063DD0();

    g_brRacePaused        = 0;                        /* 0x1001AAA5 */
    g_brRaceBeginRamped   = 0;                        /* 0x1001AAAD */
    g_brRaceBeginSeq888   = -1;                       /* 0x1001AAB3 */
    g_brRaceBeginSeqT     = 0.0f;                     /* 0x1001AABD */
    g_brRaceBeginSeqIdx   = 0;                        /* 0x1001AAC7 */
    g_brRaceBeginDrawn    = 1;                        /* 0x1001AACD */

    if (g_brRaceReplay == 0) {                        /* 0x1001AAD3 */
        if (OP(BR_RB_1002E13B, pfn1002E13B))          /* 0x1001AAD5 */
            g_brRaceBeginOps.pfn1002E13B();
        if (OP(BR_RB_10060E30, pfn10060E30))          /* 0x1001AADA */
            g_brRaceBeginOps.pfn10060E30();
    }

    g_brRaceBeginActive = 0;                          /* 0x1001AAE4 */

    if (g_brRaceReplay != 0) {                        /* 0x1001AAEC */
        if (OP(BR_RB_10063B60, pfn10063B60))          /* 0x1001AAEE */
            g_brRaceBeginOps.pfn10063B60();
    } else {
        int32_t mode;

        if (OP(BR_RB_10063A00, pfn10063A00))          /* 0x1001AAF5 */
            g_brRaceBeginOps.pfn10063A00();
        mode = g_brRaceRules.mode;                    /* 0x1001AAFA */
        if (mode == 4) {                              /* 0x1001AAFF */
            if (OP(BR_RB_10063A40, pfn10063A40))      /* 0x1001AB04 */
                g_brRaceBeginOps.pfn10063A40();
            mode = g_brRaceRules.mode;                /* 0x1001AB09 */
        }
        /* 0x1001AB0E: a BYTE test on 0x100BB2E0, which ships as 0xBF. */
        {
            int32_t music = 0xBF;   /* 0x100BB2E0's shipped value */
            if (music != 0) {
                int32_t id;
                if (mode == 4 && g_brRaceBeginStage == 2)
                    id = BR_RACEBEGIN_OUTRO_TRACK;    /* 0x1001AB28 */
                else if (mode == 4 && g_brRaceBeginStage == 1)
                    id = BR_RACEBEGIN_CREDIT_TRACK;   /* 0x1001AB35 */
                else {
                    id = 0;
                    if (OP(BR_RB_10002C00, pfn10002C00))  /* 0x1001AB39 */
                        id = g_brRaceBeginOps.pfn10002C00();
                }
                if (OP(BR_RB_10002AF0, pfn10002AF0))  /* 0x1001AB3F */
                    g_brRaceBeginOps.pfn10002AF0(id);
                if (OP(BR_RB_10002D30, pfn10002D30))  /* 0x1001AB4E */
                    g_brRaceBeginOps.pfn10002D30(music);
            }
        }
    }

    if (OP(BR_RB_10013E80, pfn10013E80))              /* 0x1001AB56 */
        g_brRaceBeginOps.pfn10013E80();
    trace();                                          /* 0x1001AB5B */
    BrRaceClockReset();                               /* 0x1001AB60 */
}

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
void BrRaceStepBegin(void)
{
    int32_t mode;

    g_brRaceBeginActive = 1;                          /* 0x10019AFF */
    trace();                                          /* 0x10019B05 */
    if (OP(BR_RB_1000CB80, pfn1000CB80))              /* 0x10019B0D */
        g_brRaceBeginOps.pfn1000CB80();

    if (g_brRaceReplay == 0) {                        /* 0x10019B12 */
        trace();                                      /* 0x10019B1A */
        trace();                                      /* 0x10019B1F */
        if (OP(BR_RB_SLOT_B7352C, pfnSlotB7352C))     /* 0x10019B24 */
            g_brRaceBeginOps.pfnSlotB7352C();
        if (OP(BR_RB_SLOT_18ED1E8, pfnSlot18ED1E8))   /* 0x10019B2A */
            g_brRaceBeginOps.pfnSlot18ED1E8();
        if (OP(BR_RB_SLOT_0B849C, pfnSlot0B849C))     /* 0x10019B30 */
            g_brRaceBeginOps.pfnSlot0B849C();

        /* 0x10019B36: mode 5 uses a fixed track and everything else the
         * chosen one. */
        if (OP(BR_RB_TRACKHANDLING, pfnTrackHandling))    /* 0x10019B4B */
            g_brRaceBeginOps.pfnTrackHandling(
                (g_brRaceRules.mode == 5) ? BR_RACEBEGIN_MODE5_TRACK
                                          : g_brCfgChosenTrack);
        if (OP(BR_RB_1006E030, pfn1006E030))          /* 0x10019B53 */
            g_brRaceBeginOps.pfn1006E030();
        if (OP(BR_RB_SLOT_B73528, pfnSlotB73528))     /* 0x10019B58 */
            g_brRaceBeginOps.pfnSlotB73528();
        BrRaceHudFrame();                             /* 0x10019B5E */
    }

    if (OP(BR_RB_1002BF24, pfn1002BF24))              /* 0x10019B68 */
        g_brRaceBeginOps.pfn1002BF24(NULL);
    trace();                                          /* 0x10019B71 */
    if (OP(BR_RB_1002BF24, pfn1002BF24))              /* 0x10019B7E */
        g_brRaceBeginOps.pfn1002BF24(NULL);
    if (OP(BR_RB_100353C0, pfn100353C0))              /* 0x10019B88 */
        g_brRaceBeginOps.pfn100353C0(BR_RACEBEGIN_100353C0_ARG);

    mode = g_brRaceRules.mode;                        /* 0x10019B8D */
    g_brRaceBeginFade = 0;                            /* 0x10019B97 */

    /* 0x10019B9D: modes 1, 6 and 2 keep whatever the camera setting was;
     * everything else is forced to 3.  0x100BCBE8 is br_race.h's. */
    if (mode != 1 && mode != 6 && mode != 2) {
        /* recorded rather than stored: 0x100BCBE8 has an owner elsewhere and
         * writing it from here would be a second model of one dword */
        ++s_aSkipped[BR_RB_DIFFICULTY];
    }
    g_brRaceBeginMirrorOff = 0;                       /* 0x10019BB5 */

    /* 0x10019BBB `ja` then 0x10019BC1's table at 0x1001C648.  Mode 3's entry
     * IS the out-of-range target, read out of the image. */
    switch (mode) {
    case 0:  begin_mode0(); break;
    case 1:  begin_mode1(); break;
    case 2:  begin_mode2(); break;
    case 4:  begin_mode4(); break;
    case 5:  begin_mode5(); break;
    case 6:  begin_mode6(); break;
    case 3:
    default: begin_mode_default(); break;
    }

    /* ---- the common tail, 0x1001A218 ------------------------------------ */

    if (g_brRaceRules.mode == 5) {                    /* 0x1001A222 */
        g_brRaceBeginDifficulty = 0;                  /* 0x1001A226 */
    } else {
        int32_t d = 0;
        if (OP(BR_RB_DIFFICULTY, pfnDifficulty))      /* 0x1001A234 */
            d = g_brRaceBeginOps.pfnDifficulty(g_brCfgChosenTrack);
        /* 0x1001A23B: bit 4 of the record's +0x04. */
        g_brRaceBeginDifficulty = (d >> 4) & 1;
    }

    if (OP(BR_RB_100627B0, pfn100627B0))              /* 0x1001A24F */
        g_brRaceBeginOps.pfn100627B0(g_brCarPhysWeather);

    if (g_brRaceRules.mode != 4 && g_brRaceReplay == 0) {   /* 0x1001A25C */
        trace();                                      /* 0x1001A269 */
        trace();                                      /* 0x1001A26E */
    }
    if (OP(BR_RB_10061310, pfn10061310))              /* 0x1001A273 */
        g_brRaceBeginOps.pfn10061310();

    if (g_brRaceReplay == 0) {                        /* 0x1001A27F */
        if (OP(BR_RB_TRACKLOAD, pfnTrackLoad))        /* 0x1001A299 */
            g_brRaceBeginOps.pfnTrackLoad(
                (g_brRaceRules.mode == 5) ? BR_RACEBEGIN_MODE5_TRACK
                                          : g_brCfgChosenTrack);
        if (g_brRaceReplay == 0) {                    /* 0x1001A2A6 */
            begin_specials();
            /* 0x1001A41C: the camera index, one less than 0x100BCBE8. */
            g_brRaceBeginCamMinus1 = -1;
        }
    }

    /* 0x1001A430.  Note the flags: the `jne` at 0x1001A43C is still reading
     * the `cmp eax, ebp` at 0x1001A428, which compared the REPLAY flag. */
    g_brRaceBeginNTexSet = (g_brRaceReplay != 0) ? 1 : g_brRaceNEntrant;

    /* 0x1001A44A: `neg / sbb / and` is "the best car if replaying, else 0",
     * and 0x1001A45A's `sete` reads that same result. */
    g_brRaceBegin6E86C8 = (g_brRaceReplay != 0) ? g_brRaceBeginBestCar : 0;
    g_brRaceBegin6E8720 = (g_brRaceBegin6E86C8 == 0);

    if (g_brRaceRules.mode == 4) {                    /* 0x1001A467 */
        /* 0x1001A46C stores the address 0x10AF3988; no host model. */
        g_brRaceBeginB1CF10 = 0xB4;                   /* 0x1001A476 */
    } else {
        int32_t i;
        int32_t iCar = 0;

        for (i = 0; i < g_brRaceNEntrant; ++i) {      /* 0x1001A487/0x1001A4ED */
            BrDriverCar *pCar = car_at(i);
            BrRaceCtl   *pCtl;
            uint8_t     *pEq  = NULL;
            int32_t      sel;

            iCar = i + 1;
            if (pCar == NULL)
                continue;
            pEq = pCar->pEquip;
            if (pEq == NULL && OP(BR_RB_EQUIPRECORD, pfnEquipRecord))
                pEq = g_brRaceBeginOps.pfnEquipRecord(pCar);
            if (pEq != NULL) {
                pCar->fE9C = (int8_t)pEq[BR_RACEBEGIN_EQ_TRANS];  /* 0x1001A498 */
                pCar->fE94 = (int8_t)pEq[BR_RACEBEGIN_EQ_SUSP];   /* 0x1001A4A3 */
                pCar->fE90 = (int8_t)pEq[BR_RACEBEGIN_EQ_TIRE];   /* 0x1001A4AE */
                pCar->fE98 = (int8_t)pEq[BR_RACEBEGIN_EQ_HANDLING];/* 0x1001A4B9 */
            }

            sel = 0;
            if (OP(BR_RB_10B71530, pfn10B71530))      /* 0x1001A4BC */
                sel = g_brRaceBeginOps.pfn10B71530();
            pCtl = ctl_of(pCar);
            if (pCtl != NULL) {
                /* 0x1001A4C2: `dec/je` three times -- 1, 2 and 3 all take 5,
                 * everything else takes 2. */
                pCtl->b25 = (sel == 1 || sel == 2 || sel == 3) ? 5u : 2u;
            }
        }

        /* 0x1001A4FB: mode 2 copies the OPPONENT's equipment out of its own
         * header instead, and skips the defaults entirely. */
        if (g_brRaceRules.mode == 2) {
            BrDriverCar *pOpp = car_at(1);            /* 0x1001A4FF, 0x10AF6730
                                                       * == 0x10AF3BC8 + 0x2B68 */
            BrRaceCtl   *pOc  = ctl_of(pOpp);
            if (pOc != NULL && pOc->pHdr != NULL) {
                g_aBrRaceBeginLink[2] = (int8_t)pOc->pHdr[2];   /* 0x1001A50F */
                g_aBrRaceBeginLink[3] = (int8_t)pOc->pHdr[3];   /* 0x1001A51C */
                g_aBrRaceBeginLink[0] = (int8_t)pOc->pHdr[4];   /* 0x1001A529 */
                g_aBrRaceBeginLink[1] = (int8_t)pOc->pHdr[5];   /* 0x1001A536 */
                pOc->b25 = pOc->pHdr[6];                        /* 0x1001A542 */
                goto tail;
            }
        }

        /* 0x1001A547: every car past the entrant count gets the defaults.
         * The cursor is the one the loop above left behind. */
        for (; iCar < g_brRaceNCar; ++iCar) {
            BrDriverCar *pCar = car_at(iCar);
            BrRaceCtl   *pCtl = ctl_of(pCar);
            if (pCar != NULL) {
                pCar->fE9C = 1;                       /* 0x1001A568 transmission */
                pCar->fE94 = 1;                       /* 0x1001A56B suspension   */
                pCar->fE90 = 2;                       /* 0x1001A56D tire         */
                pCar->fE98 = 0;                       /* 0x1001A570 handling     */
            }
            if (pCtl != NULL)
                pCtl->b25 = 0u;                       /* 0x1001A57F */
        }
    }

tail:
    if (g_brRaceReplay == 0) {                        /* 0x1001A593 */
        begin_controllers();
        begin_paint();
    } else {
        /* 0x1001A973 -- and it is NOT a second write to 0x100AA044.
         *
         * `mov eax,[0x100B3858] / mov [esp+0x14],eax` puts the entrant count
         * into the paint loop's COUNTER, which 0x1001A6A5 otherwise seeds to
         * zero; the BOUND stays at 0x100AA044, which 0x1001A436 has already
         * pinned to 1 on this path.  So a replay enters the loop with the
         * counter at or past the bound and the body does not run -- the
         * cars keep the textures they already have.
         *
         * This was written as `NTexSet = nEntrant` first, which reads the
         * two stack slots as one object.  The mutation harness caught it:
         * the two spellings agree whenever nEntrant is 1 and the shipped
         * replay path is exactly that case, so no ordinary fixture separates
         * them.  It is the same class of error as the `[esp+N]` pair at
         * 0x1001A3F6 -- a displacement is not an identity. */
        s_cReplayPaintSkipped += (g_brRaceNEntrant >= g_brRaceBeginNTexSet);
    }

    /* 0x1001A97C..0x1001AA5E is the SCRIPT SEED, and br_racestep.c already
     * transcribes it as BrRaceStepInit -- the same block of the same
     * function.  It is called rather than repeated, so the five globals it
     * writes keep exactly one host object each. */
    (void)BrRaceStepInit();

    /* 0x1001A9EF..0x1001AA4F, which BrRaceStepInit does not cover. */
    if (OP(BR_RB_10062830, pfn10062830))              /* 0x1001A9F5 */
        g_brRaceBeginOps.pfn10062830(g_brCarPhysWeather);

    /* 0x1001A9FA: an unset 0x106ED6AC plus track 0 or 6 takes the airplane
     * out.  0x106ED6AC has no host model, and .bss says zero. */
    if (g_brCfgChosenTrack == 0 || g_brCfgChosenTrack == 6)
        g_brRaceBeginAirplane = 0;                    /* 0x1001AA16 */

    if (OP(BR_RB_RAMP_A, pfnRampA))                   /* 0x1001AA26 */
        g_brRaceBeginOps.pfnRampA(BR_RACEBEGIN_RAMP_A, BR_RACEBEGIN_RAMP_B);
    if (OP(BR_RB_RAMP_B, pfnRampB))                   /* 0x1001AA38 */
        g_brRaceBeginOps.pfnRampB(BR_RACEBEGIN_RAMP_A, BR_RACEBEGIN_RAMP_B);
    if (OP(BR_RB_RAMP_C, pfnRampC))                   /* 0x1001AA4A */
        g_brRaceBeginOps.pfnRampC(BR_RACEBEGIN_RAMP_A, BR_RACEBEGIN_RAMP_B);

    /* BrRaceStepInit already stored the substate at 0x1001AA5E. */
    begin_assets();
}
