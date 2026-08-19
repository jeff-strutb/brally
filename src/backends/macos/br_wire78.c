/* br_wire78.c -- THE MAIN MENU'S TEN ACTION HOOKS, and the phase-activate
 * state they run on.
 *
 * ==========================================================================
 * WHAT THIS IS
 * ==========================================================================
 *
 * br_uiroot.c is Glide 0x100425E0, the root phase's pfnEnter: it builds the
 * main menu page and stores ten hook addresses into its controls.  It reaches
 * those ten through br_uiroot.h's BrUiRootHooks table, and until now NOTHING
 * outside port/tests filled that table -- so every row of the main menu
 * activated into a NULL slot.
 *
 * This file supplies the ten function pointers.  br_wire79.c publishes them
 * into g_brUiRoot; the split is not cosmetic, see WHY TWO FILES below.
 *
 * ==========================================================================
 * WHY THESE ARE ADAPTERS AND NOT DIRECT INSTALLS
 * ==========================================================================
 *
 * A control hook slot is br_ui.h's BrUiCtlHookFn_, `int32_t (*)(BrUiCtl_ *)`.
 * Eight of the ten bodies already exist in this tree and carry FOUR different
 * host signatures, none of them that one:
 *
 *   int  BrPhaseActivate_10045900(void)          slice3_31.c
 *   int  BrPhaseActivate_10046170(void)          slice3_31.c
 *   void BrSub10043BF0(BrGameObj *)              slice4_50.c
 *   void BrMenuSub10044B90(int32_t)              slice4_53.c
 *   int  BrPhaseActivate_10044F50(BrPhaseCtx *)  slice2_26.c
 *   int  BrPhaseActivate_100451E0(BrPhaseCtx *)  slice2_26.c
 *
 * br_uiroot.h says exactly this and is why the table exists at all: a table
 * creates no new name for an address and contradicts no existing declaration.
 * The remaining two go in unadapted -- BrUiCreditsAction_1003AED0 and
 * BrSprFontKindHook_10047360 are already control-typed.
 *
 * IGNORING THE ARGUMENT IS VERIFIED, NOT ASSUMED.  Six of these originals are
 * `int (void)` in effect, and that was read out of BRGlide.dll rather than
 * taken from a port's parameter list -- which is the check slice8_84.h insists
 * on for the same family.  Each of 0x1003ED90, 0x1003D140, 0x1003E0E0,
 * 0x1003E4A0, 0x1003E730 and 0x1003F610 opens with the same 16-byte MSVC EH
 * prologue
 *
 *     push -1 / push <handler> / push fs:[0] / push ecx      esp = E-0x10
 *
 * and EVERY `mov ecx,[esp+4]` in all six is at E-0x0C -- the saved fs:[0]
 * link, feeding `mov fs:[0],ecx`.  Not one of them reads E+4, which is where
 * the control pointer sits.  A displacement is meaningless without its esp
 * (CONVENTIONS.md), and here the same displacement names the unlink slot in
 * all six functions.
 *
 * ==========================================================================
 * THE RETURN VALUE, AND WHERE IT HAS ALREADY BEEN LOST
 * ==========================================================================
 *
 * 0x10048180 stops the page frame when a +0x08 hook returns 0, so the value is
 * load-bearing.  All six originals return 0 on at least one arm:
 *
 *   0x1003ED90  `xor eax,eax` at 0x1003EE77 -- the CD-absent arm; and again
 *               when the phase allocation failed
 *   0x1003D140 / 0x1003E0E0 / 0x1003E4A0 / 0x1003E730 / 0x1003F610
 *               0 when the phase allocation failed
 *
 * The two ported bodies that are typed `void` -- BrSub10043BF0 and
 * BrMenuSub10044B90 -- have ALREADY discarded that distinction before this
 * file can see it, so their adapters return 1.  That is a statement about the
 * existing ports, not a value invented here: on every path that does not fail
 * an allocation the original returns 1, and an allocation failure in this
 * harness would have faulted long before reaching the test.  It is recorded
 * rather than papered over because it is a real, if narrow, divergence.
 *
 * ==========================================================================
 * MEASURED ONCE THE ROWS WERE LIVE: "QUIT" CANNOT BE SELECTED BY PRESSING
 * DOWN, AND THAT IS THE ORIGINAL'S BEHAVIOUR
 * ==========================================================================
 *
 * br_uiroot.h's OBSERVATION 2 records that the "Credits" row writes
 * `aChild[0] = cCtl + 1` like the five rows above it, but the control created
 * after it is the "Quit" ROW rather than a highlight -- so selecting Credits
 * runs Quit's frame method a second time.  It then judges that "an extra f0C
 * on a row that is about to be redrawn anyway is not observably broken".
 *
 * IT IS OBSERVABLE, and this is what running the menu found:
 *
 *     ./build/brally -keys root ".dddddd"
 *       'd'  sel=5  current=12  Credits
 *       'd'  sel=6  current=-1  (none)        <- and it stays there for ever
 *     ./build/brally -keys root ".u"
 *       'u'  sel=6  current=13  Quit          <- the wrap-around works
 *
 * The mechanism is the ORDINAL COUNTER, 0x10AC5BC8 (D3D wAA2870), which the
 * control's +0x20 method bumps once per control it is dispatched on and which
 * the page frame compares against the selection cursor.  In BRGlide.dll's page
 * frame 0x10041980 the child dispatch is
 *
 *     10041A8B  movsx eax, word ptr [ebp]              ; aChild[k]
 *     10041A8F  mov   ecx, [ebx + eax*4 + 0x18]        ; page->apCtl[...]
 *     10041A95  call  dword ptr [edx + 0xc]            ; the CONTROL FRAME
 *
 * -- vtable +0x0C, the same method the main walk uses, so the child's +0x20
 * runs and the counter advances.  Credits' child IS Quit, so on the frame
 * where the cursor reaches 6 the counter is already 7 by the time the walk
 * reaches Quit itself, and no control matches.  The page's selection record at
 * +0xBC is only written for a control that HAS the current bit
 * (`mov [eax+0xbc], bp` at 0x10041AE9, under `test al,0x20` at 0x10041AC3), so
 * it stays on Credits and the same thing happens every frame afterwards.
 *
 * It is a dead spot for the KEYBOARD only.  Parking the pointer over the row
 * takes the other branch of 0x10047A60 -- the control's own rectangle hit test
 * rather than the ordinal compare -- and Quit becomes current immediately:
 *
 *     BR_CURSOR=200,245 ./build/brally -keys root ".dddddd"
 *       'd'  sel=6  current=13  Quit
 *
 * so the shipped game is not unusable, and that is probably why it shipped.
 * Recorded here rather than in br_uiroot.h because the transcription is
 * faithful -- the finding is about what the original DOES, not about the port.
 *
 * ==========================================================================
 * WHY TWO FILES
 * ==========================================================================
 *
 * br_uiroot.h pulls slice1_06.h and slice3_31.h pulls slice2_25.h, and those
 * two headers define INCOMPATIBLE `BrDPlayVtbl` structs -- the exact clash
 * CONVENTIONS.md's "COM/vtable types are named for the interface" rule exists
 * to prevent, still live in the tree.  So the table's contents and the table
 * itself cannot be written in one translation unit, and br_wire79.c declares
 * these ten entry points locally instead of including a conflicting header.
 * That is the same move br_wire71.c documents for the same reason.
 *
 * ==========================================================================
 * ALIASED STORAGE, REPORTED AND NOT INTRODUCED HERE
 * ==========================================================================
 *
 * 0x10AA2904, "the current phase", USED TO HAVE several host objects in this
 * tree, all of them `BrPhase_ *`.  They are now ONE, and this file's
 * BrHostRootActionReport prints the slot each name resolves to so a run can
 * show it rather than a comment claiming it.  What they were:
 *
 *   br_uinav.h   BrUiNav::pAA2904     -- port/host/brally.c's g_nav; this is
 *                                        the one the host's frame loop reads,
 *                                        and slice8_85.c writes it
 *   slice2_26.h  BrPhaseCtx::pAA2904  -- had no instance anywhere until this
 *                                        file; slice3_31.c writes it through
 *                                        g_pBase and slice2_26.c through its
 *                                        argument
 *   slice2_25.c  g_brPAA2904          -- real storage, written by slice2_25.c
 *                                        AND by slice4_50.c's BrOptInstall
 *
 * This file still hands slice2_26.c, slice3_31.c and slice4_53.c the ONE
 * BrPhaseCtx below, which is what the original has -- one set of globals.
 *
 * The current-phase slot itself is no longer among that struct's members, and
 * it is no longer slice2_25.c's either.  port/include/br_phasecur.h owns it,
 * port/host/brally.c binds it to `g_nav.pAA2904`, and the three ranges reach
 * it as `BR_PHASE_CUR` / `g_brPAA2904`.  So a row whose action publishes a
 * transition now moves the same dword the frame loop reads.  That was the
 * whole of "the menu appears inert": the transitions were happening.
 */

/* This order is the one that compiles.  slice3_31.h brings slice2_25.h and
 * slice2_26.h with it; slice4_53.h and slice4_50.h agree with both. */
#include "slice3_31.h"    /* BrPhaseCtx31, BrPhase31SetCtx, the two activates */
#include "slice2_26.h"    /* BrPhaseCtx, BrPhaseActivate_10044F50 / _100451E0 */
#include "slice4_53.h"    /* BrMenuSub10044B90, BrSlice4SetPhaseCtx           */
#include "slice4_50.h"    /* BrSub10043BF0, g_brOptEnterHooks, g_brP0AD300    */
#include "br_sprfont.h"   /* BrSprFontKindHook_10047360 -- 0x10047360         */
#include "br_uicredits.h" /* BrUiCreditsAction_1003AED0 -- Glide 0x1003AED0   */
#include "slice2_17.h"    /* BrS17GetState -- 0x106805B8, the cinematic index */
#include "slice2_25.h"    /* g_brAA289C    -- 0x10AA289C                      */

#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * The two context blocks.
 *
 * ONE BrPhaseCtx, shared by three modules, because the original has one set of
 * globals and three ranges of code that read them.  slice3_31.c takes it as
 * `g_pBase`, slice4_53.c as `g_pBrSlice4PhaseCtx`, and slice2_26.c takes it as
 * an explicit argument -- which is why the two adapters below pass it by hand.
 * ========================================================================== */

static BrPhaseCtx   g_phaseBase;
static BrPhaseCtx31 g_phaseExt;

/* 0x100AD300 (Glide 0x100ACAD8) -- the one-space string in .data that every
 * activate routine hands to SetStatusText, and that br_uiroot.c hands to the
 * status-line control.  port/host/brally.c already gives slice6_73 the same
 * literal for the same address. */
static const char g_szAD300[] = " ";

/* br_wire79.c binds g_brUiRoot.pszStatus to THIS object rather than to a
 * literal of its own: br_uicredits.h says in as many words that its pszStatus
 * and br_uiroot.h's are the same original buffer and "a host must bind both to
 * one pointer".  Two string literals would be two objects for one address --
 * the aliased-storage bug in miniature. */
/* Defined in port/host/brally.c -- the field 0x10AA2904 actually lives in.
 * Declared locally for the same reason the ten entry points are: brally.c's
 * headers and this file's cannot share a translation unit (WHY TWO FILES). */
BrPhase_ **BrHostNavCurSlot(void);

const char *BrHostStatusText(void);
const char *BrHostStatusText(void) { return g_szAD300; }

/* ==========================================================================
 * The seven +0x08 ACTION hooks, in the page's row order
 * ========================================================================== */

/* Row 0 "Championship" -- Glide 0x1003ED90, D3D 0x10045900.
 *
 * Opens the championship screen (enter hook 0x1004F2B0, ported), but only
 * after the CD check 0x10045A00 passes; when it does not, the original puts
 * string 0xD on the status line and returns 0 without opening anything.
 * 0x10045A00 is still a stub in this tree, so the refusal arm is the one this
 * harness takes -- which is the truthful answer for a machine with no disc in
 * it, not a failure of the wiring. */
int32_t BrHostRootAct_1003ED90(BrUiCtl_ *pCtl)
{
    (void)pCtl;                     /* E+4 is never read -- see the banner */
    return (int32_t)BrPhaseActivate_10045900();
}

/* Row 1 "Multiplayer" -- Glide 0x1003D140, D3D 0x10043BF0.
 * Enter hook 0x100563E0, which has no body in this tree. */
int32_t BrHostRootAct_1003D140(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    BrSub10043BF0(NULL);            /* the port takes and ignores a pointer */
    return 1;                       /* the port's `void` lost the 0 arm     */
}

/* Row 2 "Time Attack" -- Glide 0x1003E0E0, D3D 0x10044B90.
 *
 * BrMenuSub10044B90 is slice4_53.c's forwarder onto slice2_26.c's
 * BrPhaseActivate_10044B90; it needs BrSlice4SetPhaseCtx, done below.  Enter
 * hook 0x10059760, which IS ported (slice6_72.c).
 *
 * DO NOT read 0x1003E0E0 as a D3D address: tools/whereis.py reports it
 * AMBIGUOUS and the D3D function of that number is a different one. */
int32_t BrHostRootAct_1003E0E0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    BrMenuSub10044B90(0);           /* the argument does not exist in the
                                     * original; slice4_53.c says so       */
    return 1;                       /* the port's `void` lost the 0 arm    */
}

/* Row 3 "Quick Race" -- Glide 0x1003E4A0, D3D 0x10044F50.
 * Enter hook 0x1004B430 -- see the BLOCKED note in br_wire79.c. */
int32_t BrHostRootAct_1003E4A0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrPhaseActivate_10044F50(&g_phaseBase);
}

/* Row 4 "Options" -- Glide 0x1003E730, D3D 0x100451E0.
 * Enter hook 0x1004BDC0 -- likewise. */
int32_t BrHostRootAct_1003E730(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrPhaseActivate_100451E0(&g_phaseBase);
}

/* Row 5 "Credits" -- Glide 0x1003AED0.  NO ADAPTER: br_uicredits.c is already
 * typed over BrUiCtl_, and it is the one row that READS the control (it takes
 * pCtl->pOwner and tears the front end down through the phase vtable's
 * +0x18).  Installed directly by br_wire79.c, which includes
 * br_uicredits.h itself. */

/* Row 6 "Quit" -- Glide 0x1003F610, D3D 0x10046170.
 * Enter hook 0x10049C20, which has no body in this tree. */
int32_t BrHostRootAct_1003F610(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrPhaseActivate_10046170();
}

/* ==========================================================================
 * The three +0x0C hooks -- the per-frame caption setters, NOT actions
 *
 * 0x100407B0 is the base and the other two wrap it, both nineteen or
 * twenty-four bytes and both read out of BRGlide.dll for this file:
 *
 *   10040AF0  mov eax,[esp+4] / push eax / call 0x100407B0 / add esp,4
 *             mov eax,1 / ret
 *   10040A20  call 0x10040A40 / mov eax,[esp+4] / push eax
 *             call 0x100407B0 / add esp,4 / mov eax,1 / ret
 *
 * So both PASS THE CONTROL THROUGH and both return 1 unconditionally.
 *
 * NOT MARKED @implements, deliberately.  Their D3D twins 0x100474B0 and
 * 0x100475F0 are already claimed by slice3_31.c, whose bodies byte-address the
 * control at the original's 32-bit offsets and therefore cannot be handed a
 * BrUiCtl_ on LP64.  These two are the struct-model twins of those bodies, in
 * exactly the relationship br_sprfont.c's BrSprFontKindHook_10047360 already
 * has with slice3_31.c's BrSub10047360 -- and it is that control-typed
 * 0x10047360 they call, which is what slice8_84.h predicted would make them a
 * two-line adapter.  Claiming the addresses a second time would put two
 * manifest rows on one function.
 * ========================================================================== */

/* 0x10040A40 == D3D 0x10047610, slice3_31.c's BrPhaseKeyPush_10047610: it
 * takes the pending key into the typed-name ring.  With a zeroed context the
 * pending key is 0 and it returns immediately, which is the state of a menu
 * nobody has typed into. */
int32_t BrHostRootTick_10040AF0(BrUiCtl_ *pCtl)
{
    (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}

int32_t BrHostRootTick_10040A20(BrUiCtl_ *pCtl)
{
    BrPhaseKeyPush_10047610();
    (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}

/* The eight adapters above are EXPORTED BY NAME rather than through an index.
 * br_wire79.c assigns each one to the BrUiRootHooks member that names the same
 * original address, so a slot cannot silently receive its neighbour's action
 * -- which an ordinal table makes possible and which would be invisible: every
 * row would still "work", just not the row the player chose.  The other two
 * hooks need no export at all; br_wire79.c includes br_uicredits.h and
 * br_sprfont.h and installs those bodies directly. */

/* ==========================================================================
 * WHERE EACH ROW ACTUALLY WENT
 *
 * Every one of the seven actions caches its destination in a per-phase
 * singleton and republishes it as the current phase.  Those singletons are the
 * only place the result is visible, because the current-phase global the host's
 * frame loop reads is a DIFFERENT host object (see ALIASED STORAGE above), so
 * without this the run can only report "nothing appeared to happen".
 *
 * Nothing here computes or fabricates anything: it prints the pointers the
 * ported code stored and the page/control counts the constructor and enter
 * hook left in them.  An entry that is present but empty is exactly what an
 * unported screen builder produces, and the point of printing it is that the
 * two cases are told apart.
 * ========================================================================== */

static void Br78ReportSlot(const char *pszRow, const char *pszEnter,
                           const BrPhase_ *p)
{
    if (p == NULL) {
        printf("    %-14s %-22s not reached\n", pszRow, pszEnter);
        return;
    }
    if (p->nPages == 0 || p->aPages[0] == NULL) {
        printf("    %-14s %-22s phase %p  NO PAGES -- the enter hook built "
               "nothing\n", pszRow, pszEnter, (const void *)p);
        return;
    }
    printf("    %-14s %-22s phase %p  pages=%u  page0 cCtl=%u cSel=%u\n",
           pszRow, pszEnter, (const void *)p, (unsigned)p->nPages,
           (unsigned)p->aPages[0]->cCtl, (unsigned)p->aPages[0]->cSel);
}

void BrHostRootActionReport(void);
void BrHostRootActionReport(void)
{
    printf("\nwhere the main menu's rows went (the phase each action cached,\n"
           "read out of the globals the ported activate routines wrote):\n");

    Br78ReportSlot("Championship", "0x1004F2B0 ported",   g_phaseExt.pAA291C);
    Br78ReportSlot("Multiplayer",  "0x100563E0 UNPORTED", g_brPAA2958);
    Br78ReportSlot("Time Attack",  "0x10059760 ported",   g_phaseBase.pAA295C);
    Br78ReportSlot("Quick Race",   "0x1004B430 BLOCKED",  g_phaseBase.pAA290C);
    Br78ReportSlot("Options",      "0x1004BDC0 BLOCKED",  g_phaseBase.pAA2918);
    printf("    %-14s %-22s no phase: it starts a movie and tears the front "
           "end down\n", "Credits", "(0x1003AED0)");
    Br78ReportSlot("Quit",         "0x10049C20 UNPORTED", g_phaseExt.pAA2910);

    /* 0x10AA2904 is ONE dword and this proves it is one object here too, by
     * address rather than by assertion: every name below must print the same
     * slot, and that slot must be the field the frame loop reads. */
    printf("  current phase (0x10AA2904) -- the slot each name resolves to:\n"
           "    slice2_26/slice3_31/br_phaseact  BR_PHASE_CUR   slot %p\n"
           "    slice2_25/slice4_50/slice5_63    g_brPAA2904    slot %p\n"
           "    slice6_71 (0x10038F30)           BR_PHASE_CUR   slot %p\n"
           "    port/host/brally.c frame loop    g_nav.pAA2904  slot %p\n"
           "    -> %s;  value now %p\n",
           (const void *)BrPhaseCurSlot(),
           (const void *)&g_brPAA2904,
           (const void *)BrPhaseCurSlot(),
           (const void *)BrHostNavCurSlot(),
           (BrPhaseCurSlot() == BrHostNavCurSlot()
            && (void *)&g_brPAA2904 == (void *)BrPhaseCurSlot())
               ? "ONE OBJECT: a transition published by any of them is the one "
                 "the frame loop reads"
               : "STILL SPLIT -- a transition published here is invisible",
           (const void *)BR_PHASE_CUR);
}

/* ==========================================================================
 * The wiring itself
 * ========================================================================== */

void BrHostWire78(void);
void BrHostWire78(void)
{
    memset(&g_phaseBase, 0, sizeof g_phaseBase);
    memset(&g_phaseExt,  0, sizeof g_phaseExt);

    /* The only field of either block that is not .bss in the original.  Both
     * names are the SAME original object, 0x100AD300: slice2_26.h models it as
     * a ctx field and slice4_50.h as a bare extern, and slice4_50.h's own
     * banner says so.  Pointing both at one literal is the nearest this file
     * can get to the original's single global without editing either module. */
    g_phaseBase.p0AD300 = (void *)(size_t)(const void *)g_szAD300;
    g_brP0AD300         = (void *)(size_t)(const void *)g_szAD300;

    /* slice3_31.c and slice4_53.c now read and write THE SAME globals block
     * that the slice2_26.c calls above are handed.  Before this, slice3_31's
     * two pointers were NULL and its first activate would have faulted on
     * g_pBase->p0AD300. */
    BrPhase31SetCtx(&g_phaseBase, &g_phaseExt);
    BrSlice4SetPhaseCtx(&g_phaseBase);

    /* --- row 5's own context -------------------------------------------- *
     * br_uicredits.c has the same PORT-ONLY refusal br_uiroot.c has: it
     * returns 0 on its first line while its context is empty, and nothing
     * filled it either.  Every one of its five slots already has an owner
     * somewhere in the tree, so none of them gets fresh storage here -- which
     * is the whole point, because inventing a second object for a global is
     * how a write becomes invisible to its reader:
     *
     *   0x100AA010  the game mode -> BrPhaseCtx::n0AA010, the SAME block
     *               0x10044F50 sets to 1 two rows above
     *   0x106805B8  the cinematic index -> slice2_17.c's BrS17State, whose
     *               BrS17SetMode4 writes 2 (the outro) at the end of a race
     *   0x10AA2A40  the outro flag -> BrPhaseCtx31::nAA2A40, whose only
     *               writer in the image is 0x10047590, in slice3_31.c
     *   0x10AA289C  -> slice2_25.c's g_brAA289C, already shared with
     *               slice6_70.c and slice5_63.c
     *
     * Every one is zero here, so the arm this harness takes is the flag == 0
     * one: cinematic index 1, "RallyCredits.dat". */
    g_brUiCredits.pszStatus   = g_szAD300;
    g_brUiCredits.pnGameMode  = &g_phaseBase.n0AA010;
    g_brUiCredits.pnMovieSel  = &BrS17GetState()->f6805B8;
    g_brUiCredits.pnOutroFlag = &g_phaseExt.nAA2A40;
    g_brUiCredits.pnAA289C    = &g_brAA289C;

    /* g_brOptEnterHooks is LEFT ALL-NULL, and that is the frontier rather than
     * an omission: 0x100563E0 (row 1's destination) and 0x1005A6E0 have no
     * body anywhere in this tree.  slice4_50.c's BrOptInstall guards the NULL,
     * so row 1 publishes a constructed-but-empty phase -- which is what "the
     * screen builder is not ported" looks like from the outside, and is not
     * disguised as anything else. */
}
