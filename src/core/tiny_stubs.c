/* tiny_stubs.c -- the trivial <=16-byte accessors and forwarders whose real
 * module is not yet known.
 *
 * These are the genuinely-trivial one-liners of BRGlide.dll: byte/dword
 * setters, single-field getters, pointer clears, and thiscall/cdecl
 * forwarders.  They were extracted as a class (2026-08-31) after the small-
 * function census showed the <=16B bucket is really three things -- C++ EH
 * catch-funclets (the 0x10073000-0x10078000 region), linker-synthesised
 * import thunks, and THESE, the only ones that are hand-written C.
 *
 * The census originally landed 35 stubs here; 30 of them turned out to be
 * D3D/Glide twins of functions already matched and filed in their real
 * modules (they only ever diffed clean because the two builds share source).
 * Those 30 were deleted and their filed twins re-keyed d3d->glide (rule 0),
 * leaving only the FIVE below that have no counterpart elsewhere in the tree.
 * Re-home each one into its real module when the surrounding subsystem is
 * identified.  Every function below diffs byte-exact against orig/BRGlide.dll
 * under /O2 (verified via tools/match_diff.py).
 *
 * Globals and call targets are declared locally: at .obj level every absolute
 * address and call displacement is a relocation, so the names are decoration
 * and the storage class (byte vs dword, the calling convention) is what the
 * match actually pins.  Addresses are named g_<VA> after the operand each
 * instruction carries in the original.
 */
#include <math.h>       /* sqrt (intrinsic under MSVC, libm on the port) */
#include "br_match.h"   /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

/* ---- globals touched by these stubs (names follow the original's VAs) ---- */
extern int  g_AC0810, g_B71290;                      /* object bases */

/* ---- external call targets (addresses are relocations) ---- */
extern void  BR_THISCALL1 BrSub10008760(void *self);   /* thiscall targets  */
extern void  BR_THISCALL1 BrSub10008D60(void *self);
extern double BrSub1001DC40(int x);                   /* returns in st0      */

#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

/* forward decl: BrSub10032520 takes this one's address before it is defined */
void BR_THISCALL1 BrSub100087C0(int **p);

/* ==================== thiscall tail-forwarders ==================== */
/* WHAT IT DOES: tear-down entry point for the one global object at
 * g_AC0810 -- a no-argument wrapper the game can hand to a shutdown list,
 * forwarding to the real routine with that object's address baked in. */
/* @implements 0x10032500 glide BrSub10032500 */
void BrSub10032500(void){ BrSub10008760(&g_AC0810); }
/* WHAT IT DOES: the construction half for the same g_AC0810 object, again as
 * a no-argument wrapper. Pairs with BrSub10032500 above. */
/* @implements 0x10032520 glide BrSub10032520 */
void BrSub10032520(void){ BrSub100087C0((int **)&g_AC0810); }
/* WHAT IT DOES: the same fixed-object wrapper shape for g_B71290. */
/* @implements 0x10062AF0 glide BrSub10062AF0 */
void BrSub10062AF0(void){ BrSub10008D60(&g_B71290); }

/* ==================== remaining one-offs ==================== */
/* fastcall ctor: write a vtable/const, advance ecx, tail-call */
/* WHAT IT DOES: construct an object in place -- writes its method table
 * pointer into the first field, then hands the rest of the object to the
 * routine that initialises it. The C++ constructor of a small class, as the
 * compiler emitted it. */
/* @implements 0x100087C0 glide BrSub100087C0 */
void BR_THISCALL1 BrSub100087C0(int **p){ *p = (int *)0x10077150; BrSub10008D60(p + 1); }

/* sqrt of a call result: the split temporaries make VC5 emit fsqrt before the
 * cdecl stack cleanup, matching the original's instruction order. */
/* WHAT IT DOES: square root of whatever the helper at 0x1001DC40 computes
 * for x, narrowed to float. */
/* @implements 0x1001DC80 glide BrSub1001DC80 */
float BrSub1001DC80(int x){ double d = BrSub1001DC40(x); float r = (float)sqrt(d); return r; }
