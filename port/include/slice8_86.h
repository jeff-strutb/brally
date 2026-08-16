/* slice8_86.h -- packet 86: the stub sweep after packets 82 and 83.
 *
 * ===========================================================================
 * WHAT THIS MODULE IS
 * ===========================================================================
 * Fifteen entries of port/host/br_stubs.c, chosen by STATIC CALL-SITE DEMAND.
 * The demand figure behind each one is the sum of
 *
 *   - call sites already present in port/src (counted as calls AND as
 *     function-pointer stores -- five of these fifteen are never CALLED in
 *     ported code, they are INSTALLED, and a name-with-parens grep misses
 *     every one of them), and
 *   - direct `call` targets swept out of BOTH binaries' .text, decoded only
 *     inside flow-derived function extents so no 0xE8 operand byte is ever
 *     read as a call, PLUS every relocated dword in the image, which is what
 *     finds a function that is only ever referenced by address.
 *
 * A COUNTING TRAP WORTH RECORDING, because it inverted the ranking on the
 * first pass and would have sent this packet at the wrong function.
 * config/shared.csv pairs a D3D address with a Glide one. If you then look
 * BOTH addresses up in BOTH binaries you are counting a number that is a
 * valid address in the other image too, and it names a different function
 * there. Done that way, `BrExt_10041AC0` came out top of the demand list with
 * 23 sites; its true demand is 1. Its Glide partner 0x1003B020 is called 22
 * times -- in the D3D build, where that address is `BrVec3Mac` and has
 * nothing to do with it. A D3D address is only ever counted in BRD3D.dll and
 * a Glide address only in BRGlide.dll.
 *
 * ===========================================================================
 * NO FLOAT-RETURNING STUB REMAINS
 * ===========================================================================
 * The generated stubs return integer 0 and leave xmm0 untouched, so a
 * float-returning stub hands its caller garbage rather than zero. Every one
 * of the 65 remaining stub names was cross-checked against its declaration in
 * port/include: NONE is declared float or double. 0x1006F310
 * (BrProbe1006F310), which slice7_82.h names as the one live instance, is no
 * longer in br_stubs.c at all. So the "actively wrong" bucket is currently
 * EMPTY, and that is a measured statement rather than an assumption.
 *
 * SIXTY-FIVE, NOT SIXTY-SIX, AND THE OFF-BY-ONE IS IN THE RECIPE. br_stubs.c's
 * own header says "the true linked count is `grep -c \"return br_stub(\"` on
 * this file -- derived, not typed", and that command answers 66 because THE
 * SENTENCE ITSELF CONTAINS THE PATTERN. The line that matches is br_stubs.c:41,
 * the comment prescribing the check. `grep -c '^long '` answers 65, and so does
 * counting the distinct names passed to br_stub(). A self-matching grep is the
 * same shape of error as measuring a thing with the tool that defines it, which
 * CONVENTIONS.md already records for the 1019-byte text emitter -- the recipe
 * was written to replace a hand-typed number that kept drifting, and it drifted
 * by one on the day it was written.
 *
 * ===========================================================================
 * THE THREE GROUPS, AND WHY THE GROUP MATTERS
 * ===========================================================================
 *  1. FIVE ADAPTERS onto bodies that already exist under another name
 *     (CONVENTIONS.md's "grep the ADDRESS, not the symbol"). slice7_82.h
 *     listed these five as blocked on an ARITY CONFLICT; the conflict is
 *     adjudicated below against the disassembly and slice2_26.h wins.
 *
 *  2. TWO ADAPTERS onto br_audio.c, which is the CD-audio module in full.
 *
 *  3. SIX TRANSCRIPTIONS and TWO VTABLE OBJECTS.
 *
 * The two vtable objects are the sharpest item here. `BrUiPageVtbl_1008F6F8`
 * and `BrPhaseVtbl_1008F700` are DATA -- 2 and 9 function-pointer slots read
 * out of .rdata -- and br_stubs.c emitted each as a FUNCTION. slice3_32.c's
 * `pThis->pVtbl = &BrUiPageVtbl_1008F6F8` therefore aimed the object's vtable
 * pointer at a stub's MACHINE CODE, and the first dispatch through any slot
 * would have read instruction bytes as a function pointer and jumped to them.
 * That is strictly worse than a missing function: a stub that is called
 * announces itself, and this one would not have. The same generator mistake
 * is already on record in br_stubs.c's own header for g_brPAA29D0.
 *
 * ===========================================================================
 * HOST BINDINGS: NULL MEANS "BEHAVE EXACTLY LIKE THE RETIRED STUB"
 * ===========================================================================
 * Most of these functions read state that lives in a fixed global in the
 * original and in NO object in this tree -- the five enter hooks want a
 * build context, the two CD calls want the soundtrack, 0x10071480 wants the
 * peer table, five vtable slots want the screen globals, and three of the
 * transcriptions want OS primitives. Following the precedent
 * slice7_82.h set with g_pBrActiveFlags82 and slice8_83.h set with its six
 * pointers, each is reached through a pointer that DEFAULTS TO NULL, and a
 * NULL binding makes the function do nothing and return zero -- which is
 * precisely what the generated stub did. Binding one can only improve
 * behaviour and can never regress it. This module does not invent storage
 * for a global another module might own; that is the aliased-storage bug.
 */
#ifndef SLICE8_86_H
#define SLICE8_86_H

#include <stdint.h>
#include <stddef.h>

#include "slice3_32.h"   /* BrUiPageVtbl, BrPhaseFullVtbl, BrScrGlobals */
#include "slice3_33.h"   /* BrUiBuildCtx, BrUiPhase                     */
#include "slice1_05.h"   /* BrPeer, BR_PEER_COUNT                       */
#include "br_audio.h"    /* BrAudio                                     */

/* =========================================================================
 * 0. Host bindings
 * ========================================================================= */

/* The injected globals context slice3_33.c's five builders take as their
 * FIRST parameter. That parameter is NOT one of the original's arguments --
 * see the arity note in section 1 -- so it has to come from somewhere, and
 * this is it. NULL: the five enter hooks do nothing. */
extern BrUiBuildCtx *g_pBrUiBuildCtx86;

/* The soundtrack module the two CD entry points drive. NULL: no music, which
 * is also what the original does when its own selector global is 0. */
extern BrAudio *g_pBrAudio86;

/* The peer table at 0x11786828, modelled by slice1_05.h as BrPeer[16] and
 * given storage by NOBODY. NULL: 0x10071480 is a no-op. */
extern BrPeer *g_aBrPeer86;

/* The screen-globals block slice3_32.c lifted out of ~30 fixed globals. The
 * vtable slots below need it because slice3_32.c gave five of the nine phase
 * methods a LEADING BrScrGlobals * that the original does not have; the
 * thunks in this module put that argument back. NULL: those slots return 0
 * and write nothing. */
extern BrScrGlobals *g_pBrScrGlobals86;

/* The OS primitives 0x10072270 and the 0x100751D0 / 0x1002C2C0 pair reach
 * through the import table. Every one is called with a fixed argument
 * pattern, so the hooks take only what varies -- the same shape
 * slice2_13.h's BrDPlayOs uses. A NULL hook is never called; the state
 * writes around it still happen, because those are the observable part. */
typedef struct BrPlatOs86 {
    void (*pfnSetEvent)(void *h);
    void (*pfnWaitSingle)(void *h);       /* WaitForSingleObject(h, INFINITE) */
    void (*pfnCloseHandle)(void *h);
    void (*pfnLockPeer)(uint32_t hMutex); /* WaitForSingleObject(h, INFINITE) */
    void (*pfnUnlockPeer)(uint32_t hMutex);          /* ReleaseMutex(h)       */
    void (*pfnTimeBeginPeriod)(uint32_t ms);
    void (*pfnTimeEndPeriod)(uint32_t ms);
} BrPlatOs86;

extern const BrPlatOs86 *g_pBrPlatOs86;

/* =========================================================================
 * 1. The five menu-screen enter hooks -- ADAPTERS, and the arity settled
 * =========================================================================
 *
 * 0x1004A580 / 0x1004B430 / 0x1004BDC0 / 0x1004C4A0 / 0x1004CAC0 all have
 * bodies in port/src/slice3_33.c under the names BrExt_1004A580 ...
 * BrExt_1004CAC0. slice7_82.h found the collision and declined to adapt it,
 * because slice2_26.h declares one argument (`BrPhase *pSelf`, described
 * there as __thiscall) and slice3_33.h declares two (`BrUiBuildCtx *`,
 * `BrUiPhase *`), and it asked for an adjudication rather than a cast.
 *
 * ADJUDICATION, from Glide 0x100439B0 (== D3D 0x1004A580, `body` match in
 * config/shared.csv, 3746 bytes on both sides):
 *
 *   100439B0  push -1                 \
 *   100439B2  push 0x1007536D          | SEH frame: 12 bytes
 *   100439B7  mov  eax, fs:[0]         |
 *   100439BD  push eax                /
 *   100439BE  mov  fs:[0], esp
 *   100439C5  sub  esp, 0xc           -- 12 more
 *   100439C8  push ebx                \
 *   100439C9  push ebp                 | 16 more  => esp = entry - 0x28
 *   100439CA  push esi                 |
 *   100439CB  push edi                /
 *   100439CC  mov  edi, [esp + 0x2c]  -- entry-0x28+0x2c == entry+4
 *
 * `entry+4` is the FIRST STACK ARGUMENT. ecx is dead on entry (the next two
 * writes to it, at 100439C5.. and 10043A0B, are `xor ecx,ecx`), so the
 * function is NOT __thiscall and there is no hidden `this`.
 *
 * So: ONE argument, cdecl, and it is the phase -- edi is immediately used as
 * `phase->cScreen` (+0x10), `phase->f12` (+0x12), `phase->apScreen[]` (+0x14)
 * and `phase->aF6C[]` (+0x6C), which is exactly slice3_33.h's BrUiPhase.
 *
 * slice2_26.h IS RIGHT ABOUT THE ARITY and wrong about the calling
 * convention; slice3_33.h's first parameter is an injected host context, not
 * an original argument, and its own header says so ("Everything this range
 * only ever STORES ... is reached through BrUiBuildHooks / BrUiBuildCtx").
 * There was never a real disagreement -- the two headers were describing
 * different things -- and this is the third time in this tree that an
 * "arity conflict" turned out to be one side counting an injected parameter.
 *
 * The argument is typed `void *` here for the reason slice7_82.h gives for
 * g_pBrActiveFlags82: slice3_33.h states that its BrUiPhase and slice2_26.h's
 * BrPhase must not meet in one translation unit, and a `void *` parameter
 * lets this module supply the definition without picking a winner. Three
 * different pointee types are already declared for these names across
 * slice2_26.h, slice2_25.h and slice7_81.c; none of them is this header's to
 * change.
 *
 * While g_pBrUiBuildCtx86 is NULL every one of the five returns immediately,
 * which is the retired stub's behaviour exactly. */
void BrPhaseEnterPlaceholder_1004A580(void *pSelf);
void BrPhaseEnterPlaceholder_1004B430(void *pSelf);
void BrPhaseEnterPlaceholder_1004BDC0(void *pSelf);
void BrPhaseEnterPlaceholder_1004C4A0(void *pSelf);
void BrOptFn1004CAC0(void *pSelf);

/* =========================================================================
 * 2. The two CD entry points -- ADAPTERS onto br_audio.c
 * =========================================================================
 *
 * 0x100027C0 (slice5_63.c's BrCdTrackPlay) branches on the backend selector
 * and calls one of these two. Both are already ported, as policy, inside
 * br_audio.c: BrAudioPlayTrack carries 0x100027F0's clamp verbatim and says
 * so in a comment at port/src/br_audio.c:250.
 *
 *   0x10002870  (88 bytes, Glide 0x10002BA0, `body` match) -- the REDBOOK
 *               path. Records the track, then posts MM_MCINOTIFY (0x3B9) to
 *               the app window so the notify handler re-issues MCI_PLAY.
 *               NOTE it does NOT clamp: the clamp lives only in the sibling.
 *   0x100027F0  (122 bytes, Glide 0x10002B20, `body` match) -- the EAR path.
 *               Clamps into [first, count] and then drives the middleware.
 *
 * TRACK NUMBERING IS THE ONE JUDGEMENT CALL, and it is stated rather than
 * hidden. The originals take CD track numbers, in which the data track is 1
 * and music is 2..13. br_audio.h says in as many words that it "uses 0-based
 * track INDICES and leaves the +2 to the caller". slice5_63.c's BrCdTrackPlay
 * is a faithful transcription of 0x100027C0 and therefore still passes CD
 * numbers, so the conversion has to happen here -- this adapter is the seam
 * between the two conventions and the only place it can go without editing a
 * body. The base is a variable, not a literal, because the N64 soundtrack the
 * port actually ships has no data track. */
#define BR86_CD_FIRST_MUSIC_TRACK  2

extern int g_iBrCdFirstTrack86;

void BrSub10002870(int track);
void BrSub100027F0(int track);

/* =========================================================================
 * 3. GlobalAlloc / GlobalLock -- the missing half of slice7_82's set
 * =========================================================================
 *
 * slice7_82.c supplies BrGlobalHandle / BrGlobalUnlock / BrGlobalFree and
 * models GMEM_FIXED, where the handle IS the pointer. These two complete the
 * set on the same model, and they are needed by 0x1003D0B0 below (and by
 * 0x1003D210 and 0x1003D480, which are not in this packet).
 *
 * The original passes 0x42 == GMEM_MOVEABLE | GMEM_ZEROINIT. Under the
 * GMEM_FIXED model a moveable block and a fixed one are indistinguishable to
 * every caller in this tree, because every one of them uses the
 * `GlobalFree(GlobalHandle(p))` idiom and never keeps a bare handle across a
 * lock boundary. GMEM_ZEROINIT is honoured for real: the block IS zeroed.
 * DEVIATION, and it is the only one: a caller that asked for a non-zeroing
 * flag would get a zeroed block. Nothing in this tree does. */
#define BR86_GMEM_MOVEABLE  0x0002u
#define BR86_GMEM_ZEROINIT  0x0040u

void *BrGlobalAlloc(uint32_t uFlags, uint32_t cb);
void *BrGlobalLock(void *hMem);

/* =========================================================================
 * 4. 0x1003D0B0 -- size it, allocate it, fill it     [8 static call sites]
 * =========================================================================
 *
 * Glide 0x10036740, 127 bytes, `body` match with D3D 0x1003D0B0. The highest
 * static demand of anything in br_stubs.c that is portable at all: eight
 * `call` sites in BRD3D.dll's .text and eight in BRGlide.dll's, plus
 * slice2_26.c:171.
 *
 * The two-call "ask for the size, then ask for the data" idiom, over the host
 * object's vtable slot +0x58:
 *
 *     cb = 0;
 *     hr = pHost->vtbl[0x58](pHost, NULL, &cb);
 *     if (hr != DPERR_BUFFERTOOSMALL) return hr;          // 0x8877001E
 *     p = GlobalLock(GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, cb));
 *     if (!p) return E_OUTOFMEMORY;                       // 0x8007000E
 *     hr = pHost->vtbl[0x58](pHost, p, &cb);
 *     if (hr >= 0) { *ppOut = p; p = NULL; }              // ownership moves
 *     if (p) { GlobalUnlock(GlobalHandle(p)); GlobalFree(GlobalHandle(p)); }
 *     return hr;
 *
 * TWO THINGS THE LISTING SAYS THAT THE SHAPE DOES NOT.
 *
 *  1. THE SIZE OUT-PARAMETER IS THE CALLER'S FIRST ARGUMENT SLOT. At
 *     0x100367 4D the function computes `lea ecx,[esp+0x10]` while esp is
 *     entry-12, i.e. ecx == entry+4 == the incoming `pHost` slot, and passes
 *     THAT as the &cb out-parameter. `pHost` itself is already safe in ebx.
 *     MSVC reusing a dead argument slot as a local is ordinary; reading the
 *     displacement without tracking esp is how it stops being ordinary. Two
 *     of the three later reads of that slot (0x10036762, 0x10036792) are at
 *     DIFFERENT displacements -- 0x14 and 0x18 -- because a `push edi` has
 *     moved esp four bytes between them. Same slot, three numbers.
 *     [CONVENTIONS.md: a stack displacement is meaningless without its ESP.]
 *
 *  2. THE SUCCESS PATH DOES NOT FREE, AND IT SAYS SO BY CLEARING esi. The
 *     cleanup at 0x1003679A is `test esi,esi / je end`, and the only write
 *     to esi between the second vtable call and there is `xor esi,esi` at
 *     0x10036798, on the hr >= 0 arm only. The block's ownership passes to
 *     *ppOut. On the failure arm esi still holds the block and it IS freed,
 *     so *ppOut is left untouched -- which is why slice2_26.c initialises
 *     its `pItem` to NULL before the call and tests it afterwards.
 *
 * The return is a bare `ret`: cdecl, and the HRESULT really is returned.
 *
 * SIGNATURE CONFLICT, REPORTED. The same address is declared twice:
 *     slice2_13.c:41   int32_t BrSub1003D0B0(BrDPlay4Obj *, void **)
 *     slice2_26.h:296  void    BrExt_1003D0B0(BrHost *, BrHostItem **)
 * The disassembly returns an HRESULT in eax, so int32_t is right and `void`
 * is the lossy form. slice2_26.h's own comment says the return "is discarded
 * by every caller in this range", which is true of that range and is not a
 * statement about the function. Only the `BrExt_` name is stubbed, so only it
 * is defined here, and it is defined with slice2_26.h's `void` so the one
 * caller's declaration is not contradicted. The result is computed and
 * dropped, exactly as slice5_63.c does for 0x1007AC00.
 *
 * Typed `void *` for the same reason as section 1: slice2_13.h and
 * slice2_26.h model the same object under two names and this header owns
 * neither. The vtable slot is reached at byte offset 0x58, which is
 * BrHostVtbl::aReserved[22]. */
void BrExt_1003D0B0(void *pHost, void **ppOut);

/* The DirectPlay HRESULTs the original tests by hand. Same values
 * slice2_13.h already spells out; repeated as a local name rather than
 * included, because slice2_13.h cannot be pulled in here (see above). */
#define BR86_DPERR_BUFFERTOOSMALL  ((int32_t)0x8877001E)
#define BR86_E_OUTOFMEMORY         ((int32_t)0x8007000E)

/* =========================================================================
 * 5. 0x10071480 -- silence one peer's voices
 * =========================================================================
 *
 * Glide 0x1006A3F0, 79 bytes, `body` match. slice2_13.c calls it from the
 * DirectPlay "player destroyed" message with that player's id.
 *
 *     for (i = 0; i < 16; ++i) {
 *         lock(peer[i].hMutex);
 *         if (peer[i].f04 == id && (peer[i].f2C & 0x3F) < 5)
 *             peer[i].f2C = 0;                      // the WHOLE dword
 *         unlock(peer[i].hMutex);
 *     }
 *
 * THE TABLE EXTENT IS ARITHMETIC AND IT AGREES WITH slice1_05.h. The cursor
 * starts at 0x11786854 and runs while it is < 0x1178FF14, in steps of 0x96C;
 * 0x1178FF14 - 0x11786854 == 0x96C0 == 16 * 0x96C, so SIXTEEN records, and
 * 0x11786854 - 0x2C == 0x11786828 is slice1_05.h's BR_PEER table base. The
 * cursor is offset by 0x2C because f2C is the field the loop writes; the two
 * reads at cursor-0x2C and cursor-0x28 are f00 and f04.
 *
 * `and ecx,0x3f / cmp cl,5 / jge skip` is a SIGNED byte compare, but the
 * masked value is 0..63 so it is simply `< 5`. The clear that follows writes
 * the full dword, not just the masked bits -- preserved.
 *
 * DEVIATION, and it is slice1_05.c's, not a new one: that module already
 * drops the per-record mutex around BrPeerFind and records why. The lock is
 * kept here as an OPTIONAL hook so a threaded host can put it back, and
 * peer[i].hMutex is modelled by slice1_05.h as a uint32_t rather than a host
 * handle -- that is what makes the hook take a uint32_t. */
void BrSub10071480(uint32_t idPlayer);

/* =========================================================================
 * 6. 0x10072270 -- stop the streaming thread
 * =========================================================================
 *
 * Glide 0x1006B1E0, 92 bytes, `body` match.
 *
 *     if (!g_fRunning) return;
 *     SetEvent(g_hWake);
 *     WaitForSingleObject(g_hThread, INFINITE);
 *     CloseHandle(g_hThread);   g_hThread = NULL;
 *     CloseHandle(g_hWake);     g_hWake   = NULL;
 *     g_fRunning = 0;
 *
 * THE ORDER OF THE TWO CloseHandle CALLS IS NOT THE ORDER OF THE TWO CLEARS,
 * and the listing has to be read carefully to see it: g_hWake is loaded into
 * eax at 0x1006B214, BEFORE g_hThread is cleared at 0x1006B219, and only
 * closed at 0x1006B224. Preserved as written.
 *
 * The three globals (Glide 0x1184C078 / 0x11849E60 / 0x1184C07C) are
 * referenced NOWHERE else in port/, so this module owns them outright, on the
 * same ground slice8_83.c owns BrNetAnnounce's three. */
extern int32_t g_fBrSndThread86;      /* Glide 0x1184C078 */
extern void   *g_hBrSndWake86;        /* Glide 0x11849E60 */
extern void   *g_hBrSndThread86;      /* Glide 0x1184C07C */

void BrSub10072270(void);

/* =========================================================================
 * 7. 0x100484E0 -- re-seat the page vtable
 * =========================================================================
 *
 * Seven bytes: `mov dword ptr [ecx], 0x1008F6F8 / ret`. __thiscall, and the
 * one thing it does is store the vtable -- the tail of an MSVC destructor.
 * slice3_32.c:682 calls it from BrUiPageDelete_100484C0 and it is the reason
 * both this and the vtable object below had to land together: while
 * BrUiPageVtbl_1008F6F8 was a stub FUNCTION, this store would have aimed a
 * live object's vtable pointer at executable code. */
/* slice3_32.h:531 already declares this address as
 * `void BrSub100484E0(BrUiPage *)`, which is what the store implies and is
 * kept; the definition in the .c uses that type. */
void BrSub100484E0(BrUiPage *pThis);

/* =========================================================================
 * 8. 0x100751D0 and 0x1002C2C0 -- the frame timer, both ends
 * =========================================================================
 *
 * 0x100751D0 (111 bytes, Glide 0x1006E430) is __thiscall on the 0x24-byte
 * timer object at 0x106806B0 -- slice2_17.h's `pThis6806B0`, which it already
 * carries and already passes. Every field it touches is an integer, so the
 * object survives LP64 as a byte image.
 *
 *     if (!g_fTimerProbed) {                     // 0x118AB140
 *         g_fHasPerf = QueryPerformanceFrequency(&g_perfFreq);   // 0x118AB144
 *         g_fTimerProbed = 1;
 *     }
 *     if (g_fHasPerf) {
 *         *(int64 *)(this+0x00) = g_perfFreq / 30;   // via __alldiv
 *         0x10075190(this);
 *     } else {
 *         timeBeginPeriod(1);
 *         *(int32 *)(this+0x18) = 0x21;              // 33 ms
 *         0x10075190(this);
 *     }
 *     return this;
 *
 * and 0x10075190 (51 bytes, private to this pair, transcribed here as a
 * static) is the "restart the clock" half:
 *
 *     if (g_fHasPerf) { QueryPerformanceCounter(this+0x08);
 *                       *(int64 *)(this+0x10) = *(int64 *)(this+0x00); }
 *     else            { *(int32 *)(this+0x1C) = BrPlatTimeGetTime();
 *                       *(int32 *)(this+0x20) = *(int32 *)(this+0x18); }
 *
 * 30 is the LITERAL 0x1e pushed to __alldiv at 0x10075 20C -- the period is a
 * thirtieth of the counter frequency, i.e. a 30 Hz tick, not 60.
 *
 * 0x1002C2C0 (10 bytes) is the teardown: `mov ecx, 0x106806B0 / jmp
 * 0x10075240`, and 0x10075240 (18 bytes, also a static here) is
 * `if (!g_fHasPerf) timeEndPeriod(1);`. NOTE THE ecx LOAD IS DEAD -- the tail
 * call ignores its `this` entirely -- so the `void (void)` slice2_17.c
 * declares and registers with BrXAtExit is exactly right, and the object
 * pointer is a leftover of the compiler emitting a thiscall thunk.
 *
 * WHY THIS PAIR IS WORTH LANDING: with 0x100751D0 stubbed, the timer object's
 * period fields were never written at all, so the whole frame clock read as
 * zero. slice7_82.h records the same failure mode for 0x10075020 and calls it
 * out for the same reason -- a frozen clock reads at runtime as "the port is
 * hung", not as "one leaf is missing".
 *
 * The four globals (Glide/D3D 0x118AB138 / 3C / 40 / 44) are referenced
 * nowhere else in port/; this module owns them. The counter itself comes from
 * slice7_82.c's BrPlatQueryPerfFreq / BrPlatQueryPerfCounter / BrPlatTimeGetTime,
 * which are the portable replacements for exactly these three imports. */
void BrX100751D0(void *pThis);
void BrX1002C2C0(void);

/* Byte offsets into the 0x24-byte timer object, kept as names because the
 * object is a foreign byte image and nothing else in the tree types it. */
#define BR86_TMR_PERIOD    0x00   /* int64  -- ticks per frame                */
#define BR86_TMR_NOW       0x08   /* int64  -- QueryPerformanceCounter        */
#define BR86_TMR_DUE       0x10   /* int64  -- copy of PERIOD at (re)start    */
#define BR86_TMR_PERIOD_MS 0x18   /* int32  -- 0x21 in the no-counter path    */
#define BR86_TMR_NOW_MS    0x1C   /* int32                                    */
#define BR86_TMR_DUE_MS    0x20   /* int32                                    */

/* =========================================================================
 * 9. The two vtable objects
 * =========================================================================
 *
 * Read out of BRD3D.dll .rdata, not assumed:
 *
 *   0x1008F6F8  { 0x100484C0, 0x10048530 }                       2 slots
 *   0x1008F700  { 0x10048850, 0x100488B0, 0x100488C0, 0x100489A0,
 *                 0x1005AE70, 0x10048960, 0x10048B20, 0x10048AA0,
 *                 0x1005AFA0 }                                   9 slots
 *
 * The extents are pinned by their neighbours: 0x1008F6F8 + 2*4 == 0x1008F700
 * exactly, and 0x1008F700 + 9*4 == 0x1008F724, which holds 0 and is followed
 * by a different object at 0x1008F728. Both agree with slice3_32.h's structs,
 * whose slot comments were derived independently.
 *
 * FIVE OF THE NINE PHASE SLOTS CANNOT BE INSTALLED DIRECTLY, because
 * slice3_32.c gave them a LEADING BrScrGlobals * that the original does not
 * have (0x100488C0, 0x100489A0, 0x10048AA0, 0x10048B20 -- and 0x10048530 on
 * the page vtable). They go in through thunks that supply
 * g_pBrScrGlobals86; a NULL binding makes the thunk return 0 and write
 * nothing, which is the retired stub's answer.
 *
 * THREE SLOTS ARE NULL AND THAT IS DELIBERATE: 0x1005AE70 (+0x10),
 * 0x10048960 (+0x14) and 0x1005AFA0 (+0x20) have no body in port/src under
 * any name -- 0x1005AE70 is named in slice3_39.h and br_phase.h but not
 * defined, and slice3_32.h flags the other two "foreign". A NULL slot faults
 * visibly on dispatch; the alternative on offer was a pointer into a stub's
 * instruction stream, which does not. Neither is "working", and only one of
 * them is honest about it.
 *
 * ALIASED STORAGE, REPORTED AND NOT INTRODUCED HERE. br_uivt.c:88 defines
 * `const BrUiPageVtbl_ g_brUiPageVtbl_1008F6F8;` -- a second host object for
 * 0x1008F6F8, all-zero because it has no initialiser -- and exports
 * `g_pBrUiPageVtbl` pointing at it. That is CONVENTIONS.md's aliased-storage
 * hazard, found by grepping the ADDRESS: two names, no duplicate symbol, one
 * original object. This module does not merge them, because br_uivt.c's is
 * typed over br_ui.h's BrUiPage_ model and slice3_32.h's over BrUiPage, and
 * merging the two page models is the prerequisite. What changes today is that
 * ONE of the two now holds the real slots instead of neither. */
extern const BrUiPageVtbl    BrUiPageVtbl_1008F6F8;
extern const BrPhaseFullVtbl BrPhaseVtbl_1008F700;

/* =========================================================================
 * 10. WHAT THIS PACKET FOUND AND DID NOT LAND
 * =========================================================================
 *
 * 0x1002BD50  BrModelVtxResolve. DECLINED, and the reason is NOT the one
 *   slice6_74.h gives. slice6_74.h says an adapter "would have to invent
 *   which BrVtxCache instance is the one at 0x1067554C", which the NULL-
 *   binding pattern answers -- slice8_83.c answers six of exactly that shape.
 *   The real blocker is one level down: slice2_19.c:835 passes
 *   `(uint32_t *)(pItem + 0x04)`, a 32-BIT SLOT inside a byte image, and
 *   slice1_05.h's BrVtxCacheResolve takes `void **`. Casting the one to the
 *   other reads eight bytes out of a four-byte field on LP64, and marshalling
 *   through a temporary does not help either, because what comes back is a
 *   HOST pointer (BrVtxExpand's output) that does not fit in the uint32_t it
 *   would have to be stored into. The fix is for the vertex cache to hand
 *   back an INDEX, which is the same fix CONVENTIONS.md records for
 *   BrCollPlane, and it is a change to slice1_05, not an adapter.
 *
 * 0x100290A0  BrSub_100290A0. Still blocked, and slice6_73.h has it right:
 *   the third argument is an integer record index (`rec = *(char **)0x1057543C
 *   + 0x2B8 * n`, then rec[+0x20] and rec[+0x24] go to 0x10028720 ==
 *   BrTexSizeFromShiftAspect), and slice2_15.c:925 passes
 *   `g_weather.apTable[i]`, a POINTER. This is the third sighting of the
 *   index-vs-pointer conflict in this tree (0x10005DE0 and 0x10005E70 are the
 *   other two, adjudicated in slice8_83.h). Bounding the value the way
 *   slice8_83.c bounds those would work, but there is no storage anywhere in
 *   port/ for the 0x1057543C table, so the adapter would have had nothing to
 *   index. slice2_15.h's declaration has to become an integer first.
 *
 * 0x100664C0  BrX100664C0. DECLINED on LP64. It frees an array of pointers
 *   whose base is at slot+0x78 and whose count is at slot+0x7C, inside
 *   slice2_17's 0x80-byte byte-image slot records. A host pointer at a 32-bit
 *   offset in a byte image overruns the next field, which is the hazard
 *   CONVENTIONS.md names. Nothing in port/ ever WRITES slot+0x78, so the
 *   function is a no-op in this port today either way -- but a no-op written
 *   as if it were the real thing is worse than a stub, because the next
 *   reader will believe it.
 *
 * 0x1003D210  (BrFn1003D210 AND BrSub1003D210 -- one address, two stub lines,
 *   8 call sites each in both builds, the highest static demand left).
 *   DECLINED again, and now with the reason measured rather than "it is
 *   Win32": Glide 0x100368A0 calls 0x10036A30 and 0x10009A00, NEITHER of
 *   which has a body or a name anywhere in port/. Landing it would trade two
 *   stubs for two new undefined symbols. It is a chain job, not a leaf, and
 *   0x1003D0B0 above is the leaf of that same chain.
 *
 * 0x1005F5A0  (7 call sites, 237 bytes). DECLINED. It is
 *   IDirectDrawSurface::BltFast with a DDERR_SURFACELOST retry (it tests
 *   0x887601C2 and 0x8876021C by hand) over a 116-byte-record surface table
 *   at 0x10A9E360, and its private helper 0x1005F4E0 walks that same table
 *   calling IsLost/Restore. Every one of those records is a live DirectDraw
 *   object; the port has no surface backend outside port/src/gfx, which this
 *   packet may not touch. Worth recording for whoever owns that seam: the
 *   clip block at 0x1005F5F7 is entered only when `(id & 0xFFFF) == 1`, and
 *   it clamps the source rect's right/bottom to 640/480 minus the
 *   destination, then floors each against the rect's own left/top.
 *
 * 0x10070E60 / 0x10068260  DECLINED: each reaches exactly one unported
 *   callee (Glide 0x10069A80, an 828-byte fopen/strncmp table loader, and
 *   0x1006E560 respectively), so neither is a leaf either.
 *
 * SIGNATURE CONFLICTS FOUND (reported, never silently resolved)
 * ============================================================
 *   0x1003D0B0.  `int32_t (BrDPlay4Obj *, void **)` (slice2_13.c:41) vs
 *   `void (BrHost *, BrHostItem **)` (slice2_26.h:296). The disassembly
 *   returns an HRESULT; the int32_t form is right. See section 4.
 *
 *   0x1004A580 / 0x1004B430 / 0x1004BDC0 / 0x1004C4A0 / 0x1004CAC0. Reported
 *   by slice7_82.h as one argument vs two, and RESOLVED here: one argument,
 *   cdecl, slice2_26.h's arity, slice3_33.h's extra leading parameter is an
 *   injected host context. See section 1. slice7_81.c:32 declares a THIRD
 *   pointee type for 0x1004BDC0 (`BrPhase_ *`), which is a fourth name for
 *   the object rather than a fourth reading of the function.
 *
 *   0x100290A0.  `void (void *, void *, void *)` (slice2_15.h:512) vs an
 *   integer third argument in the disassembly. See above.
 *
 *   0x1002C2C0.  slice2_17.c:72 declares `void (void)` and IS RIGHT: the
 *   `mov ecx` in the original is dead across the tail call.
 */

#endif /* SLICE8_86_H */
