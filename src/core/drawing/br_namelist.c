/* br_namelist.c -- drawing: the hundred-slot name list and its vtable.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_06.c, an address batch and not a module.  The four
 * functions here are one connected group in the original -- glide
 * 0x10055960..0x10055A30, contiguous -- and they share the object whose
 * function-pointer table sits at 0x10077750: the constructor that fills a
 * hundred name slots with the same starting text, the scalar deleting
 * destructor, the table installer, and the forwarder that drives two of the
 * table's slots.
 *
 * The preamble below is slice1_06.c's, carried over verbatim: the header's
 * port prototype for BrNameListInit has to stay hidden for the matching twin
 * to define the real thiscall symbol, and an include set that looks
 * redundant has already been shown elsewhere in this module to move VC5's
 * register allocation (see br_rdpmode.c).
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
typedef char br06_assert_devrec[
    (sizeof(BrDevRec) == BR_DEVREC_STRIDE) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x1005CB90
 * ========================================================================== */

/* WHAT IT DOES: sets up a list of a hundred name slots and writes the same
 * starting text into every one of them, so an unused slot reads as something
 * rather than as blank. */
/* @implements 0x1005CB90 d3d BrNameListInit */
#ifdef BR_MATCHING_BUILD
/* thiscall ctor, no stack args: vtbl (0x10077750) stored, the whole slot
 * array zeroed once, then 100 inline strcpy()s of the fixed name string
 * (0x10396F08) with the dest walking 0x104. Returns this. */
extern char DAT_10396f08[];
extern int  DAT_10077750;

BrNameList *__fastcall BrNameListInit(BrNameList *pThis, int _edx_unused)
{
    char *d = (char *)pThis->asz;
    int   n;

    (void)_edx_unused;
    pThis->pVtbl = (const void *)&DAT_10077750;
    memset(d, 0, sizeof(pThis->asz));

    n = BR_NAMELIST_COUNT;
    do {
        strcpy(d, DAT_10396f08);
        d += BR_NAMELIST_STRIDE;
    } while (--n != 0);

    return pThis;
}
#else
/* WHAT IT DOES: sets up a list of a hundred name slots and writes the same
 * starting text into every one of them, so an unused slot reads as something
 * rather than as blank.  Port arm of the same function. */
/* @implements 0x1005CB90 d3d BrNameListInit */
BrNameList *BrNameListInit(BrNameList *pThis, const void *pVtbl,
                           const char *pszFill)
{
    int i;

    pThis->pVtbl = pVtbl;
    memset(pThis->asz, 0, sizeof(pThis->asz));

    /* The original re-reads the source string (and re-runs strlen on it) on
     * every one of the 100 iterations. */
    for (i = 0; i < BR_NAMELIST_COUNT; i++) {
        size_t cb = strlen(pszFill) + 1u;

        /* DEVIATION: the original copies strlen+1 bytes with no bound. A
         * source longer than 0x103 characters overruns into the next slot.
         * Truncated here. */
        if (cb > BR_NAMELIST_STRIDE) {
            cb = BR_NAMELIST_STRIDE;
        }
        memcpy(pThis->asz[i], pszFill, cb);
        pThis->asz[i][BR_NAMELIST_STRIDE - 1] = '\0';
    }

    return pThis;
}
#endif

/* ==========================================================================
 * 0x1005CB40
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* Original is thiscall: `this` in ecx, one stack argument, `ret 4`.  VC5 C
 * has no __thiscall keyword; __fastcall puts the first REGISTER-ELIGIBLE
 * argument in ecx, and a struct is never register-eligible, so a 4-byte
 * struct in second position is forced onto the stack.  Same split as
 * thiscall.  Both virtual calls below use the same trick:
 *   vt+8    thiscall(this, arg)         ecx still holds this from entry
 *   vt+0x1C thiscall(this, &scratch)    lea ecx, [scratch] / push ecx /
 *                                       mov ecx, this
 * scratch is the one stack slot (`push ecx` at entry), seeded -1, and is
 * what the function returns. */

typedef struct { uint32_t v; } BrSub1005CB40Arg;
typedef struct { uint32_t *p; } BrSub1005CB40Ref;

typedef struct BrSub1005CB40Vtbl {
    void *f00;
    void *f04;
    void (__fastcall *f08)(void *pThis, BrSub1005CB40Arg a);
    void *f0C;
    void *f10;
    void *f14;
    void *f18;
    void (__fastcall *f1C)(void *pThis, BrSub1005CB40Ref a);
} BrSub1005CB40Vtbl;

typedef struct BrSub1005CB40Obj {
    const BrSub1005CB40Vtbl *pVtbl;
} BrSub1005CB40Obj;

int32_t  g_AA28D8;   /* 0x10AA28D8 */
int32_t  g_AA2858;   /* 0x10AA2858 */
uint16_t g_AA2870;   /* 0x10AA2870 */

/* WHAT IT DOES: always forwards the incoming value through vtable slot +8,
 * then -- only when 0x10AA28D8 and 0x10AA2858 are both clear -- asks slot
 * +0x1C to write an answer over a dword that starts at -1. A 16-bit counter
 * at 0x10AA2870 is incremented either way, and the dword is returned. What
 * the two slots do with the value is not established here. */
/* @implements 0x1005CB40 d3d BrSub1005CB40 */
uint32_t __fastcall BrSub1005CB40(BrSub1005CB40Obj *pThis, BrSub1005CB40Arg arg)
{
    uint32_t scratch;
    const BrSub1005CB40Vtbl *pVtbl;
    BrSub1005CB40Ref out;

    scratch = 0xFFFFFFFFu;
    pVtbl = pThis->pVtbl;
    pVtbl->f08((void *)pThis, arg);
    if (g_AA28D8 == 0) {
        if (g_AA2858 == 0) {
            out.p = &scratch;
            pVtbl->f1C((void *)pThis, out);
        }
    }
    ++g_AA2870;
    return scratch;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int operator_delete();
typedef int (*funcptr)();
#include <windows.h>
extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;
extern funcptr PTR_FUN_10077750;


/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_10077750 (fastcall). */
/* @implements 0x10055A30 glide BrVtInit55A30 */

int __fastcall BrVtInit55A30(int *param_1)

{
  *param_1 = &PTR_FUN_10077750;
  return;
}

/* WHAT IT DOES: C++ scalar deleting destructor for the 0x10077750-vtable object: run the
 * destructor body, then operator delete if bit 0 of the flags is set. thiscall, spelled
 * as __fastcall with an unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10055A10 glide BrVt55A10DeleteDtor */

void * __fastcall BrVt55A10DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  BrVtInit55A30((int *)param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}

#endif /* BR_MATCHING_BUILD */
