/* br_uiboot.h -- GLIDE 0x10056260 (D3D 0x1005D440), 8,349 bytes: THE PRE-LOOP
 * GATE.  RallyMain calls it at 0x1001CD25 and returns without ever running a
 * frame if it answers zero.
 *
 * RESPONSIBILITY: startup/ -- "bring the game up ... which step runs each
 * frame".  br_boot.h already reserved the slot this fills:
 *
 *     int32_t (*pfnPreLoopGate)(void *pUser);       // 0x10056260
 *
 * ==========================================================================
 * WHAT 8 KB ACTUALLY CONSISTS OF -- SEVEN CONCERNS, NOT ONE
 * ==========================================================================
 *
 * By bytes, it is one concern repeated 145 times and six small ones:
 *
 *   A  0x10056279   1160-byte table clear + three 16-bit slots       37 B
 *   B  0x1005629E   call 0x10058FA0, clear three 64-dword tables      5 B
 *   C  0x100562A3   145 x { operator new(0x104); strcpy(path) }   7,978 B
 *   D  0x100581CD   fill 180 dwords with -1, 100 dwords with 0,
 *                   then zero every third dword of the first block   43 B
 *   E  0x100581F8   construct the 0xC8 phase object; publish it;
 *                   RETURN 0 IF IT FAILED                            78 B
 *   F  0x10058246   install ONE hook, then construct the 0x400
 *                   singleton, reporting error 1 if it fails         83 B
 *   G  0x10058299   seed the two save-file path buffers; call
 *                   0x10058540, build the four rectangle tables      99 B
 *
 * A, C and the save-path half of G are the image registry and live in
 * br_uiimg.c (drawing/).  B, D, E, F and the rest of G are here.  95.6% of
 * the byte count is C, and C is 145 copies of the same nine instructions --
 * so the function is large rather than complicated, and the interesting part
 * is the 200 bytes at the end.
 *
 * ==========================================================================
 * IT IS NOT A WIRING POINT
 * ==========================================================================
 *
 * ARCHITECTURE.md's installer ranking is the right question to ask of any big
 * function in this engine, and here the answer is no.  0x10056260 does not
 * appear in tools/hookmap.py's installer table, whose smallest entry hands
 * out nine hooks.  config/hookmap.csv credits it with two, and one of those
 * is a false positive worth naming: the relocation it counts at 0x10056262 is
 * `push 0x100765F6`, the SEH handler in the prologue, which is an address
 * MATERIALISED but never installed anywhere.  That is precisely the limit
 * ARCHITECTURE.md states for the tool ("installs is a superset of could
 * install"), and this function is a clean instance of it.
 *
 * The real count is ONE, in 8,349 bytes:
 *
 *     0x10058246  mov dword ptr [eax + 4], 0x100425E0
 *
 * -- the phase object's +0x04 slot, br_phase.h's `pfnEnter`.  0x100425E0 is
 * itself an installer of ELEVEN hooks in 2,659 bytes (D3D 0x100491B0, not
 * ported -- slice3_32.h lists it under "NOT PORTED"), so this function is not
 * where a subsystem is assembled; it is where the front end's ROOT OBJECT is
 * created and handed its one entry point, and the assembly happens inside
 * that entry point.
 *
 * That single store is still the load-bearing line in the file.  br_phase.h
 * records that the constructor 0x10048710 does NOT initialise +0x04 and that
 * operator new does not zero, so between the constructor returning and this
 * store the slot holds heap garbage -- a port that dropped the store would
 * call through it.
 *
 * ==========================================================================
 * THE RETURN VALUE: WHAT MAKES IT ZERO
 * ==========================================================================
 *
 * There is exactly ONE zero exit, at 0x10058232, and it is reached when the
 * 0xC8-byte allocation for the phase object fails:
 *
 *     push 0xC8 / call 0x10074572          ; operator new
 *     cmp  eax, ebx                        ; ebx has been 0 since 0x10056285
 *     je   ->  eax = 0                     ; skip the constructor
 *     else mov ecx,eax / call 0x10041B60   ; the constructor, returns this
 *     cmp  eax, ebx
 *     mov  [0x10AC5C60], eax               ; PUBLISHED EVEN WHEN NULL
 *     mov  [0x10AC5C5C], eax               ; ...into BOTH slots
 *     jne  -> carry on
 *     xor  eax,eax ; <epilogue> ; ret      ; ZERO
 *
 * Three things follow, and all three are easy to get wrong:
 *
 *   1. The two phase globals are written BEFORE the branch, so a failed boot
 *      does not leave stale pointers -- it publishes NULL into both.  Those
 *      are br_uinav.h's 0x10AC5C60 (D3D 0x10AA2908, the root/shell phase) and
 *      0x10AC5C5C (D3D 0x10AA2904, the CURRENT phase).  ONE object in TWO
 *      slots: at boot the current phase IS the root, which is exactly the
 *      invariant 0x100489A0 later tests through 0x10AC5BC0.
 *
 *   2. The constructor's own failure is NOT a way to return zero, because
 *      0x10041B60 cannot fail -- it returns `this`.  Only the allocation can.
 *
 *   3. The 0x400 object's failure is NOT a way to return zero either.  It
 *      reports error index 1 through 0x100378C0 and then FALLS THROUGH to the
 *      success tail.  Whether control comes back at all is data, not code:
 *      0x100378C0 calls exit(1) only when [0x100ABE00 + 8*idx] is non-zero.
 *      If it returns, the gate returns 1 with a NULL singleton.
 *
 * Everything else -- 145 failed path allocations, a NULL rectangle table,
 * anything at all in concerns A..D -- returns 1.  The gate answers one
 * question only: is there a phase object?
 *
 * ==========================================================================
 * CALLED TWICE, AND IT LEAKS ON PURPOSE
 * ==========================================================================
 *
 * RallyMain calls it once at 0x1001CD25 and tests the result.  State 4
 * (0x1001CE20) calls it AGAIN at 0x1001CE88 on its mode-change arm, between
 * 0x1006C290(0) and 0x1006E280, and IGNORES the result -- the eax stored
 * immediately afterwards into 0x10AC6748 is 0x1006E280's, not this one's.
 *
 * A second run therefore:
 *   - memsets the image table, dropping 145 path pointers and every loaded
 *     BrSurf without freeing any of them: 145 * 0x104 == 37,700 bytes of paths
 *     plus every surface, leaked per mode change;
 *   - allocates and constructs a SECOND phase object and overwrites both
 *     phase globals with it, leaking the first;
 *   - does NOT re-create the 0x400 singleton, because that one is guarded by
 *     `if ([0x10AC5C58] == 0)`.  It is the only thing here that is created
 *     once.
 *
 * All three are the original's behaviour and all three are preserved.  The
 * asymmetry -- one guarded singleton beside two unguarded rebuilds -- is the
 * evidence that the guard is deliberate and the leaks are not.
 *
 * ==========================================================================
 * THE STACK FRAME, TRACED, BECAUSE [esp+N] MEANS NOTHING WITHOUT ESP
 * ==========================================================================
 *
 * This is an MSVC SEH frame and three of its four displacements are frame
 * bookkeeping rather than data.  Let E be esp on entry (after the call):
 *
 *     push -1                  E-0x04   __try level, initially -1
 *     push 0x100765F6          E-0x08   the handler
 *     push fs:[0]              E-0x0C   the previous registration link
 *     mov  fs:[0], esp                  the EXCEPTION_REGISTRATION is at E-0x0C
 *     push ecx                 E-0x10   ONE LOCAL SLOT, reserved not stored
 *     push ebx                 E-0x14
 *     push esi                 E-0x18
 *     push edi                 E-0x1C   <- esp for the whole body
 *
 * so, relative to the body's esp:
 *
 *     [esp+0x0C] == E-0x10   the raw allocation, kept only so the unwinder
 *                            can free it if the constructor throws
 *     [esp+0x10] == E-0x0C   THE SAVED fs:[0] LINK.  `mov ecx,[esp+0x10] /
 *                            mov fs:[0],ecx` in both epilogues is the
 *                            unlink, not a data read.
 *     [esp+0x18] == E-0x04   the __try level: 0, then -1, then 1, then -1.
 *                            Pure EH state; it is not a variable.
 *
 * and `add esp,0x10` in each epilogue pops exactly those four dwords.  None of
 * the three reaches the port: this file has no exceptions to unwind, and the
 * only thing the frame contributes to behaviour is that both exits restore
 * fs:[0] and pop the same four slots -- i.e. the two `ret`s are the same
 * frame, which is what tells you the early one is a genuine early return and
 * not a tail of some other function.
 *
 * ==========================================================================
 * THE TWO SCRATCH BLOCKS -- WRITTEN HERE, READ NOWHERE
 * ==========================================================================
 *
 * Concern D fills two arrays that no other instruction in BRGlide.dll's
 * .text mentions.  That is not an assumption from failing to find a caller:
 * every constant-address operand in the whole image was decoded and ranged,
 * and the only reference to any address inside either block is this function
 * (plus 0x10AC4804, which belongs to the rectangle table BELOW the first
 * block -- 0x10AC46C0 + 20*16 == 0x10AC4800, br_sprfont.c's table B, so the
 * two abut exactly and neither overruns the other).
 *
 * They are transcribed anyway, and the overlap between the two writes to the
 * first block is the reason to be careful with them:
 *
 *     or  eax,-1 / mov ecx,0xB4 / mov edi,0x10AC4800 / rep stosd
 *          180 dwords <- -1
 *     mov ecx,0x64 / xor eax,eax / mov edi,0x10AC5258 / rep stosd
 *          100 dwords <- 0
 *     mov eax,0x10AC4808
 *   L: mov [eax], ebx            ; ebx == 0
 *     add eax,0xC
 *     cmp eax,0x10AC4AD8 / jl L
 *
 * The third loop writes INSIDE the block the first one just filled: it starts
 * at base+8 and steps 12, so it runs exactly 60 times and its last store is
 * at base+716 == the 180th and final dword of the -1 fill.  The result is 60
 * twelve-byte records of { -1, -1, 0 }, and 60 * 12 == 720 == 0xB4 * 4 to the
 * byte.  Reading the -1 fill and the zero loop as two independent things --
 * or reading `rep stosd`'s 0xB4 as a byte count -- gives a different answer
 * both times.
 *
 * The loop bound is a SIGNED compare (`jl`) against an address.  Every
 * address involved is below 0x80000000 so the signedness cannot be observed,
 * which is worth writing down precisely because it means the sign is not
 * evidence of anything.
 *
 * The D3D build does the same three writes at 0x10A9D778 (0xB4), 0x10A9E1D0
 * (0x64) and 0x10A9D780..0x10A9DA50 (stride 12) -- the same counts, the same
 * +8 start, the same 60 iterations.  slice1_06.c's banner describes that tail
 * from the D3D side and declines it; this is the same tail from the Glide
 * side, taken.
 */
#ifndef BR_UIBOOT_H
#define BR_UIBOOT_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, BrPhaseEnterFn_, BR_PHASE_ALLOC_SIZE */

/* ==========================================================================
 * Concern D's two blocks
 * ========================================================================== */

#define BR_UIBOOT_REC_COUNT    60    /* twelve-byte records at 0x10AC4800 */
#define BR_UIBOOT_REC_DWORDS   3     /* -1, -1, 0                          */
#define BR_UIBOOT_REC_TOTAL    (BR_UIBOOT_REC_COUNT * BR_UIBOOT_REC_DWORDS)
#define BR_UIBOOT_ZERO_DWORDS  100   /* 0x64 dwords at 0x10AC5258          */

extern int32_t g_aBrUiBootRec[BR_UIBOOT_REC_TOTAL];      /* 0x10AC4800, 0xB4 */
extern int32_t g_aBrUiBootZero[BR_UIBOOT_ZERO_DWORDS];   /* 0x10AC5258, 0x64 */

/* 0x100581CD .. 0x100581F6, in the original's order.  The order matters: the
 * strided zero pass runs AFTER the -1 fill and writes inside it. */
void BrUiBootScratchInit(void);

/* ==========================================================================
 * The platform and cross-module calls the gate makes.
 *
 * Required, never defaulted -- br_boot.h's BrRallyMainOps sets the precedent
 * and the reason is the same: a caller that supplies none must not receive a
 * plausible boot.  BrUiBootOpsComplete() is the check, and the gate refuses
 * (returning 0) rather than running half of itself.
 *
 * Which real function goes in each slot, Glide address first:
 *
 *   pfnAlloc          0x10074572  operator new via MSVCRT; does NOT zero
 *   pfnPhaseCtor      0x10041B60  D3D 0x10048710 == slice6_73.c BrOptObjCtor
 *   pfnObj400Ctor     -- not a slot.  0x10008D50 is THREE BYTES,
 *                        `mov eax,ecx / ret`: it returns `this` and does
 *                        nothing else, so it is inlined below rather than
 *                        modelled.  (D3D 0x10008B70 is the same.)
 *   pfnErrShow        0x100378C0  D3D 0x1003E260 == slice1_06.c BrErrShow;
 *                                 may not return -- it can call exit(1)
 *   pfnTables64Clear  0x10058FA0  D3D 0x1005FF30 == slice1_07.c
 *                                 BrTables64Clear over the three 64-dword
 *                                 tables at 0x10AC65E8, 0x10AC5DE0,
 *                                 0x10AC61E8 IN THAT ORDER
 *   pfnRectTablesInit 0x10058540  D3D 0x1005F800 == br_sprfont.c
 *                                 BrSprFontRectInit_1005F800
 *   pfnPhaseEnter     0x100425E0  the VALUE stored into phase->pfnEnter.
 *                                 NOT PORTED (D3D 0x100491B0); a host that
 *                                 has nothing to put here must pass NULL and
 *                                 will then reproduce a NULL slot, which is
 *                                 what this engine does with an uninstalled
 *                                 hook everywhere else.
 *
 * The three globals the gate publishes are reached through callbacks rather
 * than declared here, and that is deliberate: br_uinav.h already models all
 * three as members of BrUiNav (pAA2904, pAA2908, and the 0x10AA2900 slot),
 * and CONVENTIONS.md's aliased-storage rule says the fix for two models of
 * one address is to merge them, never to add a third.  A host points these at
 * whichever model it built.
 * ========================================================================== */
typedef struct BrUiBootOps {
    void      *(*pfnAlloc)(void *pUser, uint32_t cb);          /* 0x10074572 */
    void       (*pfnFree)(void *pUser, void *p);               /* host-only  */

    BrPhase_  *(*pfnPhaseCtor)(void *pUser, void *pRaw);       /* 0x10041B60 */

    /* 0x10058226 / 0x1005822B -- ONE object into BOTH slots, and it is
     * called with NULL on the failure path too. */
    void       (*pfnPublishPhase)(void *pUser, BrPhase_ *pPhase);

    void      *(*pfnGetObj400)(void *pUser);                   /* read  0x10AC5C58 */
    void       (*pfnPublishObj400)(void *pUser, void *p);      /* write 0x10AC5C58 */

    void       (*pfnErrShow)(void *pUser, int32_t idx);        /* 0x100378C0 */
    void       (*pfnTables64Clear)(void *pUser);               /* 0x10058FA0 */
    void       (*pfnRectTablesInit)(void *pUser);              /* 0x10058540 */

    BrPhaseEnterFn_ pfnPhaseEnter;   /* the value stored at phase +0x04     */

    /* The two save-file buffers, supplied by their owner.  See br_uiimg.h. */
    char   *pszSeasonBuf;   size_t cbSeasonBuf;    /* 0x117A6030 */
    char   *pszGhostBuf;    size_t cbGhostBuf;     /* 0x117A5F28 */

    void   *pUser;
} BrUiBootOps;

/* The size the original passes to operator new for the singleton.  Unlike the
 * phase's 0xC8 this one is opaque -- 0x10008D50 stores nothing in it -- so
 * there is no struct to take sizeof() of and the literal is the honest
 * model. */
#define BR_UIBOOT_OBJ400_SIZE  0x400u

/* The error index the singleton's failure reports through 0x100378C0. */
#define BR_UIBOOT_ERR_OBJ400   1

/* Non-zero when every required slot is filled.  pfnFree and pfnPhaseEnter are
 * NOT required: the original has no free at all, and a NULL hook value is a
 * state the engine has everywhere. */
int BrUiBootOpsComplete(const BrUiBootOps *pOps);

/* ==========================================================================
 * 0x10056260 itself.
 *
 * Returns 1, or 0 when the phase object could not be allocated -- and zero is
 * what makes RallyMain skip the main loop entirely.  See the banner.
 *
 * Returns 0 for an incomplete BrUiBootOps as well.  That is PORT-ONLY and has
 * no counterpart in the original; it is the refusal described above, and
 * BrUiBootOpsComplete() is how a caller tells the two zeroes apart.
 * ========================================================================== */
int32_t BrUiBootPreLoopGate(const BrUiBootOps *pOps);

/* Reset the globals this module owns, so a test can run the gate twice. */
void BrUiBootResetForTest(void);

#endif /* BR_UIBOOT_H */
