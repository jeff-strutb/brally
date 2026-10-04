/* br_crt.h -- the statically linked MSVC CRT functions the ported modules call.
 *
 * These are NOT decompiled and must not be. Everything at or above 0x1007CC40
 * in BRD3D.dll is MSVC's own CRT (established from `_cexit`'s body). Porting it
 * would be re-implementing Microsoft's 1997 C library for no benefit.
 *
 * Instead this module supplies host-CRT equivalents under the names the pass
 * modules already declare, so the tree links. Each entry records the original
 * address and any behaviour that differs from the naive host equivalent.
 */
#ifndef BR_CRT_H
#define BR_CRT_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The MSVC runtime entry points the original calls directly (implemented in
 * platform/src/br_crt.c). */
void   *BrOperatorNew(size_t cb);            /* 0x1007DFE0 operator new    */
void    BrOperatorDelete(void *p);           /* 0x1007DE40 operator delete */
/* The CRT's exit-handler registration (the platform runs them at exit). */
typedef int (*BrCrtOnExitFn)(void);
BrCrtOnExitFn dllonexit(BrCrtOnExitFn pfn, void *pBegin, void *pEnd);

/* C++ `delete p` on the original's classes: the scalar deleting destructor in
 * vtable slot 0, flag 1 (Itanium's vtables put destructors elsewhere, so the
 * call is made explicitly). */
static inline void br_vdelete(void *p)
{
    if (p != 0)
        ((void *(*)(void *, unsigned))(*(void *const **)p)[0])(p, 1);
}

/* C++ `new Class`: operator new, then the class's constructor if the
 * allocation succeeded (the original's constructors are C functions here). */
/* The function in slot `slot` of the object's vtable (the original's MSVC
 * layout, lifted as data): a virtual call the original made through the
 * vtable, kept as one where clang would call the class's method directly. */
#define BR_VFN(obj, slot, FnT) ((FnT)((*(void *const *const *)(obj))[slot]))

static inline void *br_new_obj(size_t cb, void *(*ctor)(void *))
{
    void *p = BrOperatorNew(cb);
    return p != 0 ? ctor(p) : 0;
}
int32_t BrFtolTrunc(float f);                /* 0x1007C8A0 __ftol          */

/* The C runtime's file names are DOS paths (C:\BOSSRALLY\..., D:\...,
 * backslashes); the platform maps them to the host's files. */
FILE *br_fopen(const char *path, const char *mode);
int   br_rename(const char *from, const char *to);
int   br_access(const char *path, int mode);
#define fopen  br_fopen
#define rename br_rename
#define access br_access
#define operator_delete BrOperatorDelete

/* The game's rand() is the MSVC runtime's: a 0..0x7FFF LCG seeded with 1.
 * The code scales it by 0x8000 (`rand() * n / 0x8000`), so the host's
 * 31-bit rand() would index past the end of its tables -- and the same
 * sequence keeps a run in step with the original. */
int  br_rand(void);
void br_srand(unsigned int seed);
#define rand  br_rand
#define srand br_srand
#undef  RAND_MAX
#define RAND_MAX 0x7FFF

/* 0x1007DFE0 -- `operator new`, i.e. _nh_malloc(cb, 1).
 * DOES NOT ZERO. Several modules allocate 0xC8-byte objects here and rely on
 * a constructor to fill them; anything the ctor misses is garbage. */
/* BrOperatorNew: prototype in br_funcs.h */

/* 0x1007DE40 -- `operator delete` -> free. 511 call sites in the original. */
/* BrOperatorDelete: prototype in br_funcs.h */

/* 0x1007C8A0 -- MSVC `__ftol`: truncates toward zero, and stores the LOW DWORD
 * of a 64-bit fistp. Out of range the x87 stores the 64-bit indefinite
 * 0x8000000000000000, and __ftol keeps its LOW dword -- so the return is 0.
 * NOT 0x80000000, and not a saturation. NaN takes the same path.
 *
 * This comment previously claimed 0x80000000. That was wrong, and it survived
 * here after br_crt.c was corrected from the disassembly at 0x1007C8BF
 * (`mov eax,[ebp-0xc]` reads the low half). Several modules' divide-by-zero
 * paths feed this infinities, so the difference is reachable, not academic.
 * CONVENTIONS.md and br_crt.c agree on 0; this header was the outlier. */
/* BrFtolTrunc: prototype in br_funcs.h */

/* the platform's per-frame hook (scripted input), BrAppFrame calls it */
void plat_app_frame(void);
void plat_text_emit(const char *psz);   /* script_game.c: what the frame drew */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif

#endif /* BR_CRT_H */
