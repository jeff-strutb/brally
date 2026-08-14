/* slice4_53.h -- link-closing packet 53.
 *
 * Every function here is one that an already-landed module CALLS but that
 * nobody had implemented.  The names and signatures are dictated by those
 * callers; they are reproduced verbatim, including the ones that disagree
 * with the original's real arity or return type.  Where that loses
 * information a second, fuller entry point is offered alongside.
 *
 * THREE THINGS TO READ BEFORE USING THIS FILE
 *
 * 1. `BrGbiStackOverflow` (0x1007CC00) is NOT a display-list handler.  It is
 *    the CRT's `exit()`.  See the note on it below -- slice2_16 models it as
 *    a call that returns, and it does not.
 *
 * 2. Several of these addresses are already implemented in another slice
 *    under another name.  Those entries here are thin forwarders, not second
 *    implementations.  The forwarding target is named in each comment.
 *
 * 3. Three forwarders (0x10044B90, 0x10044A30, and the 0x1003C230 timer)
 *    need state that the owning slice models as a passed-in context or as a
 *    Win32 call.  They read it from the hooks at the foot of this file, which
 *    integration must wire.  They are inert, not wrong, until it is.
 */
#ifndef SLICE4_53_H
#define SLICE4_53_H

#include <stddef.h>
#include <stdint.h>

#include "slice2_25.h"      /* BrOptObj, BrOptUi, and the 0x10A9xxxx globals */
#include "slice2_26.h"      /* BrPhaseCtx                                    */

struct BrCar;               /* tag only -- slice2_15.h owns the definition   */

/* ======================================================================
 * 1. x87 one-liners
 * ====================================================================== */

/* 0x10002240  `fld [esp+4]; fsin; ret`.
 *
 * GOTCHA: x87 `fsin` is only defined for |x| < 2^63; outside that it leaves
 * the operand unchanged and sets C2 instead of returning a sine.  sinf() has
 * no such cliff, so very large inputs differ.  No caller in this build gets
 * anywhere near it. */
float BrSinF(float x);

/* 0x10002240 again.  slice2_19 calls the SAME address under this name, so
 * this is BrSinF, not a second routine.  Kept because renaming either call
 * site is not this packet's job. */
float BrSub10002240(float x);

/* 0x10002250  `fld [esp+4]; fsqrt; ret`.  Negative input yields NaN with the
 * invalid flag set, exactly as sqrtf does. */
float BrSqrtF(float x);

/* ======================================================================
 * 2. String table
 * ====================================================================== */

/* The original's table is 0x11829370, indexed by id directly, so entry 0 is
 * never reachable.  It is a .data pointer array the loader fills; there is
 * nothing to decompile in it, so it is exported here for integration to
 * populate. */
#define BR_STRING_ID_MIN   1
#define BR_STRING_ID_LIMIT 0x12F        /* first id that is OUT of range */

extern char *g_apBrStringTable[BR_STRING_ID_LIMIT];

/* 0x10074030  `id` in [1, 0x12F) ? table[id] : NULL.
 *
 * GOTCHA: both bounds tests are UNSIGNED in the original (`jb` / `jae`), so
 * a negative id is not "below the minimum" -- it wraps to a huge unsigned
 * value and fails the upper test.  The result is the same (NULL) but the
 * reason matters if anyone ever widens the range.
 *
 * NAME COLLISION -- this one address carries THREE names in the tree:
 * BrStringById (slice2_24, one argument), BrStrGet (slice2_25, one argument,
 * const char*) and BrHandleLookup (br_bits.h, TWO arguments, table first).
 * br_bits.h's two-argument form is the odd one out and slice1_07 already
 * recorded why it is wrong.  This packet implements the one-argument form
 * under slice2_24's name because that is the name in this packet's brief. */
char *BrStringById(int32_t id);

/* ======================================================================
 * 3. exit()
 * ====================================================================== */

/* 0x1007CC00.  This is the CRT's `exit(code)`: it tail-calls doexit(code,0,0)
 * at 0x1007CC50, whose siblings are _exit (0x1007CC20, doexit(code,1,0)) and
 * _cexit (0x1007CC40, doexit(0,0,1)).  doexit runs the atexit chain and ends
 * with TerminateProcess.
 *
 * GOTCHA -- slice2_16.h says "the original ... falls through as if it
 * returned, so it is modelled as a returning call".  It does not return.  The
 * code after BrGbiStackOverflow(1) in BrGbiDList (the slot-9 store) is dead
 * in the original: a display-list stack overflow kills the process.  This
 * implementation calls exit() and so is equally non-returning; it is NOT
 * marked noreturn only because slice2_16.h's declaration is not. */
void BrGbiStackOverflow(int code);

/* ======================================================================
 * 4. Two-slot vtable relay
 * ====================================================================== */

/* The object at 0x10A99780 that BrModelLoad (0x10036BD0) drives.  Only two
 * vtable slots are ever reached from here; the rest are opaque so that no
 * signature is invented for them. */
typedef struct BrModelMgrVtbl {
    void  *pfn00;                                       /* +0x00 */
    void  *pfn04;                                       /* +0x04 */
    void  *pfn08;                                       /* +0x08 */
    void *(*pfn0C)(void *pThis, void *a, void *b);      /* +0x0C */
    void  *pfn10;                                       /* +0x10 */
    void  *pfn14;                                       /* +0x14 */
    void  *pfn18;                                       /* +0x18 */
    void *(*pfn1C)(void *pThis, void *p);               /* +0x1C */
} BrModelMgrVtbl;

/* 0x100088B0  __thiscall, ret 8:  return vt->pfn1C(this, vt->pfn0C(this,a,b)).
 *
 * ARGUMENT ORDER: `a` reaches pfn0C first.  BrModelLoad already flags that
 * its own arguments cross over on the way in (it calls this with (a2, a1)),
 * so do not "fix" either side. */
void *BrSub100088B0(void *pThis, void *a, void *b);

/* ======================================================================
 * 5. Config writer (0x1006A4A0)
 * ====================================================================== */

/* The full form: writes the config object at pThis to the file named by
 * pszPath and returns 1 on success, 0 on any failure (including a failed
 * fopen, after which nothing is closed because nothing was opened).
 *
 * Layout, in the order the original emits it -- note that the four 0xA8
 * blocks that cover [0, 0x2A0) come LAST, after everything above 0x2A8, and
 * that the dword at +0x2A0 is written just before them.  +0x2A4 is the one
 * hole: it is never written. */
int BrCfgSave1006A4A0(void *pThis, const char *pszPath);

/* 0x1006A4A0  the form all three call sites declare.
 *
 * DEVIATION: the original is __thiscall and returns 1/0; slice2_25.h,
 * slice3_31.h and slice3_32.h all declare it `void`, so the status is
 * dropped here.  Use BrCfgSave1006A4A0 if you need it.  pArg is the path
 * (0x10B4FBE8 is passed by address). */
void BrSub1006A4A0(void *pThis, void *pArg);

/* ======================================================================
 * 6. Forwarders onto implementations that already exist elsewhere
 * ====================================================================== */

/* 0x1002C210 -> slice2_17's BrS17BankFlip. */
void BrGfx2C210(void);

/* 0x10031227 -> slice2_17's BrRenderCountersReset. */
void BrGfx31227(void);

/* 0x10039020 -> slice2_20's BrPoolEmit.  `this` is the car record. */
void BrCarSub9020(struct BrCar *pCar);

/* Car record stride, from 0x10035520's `imul ecx, ecx, 0x15F88`. */
#define BR_RCA_CAR_STRIDE 0x15F88u

/* 0x10037740 -> slice2_20's BrRcaLoadCar.
 *
 * GOTCHA: slice2_19.h types the second argument `void *pArg`, but the
 * original uses it as an INTEGER index -- into the name table at 0x100B84F8,
 * and as the value stored into 0x10AA3444.  0x10035520 pushes the same value
 * into the dword table at 0x106C6558, which is what made it look like a
 * pointer.  It is converted back to an int here.
 *
 * GOTCHA: 0x10037740's "is this the preview buffer" test is an identity test
 * against 0x100C12A0, and its only caller computes the destination as
 * 0x100C12A0 + i*0x15F88 -- so "preview" means exactly `i == 0`. */
void BrSub10037740(void *pCar, void *pArg);

/* 0x1003551B  `push ebp; mov ebp,esp; pop ebp; ret` -- five bytes, no body.
 * A genuine do-nothing stub in this build, like 0x10008B80 and 0x100378A0.
 * The call is kept so the call graph stays faithful. */
void BrSub1003551B(void *pCar);

/* 0x1003DA40 -> slice2_22's BrDPlaySendTag4 (tag 0x60000004).
 *
 * The gate slice2_22 asks for as an argument is the global 0x10AA288C, which
 * slice2_25 owns as g_brAA288C; it is read here so that the two-argument form
 * slice2_25 declares still behaves like the original.  See br_slots.h's
 * warning: that same dword is the slot-table counter, so a non-empty slot
 * table silently suppresses this message.
 *
 * HAZARD: slice2_25's BrOptUi and slice2_22's BrDPlayLink are two models of
 * the same object (0x10A9D008) and only agree on a 32-bit host -- BrOptUi
 * uses int32_t where BrDPlayLink uses void*.  The cast here is exact on the
 * original's ABI and wrong on LP64.  Reconciling the two structs is a
 * integration job, not a per-packet one. */
void BrSub1003DA40(BrOptUi *pUi, int a);

/* 0x10041B50 -> slice2_24's BrMenuAutoSaveName. */
void BrSub10041B50(void);

/* 0x10044B90 -> slice2_26's BrPhaseActivate_10044B90.
 * slice2_24 declares an int argument; the original takes none and this
 * ignores it.  Needs BrSlice4SetPhaseCtx. */
void BrMenuSub10044B90(int32_t n);

/* 0x10044A30 -> slice2_26's BrPhaseLeave_10044A30.
 * slice2_25 declares it as a one-argument BrOptObj::pfn08 hook; slice2_26
 * implements it as (ctx, entity).  The single argument IS the entity.
 * Needs BrSlice4SetPhaseCtx. */
void BrOptFn10044A30(BrOptObj *pThis);

/* ======================================================================
 * 7. Session timer (0x1003C230)
 * ====================================================================== */

/* 0x10A9BFDC -- whatever SetTimer returned.  No other slice models it. */
extern uint32_t g_brA9BFDC;

/* DEVIATION: USER32's SetTimer is behind a hook, following the precedent
 * slice1_07 set for MessageBoxA.  The default returns idEvent, so
 * g_brA9BFDC ends up non-zero exactly as a successful SetTimer would leave
 * it.  Signature mirrors the original's push order: (hWnd, id, ms, proc). */
typedef uint32_t (*BrPlatSetTimerFn)(void *hWnd, uint32_t idEvent,
                                     uint32_t uElapseMs, void *pfnProc);
extern BrPlatSetTimerFn g_pfnBrPlatSetTimer;

/* The full form: returns the original's 1. */
int BrTimerStart1003C230(void);

/* 0x1003C230  the form slice2_25 declares (void, and drops the 1):
 *   0x1003C020, then SetTimer(0x10680584, 1, 1000, NULL) -> 0x10A9BFDC,
 *   then 0x10A9CFFC = 1. */
void BrSub1003C230(void);

/* ======================================================================
 * 8. Integration wiring
 * ====================================================================== */

/* slice2_26 deliberately gathers its globals into BrPhaseCtx and passes it
 * in; the two callers in this packet are zero/one-argument function pointers
 * that cannot.  Set this once at startup to the same context slice2_26 is
 * driven with.  While it is NULL both forwarders do nothing. */
extern BrPhaseCtx *g_pBrSlice4PhaseCtx;
void BrSlice4SetPhaseCtx(BrPhaseCtx *pCtx);

#endif /* SLICE4_53_H */
