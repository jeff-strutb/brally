/* slice2_25.c -- another module's packet, 0x10042880-0x100446D0 (46 functions).
 *
 * See slice2_25.h for what the module is and how the three repeated shapes
 * work. Everything here is a transcription; the DEVIATION list is at the
 * bottom of the file and every deviation is also marked at its line.
 *
 * A WARNING FOR INTEGRATION, about br_slots.h
 * -----------------------------------------------
 * br_slots.h declares
 *
 *     typedef struct BrSlotTable { BrSlot aSlots[8]; int count; } BrSlotTable;
 *
 * with the comment that `count` is 0x10AA288C. That struct is NOT the memory
 * layout: the slot array ends at 0x10AA2598 and 0x10AA288C is 0x2F4 bytes
 * further on. This packet reads and writes both, independently, and they
 * cannot be one object. The array is exposed here as g_aBrAA2538 and
 * 0x10AA288C as the separate flag g_brAA288C. (In this packet 0x10AA288C is
 * used as a flag -- set to 1 at 0x10043B10, tested at 0x10043925 -- not as a
 * count, which is further evidence they are unrelated.)
 */
#include "slice2_25.h"

#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * Storage
 * ========================================================================== */

int32_t g_brAA33D4, g_brAA33D0;

int32_t g_br0AC648, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC658, g_br0AC65C;
int32_t g_br0BD3E0;
int32_t g_brAA2A00, g_brAA2A08, g_brAA2A0C, g_brAA2A18;
int32_t g_brAA2A1C, g_brAA2A20, g_brAA2A24, g_brAA2A28;
int32_t g_brB4E708, g_brB4E70C;

int32_t g_br094350, g_br094354, g_br094358, g_br09435C;
int32_t g_br0B380C, g_br22B34C, g_br22B350;
int32_t g_brB4E1D0, g_brB4E1D8, g_brB4E1DC, g_brB4E1E0, g_brB4E728, g_brB4E7A0;
void   *g_brB4E1D4;

int32_t g_br0AA010, g_br0AB3D8, g_br0AB3E0, g_br0B4050, g_br22AF18;
int32_t g_br690A18, g_brA9CFFC, g_brA9D000;
int32_t g_brAA2854, g_brAA285C, g_brAA2878, g_brAA287C, g_brAA2884;
int32_t g_brAA2888, g_brAA288C, g_brAA2890, g_brAA2894, g_brAA2898;
int32_t g_brAA289C, g_brAA28D8, g_brAA28E8, g_brAA28FC;
int32_t g_brAA2958, g_brAA29A8;

int32_t     g_brAD0978, g_brAD097C, g_brAD0980, g_brAD0984, g_brAD0988, g_brAD098C;
signed char g_br680738, g_br68073F;

BrDPlay       *g_brP277B40;
BrOptUi       *g_brPA9D008;
void          *g_brP680584;
const int32_t *g_brPACED34;
BrOptFlagObj  *g_brPAA29D8;
BrObj29D4     *g_brPAA29D4;

BrOptObj *g_brPAA2904, *g_brPAA2908, *g_brPAA2940, *g_brPAA2948;
BrOptObj *g_brPAA294C, *g_brPAA2950, *g_brPAA2954, *g_brPAA296C;
BrOptObj *g_brPAA2970, *g_brPAA298C, *g_brPAA2998, *g_brPAA29B8;

int32_t g_aBrAA26F0[BR_OPT_AA26F0_COUNT];
char    g_aBrA9CDF0[BR_OPT_TEXT_MAX];
char    g_aBrA9DD28[BR_OPT_TEXT_MAX];
char    g_aBr39B720[BR_OPT_TEXT_MAX];
char    g_aBr1782BC8[BR_OPT_TEXT_MAX];

BrSlot        g_aBrAA2538[BR_SLOT_COUNT];
unsigned char g_aBrB4DF30[BR_OPT_B4DF30_COUNT][BR_OPT_B4DF30_STRIDE];

/* Literals in the DLL's read-only data. */
static const char g_szBrTimeAttack[] = "TimeAttack";   /* 0x100AD33C */
static const char g_szBrGrfExt[]     = ".grf";         /* 0x100AD334 */

/* ==========================================================================
 * Shared helpers
 *
 * These three sequences appear byte-for-byte in many of the functions below.
 * Factoring them keeps the transcription honest -- a copy-paste error in one
 * of eleven copies would be invisible.
 * ========================================================================== */

/* The tail of every "show a message" block: hand the text to the UI and then
 * copy 0x1039B720 over it. 0x1039B720 lies past the end of the DLL's
 * initialised data (see slice1_06.h), so at load time it is an empty string
 * and this is effectively "clear the buffer" -- but only effectively. */
static void BrOptFlushMessage(void)
{
    BrSub1003D210(g_brP680584, g_brPA9D008, 1);
    strcpy(g_aBrA9DD28, g_aBr39B720);        /* DEVIATION: rep movsb */
}

/* The screen-object install sequence shared by 0x10043260, 0x10043330,
 * 0x100434C0, 0x10043CD0, 0x10043DA0, 0x10043E70, 0x100440D0, 0x10044280,
 * 0x100443E0 and 0x100446D0.
 *
 * Returns 1 when *ppSlot holds an object afterwards (created or reused) and
 * 0 when the allocation failed -- which is exactly the eax the original
 * leaves behind on each path. The C++ exception frame the original sets up
 * (`push -1 / push <funclet> / fs:[0]`, and the state variable it keeps at
 * [esp+0xC]) has no observable effect and is not reproduced. */
static int BrOptEnsureObj(BrOptObj **ppSlot, BrOptObjFn pfnEnter)
{
    BrOptObj *p = *ppSlot;

    if (p != NULL) {
        g_brPAA2904 = p;
        return 1;
    }

    /* 0x1007DFE0 is operator new == _nh_malloc(size, 1): the storage is NOT
     * zeroed. The constructor at 0x10048710 is what initialises it.
     *
     * HARDENING (port): BR_PHASE_ALLOC_SIZE, not sizeof(BrOptObj).
     *
     * This line used to read `malloc(sizeof(BrOptObj))`, with a DEVIATION note
     * explaining that the literal 0xC8 was wrong on a 64-bit host because the
     * leading pointers widen. That reasoning was right and the fix was one
     * model short: BrOptObj was a five-field view padded to 0xC8, so it came
     * to 216 bytes here, while BrOptObjCtor -- which resolves at the host link
     * to slice6_73.c's faithful body -- writes the whole 304-byte BrPhase_,
     * ending with stores to +0xC0 and +0xC4. That was an 88-byte heap
     * overflow on every phase installation.
     *
     * BrOptObj is now an alias for BrPhase_ (see slice2_25.h), so sizeof()
     * would in fact be correct today; BR_PHASE_ALLOC_SIZE is used anyway
     * because it is also never smaller than the original's 0xC8, and because
     * it is the one spelling every phase allocation site in the tree shares. */
    p = (BrOptObj *)malloc(BR_PHASE_ALLOC_SIZE);
    p = (p != NULL) ? BrOptObjCtor(p) : NULL;

    *ppSlot     = p;
    g_brPAA2904 = p;
    if (p == NULL)
        return 0;

    p->pfnEnter = pfnEnter;
    /* The original re-reads the global here rather than reusing the
     * register; kept, because a constructor that publishes itself could make
     * the two differ. */
    p = *ppSlot;
    p->pfnEnter(p);
    g_brPAA2904->f0C = 1;
    g_brPAA2904->f68 = 1;
    return 1;
}

/* The plain wrapping cycler: up on 0x10AA33D4, down on 0x10AA33D0, no
 * validity filtering. `*pv` is the option, [0..max] inclusive. Returns the
 * resulting value. Note the original always writes the global back on the
 * edited paths and never writes it when neither input is set. */
static int32_t BrOptCycle(int32_t *pv, int32_t max)
{
    int32_t v;

    if (g_brAA33D4 != 0) {
        v = *pv + 1;
        *pv = v;
        if (v > max)
            *pv = 0;
    } else if (g_brAA33D0 != 0) {
        v = *pv - 1;
        *pv = v;
        if (v < 0)
            *pv = max;
    }
    return *pv;
}

/* ==========================================================================
 * 0x10042880
 * ========================================================================== */

/* Builds "TimeAttack" + decimal(*pIndex) + ".grf" and copies the result over
 * 0x11782BC8 -- which slice1_06.h identifies as one of the two fixed
 * save-file path buffers (the ghost one). So this call CLOBBERS that path.
 *
 * The two working buffers are laid out exactly as the original's stack frame
 * has them, because they OVERLAP: the itoa output sits at frame+0x10 and the
 * name being assembled starts at frame+0x14, i.e. only four bytes later. The
 * original therefore only survives while the number is at most three digits.
 * Reproduced rather than fixed. */
int BrOptBeginTimeAttack(void *pUnused, const int32_t *pIndex)
{
    char  aFrame[BR_OPT_2880_FRAME];
    char *pszNum  = aFrame + BR_OPT_2880_NUM_OFF;   /* frame+0x10, 4 bytes! */
    char *pszName = aFrame + BR_OPT_2880_STR_OFF;   /* frame+0x14 */
    int32_t iSel;

    (void)pUnused;      /* the first argument is never read by the original */

    g_br0AA010  = 2;
    g_brAA28E8  = 0;
    g_br690A18  = 0;
    BrSub1003E680();

    strcpy(pszName, g_szBrTimeAttack);          /* DEVIATION: rep movsb */
    BrItoa(*pIndex, pszNum, 10);
    strcat(pszName, pszNum);                    /* DEVIATION: rep movsb */
    strcat(pszName, g_szBrGrfExt);              /* DEVIATION: rep movsb */
    strcpy(g_aBr1782BC8, pszName);              /* DEVIATION: rep movsb */

    BrSub10071130(1, 1);

    /* movsx + `jge`: the byte at 0x10680738 is SIGNED and a negative value
     * aborts the whole restore. */
    if (g_br680738 < 0)
        return 0;

    iSel = (int32_t)g_br680738;

    g_br0AC650 = g_brAD0980;
    g_br0AC64C = g_brAD097C;
    g_br0AC65C = g_brAD0988;
    g_br0AC654 = g_brAD0984;
    g_br0B4050 = 1;
    g_br0AC648 = iSel;
    g_brAA2A00 = (int32_t)g_br68073F;   /* movsx: also SIGNED */
    g_brAA2A08 = g_brAD0978;
    g_br0AC658 = g_brAD098C;
    g_br0BD3E0 = g_brAD098C;

    /* `rep movsd` of 0x53 dwords FROM the pointer held in 0x10ACED34. */
    memcpy(g_aBrAA26F0, g_brPACED34,
           BR_OPT_AA26F0_COUNT * sizeof(int32_t));  /* DEVIATION: rep movsd */

    g_br0B380C = g_aBrAC4D8[iSel];
    g_br0BD3E0 = g_brAD098C;            /* stored twice by the original */
    g_br094350 = g_brAD0988;
    g_brAA28E8 = 1;
    g_br22B34C = g_aBrAC420[g_brAD0984];
    g_br22B350 = g_aBrAC4C0[(int32_t)g_br68073F];
    g_br09435C = g_aBrAC4A0[g_brAD097C];
    g_br094358 = g_aBrAC4B0[g_brAD0980];
    g_br094354 = g_aBrAC518[g_brAD0978];

    BrSub1005FCF0();
    g_brAA289C = 1;
    return 1;
}

/* ==========================================================================
 * 0x10042A90 / 0x10042AC0 / 0x10042B00 -- three identical copies
 * ========================================================================== */

/* GOTCHA: 0x10AA28D8 is a latch, not a debounce -- nothing in this packet
 * ever clears it, so across all three entry points the field is toggled at
 * most once per clear of that global. The return value is 1 either way. */
static int BrOptToggle2F7C(BrGameObj *pGame)
{
    if (g_brAA28D8 == 0) {
        g_brAA28D8   = 1;
        pGame->f2F7C = (pGame->f2F7C == 0) ? 1 : 0;
    }
    return 1;
}

int BrOptToggle2F7C_A(BrGameObj *pGame) { return BrOptToggle2F7C(pGame); }
int BrOptToggle2F7C_B(BrGameObj *pGame) { return BrOptToggle2F7C(pGame); }
int BrOptToggle2F7C_C(BrGameObj *pGame) { return BrOptToggle2F7C(pGame); }

/* ==========================================================================
 * 0x10042B30 -- track select
 *
 * GOTCHA, shared with 0x10042EE0: the "have I been all the way round"
 * comparison is against the value the search STARTED FROM, which is the
 * option already stepped once -- not the value on entry. So when every
 * candidate is rejected the option still ends up moved by one, on the first
 * candidate, rather than back where the user left it.
 * ========================================================================== */

int BrOptCycleTrack(void)
{
    int32_t v, vStart, iName;

    if (g_brAA33D4 != 0) {
        v = g_br0AC654 + 1;
        g_br0AC654 = v;
        if (v > BR_OPT_TRACK_MAX) {
            v = 0;
            g_br0AC654 = 0;
        }
        vStart = v;
        if (BrSub1003F320(v) == 0) {
            for (;;) {
                v = g_br0AC654 + 1;
                g_br0AC654 = v;
                if (v > BR_OPT_TRACK_MAX) {
                    v = 0;
                    g_br0AC654 = 0;
                }
                /* Unlike 0x10042EE0's loop, the wrap path here still runs
                 * the full-circle test. */
                if (v == vStart)
                    break;
                if (BrSub1003F320(v) != 0)
                    break;
            }
        }
        v = g_br0AC654;
    } else if (g_brAA33D0 != 0) {
        v = g_br0AC654 - 1;
        g_br0AC654 = v;
        if (v < 0) {
            v = BR_OPT_TRACK_MAX;
            g_br0AC654 = BR_OPT_TRACK_MAX;
        }
        vStart = v;
        if (BrSub1003F320(v) == 0) {
            for (;;) {
                v = g_br0AC654 - 1;
                g_br0AC654 = v;
                if (v < 0) {
                    v = BR_OPT_TRACK_MAX;
                    g_br0AC654 = BR_OPT_TRACK_MAX;
                }
                if (v == vStart)
                    break;
                if (BrSub1003F320(v) != 0)
                    break;
            }
        }
        v = g_br0AC654;
    } else {
        v = g_br0AC654;
    }

    g_br22B34C = g_aBrAC420[v];

    if (g_brP277B40 != NULL) {
        /* 32 tracks, 16 names: indices 0x10..0x1F reuse the first sixteen
         * name strings. */
        iName = v;
        if (iName > 0xF)
            iName -= 0x10;
        BrSprintf(g_aBrA9DD28, BrStrGet(BR_OPT_STR_TRACK),
                  BrStrGet((int)g_aBrAC368[iName]));
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10042C80 .. 0x10042E80 -- plain cyclers
 * ========================================================================== */

/* 0x10042C80 */
int BrOptCycleAC65C(void)
{
    g_br094350 = BrOptCycle(&g_br0AC65C, BR_OPT_AC65C_MAX);
    return 1;
}

/* 0x10042CF0. GOTCHA: 0x10060D90 is called on EVERY path, including the one
 * where neither input is set -- contrast 0x10044600. */
int BrOptCycleB4E708(void)
{
    g_br0AB3D8 = 1;
    (void)BrOptCycle(&g_brB4E708, BR_OPT_B4E708_MAX);
    BrSub10060D90();
    return 1;
}

/* 0x10042D60. Same as above but clears 0x100AB3D8 instead of setting it. */
int BrOptCycleB4E70C(void)
{
    g_br0AB3D8 = 0;
    (void)BrOptCycle(&g_brB4E70C, BR_OPT_B4E70C_MAX);
    BrSub10060D90();
    return 1;
}

/* 0x10042DC0 */
int BrOptCycleAC64C(void)
{
    g_br09435C = g_aBrAC4A0[BrOptCycle(&g_br0AC64C, BR_OPT_AC64C_MAX)];
    return 1;
}

/* 0x10042E20 */
int BrOptCycleAC650(void)
{
    g_br094358 = g_aBrAC4B0[BrOptCycle(&g_br0AC650, BR_OPT_AC650_MAX)];
    return 1;
}

/* 0x10042E80 */
int BrOptCycleAA2A08(void)
{
    g_br094354 = g_aBrAC518[BrOptCycle(&g_brAA2A08, BR_OPT_AA2A08_MAX)];
    return 1;
}

/* ==========================================================================
 * 0x10042EE0 -- vehicle select
 * ========================================================================== */

/* `neg / sbb / and 3 / add 0xB`: 14 when 0x10AA28FC is non-zero, else 11.
 * The original recomputes this at every single step of the search, so it is
 * a function here rather than a value hoisted out of the loop. */
static int32_t BrOptCarMax(void)
{
    return (g_brAA28FC != 0) ? BR_OPT_AC648_MAX_EXTRA : BR_OPT_AC648_MAX_BASE;
}

int BrOptCycleCar(void)
{
    int32_t v, vStart, iVal;
    const BrRec2A8 *pRec;

    if (g_brAA33D4 != 0) {
        v = g_br0AC648 + 1;
        g_br0AC648 = v;
        if (v > BrOptCarMax()) {
            v = 0;
            g_br0AC648 = 0;
        }
        vStart = v;
        if (BrSub1003F2B0(v) == 0) {
            for (;;) {
                v = g_br0AC648 + 1;
                g_br0AC648 = v;
                if (v > BrOptCarMax()) {
                    v = 0;
                    g_br0AC648 = 0;
                    /* GOTCHA: the wrap path JUMPS PAST the full-circle test
                     * (0x10042F4A -> 0x10042F54), so an entry that is
                     * rejected and sits at index 0 gets probed twice.
                     * 0x10042B30's equivalent loop does not do this. */
                } else if (v == vStart) {
                    break;
                }
                if (BrSub1003F2B0(v) != 0)
                    break;
            }
        }
        v = g_br0AC648;
    } else if (g_brAA33D0 != 0) {
        v = g_br0AC648 - 1;
        g_br0AC648 = v;
        if (v < 0) {
            v = BrOptCarMax();
            g_br0AC648 = v;
        }
        vStart = v;
        if (BrSub1003F2B0(v) == 0) {
            for (;;) {
                v = g_br0AC648 - 1;
                g_br0AC648 = v;
                if (v < 0) {
                    v = BrOptCarMax();
                    g_br0AC648 = v;
                    /* same asymmetry as the increment path */
                } else if (v == vStart) {
                    break;
                }
                if (BrSub1003F2B0(v) != 0)
                    break;
            }
        }
        v = g_br0AC648;
    } else {
        v = g_br0AC648;
    }

    iVal = g_aBrAC4D8[v];
    g_br0B380C = iVal;

    if (g_brP277B40 != NULL) {
        /* NOTE the index: 0x100AC308 is indexed by the table VALUE, not by
         * the option index. */
        BrSprintf(g_aBrA9DD28, BrStrGet(BR_OPT_STR_CAR),
                  BrStrGet((int)g_aBrAC308[iVal]));
        pRec = g_aBrBD2A8[g_br0B380C];
        if ((pRec->f04 & 0x10) != 0)
            strcat(g_aBrA9DD28, BrStrGet(BR_OPT_STR_LOCKED));  /* DEVIATION */
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x100430B0 -- the 1-based cycler
 * ========================================================================== */

int BrOptCycleBD3E0(void)
{
    char    aNum[BR_OPT_TEXT_MAX];   /* the original's is 0x20 of stack */
    int32_t v;

    if (g_brAA33D4 != 0) {
        v = g_br0BD3E0 + 1;
        g_br0BD3E0 = v;
        if (v > BR_OPT_BD3E0_MAX) {
            v = BR_OPT_BD3E0_MIN;        /* wraps to 1, NOT to 0 */
            g_br0BD3E0 = v;
        }
    } else if (g_brAA33D0 != 0) {
        v = g_br0BD3E0 - 1;
        g_br0BD3E0 = v;
        if (v < BR_OPT_BD3E0_MIN) {
            v = BR_OPT_BD3E0_MAX;
            g_br0BD3E0 = v;
        }
    } else {
        v = g_br0BD3E0;
    }

    g_br0AC658 = v;

    if (g_brP277B40 != NULL) {
        BrItoa(v, aNum, 10);
        BrSprintf(g_aBrA9DD28, BrStrGet(BR_OPT_STR_BD3E0), aNum);
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10043180
 * ========================================================================== */

int BrOptCycleAA2A00(void)
{
    int32_t iVal = g_aBrAC4C0[BrOptCycle(&g_brAA2A00, BR_OPT_AA2A00_MAX)];

    g_br22B350 = iVal;
    if (g_brP277B40 != NULL) {
        /* again indexed by the table VALUE */
        BrSprintf(g_aBrA9DD28, BrStrGet(BR_OPT_STR_AA2A00),
                  BrStrGet((int)g_aBrAC3B0[iVal]));
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10043260, 0x10043330, 0x100434C0 -- screen installers
 * ========================================================================== */

int BrOptOpen296C(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA296C, BrOptFn10051990);
}

int BrOptOpen2970(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA2970, BrOptFn10051D30);
}

int BrOptOpen2998(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA2998, BrOptFn1004CAC0);
}

/* ==========================================================================
 * 0x10043400 -- the cycler that skips 1
 * ========================================================================== */

int BrOptCycleAA2A0C(void)
{
    int32_t v;

    if (g_brAA33D4 != 0) {
        v = g_brAA2A0C + 1;
        g_brAA2A0C = v;
        if (v >= BR_OPT_AA2A0C_MAX + 1) {
            v = 0;
            g_brAA2A0C = 0;
        }
        if (v == 1) {           /* stepping up skips 1 -> 2 */
            v = 2;
            g_brAA2A0C = v;
        }
    } else if (g_brAA33D0 != 0) {
        v = g_brAA2A0C - 1;
        g_brAA2A0C = v;
        if (v < 0) {
            v = BR_OPT_AA2A0C_MAX;
            g_brAA2A0C = v;
        }
        if (v == 1) {           /* stepping down skips 1 -> 0 */
            v = 0;
            g_brAA2A0C = v;
        }
    } else {
        v = g_brAA2A0C;
    }

    g_brB4E728 = v;
    v = g_aBrAC520[v];
    g_brB4E1D0 = v;

    /* The original is a `dec/je` chain over 1, 2, 3 with everything else
     * falling through to record 0 -- i.e. exactly indexing the four-record
     * array with anything outside 1..3 clamped to 0. */
    if (v >= 1 && v <= 3)
        g_brB4E1D4 = g_aBrB4DF30[v];
    else
        g_brB4E1D4 = g_aBrB4DF30[0];
    return 1;
}

/* ==========================================================================
 * 0x10043590, 0x100435F0, 0x10043650, 0x100436B0 -- two-state cyclers
 * ========================================================================== */

int BrOptCycleAA2A1C(void)
{
    g_brB4E1E0 = g_aBrAC530[BrOptCycle(&g_brAA2A1C, 1)];
    return 1;
}

int BrOptCycleAA2A28(void)
{
    g_brB4E7A0 = g_aBrAC548[BrOptCycle(&g_brAA2A28, 1)];
    return 1;
}

int BrOptCycleAA2A20(void)
{
    g_brB4E1D8 = g_aBrAC538[BrOptCycle(&g_brAA2A20, 1)];
    return 1;
}

int BrOptCycleAA2A24(void)
{
    g_brB4E1DC = g_aBrAC540[BrOptCycle(&g_brAA2A24, 1)];
    return 1;
}

/* ==========================================================================
 * 0x10043760 .. 0x10043A00 -- transitions and the lobby
 * ========================================================================== */

/* 0x10043760. Returns 0. */
int BrOpt3760(BrGameObj *pGame)
{
    BrGameSub *pSub;

    pSub = pGame->pSub;
    pSub->f68 = 0;
    pSub = pGame->pSub;                 /* reloaded by the original */
    pSub->pVtbl->pfnSlot6(pSub, 0);

    if (g_br0AA010 == 0)
        g_br0BD3E0 = 3;

    BrSub1003E310();
    BrSub1006A4A0(g_aBrB4DF30[0], g_aBrB4FBE8);
    BrSub10041B50();
    return 0;
}

/* 0x100437B0 */
int BrOpt37B0(void)
{
    if (g_brPA9D008->f08 != g_br0AB3E0)
        BrSub1003DA40(g_brPA9D008, g_br0AB3E0);
    return 1;
}

/* 0x100437D0 */
int BrOpt37D0(BrGameObj *pGame)
{
    BrGameSub *pSub;

    if (g_brAA2894 != 0 && g_brA9D000 != 0) {
        pSub = pGame->pSub;
        pSub->f68 = 0;
        pSub = pGame->pSub;
        pSub->pVtbl->pfnSlot6(pSub, 0);
        BrSub10038F30(0);
    }
    return 1;
}

/* 0x10043810 */
int BrOpt3810(BrGameObj *pGame)
{
    BrGameSub       *pSub;
    BrOptObj        *pObj;
    BrDPSessionDesc *pDesc;
    int              i;

    if (g_brAA2894 != 0) {
        if (g_brA9D000 == 0) {
            /* --- 0x10043984 ------------------------------------------- */
            BrSub10046400(pGame);
            pObj = g_brPAA2950;
            if (pObj != NULL) {
                pObj->pVtbl->f00(pObj, 1);
                g_brPAA2950 = NULL;
            }
            g_brPAA2904 = g_brPAA2948;
            BrSub1003BF60();
            g_brAA2898 = 1;
            if (g_brAA287C == 0 || g_brAA287C == 1)
                BrSub1003C020();
            if (g_brAA287C == 2 || g_brAA287C == 3) {
                if (g_brPAA29D8 != NULL)
                    g_brPAA29D8->f1C &= ~0x10;
            }
            g_brAA2894 = 0;
            return 0;
        }
        pSub = pGame->pSub;
        pSub->f68 = 0;
        pSub = pGame->pSub;
        pSub->pVtbl->pfnSlot6(pSub, 0);
        BrSub10038F30(0);
    }

    if (g_brAA2890 != 0) {
        BrSub10046400(pGame);
        pObj = g_brPAA2950;
        if (pObj != NULL) {
            pObj->pVtbl->f00(pObj, 1);
            g_brPAA2950 = NULL;
        }
        BrOptOpen294C(NULL);
        BrOptOpen2950B(NULL);
        BrOptOpen2954(NULL);
        g_brAA2890 = 0;
        return 0;
    }

    /* --- 0x10043899 ----------------------------------------------------- */
    if (g_brAA2884 != 0) {
        pDesc = NULL;
        if (g_brP277B40 != NULL)
            BrSub1003D0B0(g_brP277B40, &pDesc);

        if (pDesc != NULL) {
            /* Find the slot whose id matches the UI selection and record
             * "there is more than one player" in its second field. */
            for (i = 0; i < BR_SLOT_COUNT; ++i) {
                if (g_aBrAA2538[i].id == g_brPA9D008->f08) {
                    g_aBrAA2538[i].a = (pDesc->dwCurrentPlayers > 1) ? 1 : 0;
                    break;
                }
            }
            BrGlobalUnlock(BrGlobalHandle(pDesc));
            BrGlobalFree(BrGlobalHandle(pDesc));
        }
    }

    if (g_brAA288C == 0)
        return 1;

    BrSub1003E310();
    BrSub1006A4A0(g_aBrB4DF30[0], g_aBrB4FBE8);
    pSub = pGame->pSub;
    pSub->f68 = 0;
    pSub = pGame->pSub;
    pSub->pVtbl->pfnSlot6(pSub, 0);
    g_brAA285C = 0;
    BrSub10072AF0(2, 0x200020);
    g_brAA2854 = 2;
    return 0;
}

/* 0x10043A00. GOTCHA: the DirectPlay pointer is NOT null-checked before
 * 0x1003D0B0 is called with it -- unlike every other use in this packet. */
int BrOpt3A00(void)
{
    BrDPSessionDesc *pDesc = NULL;
    int              fAllReady;
    int              i;

    BrSub1003D0B0(g_brP277B40, &pDesc);
    if (pDesc == NULL)
        return 1;

    if (pDesc->dwCurrentPlayers <= 1) {
        /* "not enough players": a bare string, not a format. */
        strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_TOOFEW));   /* DEVIATION */
        BrOptFlushMessage();
    } else if (g_brAA2884 != 0) {
        /* Every occupied slot (id != BR_SLOT_EMPTY) must have a non-zero
         * second field. An unoccupied slot never blocks. */
        fAllReady = 1;
        for (i = 0; i < BR_SLOT_COUNT; ++i) {
            if (g_aBrAA2538[i].a != 0)
                continue;
            if (g_aBrAA2538[i].id != BR_SLOT_EMPTY) {
                fAllReady = 0;
                break;
            }
        }
        if (fAllReady) {
            BrSub1003D9F0(g_brPA9D008);
            g_brAA288C = 1;
            pDesc->dwFlags |= 0x20;    /* DPSESSION_JOINDISABLED */
            g_brP277B40->pVtbl->pfnSetSessionDesc(g_brP277B40, pDesc, 0);
        } else {
            strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_NOTREADY)); /* DEVIATION */
            BrSub1003D210(g_brP680584, g_brPA9D008, 1);
            strcpy(g_aBrA9DD28, g_aBr39B720);
        }
    } else {
        BrSub1003D950(g_brPA9D008, BrSub10058700());
    }

    BrGlobalUnlock(BrGlobalHandle(pDesc));
    BrGlobalFree(BrGlobalHandle(pDesc));
    return 1;
}

/* ==========================================================================
 * 0x10043CD0 .. 0x10043E70 -- more screen installers
 * ========================================================================== */

int BrOptOpen2940(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA2940, BrOptFn100558A0);
}

int BrOptOpen298C(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA298C, BrOptFn10056A10);
}

/* 0x10043E70. Unlike its siblings the reuse path does NOT return early: it
 * falls into the same tail as the create path. */
int BrOptOpen2948(BrGameObj *pUnused)
{
    (void)pUnused;

    if (!BrOptEnsureObj(&g_brPAA2948, BrOptFn10056FF0))
        return 0;

    if (g_brA9CFFC == 0 && g_brA9D000 == 0 &&
        (g_brAA287C == 0 || g_brAA287C == 1))
        BrSub1003C020();
    return 1;
}

/* ==========================================================================
 * 0x10043F50 .. 0x100440B0
 * ========================================================================== */

/* 0x10043F50. Returns 0. */
int BrOpt3F50(BrGameObj *pGame)
{
    BrGameSub *pSub;
    BrOptObj  *pObj;

    g_brAA287C = 2;
    pSub = pGame->pSub;
    pSub->pVtbl->pfnSlot7(pSub);

    pObj = g_brPAA2904;
    if (pObj != NULL)
        pObj->pVtbl->f00(pObj, 1);

    g_brPAA298C = NULL;
    g_brPAA2904 = g_brPAA2948;
    return 0;
}

/* 0x10043FA0. Returns 0. */
int BrOpt3FA0(BrGameObj *pGame)
{
    BrGameSub *pSub = pGame->pSub;

    pSub->pVtbl->pfnSlot6(pSub, 1);
    g_brPAA2904 = g_brPAA2908;
    return 0;
}

/* 0x10043FC0. Returns 0. */
int BrOpt3FC0(BrGameObj *pGame)
{
    BrGameSub *pSub;
    BrOptObj  *pObj;

    pSub = pGame->pSub;
    pSub->pVtbl->pfnSlot7(pSub);

    pObj = g_brPAA2904;
    if (pObj != NULL)
        pObj->pVtbl->f00(pObj, 1);

    g_brAA2958 = 0;
    g_brAA29A8 = 0;
    g_brPAA2904 = g_brPAA2908;
    return 0;
}

int BrOpt4010(BrGameObj *pGame) { g_brAA287C = 0; BrOptOpen2948(pGame); return 1; }
int BrOpt4030(BrGameObj *pGame) { g_brAA287C = 0; BrSub10047360(pGame); return 1; }
int BrOpt4050(BrGameObj *pGame) { g_brAA287C = 1; BrOptOpen2948(pGame); return 1; }
int BrOpt4070(BrGameObj *pGame) { g_brAA287C = 1; BrSub10047360(pGame); return 1; }
int BrOpt4090(BrGameObj *pGame) { g_brAA287C = 2; BrOptOpen2948(pGame); return 1; }
int BrOpt40B0(BrGameObj *pGame) { g_brAA287C = 2; BrSub10047360(pGame); return 1; }

/* ==========================================================================
 * 0x100440D0 .. 0x100446D0
 * ========================================================================== */

int BrOptOpen294C(BrGameObj *pUnused)
{
    (void)pUnused;
    return BrOptEnsureObj(&g_brPAA294C, BrOptFn100575F0);
}

/* 0x100441A0. DEVIATION: declared void. The original falls off two of its
 * three exits without loading eax, so its "return value" is whatever the
 * last call left behind; no caller in the DLL examines it.
 *
 * GOTCHA: the session descriptor fetched here is never released -- there is
 * no GlobalHandle/GlobalFree pair on this path, unlike 0x10043810 and
 * 0x10043A00. That leak is in the original. */
void BrOpt41A0(void)
{
    BrDPSessionDesc *pDesc;

    g_brAA287C = 1;
    BrSub100586A0();

    if (g_brAA2884 != 0) {
        pDesc = NULL;
        if (g_brP277B40 != NULL)
            BrSub1003D0B0(g_brP277B40, &pDesc);
        if (pDesc != NULL) {
            pDesc->dwFlags &= ~0x20u;    /* clear DPSESSION_JOINDISABLED */
            g_brP277B40->pVtbl->pfnSetSessionDesc(g_brP277B40, pDesc, 0);
        }
    }

    BrSub10043BF0(NULL);
    BrOptOpen2940(NULL);
    BrOptOpen2948(NULL);

    if (g_brAA2884 != 0) {
        BrOptOpen294C(NULL);
        BrOptOpen2950B(NULL);
    } else {
        BrSub1003CE80();
        BrOptOpen2950A(NULL);
    }

    if (g_brAA2884 != 0) {
        if (g_brAA2888 == 0) {
            BrSub1003C150();
            g_brAA2888 = 1;
            return;
        }
        BrSub1003CDA0();
    }
}

/* 0x10044280 */
int BrOptOpen2950A(BrGameObj *pUnused)
{
    int fOpen;

    (void)pUnused;

    g_brAA2884 = 0;
    g_brAA2898 = 0;

    if (g_brAA2878 == 0) {
        fOpen = 0;
        if (g_brAA287C == 2 || g_brAA287C == 3) {
            g_brAA2898 = 1;
            /* Mode 2 additionally demands at least seven characters in the
             * text at 0x10A9CDF0. */
            if (g_brAA287C == 2 && strlen(g_aBrA9CDF0) < 7)
                return 1;
            if (g_brPAA29D8 == NULL) {
                BrSub1003C1E0();
                return 1;
            }
            if (g_brPAA29D4->f1E164 == 0) {
                BrSub1003C1E0();
                return 1;
            }
            fOpen = (BrSub1003C260() != 0);
        } else {
            fOpen = (BrSub1003C260() != 0);
        }
        if (!fOpen)
            return 1;
    }

    if (!BrOptEnsureObj(&g_brPAA2950, BrOptFn10057C10))
        return 0;

    g_brPAA29B8->pfnHook = BrOptFn10044970;
    return 1;
}

/* 0x100443E0 */
int BrOptOpen2950B(BrGameObj *pUnused)
{
    (void)pUnused;

    g_brAA2884 = 1;
    g_br22AF18 = 2;
    g_brAA2898 = 0;

    if (!BrOptEnsureObj(&g_brPAA2950, BrOptFn10057C10))
        return 0;

    g_brPAA29B8->pfnHook = BrOptFn10044A30;
    return 1;
}

/* 0x100444C0. Returns 0. */
int BrOpt44C0(BrGameObj *pGame)
{
    BrGameSub *pSub;
    BrOptObj  *pObj;

    pSub = pGame->pSub;
    pSub->pVtbl->pfnSlot7(pSub);

    pObj = g_brPAA2904;
    if (pObj != NULL)
        pObj->pVtbl->f00(pObj, 1);

    g_brPAA294C = NULL;
    g_brPAA29B8 = NULL;
    g_brPAA2904 = g_brPAA2948;

    if (g_brPAA29D8 != NULL)
        g_brPAA29D8->f1C &= ~0x10;

    if ((g_brAA287C == 0 || g_brAA287C == 1) && g_brA9D000 == 0) {
        BrSub1003BF60();
        g_brAA2898 = 1;
        BrSub1003C020();
    }
    return 0;
}

/* 0x10044600. GOTCHA: 0x10044540 is NOT called when neither input is set --
 * the "no input" branch jumps past it. Contrast 0x10042CF0. */
int BrOptCycleAA2A18(void)
{
    int32_t v;
    int     fEdited = 0;

    if (g_brAA33D4 != 0) {
        v = g_brAA2A18 + 1;
        g_brAA2A18 = v;
        if (v >= BR_OPT_AA2A18_MAX + 1)
            g_brAA2A18 = 0;
        fEdited = 1;
    } else if (g_brAA33D0 != 0) {
        v = g_brAA2A18 - 1;
        g_brAA2A18 = v;
        if (v < 0)
            g_brAA2A18 = BR_OPT_AA2A18_MAX;
        fEdited = 1;
    }

    if (fEdited)
        BrSub10044540();

    if (g_brP277B40 != NULL) {
        BrSprintf(g_aBrA9DD28, BrStrGet(BR_OPT_STR_AA2A18),
                  BrStrGet((int)g_aBrAC3C8[g_brAA2A18]));
        BrOptFlushMessage();
    }
    return 1;
}

/* 0x100446D0 */
int BrOptOpen2954(BrGameObj *pUnused)
{
    (void)pUnused;

    if (!BrOptEnsureObj(&g_brPAA2954, BrOptFn10058750))
        return 0;

    g_br0AA010 = 6;

    if (g_brAA2884 != 0) {
        if (g_brAA2888 == 0) {
            if (g_brAA287C == 2 || g_brAA287C == 3)
                BrSub1003C230();
            BrSub1003C150();
            g_brAA2888 = 1;
            BrSub1003CDA0();
        } else {
            BrSub1003CDA0();
        }
    }
    return 1;
}

/* ==========================================================================
 * DEVIATIONs, collected
 * ==========================================================================
 *  - every `rep movsb` / `rep movsd` string move is written as strcpy,
 *    strcat or memcpy; the semantics are identical (the original always
 *    recomputes the length with `repne scasb` first)
 *  - the C++ exception frames (`push -1 / push funclet / fs:[0]` and the
 *    [esp+0xC] state variable) in the ten installers are dropped; nothing
 *    in this packet can throw
 *  - BrOptEnsureObj allocates sizeof(BrOptObj) instead of the literal 0xC8,
 *    because on a 64-bit host the three leading pointers make the object
 *    larger. Anything that assumes 0xC8 (notably the constructor at
 *    0x10048710) must be ported with the same struct
 *  - 0x100441A0 is declared void; the original leaves eax undefined on two
 *    of its three exits
 *  - the text buffers 0x10A9CDF0, 0x10A9DD28, 0x1039B720 and 0x11782BC8 are
 *    given a size of 0x104. The original bounds none of them, so no true
 *    size is recoverable; 0x104 is the path-buffer size the game uses
 *    elsewhere (slice1_06.h)
 *  - BrOptCycleBD3E0's itoa scratch is 0x104 bytes where the original's
 *    frame gives it 0x20; the original cannot overflow 0x20 with a value in
 *    1..12 either way
 *
 * NOT DEVIATED FROM, though it looks like a bug: the itoa scratch inside
 * 0x10042880 sits four bytes below the name buffer it is concatenated onto,
 * so a four-digit index would corrupt "TimeAttack" before it is read. The
 * port keeps both buffers at their original frame offsets so the behaviour
 * is preserved rather than silently repaired.
 */
