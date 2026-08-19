/* br_uiboot.c -- startup/ : Glide 0x10056260, the pre-loop gate.  See
 * br_uiboot.h for the derivation, the seven concerns, the stack trace and
 * what makes the return value zero; this file is the transcription.
 *
 * Read off BRGlide.dll.  The D3D twin 0x1005D440 is the same 8,349 bytes and
 * was read as well, instruction by instruction, to check the split: the two
 * differ only in the image-record stride (br_uiimg.h) and in the three
 * addresses each build uses for the same objects.
 *
 * Concerns A, C and the save-path half of G are in port/src/drawing/br_uiimg.c.
 */
#include "br_uiboot.h"

#include "br_uiimg.h"

/* ==========================================================================
 * Concern D's storage.  Nothing else in this tree -- and nothing else in
 * BRGlide.dll's .text -- names 0x10AC4800 or 0x10AC5258.
 * ========================================================================== */

int32_t g_aBrUiBootRec[BR_UIBOOT_REC_TOTAL];      /* 0x10AC4800, 0xB4 dwords */
int32_t g_aBrUiBootZero[BR_UIBOOT_ZERO_DWORDS];   /* 0x10AC5258, 0x64 dwords */

/* The arithmetic that pins the record count, as a claim about the ORIGINAL:
 * the strided loop starts at base+8 and stops at base+8+720, so it makes 60
 * passes and its last store is the 180th dword of the 0xB4-dword fill.  If
 * someone reads 0xB4 as a byte count, or the stride as 4, these fail. */
typedef char br_uiboot_assert_fill[
    (0xB4u == BR_UIBOOT_REC_TOTAL) ? 1 : -1];
typedef char br_uiboot_assert_span[
    (0x10AC4808u + BR_UIBOOT_REC_COUNT * 12u == 0x10AC4AD8u) ? 1 : -1];
typedef char br_uiboot_assert_last[
    (2u + (BR_UIBOOT_REC_COUNT - 1u) * BR_UIBOOT_REC_DWORDS
        == BR_UIBOOT_REC_TOTAL - 1u) ? 1 : -1];

/* ==========================================================================
 * 0x100581CD .. 0x100581F6
 *
 * Transcribed as three separate passes in the original's order, NOT collapsed
 * into one loop that writes { -1, -1, 0 } per record.  Collapsing would give
 * the same bytes today and would hide the property that actually needs
 * checking -- that the third pass writes inside the first one's block -- so
 * the test can no longer catch a future edit that changes either extent.
 * ========================================================================== */
void BrUiBootScratchInit(void)
{
    int i;

    /* `or eax,-1 / mov ecx,0xB4 / mov edi,0x10AC4800 / rep stosd` */
    for (i = 0; i < BR_UIBOOT_REC_TOTAL; ++i) {
        g_aBrUiBootRec[i] = -1;
    }

    /* `mov ecx,0x64 / xor eax,eax / mov edi,0x10AC5258 / rep stosd` */
    for (i = 0; i < BR_UIBOOT_ZERO_DWORDS; ++i) {
        g_aBrUiBootZero[i] = 0;
    }

    /* `mov eax,0x10AC4808 / L: mov [eax],ebx / add eax,0xC /
     *  cmp eax,0x10AC4AD8 / jl L`   -- ebx is 0, the loop bound is signed,
     *  and base+8 is dword index 2. */
    for (i = 2; i < BR_UIBOOT_REC_TOTAL; i += BR_UIBOOT_REC_DWORDS) {
        g_aBrUiBootRec[i] = 0;
    }
}

/* ========================================================================== */

int BrUiBootOpsComplete(const BrUiBootOps *pOps)
{
    if (pOps == NULL) {
        return 0;
    }
    return (pOps->pfnAlloc          != NULL &&
            pOps->pfnPhaseCtor      != NULL &&
            pOps->pfnPublishPhase   != NULL &&
            pOps->pfnGetObj400      != NULL &&
            pOps->pfnPublishObj400  != NULL &&
            pOps->pfnErrShow        != NULL &&
            pOps->pfnTables64Clear  != NULL &&
            pOps->pfnRectTablesInit != NULL) ? 1 : 0;
}

/* ==========================================================================
 * 0x10056260
 * ========================================================================== */
/* WHAT IT DOES: the last thing done before the main loop starts: it clears
 * the image tables, sets up the menu picture paths, builds the first menu
 * screen and publishes it. It reports whether the game is fit to continue.
 * Note the picture-path setup's result is deliberately ignored, so a failure
 * there does not stop the game. */
/* @implements 0x10056260 glide BrUiBootPreLoopGate */
int32_t BrUiBootPreLoopGate(const BrUiBootOps *pOps)
{
    BrUiImgAlloc  alloc;
    BrPhase_     *pPhase;
    void         *pRaw;

    /* PORT-ONLY.  See br_uiboot.h: this zero is the refusal, not the gate. */
    if (!BrUiBootOpsComplete(pOps)) {
        return 0;
    }

    alloc.pfnAlloc = pOps->pfnAlloc;
    alloc.pfnFree  = pOps->pfnFree;
    alloc.pUser    = pOps->pUser;

    /* A -- 0x10056279.  Also where ebx is zeroed; ebx stays 0 to the end and
     * is the comparand for every allocation test below. */
    BrUiImgTableClear();

    /* B -- 0x1005629E.  `call 0x10058FA0`, three 64-dword tables. */
    pOps->pfnTables64Clear(pOps->pUser);

    /* C -- 0x100562A3.  145 paths.  The result is DELIBERATELY DISCARDED:
     * the original never tests it, and making the gate depend on it would
     * invent a second way for RallyMain to refuse to start. */
    (void)BrUiImgPathsInit(&alloc);

    /* D -- 0x100581CD. */
    BrUiBootScratchInit();

    /* E -- 0x100581F8.  THE GATE.
     *
     *   push 0xC8 / call 0x10074572 / add esp,4
     *   mov [esp+0xC], eax        <- the unwinder's copy; no behaviour
     *   cmp eax, ebx
     *   mov [esp+0x18], ebx       <- __try level 0; no behaviour
     *   je   -> eax = 0
     *   else mov ecx,eax / call 0x10041B60
     *
     * BR_PHASE_ALLOC_SIZE rather than 0xC8: br_phase.h's struct is larger
     * than 0xC8 on LP64 and CONVENTIONS.md forbids allocating the literal. */
    pRaw   = pOps->pfnAlloc(pOps->pUser, (uint32_t)BR_PHASE_ALLOC_SIZE);
    pPhase = (pRaw != NULL) ? pOps->pfnPhaseCtor(pOps->pUser, pRaw) : NULL;

    /* 0x10058226 / 0x1005822B -- BOTH slots, BEFORE the branch, so the
     * failure path publishes NULL rather than leaving a stale pointer.
     * 0x10AC5C60 (root) is written first, then 0x10AC5C5C (current). */
    pOps->pfnPublishPhase(pOps->pUser, pPhase);

    if (pPhase == NULL) {
        return 0;                    /* 0x10058232 -- the only real zero. */
    }

    /* F -- 0x10058246.  The one hook.  br_phase.h: the constructor does not
     * initialise +0x04 and operator new does not zero, so this store is what
     * stands between the frame and a call through heap garbage. */
    pPhase->pfnEnter = pOps->pfnPhaseEnter;

    /* 0x1005824D.  The singleton is built ONLY when its slot is still NULL,
     * which is why a second run of the gate rebuilds the phase and not this.
     * 0x10008D50 is `mov eax,ecx / ret`, so the "constructor" is the identity
     * and is inlined here rather than given an ops slot. */
    if (pOps->pfnGetObj400(pOps->pUser) == NULL) {
        void *p400 = pOps->pfnAlloc(pOps->pUser, BR_UIBOOT_OBJ400_SIZE);

        /* `cmp eax,ebx / je -> eax=0 / else mov ecx,eax / call 0x10008D50`.
         * 0x10008D50 is the identity, so the NULL-or-construct branch cannot
         * change p400 either way and is not written out.  Recorded here
         * because the SHAPE is the same as the phase's above and only the
         * callee's triviality collapses it. */
        pOps->pfnPublishObj400(pOps->pUser, p400);

        if (p400 == NULL) {
            /* 0x1005828F.  cdecl, one argument.  MAY NOT RETURN: 0x100378C0
             * calls exit(1) when [0x100ABE00 + 8*idx] is non-zero.  When it
             * does return, the original falls through to the success tail --
             * so this is NOT a way for the gate to answer zero. */
            pOps->pfnErrShow(pOps->pUser, BR_UIBOOT_ERR_OBJ400);
        }
    }

    /* G -- 0x10058299.  The two save-file buffers, season first. */
    BrUiImgSavePathsInit(pOps->pszSeasonBuf, pOps->cbSeasonBuf,
                         pOps->pszGhostBuf,  pOps->cbGhostBuf);

    /* 0x100582E1.  `call 0x10058540`, the four rectangle tables. */
    pOps->pfnRectTablesInit(pOps->pUser);

    /* 0x100582EC.  `mov eax,1`. */
    return 1;
}

/* ========================================================================== */

void BrUiBootResetForTest(void)
{
    int i;

    for (i = 0; i < BR_UIBOOT_REC_TOTAL; ++i) {
        g_aBrUiBootRec[i] = 0;
    }
    for (i = 0; i < BR_UIBOOT_ZERO_DWORDS; ++i) {
        g_aBrUiBootZero[i] = 0;
    }
}
