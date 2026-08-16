/* br_wire71.c -- supply slice6_71's module context for the host harness.
 *
 * WHY THIS IS A FILE OF ITS OWN
 *
 * Each slice module keeps the original's .data in a context struct it owns
 * (g_brS71 here, g_br73 in slice6_73, g_pBr72Env in slice6_72), and each of
 * those headers carries its own partial model of the shared UI types. Two of
 * them cannot be included in one translation unit -- that conflict is the whole
 * reason br_phase.h exists. So the wiring is split one file per slice, and the
 * host calls each entry point without ever seeing the conflicting headers.
 *
 * WHAT IT DOES NOT DO
 *
 * The hook pointers stay NULL. The builders only STORE them into control
 * slots during a build, so NULL is the faithful value at that point -- and if
 * something does call one, a NULL crash is the right outcome rather than a
 * silent no-op that hides a missing behaviour.
 *
 * What must be non-NULL is the context and its sub-structs themselves, because
 * the builders dereference those to reach the hook slots. That is the actual
 * bug this file fixes: `pH->p10047360` faulted at slice6_71.c:295 with
 * pHooks == NULL, which is a NULL STRUCT POINTER, not a NULL function pointer.
 * The two failures look identical in a debugger and mean opposite things.
 */
#include "slice6_71.h"
#include "br_phase.h"
#include "slice6_77.h"   /* BrSub100586A0 -- 0x100586A0 */
#include <string.h>
#include <stdlib.h>

static BrS71Hooks g_hooks71;      /* all slots NULL, deliberately */
static BrS71Env   g_env71;
static char       g_a9d018[BR71_A9D018_SIZE];

/* --- the global phase at 0x10AA2908 -------------------------------------
 * 0x1004F700 reads `g_brS71.pAA2908->fC0` as its very first act. The original
 * has a real global phase there, constructed during startup by an init path
 * that is not ported yet. So this is NOT the harness inventing an object the
 * game does not have -- it is the harness standing in for an unported init,
 * and it uses the REAL constructor to do it.
 *
 * fC0 is the season-file list. The builder walks it as `(char *)fC0 + k + 4`,
 * i.e. a vtable pointer followed by 100 fixed-width name slots -- the same
 * 4 + 100*0x104 == 0x6594 block slice1_06.h calls BrNameList. The struct
 * declares only the vtable, so the storage has to be sized by hand; sizing it
 * by sizeof(BrS71FileList) would be a 0x6590-byte overrun.
 *
 * Every name is left empty, which is a state the game genuinely has (no saved
 * seasons) rather than a fabricated one. The scan hook is a no-op for the same
 * reason: a real scan would need a filesystem layout this port does not define
 * yet, and inventing entries would put fictional data on screen. */
#define BR71_NAMELIST_SLOTS   100
#define BR71_NAMELIST_STRIDE  0x104
#define BR71_NAMELIST_BYTES   (4 + BR71_NAMELIST_SLOTS * BR71_NAMELIST_STRIDE)

/* 0x10048710, declared here rather than by including slice6_73.h: that header
 * and slice6_71.h both declare the builders, and slice2_25.h declares this same
 * address over its own BrOptObj. Same tag and signature as br_phase.h's model. */
BrPhase_ *BrOptObjCtor(BrPhase_ *pThis);

static BrPhase_ *g_pPhaseAA2908;
static unsigned char g_fileList[BR71_NAMELIST_BYTES];

static void FileListScan(BrS71FileList *pThis, const char *pszPattern)
{
    (void)pThis; (void)pszPattern;   /* finds nothing; see above */
}
static const BrS71FileListVtbl g_fileListVtbl = { 0, FileListScan };

void BrHostWire71(void)
{
    memset(&g_hooks71, 0, sizeof(g_hooks71));
    memset(&g_env71,   0, sizeof(g_env71));
    memset(g_a9d018,   0, sizeof(g_a9d018));

    /* Build the global phase with the real constructor, then hang the empty
     * name list off +0xC0. Order matters: the constructor zeroes the object. */
    if (!g_pPhaseAA2908) {
        g_pPhaseAA2908 = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
        if (g_pPhaseAA2908) BrOptObjCtor(g_pPhaseAA2908);
    }
    memset(g_fileList, 0, sizeof(g_fileList));
    *(const BrS71FileListVtbl **)g_fileList = &g_fileListVtbl;
    if (g_pPhaseAA2908) g_pPhaseAA2908->fC0 = g_fileList;

    /* The one env slot that is NOT left NULL, because it is not a hook the
     * builders merely store -- BrOptFn100575F0 CALLS it as its first
     * statement, and 0x100586A0 is now ported (slice6_77.c) rather than
     * missing. The banner above still holds for every other slot. */
    g_env71.pfn100586A0 = BrSub100586A0;

    /* The style rectangles at 0x100AB438.. -- see slice3_39.h. Left NULL until
     * 0x1005B910 was ported, because until then nothing in the port looked
     * inside one; 0x1004F700 passes p0AB538 straight into it and it reads four
     * int32s out. The banner above still holds for the HOOK slots. */
    g_brS71.p0AB438 = NULL;
    g_brS71.p0AB448 = NULL;
    g_brS71.p0AB468 = NULL;
    g_brS71.p0AB478 = NULL;
    g_brS71.p0AB488 = NULL;
    g_brS71.p0AB4D8 = NULL;
    g_brS71.p0AB508 = NULL;
    g_brS71.p0AB538 = NULL;

    g_brS71.pAA2908 = g_pPhaseAA2908;
    g_brS71.pHooks  = &g_hooks71;
    g_brS71.pA9D018 = g_a9d018;
    g_brS71Env      = &g_env71;
}
