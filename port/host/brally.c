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
 *   ./build/brally -w         also open a window and draw the menu
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
#include "br_uivt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void BrStubReport(void);

/* Per-slice context wiring. Each lives in its own translation unit because the
 * slice headers carry conflicting models of the page and control types and
 * cannot be included together -- see br_wire72.c. */
void BrHostWire71(void);
void BrHostWire72(void);

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

static void HostCtlSetText(BrUiCtl_ *pThis, const void *pText,
                           int32_t a2, int32_t a3, const void *pStyle)
{
    g_nSetText++;
    if (pText) g_lastText = (const char *)pText;
    /* br_uictl.c deliberately does not run the item's element constructor
     * (0x1005B050), so the text box arrives with a NULL vtable and
     * 0x10047EB0's measure dispatch would be skipped. Planting the pointer
     * the element ctor would have planted is the harness's job, not the
     * constructor's -- see the note in br_uictl.c. */
    if (pThis && !pThis->f2B5C.pVtbl) pThis->f2B5C.pVtbl = g_pBrTextBoxVtbl;
    BrUiCtlSetText_10047EB0(pThis, pText, a2, a3, pStyle);
}

static void HostCtlPlace(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                         int32_t flags, int32_t a4, int32_t a5,
                         int32_t a6, int32_t a7)
{
    g_nPlace++;
    BrUiCtlPlace_10047FB0(pThis, pOwner, x, y, flags, a4, a5, a6, a7);
}

static BrUiCtlVtbl_ g_hostCtlVtbl;

/* 0x1008F728 -- the text widget's vtable. Only the three slots 0x10047EB0
 * dispatches are filled; slice3_39.c ports all three. The rest stay NULL so
 * an unported method still faults. */
static BrTextBoxVtbl g_hostTextBoxVtbl;

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
 * The host reads through slice6_73.h's, and slice6_72.h's is held
 * field-for-field identical to it by test_pagemodel. slice6_71's is NOT: its
 * BrUiScreen begins at +0x10 with no pVtbl/pfn04/pfn08, so reading a page it
 * built through the host's view lands three fields off.
 *
 * The symptom is not a stable wrong number, which is what makes it dangerous:
 * BrExt_10049F40 reported 9, 10, 12, 7 and 10 across five runs of the SAME
 * binary, because the offset the host reads was never written and holds
 * whatever the heap had. setText and place stayed at 3 and 4 throughout --
 * they are counted by this file's vtable slots and never touch the struct.
 *
 * So the ctl column is SUPPRESSED where the models differ. Printing a number
 * we cannot justify is worse than printing none: two earlier status reports
 * quoted those garbage counts as if they confirmed the decompilation. */
typedef struct {
    const char *pszName;
    void      (*pfn)(BrPhase_ *);
    int         iModel;     /* 71 / 72 / 73 */
} BrBuilder;
static const BrBuilder g_aBuilders[] = {
    { "BrExt_10049F40", BrExt_10049F40, 71 },
    { "BrExt_1004D640", BrExt_1004D640, 73 },
    { "BrExt_1004DFC0", BrExt_1004DFC0, 73 },
    { "BrExt_1004E830", BrExt_1004E830, 72 },
    { "BrExt_1004F2B0", BrExt_1004F2B0, 73 },
    { "BrExt_1004F700", BrExt_1004F700, 71 },
    { "BrExt_10050060", BrExt_10050060, 73 },
    { "BrExt_10052030", BrExt_10052030, 72 },
    { "BrExt_10054B50", BrExt_10054B50, 73 },
    { "BrExt_10059760", BrExt_10059760, 72 },
    { "BrExt_1005A6E0", BrExt_1005A6E0, 72 },
    { "BrOptFn10051D30", BrOptFn10051D30, 71 },
    { "BrOptFn100558A0", BrOptFn100558A0, 73 },
    { "BrOptFn10056A10", BrOptFn10056A10, 72 },
    { "BrOptFn100575F0", BrOptFn100575F0, 71 },
    { "BrOptFn10057C10", BrOptFn10057C10, 72 }
};
#define BR_NBUILDERS ((int)(sizeof(g_aBuilders)/sizeof(g_aBuilders[0])))


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

/* Draw each control's rectangle. The rect comes from the builder (f50/f54 are
 * the truncated x/y, f58 = f50+0x7F, f5C = f54+0x21) -- this file invents no
 * geometry. */
static void DrawPhase(BrGfx *gfx, const BrPhase_ *ph, BrTexture tex, int haveTex)
{
    int i, j;
    for (i = 0; i < (int)ph->nPages && i < BR_PHASE_PAGES; i++) {
        const BrUiPage_ *pg = ph->aPages[i];
        if (!pg) continue;
        for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
            const BrUiCtl_ *c = pg->apCtl[j];
            float x, y, w, h;
            if (!c) continue;
            x = (float)c->f50;
            y = (float)c->f54;
            w = (float)(c->f58 - c->f50);
            h = (float)(c->f5C - c->f54);
            if (w <= 0.0f || h <= 0.0f) continue;
            if (haveTex) BrGfxDrawTexture(gfx, tex, x, y, w, h);
        }
    }
}

int main(int argc, char **argv)
{
    BrPhase_ *ph;
    BrGfx *gfx = NULL;
    BrTexture tex; BrImage img; int haveTex = 0;
    int windowed = (argc > 1 && strcmp(argv[1], "-w") == 0);
    int nCtl, frames = 0;

    printf("Boss Rally -- host boot\n");
    printf("phase object: sizeof=%zu, original 0x%X, allocating %zu\n",
           sizeof(BrPhase_), (unsigned)BR_PHASE_ORIG_SIZE,
           (size_t)BR_PHASE_ALLOC_SIZE);

    g_hostCtlVtbl.f34 = HostCtlSetText;
    g_hostCtlVtbl.f38 = HostCtlPlace;
    g_pBrUiCtlVtbl    = &g_hostCtlVtbl;

    /* Without this the constructed controls have a NULL text-box vtable and
     * 0x10047EB0 skips the measure, leaving width/height 0. */
    g_hostTextBoxVtbl.pfn04 = BrTextBoxMeasureA;
    g_hostTextBoxVtbl.pfn08 = BrTextBoxMeasureB;
    g_hostTextBoxVtbl.pfn28 = BrTextBoxCentreX;
    g_pBrTextBoxVtbl        = &g_hostTextBoxVtbl;

    WireContext();
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
                g_nSetText = g_nPlace = 0;
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
         * Their EFFECTS are not. The slots delegate to br_uivt.c, which is
         * typed over slice6_73.h's BrUiCtl_, and slice6_71's BrUiCtlX and
         * slice6_72's BrUi72Ctl put the same original fields at different
         * host offsets (BrUi72Ctl has a pfn18 the others lack, and nests the
         * item block). So for those builders the writes land in the wrong
         * members -- in bounds, since every model allocates at least the
         * original's 0x1E214, but meaningless. The rectangles DrawPhase
         * renders are only real for packet-73 controls. Merging the three
         * control models is what fixes this; br_ui.h is that work.
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
        printf("\ncontrols built: %d   setText=%d place=%d\n",
               nCtl, g_nSetText, g_nPlace);
        BrStubReport();
        return 0;
    }

    /* A real screen builder, 0x1004D640 (7 controls per packet 73). */
    printf("\nrunning builder 0x1004D640 ...\n");
    BrExt_1004D640(ph);
    nCtl = DumpPhase(ph);
    printf("\ncontrols built: %d   setText=%d place=%d\n",
           nCtl, g_nSetText, g_nPlace);
    if (g_lastText) printf("last text passed: \"%s\"\n", g_lastText);

    if (windowed) {
        gfx = BrGfxCreate(640, 480);
        if (!gfx) { printf("gfx init failed: %s\n", BrGfxLastError()); }
        else if (BrGfxOpenWindow(gfx, "Boss Rally") != 0) { gfx = NULL; }
        if (gfx && BrImgLoad(&img, "testdata/splash.img") == 0) {
            tex = BrGfxCreateTexture(gfx, img.width, img.height, img.pixels);
            BrImgFree(&img); haveTex = 1;
        }
        while (gfx && BrGfxPumpEvents(gfx)) {
            BrGfxBeginFrame(gfx, 0.06f, 0.06f, 0.09f, 1.0f);
            DrawPhase(gfx, ph, tex, haveTex);
            BrGfxEndFrame(gfx);
            BrGfxPresent(gfx);
            if (++frames >= 180) break;
        }
        if (gfx) BrGfxDestroy(gfx);
    }

    BrStubReport();
    return 0;
}
