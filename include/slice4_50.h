/* slice4_50.h -- a later pass, slice 4 ("close the broken links").
 *
 * WHAT THIS MODULE IS
 * ===================
 * Not an address range. Fourteen individually-requested functions that some
 * already-ported module calls but nobody implemented. Twelve are here; two
 * are skipped (see SKIPPED below and the report).
 *
 * READ THIS FIRST -- THE PACKET'S ASM WAS MISPAIRED
 * =================================================
 * work/slice4/agent50.asm attaches a `; ===== WANTED AS: <name>` banner to a
 * disassembly listing. For NINE of the fourteen the listing underneath the
 * banner is NOT the function the name refers to; it is a DIFFERENT, nearby
 * function that other headers already declare separately:
 *
 *     WANTED AS              packet listing      real address
 *     BrFatal                sub_10060E90        0x10035BBA
 *     BrNetSendFlush         sub_10004E50        0x100053F0
 *     BrSub10072AF0          sub_10071130        0x10072AF0
 *     BrSub1003C150          sub_1003BF60        0x1003C150
 *     BrSub1003C260          sub_1003C1E0        0x1003C260
 *     BrSub1003D950          sub_1003D210        0x1003D950
 *     BrSub10043BF0          sub_10041B50        0x10043BF0
 *     BrOptFn100558A0        sub_10051990        0x100558A0
 *     BrOptFn100575F0        sub_10056A10        0x100575F0
 *
 * The mispairing is provable, not a guess: slice2_25.h declares BOTH members
 * of every pair as separate externs, and for one pair the arity settles it --
 * `BrSub1003D950(BrOptUi *, int)` is 2 arguments, while sub_1003D210 (the
 * listing the packet supplied under that banner) reads three stack arguments
 * and matches slice2_25.h's `BrSub1003D210(void *, BrOptUi *, int)` exactly.
 *
 * Everything below was therefore decompiled from the asm/ dumps at the address the
 * WANTED name encodes, and the names/signatures are the ones the calling
 * modules already declare. The packet listings for the four correctly-paired
 * functions (BrSprintf, BrHookIsCurrent, BrMat4Perspective7,
 * BrMenuSub10044E20, BrSub10075020) agree with asm/ byte for byte.
 *
 * THREE OF THE FOURTEEN ARE ALREADY IMPLEMENTED UNDER ANOTHER NAME
 * ================================================================
 * The link is broken only because of the name. They are forwarders here, not
 * second copies of the body -- one address, one implementation:
 *
 *     BrFatal            0x10035BBA -> slice2_19.c's BrLogSet
 *     BrSub10072AF0      0x10072AF0 -> slice1_08.c's BrSndPlaySimple
 *     BrMat4Perspective7 0x10030930 -> br_mat.c's BrMat4Perspective (+scale)
 *
 * SKIPPED
 * =======
 * BrOptFn100558A0 (0x100558A0) and BrOptFn100575F0 (0x100575F0). See the
 * report; in short, they are slice3_33-family menu-screen builders whose
 * controls use fields (+0x2B5C sub-object, +0x2B65 string buffer,
 * +0x2F78/+0x2F80/+0x2F84/+0x2F88/+0x2F8C, +0x50..+0x5C) that slice3_33.h's
 * BrUiCtl does not model. Writing a second, competing BrUiCtl here is exactly
 * the failure the contract calls out, so they are left to whoever owns
 * slice3_33.h.
 */
#ifndef SLICE4_50_H
#define SLICE4_50_H

#include <stdint.h>

#include "br_mat.h"      /* BrMat4 */
#include "slice1_03.h"   /* BrComObj, BrComCallLocked68 */
#include "slice2_25.h"   /* BrOptObj, BrOptUi, BrGameObj, BrDPlay, globals */

/* The original allocation literals. Asked for as max(sizeof, literal) so a
 * 64-bit host, where the modelled structs grow past the 32-bit size, does not
 * under-allocate. Same DEVIATION slice3_33.c documents. */
#define BR50_ALLOC(type, cbOrig) \
    ((uint32_t)(sizeof(type) > (size_t)(cbOrig) ? sizeof(type) : (size_t)(cbOrig)))

#define BR50_OPTOBJ_ORIG_SIZE  0xC8u   /* operator new literal, 0x10044E20 */

/* ==========================================================================
 * 1. Logging / formatting
 * ========================================================================== */

/* 0x10035BBA -- declared by slice2_18.h, slice3_41.h and slice2_20.c.
 *
 * GOTCHA: "Fatal" IS A MISNOMER. The function does not abort, exit, longjmp
 * or even set a flag: it stores the message pointer in the global at
 * 0x106C2CF0 and calls the logger at 0x10035BA7. Every caller resumes
 * normally on the next line, and slice2_18.c/slice2_20.c are written that
 * way. The name comes from the strings passed to it ("HUGE GLIST ERROR",
 * "File %s missing"), not from its behaviour.
 *
 * 0x10035BBA is already implemented as slice2_19.c's BrLogSet; this is a
 * forwarder so the address keeps exactly one body. */
void BrFatal(const char *pszMsg);

/* 0x1007C830 -- declared by slice2_25.h and slice2_20.c.
 *
 * MSVC `sprintf`: the original builds a fake FILE on the stack (+0x00 _ptr,
 * +0x04 _cnt = 0x7FFFFFFF, +0x08 _base, +0x0C _flag = 0x42) and hands it to
 * _output (0x10080750) with the va_list. UNBOUNDED -- _cnt is INT_MAX, there
 * is no size argument anywhere. Returns _output's count, i.e. the number of
 * characters written, NOT counting the NUL.
 *
 * GOTCHA: the NUL terminator is written only on the `_cnt-1 >= 0` path; when
 * the counter underflows the original calls _flsbuf(0, &file) instead. With
 * _cnt starting at INT_MAX that path is unreachable in practice. */
int BrSprintf(char *pszDest, const char *pszFmt, ...);

/* ==========================================================================
 * 2. Sound
 * ========================================================================== */

/* 0x10072AF0 -- declared by slice2_25.h and slice3_32.h as
 * `void BrSub10072AF0(int a, int b)`.
 *
 * CONFLICT (pre-existing, reported): slice1_08.h already names this address
 * BrSndPlaySimple and gives it the far better signature
 * `int32_t BrSndPlaySimple(int32_t group, uint32_t packed)` -- it is
 * BrSndPlayGroup(group, packed, loop=0), which is BrSndPlayEx(group, slot=1,
 * packed, loop). The two callers throw the result away and spell the second
 * argument as a plain int (they pass 0x200020). This is a forwarder; the
 * integration should eventually retire this name in favour of
 * BrSndPlaySimple. */
void BrSub10072AF0(int a, int b);

/* ==========================================================================
 * 3. Hooks
 * ========================================================================== */

/* 0x106C0964 -- RESOLVED, and it was a three-way alias.
 *
 * This header used to declare `g_brHook6C0964` here while slice1_05.h modelled
 * the SAME dword as a member `BrHooks::pfnC`, and a third module modelled it
 * again under BRGlide's number: 0x106E79F4, br_gamestep.c's game-step slot.
 * shared.csv pairs 0x10034C51/0x10034C66/0x10034C73 with BRGlide's
 * 0x1002E302/0x1002E317/0x1002E324 as byte-identical, so the three accessors
 * are one set and the storage is one object. br_gamestep.c owns it; the
 * globals here and in slice1_05.h are gone.
 *
 * That also names the slot. slice2_19.c gates the pad's two extra buttons on
 * `BrHookIsCurrent(g_BrPadHookFn)`, and `g_BrPadHookFn` is the literal
 * 0x1002C500, which shared.csv pairs with BRGlide 0x10019A70 -- the race
 * step. The test reads "is a race the thing currently running". */

/* 0x10034C51 -- declared by slice2_19.h. Returns 1 when the game-step slot
 * equals pfn, 0 otherwise. No null handling of any kind in the original, so
 * with nothing installed NULL "is current". Body in br_gamestep.c. */
int BrHookIsCurrent(const void *pfn);

/* ==========================================================================
 * 4. Projection
 * ========================================================================== */

/* 0x10030930 -- guPerspectiveF, declared by slice2_19.h. SEVEN arguments.
 *
 * t = tan(fovy * pi/360) * n;  r = t * aspect;
 * BrMat4Frustum(pM, -r, r, -t, t, n, f);  *pPerspNorm = 1;
 *
 * GOTCHA: `scale` IS DEAD. The original does push it -- it becomes an eighth
 * dword on the call to 0x10030810 -- but br_mat.h establishes that
 * 0x10030810 (guFrustumF here) takes only seven arguments and never reads
 * [esp+0x20]. So the value is passed and discarded. Both call sites in
 * slice2_19.c pass 1.0f.
 *
 * GOTCHA: the original's return value is NOT a status. The last thing it does
 * is `mov eax,[esp+0x28]` to reload pPerspNorm for the `mov word [eax],1`,
 * and eax is still that pointer at `ret`; BrMat4Frustum's status was
 * clobbered. Neither caller reads it.
 * DEVIATION: this returns BrMat4Frustum's status instead of a pointer cast to
 * int, which is both meaningful and portable.
 *
 * 0x10030930 is already implemented as br_mat.c's six-argument
 * BrMat4Perspective; this is the seven-argument form slice2_19.h asks for,
 * forwarding rather than duplicating the body. */
int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm,
                       float fovyDegrees, float aspect,
                       float n, float f, float scale);

/* ==========================================================================
 * 5. Screen-object installers  (0x10044E20, 0x10043BF0)
 * ========================================================================== */

/* The two enter-hooks these installers poke into BrOptObj::pfn04. Both are
 * only ever STORED as code addresses, so -- following slice3_33.h's
 * BrUiBuildHooks precedent -- they are reached through a table instead of
 * through fresh externs, so that neither address gains another name.
 * 0x1005A6E0 is slice2_26.h's BrExt_1005A6E0 (declared there over a
 * differently-modelled first parameter, hence not used directly). */
typedef struct BrOptEnterHooks {
    BrOptObjFn p1005A6E0;   /* 0x1005A6E0 -- installed by 0x10044E20 */
    BrOptObjFn p100563E0;   /* 0x100563E0 -- installed by 0x10043BF0 */
} BrOptEnterHooks;

extern BrOptEnterHooks g_brOptEnterHooks;

/* --- globals these two installers own ------------------------------------ */

/* 0x10ACEE8C / 0x10ACEE94 -> 0x10AA28CC / 0x10AA28C8. slice2_26.h models the
 * four as ctx fields (nACEE8C/nACEE94/nAA28CC/nAA28C8) and slice2_24.h models
 * the two destinations as FLOATS (gAA28C8/gAA28CC). 0x10044E20 moves them
 * with plain 32-bit `mov`s, which settles nothing about their type, so the
 * bit-preserving int32_t of slice2_26.h is used here. */
extern int32_t g_brACEE8C;   /* 0x10ACEE8C */
extern int32_t g_brACEE94;   /* 0x10ACEE94 */
extern int32_t g_brAA28CC;   /* 0x10AA28CC */
extern int32_t g_brAA28C8;   /* 0x10AA28C8 */

/* 0x10AA2968 -- the slot 0x10044E20 caches its object in.
 * (slice2_26.h models it as BrPhase *pAA2968 inside a ctx struct.) */
extern BrOptObj *g_brPAA2968;

/* 0x10AA2958 -- the slot 0x10043BF0 caches its object in.
 *
 * CONFLICT (reported): slice2_25.h declares this same address as
 * `int32_t g_brAA2958` and slice2_25.c defines it that way, because there it
 * is only tested against zero and zeroed. 0x10043BF0 stores a heap POINTER in
 * it. The integration must fold the two into this pointer-typed one; an
 * int32_t truncates the pointer on a 64-bit host. */
extern BrOptObj *g_brPAA2958;

/* 0x100AD300 -- the address 0x10043BF0 hands to 0x100419D0. slice2_26.h
 * models it as the ctx field p0AD300. */
extern void *g_brP0AD300;

/* 0x10044E20 -- declared by slice2_24.h as `void BrMenuSub10044E20(int32_t)`.
 *
 * GOTCHA: THE PARAMETER DOES NOT EXIST IN THE ORIGINAL. 0x10044E20 reads no
 * stack argument at all (its `push ecx` is the usual MSVC one-dword local).
 * slice2_24.c calls it as BrMenuSub10044E20(0); __cdecl makes that harmless.
 * The declaration is kept verbatim so the link closes.
 *
 * Copies 0x10ACEE8C/0x10ACEE94 into 0x10AA28CC/0x10AA28C8, then lazily
 * creates the 0xC8-byte screen object, installs 0x1005A6E0 as pfn04, calls
 * it, and sets f0C = f68 = 1. On the cached path it only republishes the
 * object into 0x10AA2904 and returns -- the two copies above still happen
 * first, every call. */
void BrMenuSub10044E20(int32_t n);

/* 0x10043BF0 -- declared by slice2_25.h as
 * `void BrSub10043BF0(BrGameObj *p)`, and called from slice2_25.c as
 * BrSub10043BF0(NULL).
 *
 * GOTCHA: same as above -- the original takes NO argument. The declaration is
 * kept verbatim.
 *
 * The same installer shape as 0x10044E20 over 0x10AA2958 and the enter hook
 * 0x100563E0, preceded by 0x100419D0(&0x100AD300) and 0x1003E510(). */
void BrSub10043BF0(BrGameObj *p);

/* ==========================================================================
 * 6. Network  (0x100053F0)
 * ========================================================================== */

/* 0x10221324 -- the mutex 0x100053F0 waits on. Opaque handle. */
extern void *g_brH221324;
/* 0x1022AAA8 -- non-zero enables the broadcast below. */
extern int32_t g_br22AAA8;
/* 0x10ACEDB0 / 0x100B36FC -- base and count of the 0x2B68-stride entity
 * array. The original reads both as globals; slice2_12.h's port of
 * 0x10005470 takes them as parameters, so they have to exist here. */
extern void   *g_brPACEDB0;
extern int32_t g_br0B36FC;
/* 0x10094294 -- the palette index slice1_02.h's BrPalFetch is driven by. */
extern int32_t g_br094294;
/* 0x10AD0854..0x10AD0856 -- the three-byte RGB triple BrPalFetch writes
 * (slice1_02.h, 0x100049C0). 0x100053F0 passes the three bytes separately. */
extern uint8_t g_brAD0854[3];
/* 0x10277B48 */
extern int32_t g_br277B48;
/* 0x10B4E2E8 -- a text buffer; slice2_23.h models it as `char *szB4E2E8`.
 * The original pushes its ADDRESS, so this is that address. */
extern char *g_brPB4E2E8;

/* XSLICE 0x10004760 -- ten __cdecl arguments; no other header names it.
 * a3/a4/a5 are byte parameters (the original loads only al/cl/dl and leaves
 * the upper 24 bits of each register holding unrelated garbage, the classic
 * MSVC "callee reads one byte" pattern). */
extern void BrNetSend4760(BrDPlay **ppDPlay, int32_t a1, int32_t a2,
                          uint8_t r, uint8_t g, uint8_t b,
                          int32_t a6, char *pszText, int32_t a8, int32_t a9);

/* 0x100053F0 -- declared by slice2_11.h, called from slice2_11.c.
 *
 * GOTCHA: the mutex is taken and released with NOTHING IN BETWEEN. The
 * WaitForSingleObject/ReleaseMutex pair at the top guards no state at all; it
 * is a rendezvous with the network thread, not a critical section. Ported
 * behind slice1_02.h's BrNetMutexLock/BrNetMutexUnlock hooks, as that header
 * already does for the same pattern.
 *
 * GOTCHA: the "everyone is here" test compares BrEntityCountActive (a plain
 * count) against BrDPlayGetCurrentPlayers, whose failure sentinel is 0xFFFF
 * (slice2_13.h). The comparison is unsigned equality, so a failed query just
 * suppresses the broadcast -- it never matches. */
void BrNetSendFlush(void);

/* ==========================================================================
 * 7. DirectPlay host / join / send  (0x1003C150, 0x1003C260, 0x1003D950)
 * ========================================================================== */

/* The frame layouts below were recovered exactly; the sizes are not guesses.
 * 0x1003C150's frame is 0x4CC: a 0xCC-byte descriptor it zeroes with
 * `rep stosd ecx=0x33`, then the message buffer at +0xD4.
 * 0x1003C260's frame is 0x734: +0x08 a DWORD length, +0x0C a 16-byte blob
 * (the gap up to the next object; a GUID-sized out-parameter), +0x1C a
 * 0x320-byte name buffer (`rep stosd ecx=0xC8`), +0x33C the message buffer. */
#define BR50_DPDESC_SIZE   0xCCu
#define BR50_DPJOIN_SIZE   0x10u
#define BR50_DPNAME_SIZE   0x320u
#define BR50_DPNAME_CB     0xC8u    /* the length handed to GetUserNameA */
#define BR50_DPMSG_SIZE    0x400u

/* XSLICE 0x1003D130 -- fills the 0xCC-byte session descriptor. */
extern void    BrSub1003D130(void *pDesc);
/* XSLICE 0x1003C5C0 -- hosts; returns an HRESULT (negative == failure). */
extern int32_t BrSub1003C5C0(BrDPlay *pDPlay, void *pDesc, BrOptUi *pUi);
/* XSLICE 0x1003D030 -- fills the 16-byte join blob; returns an HRESULT. */
extern int32_t BrSub1003D030(void *pBlob);
/* XSLICE 0x1003C740 -- joins; returns an HRESULT. */
extern int32_t BrSub1003C740(BrDPlay *pDPlay, void *pBlob,
                             char *pszName, BrOptUi *pUi);
/* XSLICE 0x10071550 */
extern int32_t BrSub10071550(void);
/* XSLICE 0x10005B10 */
extern void    BrSub10005B10(int32_t a);
/* XSLICE 0x1003E510 */
extern void    BrSub1003E510(void);

/* XSLICE 0x10042AF0, one-argument form.
 *
 * CONFLICT (reported): slice2_18.h declares this address and arity as
 * `void BrGfx42AF0_1(void *)`. 0x1003C260 TESTS the returned eax and gives up
 * when it is zero, so the function does return a value and the `void`
 * declaration is the lossy one. Reached through a pointer here so that this
 * header does not contradict slice2_18.h. */
extern int32_t (*g_brPfn42AF0_1)(void *p0);

/* ADVAPI32 GetUserNameA. Platform hook; returns non-zero on success and
 * updates *pcb. The original ignores the result either way. */
extern int32_t BrPlatGetUserName(char *pszBuf, uint32_t *pcb);

/* 0x1003C150 -- declared by slice2_25.h. Host a session.
 *
 * GOTCHA: THE ERROR MESSAGE IS FORMATTED AND THROWN AWAY. On failure the
 * original builds "Could not host session because of error 0x%08X" into a
 * ~1KB stack buffer with 0x1007C830 and then returns without showing it to
 * anybody. 0x1003C260 does exactly the same with its own string. Reproduced
 * (the buffer is the observable part of the frame); do not "fix" it into a
 * message box. */
void BrSub1003C150(void);

/* 0x1003C260 -- declared by slice2_25.h. Join a session. Returns 1 on the
 * success paths AND on all three early-out paths, 0 only on a real failure.
 *
 * GOTCHA: 0x88770820 (DPERR_USERCANCEL) is retried, not failed: the original
 * runs 0x10042AF0 on the name buffer and, if that returns non-zero, calls the
 * join again with the same arguments.
 *
 * GOTCHA: the guard is asymmetric. The original tests 0x10AA29D8 for NULL and
 * then dereferences 0x10AA29D4 -- a DIFFERENT global -- with no check at all.
 * DEVIATION: 0x10AA29D4 is null-checked here (treated like the 29D8 == NULL
 * case, i.e. return 1). */
int BrSub1003C260(void);

/* 0x1003D950 -- declared by slice2_25.h as
 * `int32_t BrSub1003D950(BrOptUi *pUi, int a)` (early-outs return 0; the
 * send path returns the HRESULT).
 *
 * Sends an 8-byte packet {0x60000002, a} through slice1_03.h's
 * BrComCallLocked68 (IDirectPlay4A::Send, vtable +0x68), from player 0 to
 * player 1, flags... -- the literal argument list is (pUi->p00, pUi->p08,
 * 0, 1, &packet, 8).
 *
 * GOTCHA: gated on 0x10AA288C being ZERO. br_slots.h warns that 0x10AA288C is
 * dual-purpose; here it is purely a "suppress transmission" gate, matching
 * how slice2_22.h describes its `fGate`.
 *
 * CONFLICT (reported): slice2_25.h models BrOptUi as three int32_t, but
 * 0x1003D950 dereferences +0x00 and calls through +0x08, i.e. both are
 * POINTERS. Reading them as int32_t truncates on a 64-bit host, so this
 * implementation takes them through a pointer-sized view of the same object.
 * BrOptUi's field types want fixing in slice2_25.h. */
int32_t BrSub1003D950(BrOptUi *pUi, int a);

/* ==========================================================================
 * 8. Millisecond clock  (0x10075020)
 * ========================================================================== */

/* 0x100BBAD4 -- non-zero in the image; cleared by the first call, so the
 * whole calibration block runs exactly once. */
extern int32_t g_br0BBAD4;
/* 0x118AB120 (64-bit) -- QueryPerformanceFrequency's result. */
extern int64_t g_br18AB120;
/* 0x118AB128 -- QueryPerformanceFrequency's BOOL. Zero => fall back to
 * timeGetTime forever. */
extern int32_t g_br18AB128;
/* 0x118AB130 -- the millisecond value of the first sample, subtracted from
 * every later one. */
extern int32_t g_br18AB130;

extern int32_t  BrPlatQueryPerfFreq(int64_t *pFreq);
extern int32_t  BrPlatQueryPerfCounter(int64_t *pCount);
extern uint32_t BrPlatTimeGetTime(void);

/* 0x10075020 -- declared by slice3_32.h as `int32_t BrSub10075020(void)`.
 *
 * Milliseconds since the first call: (counter * 1000 + 500) / frequency,
 * minus the same expression sampled during calibration. The 64-bit multiply
 * and divide are the CRT's _allmul (0x1007ED20) and _alldiv (0x1007FD10), so
 * the division is SIGNED.
 *
 * GOTCHA: the +500 is not a rounding of the result, it is added to the
 * numerator BEFORE the divide -- i.e. round-to-nearest on the millisecond,
 * applied identically to the base and to every sample, so it cancels.
 *
 * GOTCHA: the fallback is chosen ONCE, from QueryPerformanceFrequency's BOOL,
 * but is also re-checked per call against QueryPerformanceCounter's BOOL. The
 * two clocks have unrelated epochs, so a mid-run QPC failure makes the
 * returned value jump; the original does not care.
 *
 * GOTCHA: the calibration block ignores QueryPerformanceCounter's return
 * value and divides by the frequency unconditionally.
 * DEVIATION: a zero frequency is guarded here (base := 0) instead of dividing
 * by zero. */
int32_t BrSub10075020(void);

#endif /* SLICE4_50_H */
