/* brally.c -- the first host that BOOTS the ported core instead of viewing
 * its assets.
 *
 * WHAT THIS IS, AND WHAT brview WAS NOT
 *
 * port/tools/brview.c draws two .img files at hardcoded coordinates. It proves
 * Metal works; it runs none of the game. This runs the game's OWN menu code:
 * it constructs the phase object with the real constructor (0x10048710), calls
 * a real screen builder, and then walks the structure the builder produced.
 * Every coordinate it draws was computed by decompiled game logic, not by this
 * file.
 *
 * HOW IT LINKS AT ALL
 *
 * 216 symbols are still unported. port/host/br_stubs.c defines every one of
 * them as a no-op returning 0, so the binary links today. A stub that is never
 * called costs nothing; a stub that IS called is reported at exit with its hit
 * count. That is the point of this program as much as the rendering is -- it
 * converts "which of the remaining ~600 functions matter?" from guesswork into
 * a measured, hit-ordered list.
 *
 * Run with BR_STUB_ABORT=1 to die at the first stub instead of continuing on a
 * zero return, which is how you find the one that actually breaks the boot.
 *
 *   ./build/brally            headless: boot, dump, report stubs
 *   ./build/brally -all       every screen builder, each in its own child
 *   ./build/brally -b N       one builder in-process, for a debugger
 *   ./build/brally -w         open a window and NAVIGATE the menu
 *   ./build/brally -shot N f.ppm [keys]
 *                             render one screen offscreen, optionally after
 *                             a key script, and write a PPM
 *   ./build/brally -keys N "<script>"
 *                             headless scripted navigation: d down, u up,
 *                             j activate, . idle. Dumps the selection state
 *                             after every key and reports any phase change.
 *   ./build/brally -race <track.trk> <steps>
 *                  [-drop <m>] [-cars <n>] [-phantom <n>] [-lead <m>]
 *                             THE RACE, which is not a phase: load a track,
 *                             build the collision grid from its own triangles,
 *                             spawn the field, install THE PORTED 0x10019A70
 *                             (br_racestep.c) in the slot at 0x106E79F4 and
 *                             run N fixed 1/30 s frames.  Dumps car 0's
 *                             position, velocity, |angVel|, lap/gate and all
 *                             four ground probes, the lap/gate ladder for the
 *                             whole field, every start-light transition, and
 *                             both hole tables at the end.
 *                             `-drop` is how far above the surface each grid
 *                             box starts (default 0.5 m); `-cars` and
 *                             `-phantom` size the two halves of the field
 *                             (default 16 + 4, against the original's twenty
 *                             driver slots); `-lead` is how far past the path
 *                             root a phantom slot is seeded and `-spread`
 *                             how far apart consecutive ones are (default:
 *                             one lap divided between them).
 *
 * THE MENU IS NAVIGABLE. `-keys` exists because "the selection moves" is not
 * something a terminal session or a CI job can check by looking, and an
 * unverified claim about interactive behaviour is worth nothing. It needs no
 * window server, no compositor and no human, and it prints the state before
 * and after every key so the claim can be read off the output.
 *
 *   ./build/brally -keys 4 ".ddj"
 *     '.'  sel=0 iSel=0 cSel=3 current=2 Load Season
 *     'd'  sel=1 iSel=1 cSel=3 current=3 New Season
 *     'd'  sel=2 iSel=2 cSel=3 current=4 Back
 *     'j'  ** PHASE CHANGED -- 0x10046C90 tore the screen down
 *
 *
 * CAVEAT, stated because it can bite: stubs return integer 0. A caller
 * expecting a float gets whatever was in xmm0, not 0.0. Any such gap shows up
 * in the stub report rather than being silently trusted.
 */
#include "br_phase.h"
#include "slice6_73.h"
#include "br_gfx.h"
#include "br_img.h"
#include "br_uictl.h"
#include "br_uispr.h"
#include "br_bmp.h"
#include "br_uivt.h"
/* br_uinav.h pulls slice3_32.h in under the rename slice3_32.c itself uses,
 * so it must come AFTER slice6_73.h and never the other way round -- the
 * header says so at the include. */
#include "br_uinav.h"
/* The menu's own sprite font, and the +0x0C hook that picks its sheet.
 * br_sprfont.h includes slice3_39.h and br_ui.h, both of which are already
 * in scope through the headers above; it comes last for the same reason
 * br_uinav.h does. */
#include "br_sprfont.h"
#include "br_sfxout.h"
#include "br_sfxaq.h"
#include "br_crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void BrStubReport(void);

/* Per-slice context wiring. Each lives in its own translation unit -- once
 * because the slice headers carried conflicting models of the page and control
 * types and could not be included together, now simply because each packet
 * still has its own module globals. See br_wire72.c, which no longer has to
 * hand-declare the two constructors to dodge that clash. */
void BrHostWire71(void);
void BrHostWire72(void);
/* Not a slice context: the IDirectInput root the force-feedback probe
 * (0x100795D0, slice6_77.c) dereferences via BrFfbInit. A COM object, so the
 * host owns it. See br_wire77.c. */
void BrHostWire77(void);

/* --- the wiring the original keeps in .data ----------------------------- */

/* The builders store these into control slots and never call them during a
 * build, so NULL is honest here: if one is ever invoked we want the crash, not
 * a silent no-op that hides a missing behaviour. */
static const BrUi73Hooks g_hooks;          /* all NULL, deliberately */

/* Phase vtable. The build path calls none of these; they are wired so the
 * object is not left with a NULL vtable for later passes. */
static const BrPhaseVtbl_ g_phaseVtbl;

static void ClearSub70(void *pArg) { (void)pArg; }

/* --- the control vtable -------------------------------------------------
 * BrUiCtlCtor stores an all-NULL vtable by default, so the first virtual call
 * jumps to 0. That default is right for the port -- an unported method must
 * never silently succeed -- but it ends the run before anything is observable.
 *
 * These two slots used to be FAKES: one counted calls and threw the text
 * away, the other invented the rectangle as x/y plus the +0x7F/+0x21 extents
 * every builder happens to use. Both now delegate to the real ports
 * (br_uivt.c: 0x10047EB0 and 0x10047FB0), and only the counting is still this
 * file's. So the geometry DrawPhase renders is now computed by decompiled
 * game logic, including the style rectangle's left/right edges, which the
 * fake could not see at all.
 *
 * The text box's own vtable (0x1008F728) is wired below to slice3_39.c's
 * measuring methods, which is what makes width and height real numbers rather
 * than zeroes.
 * ------------------------------------------------------------------------ */
static int g_nSetText, g_nPlace;
static const char *g_lastText;

/* Every caption the builder set, in order. This is what makes the harness show
 * a MENU rather than a control count: the text comes from BrStrGet, i.e. from
 * the real string table, and the builder chose both the id and the order. */
#define BR_MAXCAP 64
static const char *g_aCap[BR_MAXCAP];
static const void *g_aCapOwner[BR_MAXCAP];   /* the control it was set on */
static int         g_nCap;

/* The caption a control was actually given, or NULL.
 *
 * An earlier version of this dump GUESSED the mapping -- it walked captions in
 * order and advanced whenever a rectangle was non-empty. That produced
 * confident, wrong output: controls with w=0 were shown owning the caption of
 * the next real one. The association is recorded at setText time instead, so
 * it is observed rather than inferred. */
static const char *CapFor(const void *pCtl)
{
    int i;
    for (i = 0; i < g_nCap; i++)
        if (g_aCapOwner[i] == pCtl) return g_aCap[i];
    return NULL;
}

static void HostCtlSetText(BrUiCtl_ *pThis, const void *pText,
                           int32_t a2, int32_t a3, const void *pStyle)
{
    g_nSetText++;
    if (pText) {
        g_lastText = (const char *)pText;
        if (g_nCap < BR_MAXCAP) {
            g_aCapOwner[g_nCap] = pThis;
            g_aCap[g_nCap++]    = g_lastText;
        }
    }
    /* br_uictl.c deliberately does not run the item's element constructor
     * (0x1005B050), so the text box arrives with a NULL vtable and
     * 0x10047EB0's measure dispatch would be skipped. Planting the pointer
     * the element ctor would have planted is the harness's job, not the
     * constructor's -- see the note in br_uictl.c. */
    if (pThis && !pThis->aText[0].pVtbl) pThis->aText[0].pVtbl = g_pBrTextBoxVtbl;
    BrUiCtlSetText_10047EB0(pThis, pText, a2, a3, pStyle);
}

static void HostCtlPlace(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                         int32_t flags, int32_t a4, int32_t a5,
                         int32_t a6, int32_t a7)
{
    g_nPlace++;
    /* Same gap as the text box's above, one object out: BrUiCtlCtor does not
     * run the embedded list's constructor either (br_uictl.c spells out why --
     * 0x1005B7F0 lives in slice3_39.o, which would drag slice6_72.c in and
     * stop test_uictl linking br_uictl.o on its own). The control's memset
     * covers every ZERO 0x1005B7F0 writes, so what is actually missing is the
     * three non-zero things: the list's own vtable, the -1 sentinels, and the
     * hundred item vtables that BrTextListAddRow dispatches its measure
     * through. Running the real constructor here supplies all three.
     *
     * Placement is where it goes because placement is what every builder does
     * to a control first, before it touches +0x3838. Guarded on the vtable so
     * a re-placement cannot wipe a list that already has rows in it. */
    if (pThis && pThis->list.pVtbl == NULL) {
        BrTextListInit(&pThis->list);
    }
    BrUiCtlPlace_10047FB0(pThis, pOwner, x, y, flags, a4, a5, a6, a7);
}

static BrUiCtlVtbl_ g_hostCtlVtbl;

/* 0x1008F728 -- the text widget's vtable. Only the three slots 0x10047EB0
 * dispatches are filled; slice3_39.c ports all three. The rest stay NULL so
 * an unported method still faults. */
static BrTextBoxVtbl g_hostTextBoxVtbl;

/* 0x1008F758 -- the text LIST's vtable, the one every embedded control at
 * +0x3838 gets (br_ui.h ADJ-6). Two slots are now real:
 *
 *   +0x10  0x1005BC10  BrTextListAddRow
 *   +0x14  0x1005B910  BrTextListConfig
 *
 * These are the two the menu builders call, and a NULL vtable here is what
 * 0x1004F700 and 0x1005A6E0 were dying on -- EXC_BAD_ACCESS at 0x28, which is
 * slot 5 at LP64 pointer stride, i.e. f14.
 *
 * The other fourteen stay NULL on purpose, +0x2C included: BrTextListAddRow
 * calls it when the list is already 100 rows deep, and a fault there is a
 * truthful "this is not ported" rather than a silently dropped row. */
static BrTextListVtbl g_hostTextListVtbl;

/* --- every ported screen builder ----------------------------------------
 * Declared here rather than by including all six slice headers, which cannot
 * coexist in one translation unit: they carry conflicting partial models of
 * the phase object (that conflict is what br_phase.h exists to resolve). The
 * name, arity and return type below match each module's own declaration; only
 * the pointee differs, by being the merged one.
 * ------------------------------------------------------------------------ */
void BrExt_10049F40(BrPhase_ *);
void BrExt_1004D640(BrPhase_ *);
void BrExt_1004DFC0(BrPhase_ *);
void BrExt_1004E830(BrPhase_ *);
void BrExt_1004F2B0(BrPhase_ *);
void BrExt_1004F700(BrPhase_ *);
void BrExt_10050060(BrPhase_ *);
void BrExt_10052030(BrPhase_ *);
void BrExt_10054B50(BrPhase_ *);
void BrExt_10059760(BrPhase_ *);
void BrExt_1005A6E0(BrPhase_ *);
void BrOptFn10051D30(BrPhase_ *);
void BrOptFn100558A0(BrPhase_ *);
void BrOptFn10056A10(BrPhase_ *);
void BrOptFn100575F0(BrPhase_ *);
void BrOptFn10057C10(BrPhase_ *);

/* `iModel` is which module's page/control model the builder WRITES through.
 * As of the slice6_72 / slice6_73 migration there is only ONE model: both
 * packets are typed over br_ui.h's `struct BrUiPage_` and `BrUiCtl_`, which
 * is the tag br_phase.h forward-declares and which those two headers used to
 * complete DIFFERENTLY. The column is kept because it records which builders
 * were once reading a foreign layout, and because the suppression logic below
 * is the thing that stopped unjustifiable numbers being quoted as evidence.
 *
 * slice6_71's WAS NOT, and that is the bug this column exists to survive.
 * That module used to build pages through slice3_33.h's BrUiScreen, which
 * begins at +0x10 and has no pVtbl / pfn04 / pfn08, so reading one of its
 * pages through a model that begins at +0x00 landed three fields off. The
 * symptom was not a stable wrong number, which is what made it dangerous:
 * BrExt_10049F40 reported 9, 10, 12, 7 and 10 across five runs of the SAME
 * binary, because the offset the host read was never written and held
 * whatever the heap had. setText and place stayed at 3 and 4 throughout --
 * they are counted by this file's vtable slots and never touch the struct.
 *
 * slice6_71 now builds through br_ui.h's canonical BrUiPage_, whose fields
 * are the same fields in the same order as slice6_73.h's, so its four
 * builders read correctly here and are marked 73. Their counts are now
 * stable across runs and equal to the number of `inc word ptr [esi+0x14]`
 * in each disassembly: 4, 13, 3 and 8.
 *
 * The column is still SUPPRESSED for any builder whose model differs.
 * Printing a number we cannot justify is worse than printing none: two
 * earlier status reports quoted those garbage counts as if they confirmed
 * the decompilation. */
typedef struct {
    const char *pszName;
    void      (*pfn)(BrPhase_ *);
    int         iModel;     /* 71 / 72 / 73 */
} BrBuilder;
static const BrBuilder g_aBuilders[] = {
    { "BrExt_10049F40", BrExt_10049F40, 73 },
    { "BrExt_1004D640", BrExt_1004D640, 73 },
    { "BrExt_1004DFC0", BrExt_1004DFC0, 73 },
    { "BrExt_1004E830", BrExt_1004E830, 72 },
    { "BrExt_1004F2B0", BrExt_1004F2B0, 73 },
    { "BrExt_1004F700", BrExt_1004F700, 73 },
    { "BrExt_10050060", BrExt_10050060, 73 },
    { "BrExt_10052030", BrExt_10052030, 72 },
    { "BrExt_10054B50", BrExt_10054B50, 73 },
    { "BrExt_10059760", BrExt_10059760, 72 },
    { "BrExt_1005A6E0", BrExt_1005A6E0, 72 },
    { "BrOptFn10051D30", BrOptFn10051D30, 73 },
    { "BrOptFn100558A0", BrOptFn100558A0, 73 },
    { "BrOptFn10056A10", BrOptFn10056A10, 72 },
    { "BrOptFn100575F0", BrOptFn100575F0, 73 },
    { "BrOptFn10057C10", BrOptFn10057C10, 72 }
};
#define BR_NBUILDERS ((int)(sizeof(g_aBuilders)/sizeof(g_aBuilders[0])))


/* ==========================================================================
 * NAVIGATION WIRING
 *
 * Everything below is HOST wiring, not decompiled code. The dividing line
 * matters and is drawn explicitly:
 *
 *   PORTED (port/src/br_uinav.c, from BRGlide.dll):
 *     0x100489A0 THE PHASE FRAME       0x10048530 the page frame
 *     0x100484F0 the selection clamp   0x10048180 the control frame
 *     0x10047A60 current/activate      0x100480A0 the step tick
 *     0x10048010 the enter dispatch    0x10048060 the "other owns it"
 *     0x10047A10 the code hand-off     0x10048AA0 release all pages
 *     0x10045AF0 the FORWARD hook      0x10046C90 the BACK hook
 *   ...plus 0x1005FFB0 (slice3_39.c), the keyboard state read and edge pass,
 *   which 0x100489A0 calls for real.
 *
 *   SUPPLIED BY THIS FILE, and each one is a function the port has NOT
 *   transcribed (or, for the two marked, cannot yet REACH), standing in so
 *   the frame can complete rather than fault:
 *     control vtable +0x00  0x100478A0  the scalar deleting destructor
 *     control vtable +0x18  0x10047980  draw at an explicit rect
 *     control vtable +0x1C  0x10047930  draw at the control's own position
 *     page    vtable +0x00  0x100484C0  the page's deleting destructor
 *     phase   vtable +0x00  0x10048850  the phase's deleting destructor
 *     phase   vtable +0x04  0x100488B0  calls the unported 0x1005AFA0
 *     phase   vtable +0x08  0x100488C0  PORTED, unreachable: it reads the
 *                                       root phase's page 0, and the root
 *                                       phase here has no pages
 *     phase   vtable +0x18  0x10048B20  the global shutdown
 *     text box vtable +0x10 0x1005B0xx  the item's draw
 *   ...and ONE that is not a stand-in for an unported function at all: the
 *   DirectInput poll 0x100489A0 makes at 0x10060260. That one is real input,
 *   injected at the original's own call site -- see HostPoll.
 *   Every one is counted, and the counts are printed, so "the frame ran" can
 *   never quietly mean "the frame ran through nine no-ops nobody noticed".
 * ========================================================================== */

static BrScrGlobals  g_scr;       /* the ONE globals object; shared with     */
                                  /* slice3_32.c's byte-image bodies         */
static BrActiveFlags g_active;    /* the nine globals 0x1003E080 reads       */
static BrUiNav       g_nav;
static BrObjAA2E80   g_objAA2E80;

/* 0x10AA2A78 -> the cursor. Parked off-screen: every hot rect and every
 * control rectangle the builders produce has left >= 0, so a negative x can
 * be inside none of them. That is what makes the scripted runs below pure
 * KEYBOARD evidence -- if the pointer could land on a control, "the selection
 * moved" would be ambiguous between the two paths through 0x10047A60. */
static int32_t g_cursor[2] = { -1, -1 };

/* --- the stand-ins, each counted -----------------------------------------
 * The last three joined the list when the phase's OWN frame (0x100489A0)
 * replaced this file's reproduction of its page loop: that function dispatches
 * through three phase-vtable slots this port has not transcribed, and the
 * honest thing is to count them rather than to keep a frame that never reached
 * them. What each one is, and why it is not simply ported here:
 *
 *   +0x04 0x100488B0  two instructions -- `this->vtbl+0x20(this)` then return
 *                     1. Its callee 0x1005AFA0 is unported, so a twin of
 *                     0x100488B0 would only move this stand-in one slot down.
 *   +0x08 0x100488C0  the phase TICK, and it is ported (slice3_32.c's
 *                     BrPhaseTick_100488C0). It is unreachable here for a
 *                     reason worth stating: it reads
 *                     `0x10AA2908->aPages[0]->aItems[199]`, and the root phase
 *                     this harness stands in for start-up with has no pages at
 *                     all. It is blocked on the start-up path, not on itself.
 *   +0x18 0x10048B20  the global shutdown. Reached only from 0x100489A0's two
 *                     failure exits, i.e. only when a screen clears +0x68.
 * ------------------------------------------------------------------------ */
#define SI_COUNT 9
static int g_nStandIn[SI_COUNT];
enum { SI_CTLDEL, SI_CTLDRAWRECT, SI_CTLDRAW, SI_PAGEDEL, SI_PHASEDEL,
       SI_TEXTDRAW, SI_PHASEF04, SI_PHASEF08, SI_PHASEF18 };
static const char *const g_aStandInName[SI_COUNT] = {
    "0x100478A0 control dtor", "0x10047980 draw(rect)", "0x10047930 draw",
    "0x100484C0 page dtor",    "0x10048850 phase dtor", "0x1005B0xx item draw",
    "0x100488B0 phase +0x04",  "0x100488C0 phase tick", "0x10048B20 shutdown"
};

static void *StandInCtlDel(BrUiCtl_ *p, int32_t f)
{ (void)f; g_nStandIn[SI_CTLDEL]++; return p; }
static void StandInCtlDrawRect(BrUiCtl_ *p, void *pRect)
{ (void)p; (void)pRect; g_nStandIn[SI_CTLDRAWRECT]++; }
static void StandInCtlDraw(BrUiCtl_ *p)
{ (void)p; g_nStandIn[SI_CTLDRAW]++; }
static void *StandInPageDel(BrUiPage_ *p, int32_t f)
{ (void)f; g_nStandIn[SI_PAGEDEL]++; return p; }
static void *StandInPhaseDel(BrPhase_ *p, int32_t f)
{ (void)f; g_nStandIn[SI_PHASEDEL]++; return p; }
static void StandInTextDraw(BrTextBox *p)
{ (void)p; g_nStandIn[SI_TEXTDRAW]++; }
static int32_t StandInPhaseF04(BrPhase_ *p)
{ (void)p; g_nStandIn[SI_PHASEF04]++; return 1; }
static int32_t StandInPhaseF08(BrPhase_ *p)
{ (void)p; g_nStandIn[SI_PHASEF08]++; return 1; }
static void StandInPhaseF18(BrPhase_ *p, void *pArg)
{ (void)p; (void)pArg; g_nStandIn[SI_PHASEF18]++; }

/* --- the input seam, which is NOT a stand-in for an unported function -----
 *
 * 0x100489A0 polls DirectInput from inside its own body, and br_uinav.c calls
 * out to this at exactly that point. So a scripted key is applied where the
 * original applies a held key: after the phase has been checked, before any
 * page runs. It writes the two words 0x100603A0 writes and nothing else --
 * BrUiNavMove is the cursor inc/dec plus the step, BrUiNavSetActivate is
 * 0x10AA2AF0.
 *
 * The counter is printed alongside the stand-ins so "the game's frame ran"
 * cannot quietly mean "the frame ran without ever asking for input". */
static int g_nPoll;
static int g_pendDir;      /* -1 / 0 / +1, one frame's worth */
static int g_pendFire;

static void HostPoll(BrUiNav *pNav)
{
    g_nPoll++;
    if (g_pendDir != 0)
        BrUiNavMove(pNav, g_pendDir);
    BrUiNavSetActivate(pNav, g_pendFire);
}

/* The phase vtable is `static const` above so the object is never left with a
 * NULL table; navigation needs two of its slots, so the navigable path uses
 * this writable one instead. */
static BrPhaseVtbl_  g_navPhaseVtbl;
static BrUiPageVtbl_ g_navPageVtbl;

/* The hook table the builders read. Two entries are the ported hooks; every
 * other stays NULL, which is what the all-NULL g_hooks above already gave
 * them -- an unwired hook must stay a visible hole, not a silent no-op. */
static BrUi73Hooks g_navHooks;

static void WireNav(void)
{
    memset(&g_scr, 0, sizeof g_scr);
    memset(&g_active, 0, sizeof g_active);
    memset(&g_objAA2E80, 0, sizeof g_objAA2E80);
    memset(&g_nav, 0, sizeof g_nav);

    /* 0x100AB3DC == 1 in the shipped .data: the selection moves ONE row per
     * frame in which a direction is held. The port must not "improve" this to
     * an instant jump -- the step is data, and the frame is the clock. */
    g_scr.w0AB3DC = 0;              /* no movement until a key sets it */
    g_scr.pAA2E80 = &g_objAA2E80;

    /* 0x100AB568, the stride-24 sprite table, which slice3_32.c reaches
     * through this context field and which no wiring layer had ever filled
     * in -- so its ports of 0x10047930 / 0x10047980 / 0x100479D0 were
     * unreachable. br_uispr.c owns the storage; this points at it. The two
     * structs are the same six int32s under two names (br_uispr.h says which
     * is the owner), so the cast is a rename, not a reinterpretation. */
    g_scr.aAB568 = (const BrScrRectEnt *)g_aBrUiSprite;

    /* BR_CURSOR="x,y" parks 0x10AA2A78 and BR_MOUSE=1 raises the button flag
     * 0x10047A60 and 0x10048180 consult at BrObjAA2E80 +0x2C.
     *
     * These exist because the DOWN sprite cannot be reached from the keyboard
     * at all -- 0x10048180 gates the aStepId[1] swap on the control being hit
     * by the CURSOR and on a button being held, so `-keys` can never show it.
     * Two environment variables are the smallest seam that lets a screenshot
     * prove the swap happens, and they write nothing but the two globals the
     * original writes from its own input layer. The default is the off-screen
     * (-1,-1) below, which is inside no rectangle any builder produces, so
     * every other run stays pure keyboard evidence. */
    {
        const char *pszCur = getenv("BR_CURSOR");
        int cxr, cyr;
        if (pszCur != NULL && sscanf(pszCur, "%d,%d", &cxr, &cyr) == 2) {
            g_cursor[0] = cxr;
            g_cursor[1] = cyr;
        }
        if (getenv("BR_MOUSE") != NULL) {
            g_objAA2E80.f2C = 1;
        }
    }

    g_nav.pG       = &g_scr;
    /* 0x10060260's call site inside 0x100489A0. See HostPoll. */
    g_nav.pfnPoll  = HostPoll;
    g_nav.pCursor  = g_cursor;
    g_nav.pActive  = &g_active;
    g_nav.apHot[0] = BR_UI_STYLE(0x100AB448);
    g_nav.apHot[1] = BR_UI_STYLE(0x100AB418);
    g_nav.apHot[2] = BR_UI_STYLE(0x100AB428);
    g_pBrUiNav     = &g_nav;

    /* The frame chain, straight into the control vtable this file already
     * owns. BrUiNavInstallCtlVtbl touches only the six slots it ports. */
    BrUiNavInstallCtlVtbl(&g_hostCtlVtbl);
    g_hostCtlVtbl.f00 = (void *)StandInCtlDel;
    g_hostCtlVtbl.f18 = StandInCtlDrawRect;
    g_hostCtlVtbl.f1C = StandInCtlDraw;
    /* f34 / f38 are re-planted by the caller after this returns: they are the
     * counting wrappers, and BrUiNavInstallCtlVtbl must not be allowed to
     * look as though it had installed them. */

    BrUiNavInstallPageVtbl(&g_navPageVtbl);
    g_navPageVtbl.f00 = StandInPageDel;
    g_pBrUiPageVtbl   = &g_navPageVtbl;

    /* 0x1008F700. br_uinav.c fills the two slots it ports -- +0x0C, the
     * FRAME, and +0x1C, release-all-pages. The four below are this file's,
     * and three of them are dispatched through by the frame itself. */
    g_navPhaseVtbl     = g_phaseVtbl;
    BrUiNavInstallPhaseVtbl(&g_navPhaseVtbl);
    g_navPhaseVtbl.f00 = StandInPhaseDel;
    g_navPhaseVtbl.f04 = StandInPhaseF04;
    g_navPhaseVtbl.f08 = StandInPhaseF08;
    g_navPhaseVtbl.f18 = StandInPhaseF18;

    g_hostTextBoxVtbl.pfn10 = StandInTextDraw;

    g_navHooks = g_hooks;                       /* all NULL */
    g_navHooks.p10045AF0 = BrUiNavHook_10045AF0;
    g_navHooks.p10046C90 = BrUiNavHook_10046C90;
    /* The control +0x0C hook every selectable menu row is given. Until this
     * was wired the hook was NULL, and a NULL +0x0C is not a harmless gap:
     * 0x10048180's not-current tail tests it and skips the whole recolour
     * when it is unset, so EVERY label -- selected or not -- kept the kind
     * byte its builder passed and the menu had no selection feedback at all.
     * br_sprfont.c's transcription is the struct-model twin of slice3_31.c's
     * byte-image BrSub10047360; see the banner there for why both exist. */
    g_navHooks.p10047360 = BrSprFontKindHook_10047360;
    g_br73.pHooks     = &g_navHooks;
    g_br73.pPhaseVtbl = &g_navPhaseVtbl;

    /* 0x10AA2908, the root phase 0x10046C90 goes BACK to.
     *
     * The original builds it during start-up, in a range this port has not
     * reached; the harness constructs one with the REAL constructor and gives
     * it no pages, which is enough for "back" to have a destination and for
     * the destination to be observable. Its emptiness is the honest state of
     * the port, not a simplification: nothing has been ported that would fill
     * it. */
    {
        BrPhase_ *pRoot = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
        if (pRoot != NULL)
            g_nav.pAA2908 = BrOptObjCtor(pRoot);
    }
}

static char g_scratchA[64], g_scratchB[64];
static int32_t g_blkA[0x53], g_blkB[0x53], g_blkC[0x46];
static unsigned char g_recAA29CC[0x438 * 16];

static void WireContext(void)
{
    memset(&g_br73, 0, sizeof(g_br73));
    g_br73.pHooks        = &g_hooks;
    g_br73.pPhaseVtbl    = &g_phaseVtbl;
    g_br73.pfnClearSub70 = ClearSub70;
    g_br73.pAA29CC       = g_recAA29CC;
    g_br73.aAA26F0       = g_blkA;
    g_br73.aA9DBD8       = g_blkB;
    g_br73.a220B20       = g_blkC;
    g_br73.szAA2518      = g_scratchA;
    g_br73.szA9D618      = g_scratchB;
    g_br73.cbScratch     = sizeof(g_scratchA);

    /* 0x100AB428/0x100AB42C are read with fild -- ints, not floats. The values
     * are the ones slice3_33 recovered; 0x10054B50 uses them as the x/y of its
     * three rectangles. */
    g_br73.n0AB428 = 0;
    g_br73.n0AB42C = 380;

    /* The one-space string the original uses as placeholder TEXT. */
    g_br73.aStyles.p0AD300 = " ";
    g_br73.aStyles.p0AD348 = "RallySeason*.BRF";

    /* The style rectangles. These used to stay NULL on the grounds that the
     * builders only pass them along -- which was true right up until
     * slice3_39.c's list methods were ported, because 0x1005B910 is the first
     * ported function that READS one, four int32s deep. See the pool in
     * slice3_39.h; the values are the image's, not invented. */
    g_br73.aStyles.p0AB438 = BR_UI_STYLE(0x100AB438);
    g_br73.aStyles.p0AB448 = BR_UI_STYLE(0x100AB448);
    g_br73.aStyles.p0AB458 = BR_UI_STYLE(0x100AB458);
    g_br73.aStyles.p0AB468 = BR_UI_STYLE(0x100AB468);
    g_br73.aStyles.p0AB478 = BR_UI_STYLE(0x100AB478);
    g_br73.aStyles.p0AB488 = BR_UI_STYLE(0x100AB488);
    g_br73.aStyles.p0AB4A8 = BR_UI_STYLE(0x100AB4A8);
    g_br73.aStyles.p0AB4D8 = BR_UI_STYLE(0x100AB4D8);
    g_br73.aStyles.p0AB4F8 = BR_UI_STYLE(0x100AB4F8);
    g_br73.aStyles.p0AB508 = BR_UI_STYLE(0x100AB508);
    g_br73.aStyles.p0AB528 = BR_UI_STYLE(0x100AB528);
    g_br73.aStyles.p0AB548 = BR_UI_STYLE(0x100AB548);
}

/* --- reporting ---------------------------------------------------------- */

static int DumpPhase(const BrPhase_ *ph)
{
    int i, total = 0;
    printf("phase %p  nPages=%u iPage=%u f0C=%d f68=%d\n",
           (const void *)ph, (unsigned)ph->nPages, (unsigned)ph->iPage,
           (int)ph->f0C, (int)ph->f68);
    for (i = 0; i < (int)ph->nPages && i < BR_PHASE_PAGES; i++) {
        const BrUiPage_ *pg = ph->aPages[i];
        if (!pg) continue;
        printf("  page %d @%p  cCtl=%u cSel=%u origin=(%.1f,%.1f) flags=%d\n",
               i, (const void *)pg, (unsigned)pg->cCtl, (unsigned)pg->cSel,
               (double)pg->fX, (double)pg->fY, (int)ph->aFlags[i]);
        total += pg->cCtl;
    }
    return total;
}

/* Draw each control's rectangle. The rect comes from the builder (rcLeft/rcTop
 * are the truncated x/y, rcRight = rcLeft+0x7F, rcBottom = rcTop+0x21) -- this
 * file invents no geometry. */
/* Print every control's rectangle and caption.
 *
 * This is the layout the ported builders actually computed -- the coordinates
 * come from 0x10047FB0 (place) and the widths from the text box's measure
 * methods, both decompiled. Nothing here is invented, which is what makes the
 * dump usable as evidence: the numbers can be checked against the original. */
static void DumpRects(const BrPhase_ *ph)
{
    int i, j;
    for (i = 0; i < (int)ph->nPages && i < BR_PHASE_PAGES; i++) {
        const BrUiPage_ *pg = ph->aPages[i];
        if (!pg) continue;
        printf("  page %d  origin=(%.1f,%.1f)  cCtl=%u cSel=%u\n",
               i, (double)pg->fX, (double)pg->fY,
               (unsigned)pg->cCtl, (unsigned)pg->cSel);
        for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
            const BrUiCtl_ *c = pg->apCtl[j];
            if (!c) continue;
            {
                const char *pszCap = CapFor(c);
                char        szChrome[56];
                BrUiChrome  ch;

                /* What 0x10048010 would draw, spelled out, so the claim is
                 * checkable from a terminal and not only from a screenshot. */
                (void)BrUiCtlChrome(c, 640, 480, &ch);
                if (ch.kind == BR_UI_CHROME_SPRITE) {
                    const char *pszArt =
                        (ch.iSprite >= 0 && ch.iSprite < BR_UI_SPR_COUNT &&
                         g_aBrUiSpriteName[ch.iSprite] != NULL)
                        ? g_aBrUiSpriteName[ch.iSprite] : "(unnamed)";
                    snprintf(szChrome, sizeof szChrome, "spr %3d %dx%d %s%s",
                             (int)ch.iSprite, (int)ch.w, (int)ch.h, pszArt,
                             ch.fDown ? " DOWN" : "");
                } else if (ch.kind == BR_UI_CHROME_TEXT) {
                    snprintf(szChrome, sizeof szChrome, "text kind=%d",
                             (int)c->aText[0].f08);
                } else {
                    snprintf(szChrome, sizeof szChrome, "-");
                }
                printf("    [%2d] x=%-5d y=%-5d w=%-4d h=%-3d %-28s %s\n",
                       j, (int)c->rcLeft, (int)c->rcTop,
                       (int)(c->rcRight - c->rcLeft),
                       (int)(c->rcBottom - c->rcTop), szChrome,
                       pszCap ? pszCap : "(no caption)");
            }
        }
    }
}

/* ==========================================================================
 * The frame driver and the scripted-input mode
 * ========================================================================== */

/* ONE FRAME OF ONE PHASE -- and it is the GAME'S frame, not this file's.
 *
 * WHAT THIS USED TO BE, since the change is the point of the function.
 *
 * This was a hand-written reproduction of the PAGE LOOP inside 0x100489A0,
 * and its own comment said so. It walked the phase's pages, wrote pCur before
 * the NULL test and called each page's vtable +0x04, and it left out
 * everything else 0x100489A0 does -- the +0x68 check that lets a screen ask
 * to be dropped, the phase's own +0x04 and +0x08 methods, the DirectInput
 * poll, the current-phase swap around it, 0x10AA2868, and the two failure
 * exits. A driver that reproduces a function is a lookalike: it agrees with
 * the original exactly where someone remembered to make it agree.
 *
 * 0x100489A0 is ported. It was ported twice, in fact -- slice3_32.c has it
 * over that module's byte-image objects and br_uinav.c now has the struct-model
 * twin -- and the only thing keeping it out of this harness was that neither
 * copy had ever been installed in the phase vtable slot it belongs to.
 *
 * So this is now one indirect call through phase vtable +0x0C, which is how
 * the original's own main loop reaches it. Everything above -- the +0x68 test,
 * the page loop, the bail -- is decompiled game logic; see
 * BrUiNavPhaseRun_100489A0. What this FILE still supplies is named in the
 * stand-in table above and in HostPoll, and both are counted and printed.
 *
 * BrOptObjCtor leaves +0x68 = 1, and the builder leaves aFlags[0] = 1 with
 * the rest 0, so page 0 runs and the frame reports success. */
static int NavFrame(BrPhase_ *ph)
{
    return (int)ph->pVtbl->f0C(ph);
}

/* The page a phase is currently showing, or NULL. */
static const BrUiPage_ *NavPage(const BrPhase_ *ph)
{
    if (ph == NULL || ph->nPages == 0)
        return NULL;
    return ph->aPages[0];
}

/* Which control carries the CURRENT bit (+0x20), by index. -1 if none.
 *
 * Read out of the control flags rather than computed, because the whole point
 * is to observe what the ported code did. 0 is a real index and -1 really
 * means "none" -- they are not the same value and are not conflated. */
static int NavCurrentCtl(const BrUiPage_ *pg)
{
    int j;
    if (pg == NULL)
        return -1;
    for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
        const BrUiCtl_ *c = pg->apCtl[j];
        if (c != NULL && ((uint32_t)c->flags1C & 0x20u) != 0)
            return j;
    }
    return -1;
}

/* THE FRAME CLOCK, and why a scripted frame has to take real time.
 *
 * 0x100480A0 (control vtable +0x04, the step timer) is what raises +0x1C bit
 * 0x100, and its gate is SIXTY MILLISECONDS OF WALL CLOCK:
 *
 *     f2974 += now - f2970;   if (f2974 > 0x3C) { f2974 = 0; flags |= 0x100; }
 *
 * Bit 0x100 is the only thing that lets 0x10047360 step a selected row's
 * colour, so without it a menu has no selection feedback at all -- every label
 * sits on the kind byte its builder passed.
 *
 * A scripted frame here takes microseconds, so back-to-back frames can never
 * cross that gate and the pulse never runs. The driver therefore gives each
 * scripted frame a real duration. The number is the GAME'S -- 0x3C, plus one
 * so the strict `>` can be satisfied -- and not one chosen to look right.
 *
 * What this deliberately does NOT model is the game's video frame rate: a
 * real 60 Hz frame is 16 ms and the pulse fires every fourth one. Here one
 * scripted key is one MENU STEP, so it advances the pulse exactly once, which
 * is what makes a screenshot pair reproducible instead of dependent on how
 * fast the machine decoded 138 BMPs. */
#define BR_STEP_MS  (0x3C + 1)

static void NavFrameWait(void)
{
    usleep((useconds_t)BR_STEP_MS * 1000u);
}

static void NavDumpState(const char *pszWhen, const BrPhase_ *ph)
{
    const BrUiPage_ *pg = NavPage(ph);
    int iCur = NavCurrentCtl(pg);
    const char *pszCap = (pg != NULL && iCur >= 0) ? CapFor(pg->apCtl[iCur])
                                                   : NULL;
    printf("  %-14s phase=%p sel=%-3d iSel=%-3d cSel=%-3d current=%-3d %s\n",
           pszWhen, (const void *)ph,
           BrUiNavSelection(&g_nav),
           (pg != NULL) ? (int)pg->iSel : -1,
           (pg != NULL) ? (int)pg->cSel : -1,
           iCur, pszCap ? pszCap : "(none)");
}

/* `-keys <builder> "<script>"` -- drive the ported navigation with no window,
 * no compositor and no human, and dump the state after every key.
 *
 *   d   down       0x100603A0's "down" edge, then one frame
 *   u   up         0x100603A0's "up" edge, then one frame
 *   j   activate   one frame with 0x10AA2AF0 set, which is what makes
 *                  0x10047A60 raise the ACTIVATE bit on the current control
 *                  and 0x10048180 call its +0x08 hook
 *   .   idle       one frame with nothing held
 *
 * Every one of those is ONE frame, because the original moves the selection
 * one step per frame in which a direction is held (0x100AB3DC is the step and
 * the frame is the clock). A script of "ddd" therefore means three frames and
 * three rows, not one jump of three. */
static int NavRunScript(BrPhase_ *ph, const char *pszKeys)
{
    const char *p;
    BrPhase_ *phCur = ph;
    int i;

    printf("scripted input: \"%s\"\n", pszKeys);
    NavDumpState("start", phCur);

    for (p = pszKeys; *p; ++p) {
        BrPhase_ *phBefore = phCur;

        /* The key only ARMS the seam. It is applied by HostPoll, which
         * 0x100489A0 calls from inside the frame at the point the original
         * polls DirectInput -- so the key reaches the menu by the game's own
         * route rather than by this loop reaching past it. */
        switch (*p) {
        case 'd': g_pendDir  = +1; break;
        case 'u': g_pendDir  = -1; break;
        case 'j': g_pendFire =  1; break;
        case '.': break;
        default:
            printf("  unknown key '%c' (use d u j .)\n", *p);
            return 1;
        }

        NavFrameWait();
        (void)NavFrame(phCur);

        g_pendDir = 0;
        g_pendFire = 0;
        BrUiNavSetStep(&g_nav, 0);
        BrUiNavSetActivate(&g_nav, 0);

        /* A hook may have republished the current phase. That IS the
         * transition, and it is the hook's doing, not this loop's. */
        if (g_nav.pAA2904 != NULL)
            phCur = g_nav.pAA2904;

        {
            char szWhen[16];
            szWhen[0] = '\''; szWhen[1] = *p; szWhen[2] = '\''; szWhen[3] = 0;
            NavDumpState(szWhen, phCur);
            if (phCur != phBefore) {
                printf("  ** PHASE CHANGED %p -> %p (a ported +0x08 hook did "
                       "this, not the driver)\n",
                       (const void *)phBefore, (const void *)phCur);
                printf("     the screen now showing:\n");
                DumpRects(phCur);
            }
        }
    }

    printf("\nstand-ins reached (each is a function this port has NOT "
           "transcribed):\n");
    for (i = 0; i < SI_COUNT; i++)
        printf("    %-26s %d\n", g_aStandInName[i], g_nStandIn[i]);
    printf("    %-26s %d   (0x10060260's site, host-injected)\n",
           "input poll", g_nPoll);
    return 0;
}

/* Solid-colour fill, built from 1x1 RGBA textures scaled to the rectangle.
 * The Metal backend only exposes a textured quad, and a 1x1 texture is the
 * standard way to get a flat fill out of one without adding a second
 * pipeline. */
static BrTexture g_texUp;      /* a control showing its aStepId[0] art  */
static BrTexture g_texDown;    /* ... its aStepId[1] art                */
static BrTexture g_texEdge;    /* the one-pixel outline                 */
static int       g_haveFill;

static void FillRect(BrGfx *gfx, BrTexture t, float x, float y,
                     float w, float h)
{
    if (!g_haveFill || !t) return;
    BrGfxDrawTexture(gfx, t, x, y, w, h);
}

/* One texture per SPRITE, not per sheet.
 *
 * A sheet holds many sprites and each table entry names a source rectangle
 * inside it, but the Metal backend only draws whole textures -- it has no
 * source-rect or UV parameter. Rather than widen that interface for the menu's
 * sake, each sprite is CROPPED out of its sheet once at load time. 145 small
 * textures cost less than a general blitter nobody else needs yet.
 *
 * Sheets are cached while cropping so a sheet shared by twenty sprites is
 * decoded once. */
#define BR_SPR_MAX  BR_UI_SPR_COUNT
static BrTexture g_aSprTex[BR_SPR_MAX];
static int       g_cSpr;

static int CropSprite(BrGfx *gfx, const BrBmp *pSheet, const int32_t *pRect,
                      BrTexture *pOut)
{
    int32_t  l = pRect[0], t = pRect[1], r = pRect[2], b = pRect[3];
    int32_t  w, h, y;
    uint8_t *pBuf;

    if (!pSheet->pRgba) return 0;
    /* Clamp to the sheet. A table rect that overruns its art is the original's
     * problem to have; here it must not read out of bounds. */
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > (int32_t)pSheet->w) r = (int32_t)pSheet->w;
    if (b > (int32_t)pSheet->h) b = (int32_t)pSheet->h;
    w = r - l; h = b - t;
    if (w <= 0 || h <= 0) return 0;

    pBuf = (uint8_t *)malloc((size_t)w * h * 4u);
    if (!pBuf) return 0;
    for (y = 0; y < h; y++)
        memcpy(pBuf + (size_t)y * w * 4u,
               pSheet->pRgba + ((size_t)(t + y) * pSheet->w + l) * 4u,
               (size_t)w * 4u);
    *pOut = BrGfxCreateTexture(gfx, (uint32_t)w, (uint32_t)h, pBuf);
    free(pBuf);
    return *pOut != 0;
}

/* ONE CONTROL'S CHROME, at the size and position br_uispr resolved.
 *
 * WHAT THIS IS AND IS NOT. The rectangle is the game's: br_uispr.c walked
 * 0x10048010 -> 0x10047A10 -> 0x10047930 and produced the sprite the frame
 * chose, the sprite's own width and height out of the table at 0x100AB568,
 * and the control's own truncated x/y. What is NOT the game's is the FILL:
 * the sprite is a BMP on the disc and this tree has no copy of it, so the
 * quad is a placeholder standing in for art that does not exist here.
 *
 * The one thing the placeholder is careful to carry is the distinction that
 * matters -- `fDown` is br_uispr's report that 0x10048180 swapped aStepId[1]
 * in for aStepId[0], i.e. that the control is drawing but-maind.bmp instead
 * of but-main.bmp. That comes out of the ported navigation state, not out of
 * a selection index this file keeps for itself. */
static void DrawChrome(BrGfx *gfx, const BrUiChrome *pCh)
{
    float x = (float)pCh->x, y = (float)pCh->y;
    float w = (float)pCh->w, h = (float)pCh->h;

    if (w <= 0.0f || h <= 0.0f) return;

    /* Real art when it was extracted; the placeholder otherwise. The outline
     * below is drawn only for placeholders -- on real art it would be a border
     * the game does not have. */
    if (pCh->iSprite >= 0 && pCh->iSprite < BR_SPR_MAX &&
        g_aSprTex[pCh->iSprite]) {
        BrGfxDrawTexture(gfx, g_aSprTex[pCh->iSprite], x, y, w, h);
        return;
    }
    FillRect(gfx, pCh->fDown ? g_texDown : g_texUp, x, y, w, h);
    /* The outline is what makes the sprite's exact extent readable in a
     * screenshot; without it a 640x480 backdrop and a 127x33 button are just
     * two flat areas and a one-pixel placement error is invisible. */
    FillRect(gfx, g_texEdge, x,         y,         w,    1.0f);
    FillRect(gfx, g_texEdge, x,         y + h - 1, w,    1.0f);
    FillRect(gfx, g_texEdge, x,         y,         1.0f, h);
    FillRect(gfx, g_texEdge, x + w - 1, y,         1.0f, h);
}

/* The real sprite sheets, when they have been extracted; flat placeholders
 * when they have not.
 *
 * The sheets are 24-bit BMPs in IMAGES\ on the retail disc, named by
 * g_aBrUiSpriteName. tools/extract_assets.sh pulls them at build time, per the
 * project's asset policy -- nothing is committed, and a missing sheet must
 * degrade rather than fail. So this loads what it finds and leaves the rest as
 * the outlined quads that were there before: a screenshot without the disc
 * still shows every sprite at the right size in the right place, which is the
 * property worth keeping.
 *
 * The up/down placeholder pair is retained for the same reason it existed --
 * it is what makes the sprite swap at 0x10048180 visible when the art is
 * absent. */
static void MakeChromeTextures(BrGfx *gfx)
{
    static const uint8_t up[4]   = { 0x38, 0x3C, 0x4C, 0xFF };
    static const uint8_t down[4] = { 0x96, 0x78, 0x24, 0xFF };
    static const uint8_t edge[4] = { 0xB4, 0xB8, 0xC4, 0xFF };
    int i;

    g_texUp    = BrGfxCreateTexture(gfx, 1, 1, up);
    g_texDown  = BrGfxCreateTexture(gfx, 1, 1, down);
    g_texEdge  = BrGfxCreateTexture(gfx, 1, 1, edge);
    g_haveFill = (g_texUp != 0 && g_texDown != 0 && g_texEdge != 0);

    {
        BrBmp sheet; int iCached = -1;
        memset(&sheet, 0, sizeof sheet);
        g_cSpr = 0;
        for (i = 0; i < BR_SPR_MAX; i++) {
            const BrUiSprite *pS = BrUiSpriteAt(i);
            const char *pszName;
            char szPath[256];
            g_aSprTex[i] = 0;
            if (!pS) continue;
            if (pS->iImage < 0 || pS->iImage >= BR_UI_SPR_COUNT) continue;
            pszName = g_aBrUiSpriteName[pS->iImage];
            if (!pszName || !*pszName) continue;
            if (pS->iImage != iCached) {
                BrBmpFree(&sheet);
                snprintf(szPath, sizeof szPath, "testdata/images/%s", pszName);
                if (BrBmpLoad(&sheet, szPath) != 0) { iCached = -1; continue; }
                iCached = pS->iImage;
            }
            /* The table entry's bit 0 selects the colour-keyed blit; the key
             * itself is BR_UI_COLOUR_KEY. Applied here rather than in the
             * decoder because it is the TABLE that decides, not the file. */
            if (pS->fBlit & 1) BrBmpApplyKey(&sheet, BR_UI_COLOUR_KEY);
            if (CropSprite(gfx, &sheet, pS->rect, &g_aSprTex[i])) g_cSpr++;
        }
        BrBmpFree(&sheet);
    }
    if (g_cSpr)
        printf("chrome: %d of %d sprites cropped from the disc art\n",
               g_cSpr, BR_SPR_MAX);
    else
        printf("chrome: no art in testdata/images -- drawing placeholders\n");
}

/* Captions, rasterised with the GAME'S OWN MENU FONT and cached as textures.
 *
 * ==========================================================================
 * WHICH FONT, AND WHY IT IS NOT br_font.c's
 * ==========================================================================
 *
 * This used to draw captions with br_font.c, which recovers the DISPLAY-LIST
 * font out of the DLL's .data. That font is the one the software RSP draws
 * in-game; the menus never touch it. A menu caption is one SPRITE PER
 * CHARACTER, blitted out of images\type_gry|wit|mid|yel.bmp, and the sheet is
 * chosen by the text box's kind byte. port/src/br_sprfont.c is that path,
 * ported from 0x1005B2B0 / 0x1005B730 / 0x1005F800; this file supplies the
 * blit those functions call out to, because the original's endpoint
 * (0x10058380 -> 0x10001320) is a 16-bit software blit against a surface this
 * port does not have.
 *
 * So the typeface, the per-character advances, the pen start and the colour
 * are now all the game's. What is still this file's is only the target: the
 * glyphs land in an RGBA buffer that becomes one texture per caption, instead
 * of in a 640x480 16-bit surface.
 *
 * ==========================================================================
 * WHERE A CAPTION GOES, DERIVED RATHER THAN CHOSEN
 * ==========================================================================
 *
 *   0x10047FB0  (place)   stores the builder's x and y in the control's
 *                         +0x3C / +0x40.
 *
 *   0x10047EB0  (setText) copies those into the text box's +0x410 / +0x414,
 *                         zeroes width and height, dispatches the box's
 *                         MEASURE method, and reads height back:
 *                             rcTop    = __ftol(ctl->y)
 *                             rcBottom = rcTop + box->height
 *                         so the control's vertical extent IS the measured
 *                         text, not a box the text sits inside.
 *
 *   0x1005B2B0  (the text box's own draw, vtable +0x10) walks the string and
 *                         hands EVERY glyph the same y -- the box's +0x414,
 *                         unchanged -- as the destination top-left. There is
 *                         no baseline offset anywhere on that path: no
 *                         ascent, no descent, no per-glyph bearing.
 *
 * So the caption's cell TOP is box->y, exactly, and the pen's start is what
 * BrSprFontPenStart_1005B2B0 returns -- the box's +0x410, which
 * BrTextBoxCentreX has centred in the style rectangle. The buffer is placed at
 * (__ftol(penStart), __ftol(box->y)) using the SAME truncation the glyph
 * drawer applies, so glyph i lands on buffer column (x_i - penStart) with no
 * rounding slack anywhere.
 *
 * The old code's two correction terms -- a pen line of (30*scale)/40 and a
 * `scale` taken from the measured height -- are both gone. They existed only
 * to line up the display-list font's baseline, and the sprite font has none.
 *
 * ==========================================================================
 * COLOUR, AND WHY THERE IS NO LONGER A HIGHLIGHT MARKER
 * ==========================================================================
 *
 * The game draws no background behind a selected label. It changes which
 * SHEET the label's glyphs come out of, one byte at control +0x2B64:
 *
 *   0x10048180's not-current tail pins every unselected label to kind 1
 *   (type_wit, white) and its +0x1E20C to 3;
 *
 *   the current control instead gets its +0x0C hook called, which for every
 *   menu row is 0x10047360 (ported in br_sprfont.c) -- and that steps
 *   +0x1E20C once per 60 ms tick and maps it onto kind 0/1/2/4, i.e. onto
 *   type_gry / type_wit / type_mid / type_yel.
 *
 * A caption texture is therefore keyed on (control, kind): the same string
 * needs a different texture when the row's colour changes, and it is built
 * on demand the first time that pair is seen. */
#define BR_CAP_W     512
#define BR_CAP_H      32
#define BR_CAP_MAX   192

/* --- the sheets the sprite font blits out of ----------------------------- */

/* Only six sprite ids can ever reach the blit: 2, 3, 4 and 0x34 are the four
 * type_*.bmp sheets, 5 is bignums.bmp, and 0 is the work1a.bmp the kind
 * fall-through selects (see BrSprFontSheet_1005B730's GOTCHA). They are
 * cached decoded and keyed, because a caption is re-rasterised whenever its
 * row changes colour and decoding a 128x144 BMP per frame would be silly. */
static BrBmp g_aFontSheet[BR_UI_SPR_COUNT];
static char  g_aFontSheetTried[BR_UI_SPR_COUNT];

static const BrBmp *FontSheet(int32_t iSprite)
{
    const BrUiSprite *pS;
    const char *pszName;
    char szPath[256];

    if (iSprite < 0 || iSprite >= BR_UI_SPR_COUNT) return NULL;
    if (g_aFontSheetTried[iSprite]) {
        return g_aFontSheet[iSprite].pRgba ? &g_aFontSheet[iSprite] : NULL;
    }
    g_aFontSheetTried[iSprite] = 1;

    pS = BrUiSpriteAt(iSprite);
    if (!pS || pS->iImage < 0 || pS->iImage >= BR_UI_SPR_COUNT) return NULL;
    pszName = g_aBrUiSpriteName[pS->iImage];
    if (!pszName || !*pszName) return NULL;
    snprintf(szPath, sizeof szPath, "testdata/images/%s", pszName);
    if (BrBmpLoad(&g_aFontSheet[iSprite], szPath) != 0) return NULL;
    /* The key is the table's decision, and it is applied here for the same
     * reason MakeChromeTextures applies it: br_bmp.c decodes, it does not
     * decide. */
    if (pS->fBlit & 1) BrBmpApplyKey(&g_aFontSheet[iSprite], BR_UI_COLOUR_KEY);
    return &g_aFontSheet[iSprite];
}

/* --- the blit 0x10058380 stands in for ----------------------------------- */

typedef struct CapRaster {
    uint8_t *pBuf;          /* BR_CAP_W x BR_CAP_H RGBA                     */
    int32_t  ox, oy;        /* the buffer's top-left in surface coordinates */
} CapRaster;

/* One glyph. The arguments are 0x10058380's, in its order; everything about
 * WHICH pixels move is the game's, and the only thing this adds is the
 * translation into the caption buffer and the clip to its edges.
 *
 * The 640x480 clip is the game's own -- BrUiSprClip is br_uispr.c's port of
 * 0x10001320's first eighteen instructions, and 640x480 is the surface it
 * reads out of 0x10AC5D84. A glyph that the game would have clipped is
 * clipped here by the same arithmetic, before the buffer is considered. */
static void CapBlit(void *pCtx, int32_t x, int32_t y, int32_t iSprite,
                    const int32_t *pRect, int32_t fBlit)
{
    CapRaster    *pR = (CapRaster *)pCtx;
    const BrBmp  *pSheet = FontSheet(iSprite);
    int32_t       w, h, j, i;

    if (!pSheet || !pSheet->pRgba) return;
    if (!BrUiSprClip(x, y, pRect, 640, 480, &w, &h)) return;

    for (j = 0; j < h; j++) {
        int32_t sy = pRect[1] + j;
        int32_t dy = y + j - pR->oy;
        if (dy < 0 || dy >= BR_CAP_H) continue;
        if (sy < 0 || sy >= (int32_t)pSheet->h) continue;
        for (i = 0; i < w; i++) {
            int32_t sx = pRect[0] + i;
            int32_t dx = x + i - pR->ox;
            const uint8_t *pS;
            uint8_t *pD;
            if (dx < 0 || dx >= BR_CAP_W) continue;
            if (sx < 0 || sx >= (int32_t)pSheet->w) continue;
            pS = pSheet->pRgba + ((size_t)sy * pSheet->w + sx) * 4u;
            /* fBlit bit 0 is the keyed copy; BrBmpApplyKey has already turned
             * the key into alpha 0, so the test is on alpha. Without the bit
             * the original copies every texel, key included. */
            if ((fBlit & 1) && pS[3] == 0) continue;
            pD = pR->pBuf + ((size_t)dy * BR_CAP_W + dx) * 4u;
            pD[0] = pS[0]; pD[1] = pS[1]; pD[2] = pS[2]; pD[3] = 0xFF;
        }
    }
}

/* --- the cache ----------------------------------------------------------- */

static struct { BrTexture tex; const void *pOwner; uint8_t kind; float x, y; }
              g_aCapTex[BR_CAP_MAX];
static int    g_cCap;

/* Rasterise one control's caption at the kind its text box is CURRENTLY
 * carrying, and cache the texture against that pair.
 *
 * Whether a control has a caption at all is not this file's judgement: it is
 * BrUiCtlChrome's, i.e. 0x10048010's label arm. A control the page frame
 * skips (the 0x1000-without-0x10 ordinal controls) and a control that draws
 * sprite chrome instead both report something other than BR_UI_CHROME_TEXT
 * and get no caption here, which is exactly which controls reach
 * 0x1005B2B0 in the original. */
static BrTexture CapBuild(BrGfx *gfx, const BrUiCtl_ *pCtl, float *pX, float *pY)
{
    static uint8_t buf[BR_CAP_W * BR_CAP_H * 4];
    BrTextBox  *pBox;
    BrUiChrome  ch;
    CapRaster   ras;
    BrTexture   t;
    const char *psz;
    uint8_t     kind;
    int         i;

    psz = CapFor(pCtl);
    if (!psz || !*psz) return 0;
    if (!BrUiCtlChrome(pCtl, 640, 480, &ch) || ch.kind != BR_UI_CHROME_TEXT)
        return 0;

    /* The box is written by 0x10047EB0 and by the frame's recolour; the cast
     * is because 0x1005B2B0 re-dispatches the centring method, which stores
     * into the box. Idempotent here -- nothing has changed the measured width
     * since setText ran -- but reproduced rather than skipped. */
    pBox = (BrTextBox *)&pCtl->aText[0];
    kind = pBox->f08;

    for (i = 0; i < g_cCap; i++) {
        if (g_aCapTex[i].pOwner == pCtl && g_aCapTex[i].kind == kind) {
            *pX = g_aCapTex[i].x; *pY = g_aCapTex[i].y;
            return g_aCapTex[i].tex;
        }
    }
    if (g_cCap >= BR_CAP_MAX) return 0;

    ras.ox = BrFtolTrunc(BrSprFontPenStart_1005B2B0(pBox));
    ras.oy = BrFtolTrunc(pBox->y);
    ras.pBuf = buf;
    memset(buf, 0, sizeof buf);

    (void)BrSprFontDraw_1005B2B0(pBox, CapBlit, &ras);

    t = BrGfxCreateTexture(gfx, BR_CAP_W, BR_CAP_H, buf);
    if (!t) return 0;
    g_aCapTex[g_cCap].tex    = t;
    g_aCapTex[g_cCap].pOwner = pCtl;
    g_aCapTex[g_cCap].kind   = kind;
    g_aCapTex[g_cCap].x      = (float)ras.ox;
    g_aCapTex[g_cCap].y      = (float)ras.oy;
    *pX = (float)ras.ox; *pY = (float)ras.oy;
    g_cCap++;
    return t;
}

/* Prime every caption on a phase. Kept as a named step because both the
 * screenshot path and the windowed loop want the first frame to be complete;
 * CapBuild is idempotent, so DrawPhase calling it again costs a lookup. */
static void BuildCaptions(BrGfx *gfx, const BrPhase_ *ph)
{
    int i, j;

    for (i = 0; i < (int)ph->nPages && i < BR_PHASE_PAGES; i++) {
        const BrUiPage_ *pg = ph->aPages[i];
        if (!pg) continue;
        for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
            const BrUiCtl_ *c = pg->apCtl[j];
            float x = 0.0f, y = 0.0f;
            if (!c) continue;
            (void)CapBuild(gfx, c, &x, &y);
        }
    }
}

/* Draw the menu the builder produced, one control at a time and in the page's
 * own order -- which is the order 0x10048530 runs them in, so a backdrop
 * control really does end up behind a button placed after it.
 *
 * WHAT IS THE GAME'S AND WHAT IS NOT, stated plainly because this used to be
 * overstated in both directions:
 *
 *   the game's -- which controls draw chrome at all (br_uispr's transcription
 *   of 0x10048010's three arms), which sprite each one is showing, that
 *   sprite's size and position, every caption's cell top and centred pen
 *   start, the typeface, the per-character advances, and WHICH OF THE FOUR
 *   FONT SHEETS each caption comes out of -- which is how the game shows a
 *   selection;
 *
 *   NOT the game's -- the target. The original blits 16-bit pixels into a
 *   640x480 surface; here each sprite is a texture and each caption is a
 *   texture, because the Metal backend draws textured quads. Where a sheet is
 *   missing from testdata/images the chrome degrades to an outlined
 *   placeholder quad and the caption to nothing.
 *
 * There is NO harness-drawn selection marker any more, and there should not
 * be one: the game draws no background behind a selected label, it recolours
 * the label. That recolour is now real -- see the caption section above and
 * br_sprfont.c's transcription of 0x10047360. */
static void DrawPhase(BrGfx *gfx, const BrPhase_ *ph, BrTexture tex, int haveTex)
{
    int i, j;
    (void)tex; (void)haveTex;
    for (i = 0; i < (int)ph->nPages && i < BR_PHASE_PAGES; i++) {
        const BrUiPage_ *pg = ph->aPages[i];
        if (!pg) continue;
        for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
            const BrUiCtl_ *c = pg->apCtl[j];
            BrUiChrome ch;
            BrTexture  cap;
            float      cx = 0.0f, cy = 0.0f;

            if (!c) continue;
            /* 640x480 is the surface 0x10001320 clips against here; it reads
             * the same two numbers out of the surface at 0x10AC5D84. */
            if (BrUiCtlChrome(c, 640, 480, &ch) &&
                ch.kind == BR_UI_CHROME_SPRITE)
                DrawChrome(gfx, &ch);

            /* Built here rather than looked up, because the row's colour can
             * have changed since the last frame and the texture is keyed on
             * it. Already-seen (control, kind) pairs cost a lookup. */
            cap = CapBuild(gfx, c, &cx, &cy);
            if (cap)
                BrGfxDrawTexture(gfx, cap, cx, cy,
                                 (float)BR_CAP_W, (float)BR_CAP_H);
        }
    }
}

/* ==========================================================================
 * `-race <track.trk> <steps> [x y]` -- THE HEADLESS RACE
 *
 * WHAT THIS RUNS, AND WHERE THE SEAM IS
 *
 *   PORTED, and every one of these is decompiled game logic:
 *     br_gamestep.c   0x1002E317 / 0x1002E324, the game-step slot at
 *                     0x106E79F4 and the pump arm that calls it.
 *     br_track.c      0x100311C0, the .TRK loader.
 *     br_collgrid.c   the five header fields 0x1006F720's cell builder reads.
 *     slice6_73.c     0x1006F720 itself -- the cell is built from the track's
 *                     REAL triangles, normals computed by the ported code.
 *     br_phys.c       0x10068070 / 0x100682C0 / 0x10068450, the probes.
 *     br_carphys.c    0x1005A7A0 and its force generators.
 *     br_race.c       0x1005FF00, the lap/gate machine.
 *     br_ai.c         the path ring, for where the grid actually starts.
 *
 *     br_racestep.c   0x10019A70 ITSELF -- the start-light script, the
 *                     freeze, the field loops, the all-finished condition --
 *                     plus 0x10061F60, 0x100623E0, 0x10061430 and 0x1005ECF0.
 *                     BrGameStepId now reports the slot as "0x10019A70 race"
 *                     because the body in it is a port of that function and
 *                     not, as before, a stand-in for it.
 *
 *   THIS FILE'S, and it is now only two things:
 *     - HostCarDrive, standing in for car+0xF08's four unported functions and
 *       running the one that is ported (0x1005A7A0).  Counted by the module,
 *       not by this file.
 *     - mirroring each car's integrated position into the BrDriverCar the
 *       gate machine reads, at the point inside the controller where the
 *       original's physics writes car+0x30.  In the original both are fields
 *       of one 0x2B68 record and no copy exists.
 *
 * THE FIELD.  Sixteen cars in one contiguous array with a parallel array of
 * driver records, and the starting order REVERSED -- slot 0 gets the LAST
 * grid box.  The original's stride is 0x2B68 and cannot be reproduced on
 * LP64 (five pointers inside the rigid body widen), so the array is of a host
 * struct; CONVENTIONS.md's rule is `sizeof`, never the literal.
 * ========================================================================== */

#include "br_carphys.h"
#include "br_collgrid.h"
#include "br_gamestep.h"
#include "br_race.h"
#include "br_racestep.h"
#include "br_ai.h"
#include "br_track.h"
#include <math.h>

#define BR_RACE_FIELD    16
/* 0x10019BD3 sets g_100B2F00 (the DRIVER count) to 0x14 against a car count
 * of 3, so more slots than cars is the original's own shape, not a harness
 * invention.  The slots past the car count are the PHANTOM entrants whose
 * arm br_racestep.c ports. */
#define BR_RACE_PHANTOM   4
#define BR_RACE_SLOTS    (BR_RACE_FIELD + BR_RACE_PHANTOM)

/* Two parallel arrays where the original has one 0x2B68 record.  They were
 * one struct here too until the ported step needed them: 0x1001B18B walks
 * the CAR array on its own stride and br_racestep.c takes a
 * `BrDriverCar *` base, so the two halves cannot be interleaved. */
static BrCarPhys    g_aRacePhys[BR_RACE_FIELD];
static BrDriverCar  g_aRaceCarRec[BR_RACE_FIELD];
static BrDriver     g_aRaceDrv[BR_RACE_SLOTS];
static BrRaceGate   g_aRaceGate[BR_AI_GATE_MAX];
static float        g_raceLapLen;
static int          g_nRaceStep;
static int          g_cRaceCars;
static int          g_cRacePhantom = BR_RACE_PHANTOM;
/* How far past the path root a phantom slot is seeded.  It has to clear
 * gate 0, which on race.trk sits 24 m along the path from the root; see
 * BrRaceSeedPhantom's banner for what a short lead does. */
static float        g_raceLead = 60.0f;
/* How far apart consecutive phantom slots are seeded, `-spread <m>`; 0 means
 * one lap divided by the number of them, i.e. evenly round the ring.  This is
 * a DISPLAY choice and nothing else: seeded a grid's width apart, the slots
 * cross every gate on the same frame and the ladder shows one column
 * repeated, which proves less than four slots at four different gates does.
 * Nothing in the ported code cares. */
static float        g_raceSpread;
/* How far above the surface each grid box starts.  `-drop <m>` on the command
 * line.  The suspension's whole travel is 0.4 and the probe's window is +-2,
 * so anything in (0, 0.4] starts a car already ON its springs and anything
 * larger drops it. */
static float        g_raceDrop = 0.5f;

/* ---- car+0xF08, the controller ------------------------------------------
 *
 * THE ONE SEAM LEFT IN THE RACE, and it is a five-deep chain of which
 * exactly one function is ported:
 *
 *   0x1005D050 / 0x1005E690   the two thunks 0x1001A5CF and 0x1001A60C
 *                             install per slot
 *   0x1005C8B0 / 0x1005D770   the human and AI controllers
 *   0x1006F170                the per-car drive pass, 1295 B
 *   0x1005A7A0                THE PHYSICS -- ported, br_carphys.c
 *
 * So the host stands in for the four unported ones and runs the fifth.
 * br_racestep.c counts every dispatch as BR_RS_HOLE_CONTROL whether or not
 * a body is installed, so the count is the number of times the original
 * would have entered the chain, not the number of times this file did
 * something.
 *
 * Two bodies rather than one, because 0x10061F82 tests the pointer against
 * 0x1005E690 to decide whether to clear the AI's control word: collapsing
 * them would make every slot look like an AI slot. */
static void HostCarDrive(BrDriverCar *pCar)
{
    long i = (long)(pCar - g_aRaceCarRec);
    const BrRbState *pS;

    if (i < 0 || i >= (long)g_cRaceCars)
        return;
    BrCarPhysStep(&g_aRacePhys[i]);
    /* In the original car+0x30 IS the body matrix's row 3 and no copy
     * exists.  Here the physics owns the state, so the two fields the gate
     * machine reads are mirrored at the point the physics wrote them --
     * posPrev is NOT touched, because 0x10061F60 latched it before this
     * ran and that ordering is the whole motion segment. */
    pS = BrCarPhysBodyState(&g_aRacePhys[i].body);
    pCar->pos   = pS->pos;
    pCar->f1030 = BrVec3Length(&pS->vel);
}

static void HostCarDriveAi(BrDriverCar *pCar)    /* stands for 0x1005E690 */
{
    HostCarDrive(pCar);
}

/* header +0x98, BR_AI_GATE_MAX records of BR_AI_GATE_STRIDE, count at +0x160.
 * The swapper reverses every dword in [0x84, 0x164), so both are host order
 * in abHdr and are read as raw dwords and reinterpreted -- never overlaid. */
static int32_t RaceLoadGates(const BrTrack *pTrack)
{
    int32_t n = (int32_t)BrTrackHdrU32(pTrack, BR_TRK_H_CGATES);
    int32_t i;

    if (n < 0) n = 0;
    if (n > (int32_t)BR_AI_GATE_MAX) n = (int32_t)BR_AI_GATE_MAX;

    for (i = 0; i < n; ++i) {
        unsigned off = BR_TRK_H_GATES + (unsigned)i * BR_AI_GATE_STRIDE;
        uint32_t a[5];
        int      k;
        for (k = 0; k < 5; ++k)
            a[k] = BrTrackHdrU32(pTrack, off + (unsigned)k * 4u);
        memcpy(&g_aRaceGate[i].postA.x, &a[0], 4);
        memcpy(&g_aRaceGate[i].postA.y, &a[1], 4);
        memcpy(&g_aRaceGate[i].postB.x, &a[2], 4);
        memcpy(&g_aRaceGate[i].postB.y, &a[3], 4);
        memcpy(&g_aRaceGate[i].tAward,  &a[4], 4);
    }
    return n;
}

/* The start line: the first two points of the AI path ring, which is track
 * header +0x70 (br_ai.h pins that).  pts[0].centre is on the racing line and
 * pts[1] gives the direction, so the grid is laid out along the real track
 * rather than along an axis this file picked. */
static int RaceStartFrame(const BrTrack *pTrack, BrVec3 *pOrigin,
                          float *pYaw, BrVec3 *pFwd, BrVec3 *pRight)
{
    BrAiNode  node;
    BrAiPoint p0, p1;
    float     dx, dy, len;

    if (BrAiRoot(pTrack, &node) != 0) return 1;
    if (BrAiPoint_(&node, 0u, &p0) != 0) return 1;
    if (BrAiPoint_(&node, 1u, &p1) != 0) return 1;

    dx = p1.centre.x - p0.centre.x;
    dy = p1.centre.y - p0.centre.y;
    len = (float)sqrt((double)(dx * dx + dy * dy));
    if (!(len > 1e-4f)) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    dx /= len; dy /= len;

    *pOrigin = p0.centre;
    *pYaw    = (float)atan2((double)dy, (double)dx);
    pFwd->x = dx;  pFwd->y = dy;  pFwd->z = 0.0f;
    pRight->x = dy; pRight->y = -dx; pRight->z = 0.0f;
    return 0;
}

static void RaceDumpHeader(void)
{
    printf("\n step |        position          |         velocity         "
           "|  |w|  | lap gate  wheel-probe (m below wheel)   world-probe\n");
    printf("------+--------------------------+--------------------------"
           "+-------+---------------------------------------------------\n");
}

static void RaceDumpCar(int step, const BrCarPhys *pC, const BrDriver *pD)
{
    const BrRbState *pS =
        BrCarPhysBodyState((BrRbBodyFull *)(void *)&pC->body);
    BrVec3 p = pS->pos;
    float  world;

    /* BrGroundProbeZ is the WORLD-KEYED sibling of the wheel probe: same
     * search, but its grid cell comes from the point it was asked about.
     * Printing both is what makes the wheel probe's grid-key defect visible
     * instead of merely reproduced -- see the note in the run banner. */
    world = BrGroundProbeZ(&p);

    printf("%5d | %8.3f %8.3f %7.3f | %8.3f %8.3f %7.3f | %5.2f | %3d %4d  "
           "%7.2f %7.2f %7.2f %7.2f   %8.2f\n",
           step,
           (double)p.x, (double)p.y, (double)p.z,
           (double)pS->vel.x, (double)pS->vel.y, (double)pS->vel.z,
           sqrt((double)(pS->angVel.x * pS->angVel.x
                         + pS->angVel.y * pS->angVel.y
                         + pS->angVel.z * pS->angVel.z)),
           (int)pD->f40, (int)pD->f4C,
           (double)-pC->wheel[0].f1D8, (double)-pC->wheel[1].f1D8,
           (double)-pC->wheel[2].f1D8, (double)-pC->wheel[3].f1D8,
           (double)world);
}

/* The gate/lap ladder for the WHOLE field, which is the evidence the physics
 * dump above cannot give: a car that is not being driven cannot reach a
 * gate, and a phantom entrant walks the ring by construction. */
static void RaceDumpLadder(int step)
{
    int i;
    printf("%6d |", step);
    for (i = 0; i < g_brRaceNDriver; ++i)
        printf(" %d/%-2d%s", (int)g_aRaceDrv[i].f40, (int)g_aRaceDrv[i].f4C,
               ((g_aRaceDrv[i].f68 & BR_DRIVER_SKIP) != 0) ? "*" : " ");
    printf("\n");
}

/* The surface height under (x, y).  BrGroundProbeZ is a SEGMENT test with a
 * +-2 window, not a ray cast, so one call only answers when the probe point is
 * already within two metres of the ground.  Walking the window down from the
 * top of the track's bounding box is the honest way to find the surface with
 * the ported probe and nothing else. */
static int RaceGroundAt(float x, float y, float zTop, float zBot, float *pZ)
{
    BrVec3 p;
    float  z;

    p.x = x; p.y = y;
    for (z = zTop; z > zBot - 2.0f; z -= 2.0f) {
        float d;
        p.z = z;
        d = BrGroundProbeZ(&p);
        if (d != BR_PHYS_PROBE_MISS) { *pZ = z - d; return 0; }
    }
    return 1;
}

static int RunRace(int argc, char **argv)
{
    BrTrack   trk;
    BrVec3    origin, fwd, right, lo, hi;
    float     yaw = 0.0f, zGround;
    int       nSteps = (argc > 3) ? atoi(argv[3]) : 60;
    int       i, cells = 0, planes = 0;
    int32_t   nGates;

    g_cRaceCars    = BR_RACE_FIELD;
    g_cRacePhantom = BR_RACE_PHANTOM;
    for (i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "-drop") == 0 && i + 1 < argc)
            g_raceDrop = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "-lead") == 0 && i + 1 < argc)
            g_raceLead = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "-spread") == 0 && i + 1 < argc)
            g_raceSpread = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "-cars") == 0 && i + 1 < argc)
            g_cRaceCars = atoi(argv[++i]);
        else if (strcmp(argv[i], "-phantom") == 0 && i + 1 < argc)
            g_cRacePhantom = atoi(argv[++i]);
    }
    if (g_cRaceCars < 0)               g_cRaceCars = 0;
    if (g_cRaceCars > BR_RACE_FIELD)   g_cRaceCars = BR_RACE_FIELD;
    if (g_cRacePhantom < 0)            g_cRacePhantom = 0;
    if (g_cRacePhantom > BR_RACE_SLOTS - g_cRaceCars)
        g_cRacePhantom = BR_RACE_SLOTS - g_cRaceCars;

    if (nSteps < 1)   nSteps = 1;
    if (nSteps > 100000) nSteps = 100000;

    printf("\n=== headless race ===\n");

    if (BrTrackOpen(&trk, argv[2]) != 0) {
        printf("SKIP: cannot open %s (asset policy: nothing is committed, a "
               "missing track SKIPs)\n", argv[2]);
        return 0;
    }
    printf("track %s: %u vertices, %u faces, %u grid items\n",
           argv[2], (unsigned)BrTrackVertexCount(&trk),
           (unsigned)BrTrackFaceCount(&trk),
           (unsigned)BrTrackGridItemCount(&trk));

    if (BrCollGridBind(&trk) != 0) {
        printf("SKIP: the track carries no usable collision tables\n");
        BrTrackClose(&trk);
        return 0;
    }

    nGates = RaceLoadGates(&trk);
    /* The six fields br_race.h gathers, filled where 0x10019A70's one-time
     * arm fills them: 0x100BCBE8 (laps), 0x100A9360 (mode), 0x118EE588
     * (the finishing counter) and the gate ring at 0x106EED70/0x106EEE38. */
    memset(&g_brRaceRules, 0, sizeof g_brRaceRules);
    g_brRaceRules.aGates      = g_aRaceGate;
    g_brRaceRules.nGates      = nGates;
    g_brRaceRules.nLaps       = 3;
    g_brRaceRules.mode        = 6;    /* BrPhaseActivate_100447D0's value */
    g_brRaceRules.nFinished   = 0;
    g_raceLapLen              = BrAiLapLength(&trk);
    g_brRaceRules.pfLapLength = &g_raceLapLen;
    g_pBrRaceTrack            = &trk;
    printf("gate ring: %d gates (header +0x160), lap length %.1f "
           "(path root pts[0].arc)\n", (int)nGates, (double)g_raceLapLen);

    if (RaceStartFrame(&trk, &origin, &yaw, &fwd, &right) != 0) {
        printf("SKIP: the track has no AI path, so there is no start line\n");
        BrCollGridRelease();
        BrTrackClose(&trk);
        return 0;
    }
    printf("start line at (%.1f, %.1f, %.2f), heading %.1f deg\n",
           (double)origin.x, (double)origin.y, (double)origin.z,
           (double)yaw * 57.29578);

    /* Put the grid ON the surface the ported probe finds, not on the path
     * point's own Z -- the racing line runs above the mesh by a metre or so
     * on this track, and the probe's window is only +-2. */
    if (BrTrackBounds(&trk, &lo, &hi) != 0) { lo.z = -100.0f; hi.z = 200.0f; }
    if (RaceGroundAt(origin.x, origin.y, hi.z + 2.0f, lo.z, &zGround) == 0) {
        printf("surface under the start line: z = %.3f "
               "(path point was %.3f, %.3f above it)\n",
               (double)zGround, (double)origin.z,
               (double)(origin.z - zGround));
        origin.z = zGround;
    } else {
        printf("no surface found under the start line; using the path Z\n");
    }
    if (g_brPhysWheelGridWorldKey) {
    }

    /* --- the field ---------------------------------------------------- */
    memset(g_aRacePhys,   0, sizeof g_aRacePhys);
    memset(g_aRaceCarRec, 0, sizeof g_aRaceCarRec);
    memset(g_aRaceDrv,    0, sizeof g_aRaceDrv);

    for (i = 0; i < g_cRaceCars; ++i) {
        /* REVERSED, which is the driver constructor's own ordering: slot 0
         * gets the last box on the grid. */
        int    box = g_cRaceCars - 1 - i;
        int    row = box / 2;
        float  lat = (box & 1) ? 3.0f : -3.0f;
        BrVec3 p;

        p.x = origin.x - fwd.x * (float)row * 7.0f + right.x * lat;
        p.y = origin.y - fwd.y * (float)row * 7.0f + right.y * lat;
        /* Each box gets the surface height under IT, so a sloping grid is a
         * sloping grid rather than sixteen cars dropped from one plane.
         * Half a metre of clearance: the suspension's whole travel is 0.4 and
         * the probe's window is +-2, so this is inside both. */
        if (RaceGroundAt(p.x, p.y, hi.z + 2.0f, lo.z, &p.z) != 0)
            p.z = origin.z;
        p.z += g_raceDrop;

        BrCarPhysInit(&g_aRacePhys[i], NULL);
        /* Everything past the front row is AI in the original (0x10019A70
         * installs 0x1005E690 for every slot past the human count). */
        g_aRacePhys[i].fAi = (i == 0) ? 0 : 1;
        BrCarPhysPlace(&g_aRacePhys[i], &p, yaw);

        g_aRaceDrv[i].pCar = &g_aRaceCarRec[i];
        g_aRaceDrv[i].f64  = i;
        g_aRaceCarRec[i].pos        = p;
        g_aRaceCarRec[i].posPrev    = p;
        g_aRaceCarRec[i].pfnControl = (i == 0) ? HostCarDrive : HostCarDriveAi;
    }

    /* Counted from here, so the seeds below land in the grid hole with the
     * eight 0x1005F310 calls the one-time arm would make. */
    /* 0x100B2F00 / 0x100B2F04 / 0x100B3858, and the AI controller's address
     * the identity test at 0x10061F82 wants.  Set BEFORE the phantom slots,
     * because seeding one runs the ported arm and the arm reads these. */
    g_pBrRaceDriver      = g_aRaceDrv;
    g_pBrRaceCar         = g_aRaceCarRec;
    g_brRaceNDriver      = g_cRaceCars + g_cRacePhantom;
    g_brRaceNCar         = g_cRaceCars;
    g_brRaceNEntrant     = g_cRaceCars;
    g_pfnBrRaceAiControl = HostCarDriveAi;
    g_brRaceStepDt       = BR_PHYS_DT;
    g_brRaceSubstate     = 0;      /* the one-time arm has not run yet */
    g_brRaceTick         = 1;      /* a fixed-timestep host ticks every frame */

    /* The phantom slots.  0x1005F310 -> 0x1005EB90 would derive each one's
     * lap, gate and progress from its distance along the track;
     * BrRaceSeedPhantom does the part that needs no geometry (the slot at
     * the LEAD, gate 0) and the walk below does the rest by running the
     * REAL arm forward -- so a slot that starts a third of a lap round the
     * ring gets there through 0x10061F60 and 0x1005FF00 rather than through
     * a gate index this file made up.  Inventing one is exactly the
     * inconsistency BrRaceSeedPhantom's banner describes, and it shows up
     * as a gate counter that walks backwards for ever. */
    if (!(g_raceSpread > 0.0f) && g_cRacePhantom > 0)
        g_raceSpread = g_raceLapLen / (float)g_cRacePhantom;
    for (i = g_cRaceCars; i < g_cRaceCars + g_cRacePhantom; ++i) {
        g_aRaceDrv[i].pCar = NULL;
        g_aRaceDrv[i].f64  = i;
        if (BrRaceSeedPhantom(&g_aRaceDrv[i], g_raceLead) != 0) {
            printf("  (slot %d: no path to seed a phantom on)\n", i);
            continue;
        }
        {
            float d = g_raceSpread * (float)(i - g_cRaceCars);
            int   n = (int)(d / BR_RS_PHANTOM_STEP);
            while (n-- > 0)
                BrRaceDriverStep(&g_aRaceDrv[i]);
        }
    }
    /* The walk above is placement, not racing: its lap times and finishing
     * order are not the race's. */
    g_brRaceRules.nFinished = 0;

    BrRaceStepHoleReset();

    (void)BrCollGridLoaded(&cells, &planes);
    printf("field: %d cars + %d phantom slots = %d drivers "
           "(0x100B2F00 is 0x14 in the original)\n"
           "       car record %zu B, physics %zu B (one 0x2B68 blob there)\n",
           g_cRaceCars, g_cRacePhantom, g_brRaceNDriver,
           sizeof(BrDriverCar), sizeof(BrCarPhys));
    printf("collision cache after placement: %d of 4 cells hold %d planes\n",
           cells, planes);

    /* --- install the step --------------------------------------------- */
    /* THE PORTED 0x10019A70, and BrGameStepId is now entitled to say so. */
    BrRaceStepInstall();
    printf("game step 0x106E79F4 <- %s\n", BrGameStepName(BrGameStepId()));
    if (BrGameStepPump(0) != -1) {
        printf("  (unexpected: a non-2 engine state claimed to run)\n");
    }

    printf("\nCAR 0, one line per fixed 1/30 s step.\n"
           "'wheel-probe' is -f1D8 per wheel: the distance 0x10068070 found\n"
           "below that wheel, or 100.00 for a miss.  'world-probe' is\n"
           "0x100682C0 at the car's own position -- the SAME search, keyed on\n"
           "the world point instead of on the wheel's body-local mount\n"
           "offset.  When the two disagree, that gap IS the grid-key defect\n"
           "br_phys.h documents, measured rather than described.\n");
    if (g_cRacePhantom > 0)
        printf("\nThe LADDER line under each one is lap/gate for every slot in\n"
               "driver order -- %d cars then %d phantom entrants -- with `*` for\n"
               "the +0x68 bit 0x1005FF00 sets at the flag.  A car cannot reach a\n"
               "gate because nothing writes its drive torque (br_carphys.h); a\n"
               "phantom is walked round the ring by 0x1005ECF0, which is what\n"
               "0x10061F60's carless arm does in the original.\n",
               g_cRaceCars, g_cRacePhantom);
    RaceDumpHeader();

    g_nRaceStep = 0;
    {
        int nEvery = (nSteps / 12) + 1;
        int iLights = -1;

        for (i = 0; i < nSteps; ++i) {
            int rc = BrGameStepPump(BR_GAMESTATE_STEP);
            if (rc != 1) { printf("step slot did not run (rc=%d)\n", rc); break; }
            ++g_nRaceStep;
            if (g_brRaceLights != iLights) {
                printf("      *** step %d: start lights 0x105BC8F8 %d -> %d "
                       "(script entry %d, %.2f s)%s\n",
                       i + 1, iLights, (int)g_brRaceLights,
                       (int)g_brRaceScript, (double)g_brRaceLightT,
                       (g_brRaceLights == BR_RS_LIGHTS_GO) ? "  GREEN" : "");
                iLights = g_brRaceLights;
                RaceDumpHeader();
            }
            if (i < 8 || (i % nEvery) == 0 || i == nSteps - 1) {
                RaceDumpCar(i + 1, &g_aRacePhys[0], &g_aRaceDrv[0]);
                if (g_cRacePhantom > 0)
                    RaceDumpLadder(i + 1);
            }
        }
    }

    (void)BrCollGridLoaded(&cells, &planes);
    printf("\ncollision cache at the end: %d of 4 cells hold %d planes\n",
           cells, planes);
    printf("physics NOT ported, entered as counted no-ops:\n");
    for (i = 0; i < BR_CP_HOLE_COUNT; ++i)
        printf("    %-44s %u\n", BrCarPhysHoleName(i),
               (unsigned)g_aBrCarPhysHole[i]);
    printf("0x10019A70 NOT ported, entered as counted no-ops:\n");
    for (i = 0; i < BR_RS_HOLE_COUNT; ++i)
        printf("    %-44s %u\n", BrRaceStepHoleName(i),
               (unsigned)g_aBrRaceStepHole[i]);

    printf("\nfinal state of the whole field:\n");
    for (i = 0; i < g_brRaceNDriver; ++i) {
        const BrDriver *pD = &g_aRaceDrv[i];
        const char     *pszFin =
            ((pD->f68 & BR_DRIVER_SKIP) != 0) ? "  FINISHED" : "";

        if (pD->pCar != NULL) {
            const BrRbState *pS = BrCarPhysBodyState(&g_aRacePhys[i].body);
            printf("  slot %2d box %2d  car     (%8.2f %8.2f %7.2f)  "
                   "lap %d gate %-3d rank %d%s%s\n",
                   i, g_cRaceCars - 1 - i,
                   (double)pS->pos.x, (double)pS->pos.y, (double)pS->pos.z,
                   (int)pD->f40, (int)pD->f4C, (int)pD->pCar->fFF8,
                   (i == 0) ? "   <- human slot" : "", pszFin);
        } else {
            printf("  slot %2d         phantom (%8.2f %8.2f %7.2f)  "
                   "lap %d gate %-3d rank %d  progress %.1f%s\n",
                   i, (double)pD->f00.x, (double)pD->f00.y, (double)pD->f00.z,
                   (int)pD->f40, (int)pD->f4C, (int)pD->f54,
                   (double)pD->f50, pszFin);
        }
    }
    printf("finishing counter 0x118EE588: %d\n", (int)g_brRaceRules.nFinished);

    BrCollGridRelease();
    BrTrackClose(&trk);
    return 0;
}

int main(int argc, char **argv)
{
    BrPhase_ *ph;
    BrGfx *gfx = NULL;
    BrTexture tex; BrImage img; int haveTex = 0;
    int windowed = (argc > 1 && strcmp(argv[1], "-w") == 0);
    int nCtl, frames = 0;

    printf("Boss Rally -- host boot\n");

    /* `-race` runs BEFORE any menu wiring: it needs none of it, and the
     * whole point of the mode is that the race step is not a phase. */
    if (argc > 2 && strcmp(argv[1], "-race") == 0) {
        return RunRace(argc, argv);
    }

    /* Sound, likewise: no phase, no window, no renderer.  Both modes render
     * to a .wav as well as to the speakers, because the file is the evidence
     * and the speaker is only the demonstration.  See port/include/br_mix.h.
     *
     *   -sfx <group>   one bank entry (name or number)
     *   -sfxrpm [car]  the three engine loops swept from idle to redline
     */
    if (argc > 1 && strcmp(argv[1], "-sfx") == 0) {
        return BrSfxDemoPlay(argc > 2 ? argv[2] : "beep",
                             "build/sfx.wav", BrSfxAqPlay, NULL);
    }
    if (argc > 1 && strcmp(argv[1], "-sfxrpm") == 0) {
        return BrSfxDemoRpmSweep(argc > 2 ? argv[2] : "ce",
                                 "build/sfxrpm.wav", BrSfxAqPlay, NULL);
    }

    printf("phase object: sizeof=%zu, original 0x%X, allocating %zu\n",
           sizeof(BrPhase_), (unsigned)BR_PHASE_ORIG_SIZE,
           (size_t)BR_PHASE_ALLOC_SIZE);

    g_hostCtlVtbl.f34 = HostCtlSetText;
    g_hostCtlVtbl.f38 = HostCtlPlace;
    g_pBrUiCtlVtbl    = &g_hostCtlVtbl;

    /* 0x1005F800. The original runs it once during start-up and every glyph
     * rectangle in the game comes out of it; nothing draws a caption before
     * it has. */
    BrSprFontRectInit_1005F800();

    /* START THE FRAME CLOCK, and it has to be here rather than wherever it
     * first gets asked for.
     *
     * 0x10075020 latches its epoch on its FIRST call -- slice4_50.c seeds
     * g_br18AB130 from the counter when g_br0BBAD4 is still 1, and every
     * later call returns the time SINCE THAT MOMENT. The original's start-up
     * calls it long before any menu is built, so by the time 0x100480A0 ticks
     * a freshly placed control (whose +0x2970 is 0, from the constructor's
     * memset) the delta is the whole of start-up and the 60 ms gate is
     * crossed on the very first frame. That is what puts bit 0x100 on every
     * control before the first paint, and bit 0x100 is what lets 0x10047360
     * colour the selected row.
     *
     * Latch it here and the ordering matches. Leave it to be latched by the
     * first tick instead and that tick's delta is zero BY CONSTRUCTION, so
     * the first frame can never raise the bit -- which is exactly the shape
     * of bug that reads as "the recolour is not ported". */
    (void)BrSub10075020();

    /* Without this the constructed controls have a NULL text-box vtable and
     * 0x10047EB0 skips the measure, leaving width/height 0. */
    g_hostTextBoxVtbl.pfn04 = BrTextBoxMeasureA;
    g_hostTextBoxVtbl.pfn08 = BrTextBoxMeasureB;
    g_hostTextBoxVtbl.pfn28 = BrTextBoxCentreX;
    g_pBrTextBoxVtbl        = &g_hostTextBoxVtbl;

    g_hostTextListVtbl.f10  = BrTextListAddRow;
    g_hostTextListVtbl.f14  = BrTextListConfig;
    g_pBrTextListVtbl       = &g_hostTextListVtbl;

    WireContext();
    /* The navigation wiring replaces two of WireContext's choices (the hook
     * table and the phase vtable), so it runs after it, and the two counting
     * slots are re-planted after it because it owns the rest of the table. */
    WireNav();
    g_hostCtlVtbl.f34 = HostCtlSetText;
    g_hostCtlVtbl.f38 = HostCtlPlace;
    /* 77 first: br_wire72.c seeds its copy of 0x118ABDBC, and the probe that
     * writes that global needs the root this installs. */
    BrHostWire77();
    BrHostWire71();
    BrHostWire72();

    ph = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
    if (!ph) { printf("alloc failed\n"); return 1; }

    /* The real constructor, 0x10048710. */
    if (!BrOptObjCtor(ph)) { printf("BrOptObjCtor returned NULL\n"); return 1; }
    printf("\nafter ctor:\n");
    DumpPhase(ph);

    if (argc > 1 && strcmp(argv[1], "-all") == 0) {
        /* Each builder runs in a FORKED CHILD.
         *
         * Without this, the first builder that dereferences an unwired hook
         * takes the whole process with it, and the run reports one failure no
         * matter how many there are. That is the difference between "it
         * crashes" and "13 of 16 work, these 3 fail and here is where" -- and
         * the second is the entire point of the harness.
         *
         * A crash here is EXPECTED, not exceptional: unported functions are
         * NULL on purpose so they fault loudly rather than silently doing
         * nothing. Isolation lets that stay true without costing coverage. */
        int b, built = 0, crashed = 0;
        printf("\nrunning all %d ported screen builders (each in its own child)\n",
               BR_NBUILDERS);
        fflush(stdout);
        for (b = 0; b < BR_NBUILDERS; b++) {
            pid_t pid = fork();
            if (pid < 0) { printf("  fork failed\n"); continue; }
            if (pid == 0) {
                BrPhase_ *p = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
                int n = 0, i;
                if (!p || !BrOptObjCtor(p)) _exit(2);
                g_nSetText = g_nPlace = g_nCap = 0;
                g_aBuilders[b].pfn(p);
                for (i = 0; i < (int)p->nPages && i < BR_PHASE_PAGES; i++)
                    if (p->aPages[i]) n += p->aPages[i]->cCtl;
                if (g_aBuilders[b].iModel == 71)
                    printf("  %-22s pages=%-2u ctl=%-3s setText=%-3d place=%-3d"
                           "  [ctl unreadable: model %d]\n",
                           g_aBuilders[b].pszName, (unsigned)p->nPages, "--",
                           g_nSetText, g_nPlace, g_aBuilders[b].iModel);
                else
                    printf("  %-22s pages=%-2u ctl=%-3d setText=%-3d place=%-3d\n",
                           g_aBuilders[b].pszName, (unsigned)p->nPages, n,
                           g_nSetText, g_nPlace);
                {
                    int k;
                    for (k = 0; k < g_nCap; k++)
                        printf("        %-22s \"%s\"\n", "", g_aCap[k]);
                }
                fflush(stdout);
                _exit(0);
            } else {
                int st = 0;
                waitpid(pid, &st, 0);
                if (WIFSIGNALED(st)) {
                    printf("  %-22s CRASHED (signal %d)\n",
                           g_aBuilders[b].pszName, WTERMSIG(st));
                    crashed++;
                } else if (WEXITSTATUS(st) != 0) {
                    printf("  %-22s failed to construct (exit %d)\n",
                           g_aBuilders[b].pszName, WEXITSTATUS(st));
                    crashed++;
                } else {
                    built++;
                }
                fflush(stdout);
            }
        }
        printf("\n%d/%d builders ran clean, %d crashed\n",
               built, BR_NBUILDERS, crashed);
        /* TYPE-MODEL WARNING -- read the ctl column with care.
         *
         * setText and place are COUNTED by this file's vtable slots, so the
         * two counts are measured directly and are trustworthy for every
         * builder.
         *
         * Their EFFECTS used to be suspect: the slots delegate to br_uivt.c,
         * which was typed over slice6_73.h's BrUiCtl_, while slice6_71's
         * BrUiCtlX and slice6_72's BrUi72Ctl put the same original fields at
         * different host offsets (BrUi72Ctl had a pfn18 the others lacked and
         * nested the item block). The writes landed in the wrong members --
         * in bounds, since every model allocated at least the original's
         * 0x1E214, but meaningless. Merging the control models is what fixed
         * it, and br_ui.h is that merge: br_uivt.c and every packet are typed
         * over the one BrUiCtl_ now, so the rectangles DrawPhase renders are
         * real for every builder.
         *
         * ctl is read out of the page struct through slice6_73.h's
         * BrUiPage_, and that is only correct for builders whose module uses
         * the same model. slice3_33.h's BrUiScreen -- which slice6_71's
         * builders allocate and write -- begins at +0x10 and has NO pVtbl,
         * pfn04 or pfn08. The two views are shifted relative to each other, so
         * reading one through the other lands in the wrong field entirely.
         *
         * That is why 0x10049F40 and 0x10051D30 both report ctl=7 when their
         * disassembly says 4 and 3: same wrong offset, same garbage. Their
         * place counts (4 and 3) DO match, because those bypass the struct.
         *
         * Do not "fix" this by trusting the larger number. It is the page and
         * control type models that need merging, the way br_phase.h merged the
         * phase models. */
        printf("\nNOTE: the ctl column is read through slice6_73.h's page model\n"
               "      and is only valid for builders that use it. The setText/\n"
               "      place COUNTS are measured directly and are valid for all,\n"
               "      but what those calls WRITE is only valid for packet 73 --\n"
               "      see the TYPE-MODEL WARNING in port/host/brally.c.\n");
        printf("(a crash means an unported callee is still NULL -- that is the\n"
               " next thing to port, not a defect in the builder)\n");
        return 0;
    }

    /* `-b <n>` runs ONE builder in this process, no fork. -all's isolation is
     * what makes the survey trustworthy and is also what makes a crash
     * undebuggable: the child dies, the parent prints "signal 11" and moves
     * on, and there is no process left to attach to. This runs the same
     * builder in the foreground so a debugger sees the fault. */

    /* `-shot <n> <file.ppm>` renders builder n OFFSCREEN and writes the frame.
     *
     * This exists because "the window shows text" is not something a terminal
     * session can check, and an unverified rendering claim is worth nothing.
     * Offscreen means no window server, no compositor and no user present, so
     * it also runs in CI. The readback is the same buffer the window would
     * present. */
    if (argc > 3 && strcmp(argv[1], "-shot") == 0) {
        int b = atoi(argv[2]);
        BrGfx *g;
        uint8_t *px;
        FILE *fh;
        int32_t x, y, lit = 0;
        const int32_t W = 640, H = 480;

        if (b < 0 || b >= BR_NBUILDERS) {
            printf("builder index must be 0..%d\n", BR_NBUILDERS - 1);
            return 1;
        }
        g = BrGfxCreate(W, H);
        if (!g) { printf("gfx init failed: %s\n", BrGfxLastError()); return 1; }
        MakeChromeTextures(g);
        g_aBuilders[b].pfn(ph);
        g_nav.pAA2904 = ph;
        /* An OPTIONAL fourth argument is a key script, run before the
         * capture: `-shot 4 out.ppm dd` screenshots the third row selected.
         * Without it a single idle frame still runs, because the highlight is
         * the CURRENT bit and no control carries it until a frame has been
         * through 0x10047A60 -- a screenshot taken before that would show the
         * layout with nothing selected and look like a regression. */
        {
            const char *pszKeys = (argc > 4) ? argv[4] : ".";
            const char *pk;
            BrPhase_ *phShot = ph;
            for (pk = pszKeys; *pk; ++pk) {
                /* Armed here, applied by HostPoll from inside the frame --
                 * see NavRunScript. */
                if (*pk == 'd') g_pendDir = +1;
                else if (*pk == 'u') g_pendDir = -1;
                else if (*pk == 'j') g_pendFire = 1;
                NavFrameWait();
                (void)NavFrame(phShot);
                g_pendDir = 0;
                g_pendFire = 0;
                BrUiNavSetStep(&g_nav, 0);
                BrUiNavSetActivate(&g_nav, 0);
                if (g_nav.pAA2904 != NULL) phShot = g_nav.pAA2904;
            }
            ph = phShot;
        }
        BuildCaptions(g, ph);
        BrGfxBeginFrame(g, 0.06f, 0.06f, 0.09f, 1.0f);
        DrawPhase(g, ph, 0, 0);
        BrGfxEndFrame(g);

        px = (uint8_t *)calloc((size_t)W * H, 4);
        if (!px || BrGfxReadPixels(g, px) != 0) {
            printf("readback failed\n"); return 1;
        }
        /* Count pixels that are neither the clear colour nor black. This is
         * the actual evidence: a frame that drew nothing reads back uniform. */
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                const uint8_t *p = px + ((size_t)y * W + x) * 4;
                if (p[0] > 40 || p[1] > 40 || p[2] > 40) lit++;
            }
        fh = fopen(argv[3], "wb");
        if (fh) {
            fprintf(fh, "P6\n%d %d\n255\n", W, H);
            for (y = 0; y < H; y++)
                for (x = 0; x < W; x++) {
                    const uint8_t *p = px + ((size_t)y * W + x) * 4;
                    fwrite(p, 1, 3, fh);
                }
            fclose(fh);
        }
        printf("%s: %d lit pixels of %d -> %s\n",
               g_aBuilders[b].pszName, lit, W * H, argv[3]);
        BrGfxDestroy(g);
        return 0;
    }

    /* `-keys <n> "<script>"` -- the navigation evidence. Headless, so it runs
     * in CI, and it prints the selection state before and after every key. */
    if (argc > 3 && strcmp(argv[1], "-keys") == 0) {
        int b = atoi(argv[2]);
        int rc;
        if (b < 0 || b >= BR_NBUILDERS) {
            printf("builder index must be 0..%d\n", BR_NBUILDERS - 1);
            return 1;
        }
        printf("\nbuilding %s, then driving it with scripted keys\n",
               g_aBuilders[b].pszName);
        g_aBuilders[b].pfn(ph);
        g_nav.pAA2904 = ph;
        nCtl = DumpPhase(ph);
        DumpRects(ph);
        printf("\ncontrols built: %d\n\n", nCtl);
        rc = NavRunScript(ph, argv[3]);
        BrStubReport();
        return rc;
    }

    if (argc > 2 && strcmp(argv[1], "-b") == 0) {
        int b = atoi(argv[2]);
        if (b < 0 || b >= BR_NBUILDERS) {
            printf("builder index must be 0..%d\n", BR_NBUILDERS - 1);
            return 1;
        }
        printf("\nrunning builder %s in-process ...\n", g_aBuilders[b].pszName);
        fflush(stdout);
        g_aBuilders[b].pfn(ph);
        nCtl = DumpPhase(ph);
        DumpRects(ph);
        printf("\ncontrols built: %d   setText=%d place=%d\n",
               nCtl, g_nSetText, g_nPlace);
        BrStubReport();
        return 0;
    }

    /* A real screen builder, 0x1004D640 (7 controls per packet 73). */
    printf("\nrunning builder 0x1004D640 ...\n");
    BrExt_1004D640(ph);
    nCtl = DumpPhase(ph);
    DumpRects(ph);
    printf("\ncontrols built: %d   setText=%d place=%d\n",
           nCtl, g_nSetText, g_nPlace);
    if (g_lastText) printf("last text passed: \"%s\"\n", g_lastText);

    if (windowed) {
        gfx = BrGfxCreate(640, 480);
        if (!gfx) { printf("gfx init failed: %s\n", BrGfxLastError()); }
        else if (BrGfxOpenWindow(gfx, "Boss Rally") != 0) { gfx = NULL; }
        if (gfx) {
            MakeChromeTextures(gfx);
            BuildCaptions(gfx, ph);
        }
        if (gfx && BrImgLoad(&img, "testdata/splash.img") == 0) {
            tex = BrGfxCreateTexture(gfx, img.width, img.height, img.pixels);
            BrImgFree(&img); haveTex = 1;
        }
        /* The interactive loop. Every key is turned into one of the four
         * verbs by the backend and into one of the two seam writes here; the
         * frame itself is the ported 0x10048530 reached through NavFrame.
         * Nothing between the key and the selection is this file's logic. */
        {
            BrPhase_ *phCur = ph;
            /* HARNESS-ONLY builder paging, restored -- see BrKey in br_gfx.h.
             * `[` and `]` re-run a different one of the sixteen builders onto
             * the live phase, so any screen can be reached without navigating
             * to it. This is the harness's, not the game's: the retail title
             * has no such key and no way to jump between screens. */
            int iBuilder = -1;
            g_nav.pAA2904 = ph;
            while (gfx && BrGfxPumpEvents(gfx)) {
                BrKey k;
                BrPhase_ *phBefore = phCur;
                int fire = 0, dir = 0;

                while ((k = BrGfxPollKey(gfx)) != BR_KEY_NONE) {
                    switch (k) {
                    case BR_KEY_UP:       dir  = -1; break;
                    case BR_KEY_DOWN:     dir  = +1; break;
                    case BR_KEY_ACTIVATE: fire =  1; break;
                    /* ESCAPE is BACK. There is no ported "escape" path -- the
                     * original's back is a control's +0x08 hook, not a global
                     * key -- so this activates the LAST selectable control,
                     * which is where every one of the sixteen screens puts
                     * its Back row. Stated rather than dressed up: this one
                     * mapping is the harness's, not the game's. */
                    case BR_KEY_BACK:
                        if (NavPage(phCur) != NULL)
                            g_scr.wAA286C =
                                (uint16_t)(NavPage(phCur)->cSel - 1);
                        fire = 1;
                        break;
                    /* Paging rebuilds the phase in place, so it must not
                     * also feed a verb into the frame below -- the rebuilt
                     * page would receive a stale up/down against a control
                     * list that no longer exists. */
                    case BR_KEY_PREV_SCREEN:
                    case BR_KEY_NEXT_SCREEN:
                        iBuilder += (k == BR_KEY_NEXT_SCREEN) ? 1 : -1;
                        if (iBuilder < 0)             iBuilder = BR_NBUILDERS - 1;
                        if (iBuilder >= BR_NBUILDERS) iBuilder = 0;
                        printf("[harness] builder %d/%d: %s\n",
                               iBuilder, BR_NBUILDERS - 1,
                               g_aBuilders[iBuilder].pszName);
                        fflush(stdout);
                        g_aBuilders[iBuilder].pfn(phCur);
                        g_cCap = 0;
                        BuildCaptions(gfx, phCur);
                        dir = 0; fire = 0;
                        break;
                    default: break;
                    }
                }

                g_pendDir  = dir;
                g_pendFire = fire;
                (void)NavFrame(phCur);
                g_pendDir  = 0;
                g_pendFire = 0;
                BrUiNavSetStep(&g_nav, 0);
                BrUiNavSetActivate(&g_nav, 0);
                if (g_nav.pAA2904 != NULL) phCur = g_nav.pAA2904;

                if (phCur != phBefore) {
                    g_cCap = 0;              /* the old screen's textures */
                    BuildCaptions(gfx, phCur);
                    NavDumpState("transition", phCur);
                }

                BrGfxBeginFrame(gfx, 0.06f, 0.06f, 0.09f, 1.0f);
                DrawPhase(gfx, phCur, tex, haveTex);
                BrGfxEndFrame(gfx);
                BrGfxPresent(gfx);
                if (++frames >= 100000) break;
            }
        }
        if (gfx) BrGfxDestroy(gfx);
    }

    BrStubReport();
    return 0;
}
