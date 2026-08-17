/* br_racestep.c -- 0x10019A70, the race step, and the two per-driver passes
 * it drives.  See br_racestep.h for the mechanism, the address pairs and the
 * blocks of the original that are deliberately not here.
 *
 * Transcribed from orig/BRGlide.dll.  Every branch below carries the address
 * of the instruction it is, so the two can be diffed.
 */
#include <stddef.h>
#include <string.h>

#include "br_racestep.h"
#include "br_gamestep.h"

/* ==========================================================================
 * The data, read out of BRGlide.dll rather than assumed
 * ========================================================================== */

/* 0x100A9578, eight {int32 state; float seconds} pairs.  Entry 8 onward is
 * unrelated .rdata (0x100A95C0 is the start of a string), and the original
 * never indexes past 7: the script's last entry is state 7, whose arm does
 * not advance. */
const BrRaceLightStep g_aBrRaceLightScript[BR_RS_SCRIPT_LEN] = {
    { 0, 0.0f },   /* 0x100A9578 */
    { 1, 2.2f },   /* 0x100A9580 */
    { 2, 2.3f },   /* 0x100A9588 -- the 3-2-1 */
    { 3, 2.0f },   /* 0x100A9590 -- GREEN */
    { 4, 0.0f },   /* 0x100A9598 -- RACING; 0.0 and no timer arm */
    { 5, 2.0f },   /* 0x100A95A0 */
    { 6, 2.0f },   /* 0x100A95A8 */
    { 7, 0.0f }    /* 0x100A95B0 */
};

/* 0x100A9548.  The state-2 timer counts DOWN from 2.3, so these fire in
 * order.  See br_racestep.h for why there are five of them. */
const float g_aBrRaceBeepT[BR_RS_BEEP_COUNT] = {
    2.15f,   /* 0x100A9548 */
    1.50f,   /* 0x100A954C */
    0.85f,   /* 0x100A9550 */
    0.20f,   /* 0x100A9554 */
    0.00f    /* 0x100A9558 -- what the unbounded index reaches, and the
              *               reason the countdown stops at four sounds  */
};

/* ==========================================================================
 * The holes
 * ========================================================================== */

uint32_t g_aBrRaceStepHole[BR_RS_HOLE_COUNT];

static const char *const g_aBrRaceStepHoleName[BR_RS_HOLE_COUNT] = {
    "car+0xF08 control chain -> 0x1006F170",
    "0x10060A30 lap info      (286 B)",
    "0x1006E9E0..0x1006EBC0 skid/trail",
    "0x100623A0 -> 0x1005ACE0 + 0x10001CF0",
    "0x10060E00 / 0x10060DF0 countdown sound",
    "0x1001B27A HUD + mirror + render (~5.5 KB)",
    "0x1005F310 grid placement (538 B)",
    "0x1005F6C0 lap save/restore (2104 B)",
    "0x1001AD08 difficulty -> car+0xFF0",
    "0x1005C450 scratch clear (62 B)",
    "0x1005ECF0 walked off the track image"
};

void BrRaceStepHoleReset(void)
{
    int i;
    for (i = 0; i < BR_RS_HOLE_COUNT; ++i)
        g_aBrRaceStepHole[i] = 0u;
}

const char *BrRaceStepHoleName(int i)
{
    if (i < 0 || i >= BR_RS_HOLE_COUNT)
        return "(no such hole)";
    return g_aBrRaceStepHoleName[i];
}

BrRaceStepHooks g_brRaceStepHooks;

#define BR_RS_HOLE(id, hook, arg)                       \
    do {                                                \
        ++g_aBrRaceStepHole[(id)];                      \
        if (g_brRaceStepHooks.hook != NULL)             \
            g_brRaceStepHooks.hook(arg);                \
    } while (0)

/* ==========================================================================
 * The globals
 * ========================================================================== */

BrDriver     *g_pBrRaceDriver;
BrDriverCar  *g_pBrRaceCar;
int32_t       g_brRaceNDriver;
int32_t       g_brRaceNCar;
int32_t       g_brRaceNEntrant;

int32_t       g_brRaceLights;
float         g_brRaceLightT;
int32_t       g_brRaceScript;
int32_t       g_brRaceSubstate;
int32_t       g_brRaceBeep;
float         g_brRaceFade;

float         g_brRaceStepDt = 1.0f / 30.0f;   /* 0x3D088889, BR_PHYS_DT */

int32_t       g_brRacePaused;
int32_t       g_brRaceReplay;
int32_t       g_brRaceTick   = 1;   /* a fixed-timestep host ticks always */
int32_t       g_brRaceNet;
int32_t       g_brRaceHudA;
int32_t       g_brRaceHudB;

BrRaceRules   g_brRaceRules;
const BrTrack *g_pBrRaceTrack;

uint32_t      g_brRacePathNode;
uint32_t      g_brRacePathIndex;
BrVec3        g_brRacePathPos;

void        (*g_pfnBrRaceAiControl)(BrDriverCar *);

/* ==========================================================================
 * 0x1005ECF0 -- the path walk
 * ========================================================================== */

/* WHAT IT DOES: slides a position a given distance forward along the track's
 * built-in racing line, stepping from one stretch of the line to the next and
 * skipping over stretches marked to be ignored, and leaves the resulting
 * position and place-in-the-line where the caller can pick them up. This is
 * how a car that has no physics of its own -- an entrant the player never
 * sees driving -- is moved round the circuit. If it runs out of line it leaves
 * the answer untouched rather than reporting an error. */
/* @implements 0x1005ECF0 glide BrRacePathAdvance */
void BrRacePathAdvance(uint32_t offNode, uint32_t index,
                       float ratio, float dist)
{
    BrAiNode node;

    if (g_pBrRaceTrack == NULL)
        return;

    for (;;) {
        uint32_t count;

        if (offNode == 0)                             /* 0x1005ECFF */
            return;
        if (BrAiNodeAt(g_pBrRaceTrack, offNode, &node) != 0) {
            ++g_aBrRaceStepHole[BR_RS_HOLE_PATHEDGE];
            return;
        }
        /* 0x1005ED05: hop SKIP nodes through the SIBLING link.  The original
         * exits this loop on a NULL sibling and then tests the node again;
         * both spellings are kept because the second test is what returns. */
        while ((node.flags & BR_AI_NODE_SKIP) != 0) {
            offNode = node.offSib;
            if (offNode == 0)
                break;
            if (BrAiNodeAt(g_pBrRaceTrack, offNode, &node) != 0) {
                ++g_aBrRaceStepHole[BR_RS_HOLE_PATHEDGE];
                return;
            }
        }
        if (offNode == 0)                             /* 0x1005ED11 */
            return;

        count = node.count;                           /* 0x1005ED1B */
        if ((int32_t)index >= (int32_t)count)         /* 0x1005ED1F */
            goto next_node;

        for (;;) {                                    /* 0x1005ED2D */
            BrAiPoint a, b;
            float     segLen, avail;

            if (BrAiPoint_(&node, index, &a) != 0 ||
                BrAiPoint_(&node, index + 1u, &b) != 0) {
                ++g_aBrRaceStepHole[BR_RS_HOLE_PATHEDGE];
                return;
            }
            segLen = a.arc - b.arc;                   /* 0x1005ED30 */
            avail  = segLen * ratio;                  /* 0x1005ED38 */

            /* 0x1005ED40 `fcomp` + `test ah,0x41` + `jne <found>`: C0 is set
             * for LESS and C3 for EQUAL, and BOTH are set for UNORDERED, so
             * the walk STOPS on less, on equal and on a NaN distance.
             * `!(dist > avail)` is false only for an ordered greater-than,
             * which is exactly the fall-through. */
            if (!(dist > avail)) {
                BrVec3 p;

                /* 0x1005ED85: where the slot is standing right now --
                 * BrVec3Lerp is (a - b) * t + b, so t == 1 gives pts[i]. */
                BrVec3Lerp(&p, &a.centre, &b.centre, ratio);
                /* 0x1005ED8A: how far into what is left it has travelled,
                 * then 0x1005EDA4 moves it that far toward pts[i+1]. */
                BrVec3Lerp(&p, &b.centre, &p, dist / avail);

                g_brRacePathPos   = p;                /* 0x10B1CE98 */
                g_brRacePathNode  = offNode;          /* 0x10B1CBEC */
                g_brRacePathIndex = index;            /* 0x10AF07F0 */
                return;
            }

            dist  -= avail;                           /* 0x1005ED4F */
            ++index;                                  /* 0x1005ED53 */
            ratio  = 1.0f;                            /* 0x1005ED59 */
            if ((int32_t)index >= (int32_t)count)     /* 0x1005ED57 */
                break;
        }

    next_node:
        offNode = node.offNext;                       /* 0x1005ED67 */
        index   = 0u;
    }
}

/* NOT A PORT, and it is here rather than in a host because it is the exact
 * INVARIANT the phantom arm above requires, which is a fact about
 * 0x10061F60 and belongs beside it.
 *
 * 0x10062260 forms  (f44 + 1) * lapLen - f50 - pts[i+1].arc  and divides it
 * by the segment length to get how much of the segment is still ahead.  For
 * that to be a fraction in [0, 1], the slot's progress key must satisfy
 *
 *      f50 == (f44 + 1) * lapLen - arc(position)
 *
 * and the arm keeps it: f50 accumulates exactly the distance walked, so it
 * gains a lap length over the same circuit on which f44 gains one.  What it
 * cannot do is ESTABLISH it -- a slot seeded with the wrong triple walks off
 * the ring on its first lap rollover, extrapolating along one segment for
 * ever, and the position dump looks superficially plausible while doing it.
 *
 * The original establishes it in 0x1005F310 through 0x1005EB90, "position
 * along track from a distance", which is 538 + n bytes this module does not
 * own -- so this is a seed, not a transcription, and it is counted as the
 * grid hole.  `dist` is metres past the path root; it must put the slot
 * PAST gate 0, or the slot's first act is a backwards crossing that takes
 * f44 to -1 while f50 stays where it was and breaks the invariant. */
int BrRaceSeedPhantom(BrDriver *pDrv, float dist)
{
    BrAiNode root;

    ++g_aBrRaceStepHole[BR_RS_HOLE_GRID];

    if (g_pBrRaceTrack == NULL || BrAiRoot(g_pBrRaceTrack, &root) != 0)
        return 1;

    {
        BrAiPoint p0;
        if (BrAiPoint_(&root, 0u, &p0) != 0)
            return 1;
        /* The walk leaves the three globals alone when it runs out of ring,
         * so seed them with the root first and the failure is visible. */
        g_brRacePathNode  = 0u;
        g_brRacePathIndex = 0u;
        g_brRacePathPos   = p0.centre;
    }
    BrRacePathAdvance(root.off, 0u, 1.0f, dist);
    if (g_brRacePathNode == 0u)
        return 1;

    pDrv->f28 = (int32_t)g_brRacePathNode;
    pDrv->f2C = (int32_t)g_brRacePathIndex;
    pDrv->f00 = g_brRacePathPos;
    pDrv->f0C = g_brRacePathPos;
    pDrv->f50 = dist;
    pDrv->f40 = 0;
    pDrv->f44 = 0;
    pDrv->f48 = 0;
    pDrv->f4C = 0;
    return 0;
}

/* ==========================================================================
 * 0x10061430 -- eleven bytes, and all of them
 * ========================================================================== */

/* WHAT IT DOES: clears one value on a car at the very start of every race
 * frame, before anything else touches it. What that value is FOR is unknown --
 * nothing transcribed so far reads it -- so all that can honestly be said is
 * that each car begins the frame with it at zero. */
/* @implements 0x10061430 glide BrRaceCarPre */
void BrRaceCarPre(BrDriverCar *pCar)
{
    /* `mov dword ptr [ecx+0xF78], 0` / `ret`.  Nothing ported here reads
     * car+0xF78; the store is kept because the whole function is the store. */
    pCar->fF78 = 0;
}

/* ==========================================================================
 * 0x10061F60 -- one driver, part one
 * ========================================================================== */

/* 0x10062046 / 0x1006212B / 0x100621E6 -- the same three dwords, three times.
 * car+0x30 is the position the physics wrote and car+0xF80 is last frame's;
 * 0x1005FF00 reads exactly this pair as its motion segment. */
static void BrRaceMirrorPos(BrDriverCar *pCar)
{
    pCar->posPrev = pCar->pos;
}

/* 0x10062064 / 0x1006214C / 0x10062204 -- `mov ecx,[eax+0xF08]; test; call`.
 * The original does NOT null-check on the frozen arm and DOES on the other
 * two; both spellings are kept by checking here and by the callers' order. */
static void BrRaceControl(BrDriverCar *pCar)
{
    /* Counted whether or not a body is installed: the chain behind the
     * pointer -- 0x1005C8B0 / 0x1005D770 and 0x1006F170 -- is unported in
     * either case, and whatever the host puts here is standing in for it. */
    ++g_aBrRaceStepHole[BR_RS_HOLE_CONTROL];
    if (pCar->pfnControl != NULL)
        pCar->pfnControl(pCar);
    else if (g_brRaceStepHooks.pfnControl != NULL)
        g_brRaceStepHooks.pfnControl(pCar);
}

/* 0x10062238 -- THE PHANTOM ENTRANT */
static void BrRaceDriverPhantom(BrDriver *pDrv)
{
    BrAiNode  node;
    BrAiPoint a, b;
    float     lapLen, remain, segLen, ratio, dist;
    int32_t   iBorrow;

    pDrv->f0C = pDrv->f00;                            /* 0x1006223D */

    if (g_pBrRaceTrack == NULL)
        return;
    /* 0x10062256 `mov ecx,[0x106EED48]` then `fmul [ecx+0x64]` -- the path
     * ring's root node, whose pts[0].arc is the lap length.  br_ai.h's
     * BrAiLapLength is that read. */
    lapLen = BrAiLapLength(g_pBrRaceTrack);

    if (BrAiNodeAt(g_pBrRaceTrack, (uint32_t)pDrv->f28, &node) != 0 ||
        BrAiPoint_(&node, (uint32_t)pDrv->f2C, &a) != 0 ||
        BrAiPoint_(&node, (uint32_t)pDrv->f2C + 1u, &b) != 0) {
        ++g_aBrRaceStepHole[BR_RS_HOLE_PATHEDGE];
        return;
    }

    /* 0x1006225C..0x10062289.  `fild (f44 + 1)` -- the lap counter, not the
     * gate -- then the two subtractions and the divide, in that order. */
    remain = (float)(pDrv->f44 + 1) * lapLen - pDrv->f50 - b.arc;
    segLen = a.arc - b.arc;
    ratio  = remain / segLen;

    /* 0x100622A0: the slot borrows the car at (entrantCount + f74) and asks
     * whether that body is live.  A slot whose borrowed index is outside the
     * array cannot be live, which is the .bss answer the original gets. */
    iBorrow = g_brRaceNEntrant + pDrv->f74;
    if (g_pBrRaceCar != NULL && iBorrow >= 0 && iBorrow < g_brRaceNCar &&
        g_pBrRaceCar[iBorrow].fF00 != 0) {
        /* 0x100622AE: |car+0x1024| * dt, computed TWICE by the original --
         * once for the walk and once for the progress -- from the same two
         * inputs.  Kept as two multiplies for the same reason. */
        dist = BrVec3Length(&g_pBrRaceCar[iBorrow].f1024) * g_brRaceStepDt;
        BrRacePathAdvance((uint32_t)pDrv->f28, (uint32_t)pDrv->f2C,
                          ratio, dist);
        pDrv->f50 += BrVec3Length(&g_pBrRaceCar[iBorrow].f1024)
                     * g_brRaceStepDt;                /* 0x1006230E */
    } else {
        /* 0x10062313: the flat step.  0x400E147B is pushed as the distance
         * and 0x10077A14 (-2.22) is SUBTRACTED from the progress, so the two
         * are the same number spelled twice. */
        BrRacePathAdvance((uint32_t)pDrv->f28, (uint32_t)pDrv->f2C,
                          ratio, BR_RS_PHANTOM_STEP);
        pDrv->f50 -= -BR_RS_PHANTOM_STEP;             /* 0x10062327 */
    }

    /* 0x10062333: read the walk's three outputs back. */
    pDrv->f28 = (int32_t)g_brRacePathNode;
    pDrv->f2C = (int32_t)g_brRacePathIndex;
    pDrv->f00 = g_brRacePathPos;

    /* 0x10062363 / 0x1006237C: the slot's velocity, from the two positions
     * and nothing else. */
    BrVec3Sub(&pDrv->f18, &pDrv->f00, &pDrv->f0C);
    BrVec3ScaleBy(&pDrv->f18, 1.0f / g_brRaceStepDt);

    (void)BrRaceGateStep(&g_brRaceRules, pDrv);       /* 0x10062386 */
}

void BrRaceDriverStep(BrDriver *pDrv)
{
    BrDriverCar *pCar;

    /* 0x10061F6B: a networked slot with no car is somebody else's. */
    if (g_brRaceNet != 0 && pDrv->pCar == NULL)
        return;

    pCar = pDrv->pCar;
    if (pCar != NULL) {
        /* 0x10061F82: the AI controller's own command is cleared here, at
         * the TOP of the frame, before anything runs -- so the value the
         * controller writes survives exactly one frame. */
        if (pCar->pfnControl != NULL &&
            pCar->pfnControl == g_pfnBrRaceAiControl &&
            g_brRaceRules.mode != 5) {
            pCar->f29C0Ctl  &= 0x0F000000u;           /* 0x10061F9F */
            pCar->f29C0Steer = 0.0f;                  /* 0x10061FB0 */
        }
    }

    if (g_brRacePaused != 0) {
        /* 0x10061FBB: fifteen render globals at 0x118EEF48 zeroed and an
         * immediate return.  None of them is race state. */
        return;
    }

    if ((pDrv->f68 & BR_RS_DRIVER_FROZEN) != 0) {     /* 0x1006201E */
        pCar = pDrv->pCar;
        if (pCar == NULL)                             /* 0x10062025 */
            return;
        pCar->f29C0Ctl |= BR_DRIVERCAR_CTL_BRAKE;     /* 0x10062035 */
        pCar->fE70      = 0;                          /* 0x10062040 */
        BrRaceMirrorPos(pCar);                        /* 0x10062049 */
        BrRaceControl(pCar);                          /* 0x10062072 */
        pCar->fE70      = 0;                          /* 0x1006207A */
        return;                                       /* NO gate step */
    }

    if ((pDrv->f68 & BR_DRIVER_SKIP) != 0) {          /* 0x1006208B */
        pCar = pDrv->pCar;
        if (pCar == NULL)                             /* 0x10062096 */
            return;
        /* 0x1006209E: mode 0 leaves the leading `nEntrant` slots alone. */
        if (!(g_brRaceRules.mode == 0 && pDrv->f64 >= g_brRaceNEntrant)) {
            pCar->f29C0Ctl   = BR_DRIVERCAR_CTL_FIN;  /* 0x100620B9 */
            pCar->b29C024    = 0x81u;                 /* 0x100620C8 */
            pCar->f29C0Steer = -1.0f;                 /* 0x100620D5 */
        }
        if (g_brRaceRules.mode == 0 && pDrv->f64 >= g_brRaceNEntrant) {
            pCar->b29AF  = 2u;                        /* 0x100620F4 */
            pCar->f29B0 -= g_brRaceStepDt;            /* 0x100620FD */
            /* 0x10062120 `test ah,1` + `je` -- C0 is set for LESS and for
             * UNORDERED, and the zeroing is on C0 SET. */
            if (pCar->f29B0 < 0.0f)
                pCar->f29B0 = 0.0f;                   /* 0x10062125 */
        }
        BrRaceMirrorPos(pCar);                        /* 0x1006212E */
        if (pCar->pfnControl == NULL)                 /* 0x10062152 */
            return;
        BrRaceControl(pCar);                          /* 0x1006215B */
        return;                                       /* NO gate step */
    }

    pCar = pDrv->pCar;
    if (pCar == NULL) {                               /* 0x10062168 */
        BrRaceDriverPhantom(pDrv);
        return;
    }

    /* --- the racing arm, 0x1006216E ---------------------------------- */
    if (pCar->b29AF == 2u) {
        /* 0x10062176: `dt * -1.6` SUBTRACTED FROM f29B0, i.e. added. */
        pCar->f29B0 -= g_brRaceStepDt * BR_RS_BLEED_K;
        if (g_brRaceRules.mode == 2 && pDrv->f64 != 0) {
            /* 0x100621A4 `test ah,0x41` + `jne <skip>`: the clamp is on an
             * ordered GREATER-THAN, so a NaN is left alone. */
            if (pCar->f29B0 > BR_RS_BLEED_LO)
                pCar->f29B0 = BR_RS_BLEED_LO;         /* 0x100621B1 */
        } else {
            /* 0x100621CE `test ah,1` + `jne <skip>`: C0 is set for LESS and
             * for UNORDERED, so a NaN takes the clamp. */
            if (!(pCar->f29B0 < BR_RS_BLEED_HI)) {
                pCar->f29B0 = BR_RS_BLEED_HI;         /* 0x100621D3 */
                pCar->b29AF = 0u;                     /* 0x100621E0 */
            }
        }
    }

    BrRaceMirrorPos(pCar);                            /* 0x100621E9 */
    BrRaceControl(pCar);                              /* 0x10062212 */
    /* 0x1006221A -- slice3_41.h already cites this line for what car+0x1030
     * and car+0x1034 are. */
    pCar->f1034 += pCar->f1030 * g_brRaceStepDt;
}

/* ==========================================================================
 * 0x100623A0 -- a pure hole, transcribed for its control flow only
 * ========================================================================== */

void BrRaceDriverAnim(BrDriver *pDrv)
{
    BrDriverCar *pCar = pDrv->pCar;

    if (pCar == NULL)                                 /* 0x100623A8 */
        return;
    if (g_brRaceNet != 0) {
        /* 0x100623BC: 0x10059D30(car, car+0x144) decides whether this slot
         * animates at all.  Unported, so the gate is entered and counted. */
        BR_RS_HOLE(BR_RS_HOLE_ANIM, pfnAnim, pCar);
        return;
    }
    /* 0x100623CB / 0x100623D3: 0x1005ACE0 and 0x10001CF0. */
    BR_RS_HOLE(BR_RS_HOLE_ANIM, pfnAnim, pCar);
}

/* ==========================================================================
 * 0x100623E0 -- one driver, part two.  The car entrant's gate step.
 * ========================================================================== */

/* WHAT IT DOES: the tidying-up pass over one driver after the driving has been
 * done for the frame: lay down skid marks if the car has been sliding, credit
 * the car with any lap gates it has just crossed, work out how fast it is
 * actually travelling from how far it moved, and run down a short per-car
 * countdown. Nothing happens at all while the game is paused. */
/* @implements 0x100623E0 glide BrRaceDriverPost */
void BrRaceDriverPost(BrDriver *pDrv)
{
    BrDriverCar *pCar;

    if (g_brRacePaused != 0)                          /* 0x100623EA */
        return;
    pCar = pDrv->pCar;
    if (pCar == NULL)                                 /* 0x100623F5 */
        return;

    if (pCar->b360 != 0u) {                           /* 0x10062403 */
        /* 0x10062405..0x10062459: the skid trail -- a 2-D length, a random
         * draw, an emitter and a decal append.  Four unported callees. */
        BR_RS_HOLE(BR_RS_HOLE_SKID, pfnSkid, pCar);
        pCar->b360 = 0u;                              /* 0x1006245C */
    }
    /* 0x10062466 / 0x1006246E / 0x10062476 */
    BR_RS_HOLE(BR_RS_HOLE_SKID, pfnSkid, pCar);

    (void)BrRaceGateStep(&g_brRaceRules, pDrv);       /* 0x1006247D */

    BR_RS_HOLE(BR_RS_HOLE_SKID, pfnSkid, pCar);       /* 0x10062485 */

    /* 0x1006249E / 0x100624BF: the world velocity, from the two positions
     * 0x1005FF00 has just finished reading. */
    BrVec3Sub(&pCar->f1024, &pCar->pos, &pCar->posPrev);
    BrVec3ScaleBy(&pCar->f1024, 1.0f / g_brRaceStepDt);

    /* 0x100624CA: car+0x2718 from a 2-D length of car+0x2734's first two
     * floats.  car+0x2734 is a pointer this port does not have. */
    BR_RS_HOLE(BR_RS_HOLE_SKID, pfnSkid, pCar);

    if (pCar->fF04 != 0)                              /* 0x100624F1 */
        --pCar->fF04;                                 /* 0x100624F5 */
}

/* ==========================================================================
 * The start-light state machine
 * ========================================================================== */

/* 0x1001AEE2..0x1001B08C.  The original builds the answer in a stack slot
 * seeded to 1 at 0x1001AE4E and cleared at 0x1001B085 for every driver that
 * is NOT finished; the loop body it skips is the per-slot results bookkeeping,
 * which is HUD state.  What survives is the predicate. */
int BrRaceStepAllFinished(void)
{
    int32_t i;

    for (i = 0; i < g_brRaceNEntrant; ++i) {
        if ((g_pBrRaceDriver[i].f68 & BR_DRIVER_SKIP) == 0)
            return 0;                                 /* 0x1001B085 */
    }
    /* An empty field leaves the flag at 1, which is the original's answer
     * too: 0x1001AEEC jumps straight past the loop. */
    return 1;
}

/* 0x1001B0F8 -- the advance itself, reached from three places. */
static void BrRaceStepAdvanceScript(void)
{
    if (g_brRaceTick == 0)                            /* 0x1001B0FE */
        return;
    ++g_brRaceScript;                                 /* 0x1001B105 */
    if (g_brRaceScript < 0 || g_brRaceScript >= BR_RS_SCRIPT_LEN) {
        /* DEVIATION: the original indexes 0x100A9578 without a bound.  The
         * script's last entry is state 7, whose arm never advances, so the
         * index cannot legally get here; clamping is what a portable build
         * does instead of reading .rdata past the table. */
        g_brRaceScript = BR_RS_SCRIPT_LEN - 1;
        return;
    }
    g_brRaceLightT = g_aBrRaceLightScript[g_brRaceScript].dur;   /* 0x1001B119 */
    g_brRaceLights = g_aBrRaceLightScript[g_brRaceScript].state; /* 0x1001B11F */
}

/* 0x1001B0CD -- the timer arm.  Shared by fallthrough from the state-6 case
 * and by explicit jumps from the two arms below 4. */
static void BrRaceStepTimer(void)
{
    if (g_brRacePaused != 0)                          /* 0x1001B0D3 */
        return;
    g_brRaceLightT -= g_brRaceStepDt;                 /* 0x1001B0DF */
    /* 0x1001B0EB `fcomp 0.0f` + `test ah,1` + `je <return>`: C0 is set for
     * LESS and for UNORDERED, and the advance is on C0 SET. */
    if (!(g_brRaceLightT < BR_RS_TIMER_END))
        return;
    BrRaceStepAdvanceScript();
}

void BrRaceStepLights(void)
{
    int32_t i;

    if (g_brRaceLights < BR_RS_LIGHTS_GO) {           /* 0x1001AC09 */
        /* 0x1001AC0F..0x1001ACC2: three arms that differ only in which HUD
         * string ("GET READY", id 0xED / 0xEE) they hang on each car. */
        ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];

        g_brRaceHudB = 1;                             /* 0x1001ACCD */
        for (i = 0; i < g_brRaceNDriver; ++i) {
            BrDriver    *pDrv = &g_pBrRaceDriver[i];
            BrDriverCar *pCar;

            pDrv->f68 |= BR_RS_DRIVER_FROZEN;         /* 0x1001ACEC */

            pCar = pDrv->pCar;
            if (pCar == NULL)                         /* 0x1001ACF6 */
                continue;
            if (pCar->f140 >= g_brRaceNEntrant)       /* 0x1001AD02 */
                continue;

            /* 0x1001AD08..0x1001AD5A: the difficulty table lookup into
             * 0x100BCAB0 that writes car+0xFF0, and car+0x1000 = 1.0f.
             * Both feed the AI controller, which is already a hole. */
            ++g_aBrRaceStepHole[BR_RS_HOLE_DIFFICULTY];

            if (g_brRaceLights == 0) {                /* 0x1001AD67 */
                g_brRaceHudA = -1;                    /* 0x1001ADAC */
                g_brRaceBeep = 0;                     /* 0x1001ADB2 */
            } else if (g_brRaceLights == BR_RS_LIGHTS_COUNT) {  /* 0x1001AD6C */
                int iBeep = g_brRaceBeep;

                g_brRaceHudA = 1;                     /* 0x1001AD74 */
                if (iBeep < 0)                    iBeep = 0;
                if (iBeep >= BR_RS_BEEP_COUNT)    iBeep = BR_RS_BEEP_COUNT - 1;
                /* 0x1001AD8D `test ah,0x41` + `jne <skip>`: the beep fires
                 * only on an ordered greater-than, i.e. once the timer has
                 * dropped past this threshold.
                 *
                 * THE WHOLE BLOCK IS INSIDE THE PER-DRIVER LOOP, which is
                 * the original's own doing and is why the counter can step
                 * more than once in a frame.  It is self-limiting: the next
                 * threshold is smaller than the timer until the next frame. */
                if (g_aBrRaceBeepT[iBeep] > g_brRaceLightT) {
                    ++g_brRaceBeep;                   /* 0x1001AD92 */
                    /* 0x1001AD9E is the GO horn (the counter has reached 4)
                     * and 0x1001ADA5 a beep.  BOTH ARE NOW PORTED, in
                     * port/src/br_sfxsrc.c: 0x10060E00 and 0x10060DF0 are
                     * eleven bytes each over 0x10060DB0, which plays source
                     * 0xE (group 14, beep2) or 0xD (group 13, beep) on
                     * channel 3 at 0x00200020.  br_sfxsrc.c's
                     * BrSfxSrcRaceCountdown is exactly this two-way branch
                     * and port/src/br_wireaudio.c installs it as pfnSound.
                     *
                     * The hole COUNTER stays, and it stays incremented on
                     * every fire whether or not a hook is installed: it is
                     * how the four-per-race invariant is observed, and a
                     * counter that stopped counting once the hook existed
                     * would have removed the only evidence that the hook is
                     * being reached the right number of times. */
                    ++g_aBrRaceStepHole[BR_RS_HOLE_SOUND];
                    if (g_brRaceStepHooks.pfnSound != NULL)
                        g_brRaceStepHooks.pfnSound(g_brRaceBeep);
                }
            }
        }
        g_brRaceFade = 0.0f;                          /* 0x1001ADCC */
        BrRaceStepTimer();                            /* 0x1001ADD6 */
        return;
    }

    if (g_brRaceLights == BR_RS_LIGHTS_GO) {          /* 0x1001ADDB */
        float dur;

        g_brRaceHudA = 1;                             /* 0x1001ADE3 */
        g_brRaceHudB = 1;                             /* 0x1001ADEB */
        for (i = 0; i < g_brRaceNDriver; ++i)         /* 0x1001ADF8 */
            g_pBrRaceDriver[i].f68 &= ~(uint32_t)BR_RS_DRIVER_FROZEN;

        /* 0x1001AE0D: the fade curve, ((dur - t) / dur)^2 * 15. */
        dur = (g_brRaceScript >= 0 && g_brRaceScript < BR_RS_SCRIPT_LEN)
              ? g_aBrRaceLightScript[g_brRaceScript].dur : 0.0f;
        {
            float f = (dur - g_brRaceLightT) / dur;
            g_brRaceFade = f * f * BR_RS_FADE_SCALE;
        }
        BrRaceStepTimer();                            /* 0x1001AE33 */
        return;
    }

    if (g_brRaceLights == BR_RS_LIGHTS_RACE) {        /* 0x1001AE38 */
        /* 0x1001AE41..0x1001AEDC: the time-limit tests on car[0]'s +0xFEC
         * against 140.0 / 39.6 / 16.0 for modes 4 and 5, and the fade-out
         * they trigger.  Neither mode is what a plain race is, and the block
         * writes only screen state. */
        ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];

        /* 0x1001B092: the ONLY exit from state 4 -- and it is not a timer. */
        if (BrRaceStepAllFinished())
            BrRaceStepAdvanceScript();
        return;
    }

    if (g_brRaceLights == 5) {                        /* 0x1001B09D */
        /* 0x1001B0A2: modes 0, 1, 2 and 6 run 0x1001C810 (the results
         * screen) when not replaying; then the advance, with no timer. */
        ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];
        BrRaceStepAdvanceScript();                    /* 0x1001B0C6 */
        return;
    }

    if (g_brRaceLights == 6) {                        /* 0x1001B0C8 */
        BrRaceStepTimer();                            /* falls into 0x1001B0CD */
        return;
    }

    /* 0x1001B127: state 7, the fade out.  Three unported sound ramps and a
     * latch on 0x105CCB98; no race state at all. */
    ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];
}

/* ==========================================================================
 * 0x10019A70
 * ========================================================================== */

/* 0x10019A70 IS DELIBERATELY UNCLAIMED, AND THIS IS WHERE THE CLAIM USED TO BE
 * ============================================================================
 * `/ * @implements 0x10019A70 glide BrRaceStepInit * /` sat on this line and
 * it was false.  0x10019A70 is 11,223 bytes making 131 calls.  The body below
 * annotates 0x1001A97C..0x1001AA5E -- about 230 bytes, TWO PER CENT, and it
 * starts roughly 4 KB into the function.  Under that claim the address counted
 * as ported and the other 98% was neither transcribed, nor marked, nor counted
 * as missing.
 *
 * WHY IT WAS DROPPED RATHER THAN MOVED OR NARROWED
 *
 *   - NARROWED is not available.  tools/manifest.py's form is
 *     `@implements 0xADDR BUILD SYMBOL` and nothing else; there is no
 *     sub-range or partial syntax anywhere in the tree.  A claim is
 *     whole-function or it is absent.
 *   - MOVED to BrRaceStepFrame was the obvious fix and is still wrong.
 *     BrRaceStepFrame is the right SHAPE -- it is 0x10019A70's entry and its
 *     substate branch, the per-frame arm the pump calls -- but it and
 *     BrRaceStepLights together cover 0x1001AB71..0x1001B261, about 1,780
 *     bytes.  That is 16%, not 100%.  Trading a 2% lie for a 16% one is not
 *     an honest outcome; it is a quieter one.
 *
 * SO THE ADDRESS READS AS UNPORTED, WHICH IT IS.  What exists is:
 *
 *     BrRaceStepInit    0x1001A97C..0x1001AA5E   the script seed          ~230 B
 *     BrRaceStepFrame   0x1001AB71..0x1001B261   the per-frame arm      ~1,780 B
 *     BrRaceStepLights  0x1001ABFB..0x1001B171   (inside the above)
 *
 * and what does not:
 *
 *     0x10019AFE..0x1001AB6F   the one-time arm's asset loading and screen
 *                              setup, ~4.2 KB, of which 0x1005F310 (538 B,
 *                              the driver-record constructor and the grid
 *                              placement) is the counter increment below
 *     0x1001B261..0x1001C647   the HUD, the rear-view mirror and the whole
 *                              renderer, ~5.1 KB, counted as BR_RS_HOLE_HUD
 *
 * DO NOT READ "UNPORTED" AS "UNTOUCHED" AND SEND SOMEONE TO START OVER.
 * br_racestep.h is 500 lines of derivation for this address and the three
 * functions above are checked transcriptions of the parts they name.  The
 * missing 84% is asset loading and rendering, which this module does not own.
 * Restoring an @implements line here without also transcribing that 84% just
 * puts the same defect back. */

/* WHAT IT DOES: starts a race. It rewinds the starting-light sequence to the
 * beginning and clears the finished-driver count. A replay, or the one mode
 * that has no start procedure, skips straight past the lights to the racing
 * state. Putting the cars on the grid is the one thing it does NOT do that
 * the original does: that is a separate 538-byte routine this module does not
 * own, so the per-driver loop below only counts how many times it would have
 * been called. */
int BrRaceStepInit(void)
{
    int32_t i;
    int32_t iScript;

    /* 0x1001A97C..0x1001A99C.  `xor eax,eax / cmp mode,5 / setne al / dec eax
     * / and eax,4` is `(mode == 5) ? 4 : 0`, and a replay starts at 4
     * outright. */
    if (g_brRaceReplay != 0)
        iScript = 4;
    else
        iScript = (g_brRaceRules.mode == 5) ? 4 : 0;

    g_brRaceScript = iScript;                         /* 0x1001A9AD */
    g_brRaceLightT = g_aBrRaceLightScript[iScript].dur;   /* 0x1001A9B2 */
    g_brRaceLights = g_aBrRaceLightScript[iScript].state; /* 0x1001A9BA */

    if (g_brRaceReplay == 0) {
        g_brRaceRules.nFinished = 0;                  /* 0x1001A9CB, 0x118EE588 */
        for (i = 0; i < g_brRaceNDriver; ++i) {
            /* 0x1001A9DA: 0x1005F310, the driver-record constructor and the
             * grid placement.  br_race.h already names it; it is 538 bytes
             * of table-driven layout that this module does not own. */
            ++g_aBrRaceStepHole[BR_RS_HOLE_GRID];
        }
    }

    g_brRaceBeep     = 0;
    g_brRaceFade     = 0.0f;
    g_brRaceSubstate = 1;                             /* 0x1001AA5E */
    return g_brRaceLights;
}

void BrRaceStepFrame(void)
{
    int32_t i;

    /* 0x10019AF8: the substate branch.  The one-time arm is BrRaceStepInit
     * and is not re-entered here; a caller that has not run it gets the
     * frame the original would run with a zeroed script, which is state 0. */
    if (g_brRaceSubstate == 0)
        (void)BrRaceStepInit();

    /* 0x1001AB9F: state 4 alone runs 0x10060A30 over the whole field. */
    if (g_brRaceLights == BR_RS_LIGHTS_RACE) {
        for (i = 0; i < g_brRaceNEntrant; ++i)
            BR_RS_HOLE(BR_RS_HOLE_LAPINFO, pfnLapInfo, &g_pBrRaceCar[i]);
    }

    /* 0x1001ABD0..0x1001ABEA: the 0x11778848 toggle and 0x1002E186, both
     * render-side, and counted with the HUD below. */

    g_brRaceHudA = 0;                                 /* 0x1001ABF1 */
    g_brRaceHudB = 0;                                 /* 0x1001AC03 */

    BrRaceStepLights();                               /* 0x1001ABFB */

    /* 0x1001B186 */
    ++g_aBrRaceStepHole[BR_RS_HOLE_SCRATCH];

    /* 0x1001B18B */
    for (i = 0; i < g_brRaceNCar; ++i)
        BrRaceCarPre(&g_pBrRaceCar[i]);

    /* 0x1001B1B2 */
    for (i = 0; i < g_brRaceNDriver; ++i)
        BrRaceDriverStep(&g_pBrRaceDriver[i]);

    /* 0x1001B1D9: `jne` then `je`, i.e. either flag runs the loop. */
    if (g_brRaceNet != 0 || g_brRaceReplay != 0) {
        for (i = 0; i < g_brRaceNDriver; ++i)
            BrRaceDriverAnim(&g_pBrRaceDriver[i]);
    }

    /* 0x1001B20B */
    for (i = 0; i < g_brRaceNDriver; ++i)
        BrRaceDriverPost(&g_pBrRaceDriver[i]);

    /* 0x1001B22D */
    if (g_brRaceRules.mode == 0) {
        for (i = 0; i < g_brRaceNEntrant; ++i)
            ++g_aBrRaceStepHole[BR_RS_HOLE_SAVELAP];
    }

    /* 0x1001B25C -- 0x1005F580.  Its g_226A48 arm is a per-car remote query
     * and its other arm IS slice3_41.c's BrRankAssign, from the D3D twin
     * 0x10066510.  See the banner in br_racestep.h. */
    if (g_brRaceNet != 0) {
        for (i = 0; i < g_brRaceNCar; ++i)
            ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];
    } else if (g_pBrRaceDriver != NULL) {
        BrRankAssign(g_pBrRaceDriver, g_brRaceNDriver);
    }

    /* 0x1001B261 to the end: the HUD, the rear-view mirror and the whole
     * renderer.  About half the function by bytes and none of it state. */
    ++g_aBrRaceStepHole[BR_RS_HOLE_HUD];
}

void BrRaceStepInstall(void)
{
    BrGameStepRegister(BrRaceStepFrame, BR_GAMESTEP_RACE);
    BrGameStepSet(BrRaceStepFrame);
}
