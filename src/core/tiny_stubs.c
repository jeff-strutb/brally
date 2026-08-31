/* tiny_stubs.c -- the trivial <=16-byte accessors and forwarders.
 *
 * These are the genuinely-trivial one-liners of BRGlide.dll: byte/dword
 * setters, single-field getters, pointer clears, and thiscall/cdecl
 * forwarders.  They were extracted as a class (2026-08-31) after the small-
 * function census showed the <=16B bucket is really three things -- C++ EH
 * catch-funclets (the 0x10073000-0x10078000 region), linker-synthesised
 * import thunks, and THESE, the only ones that are hand-written C.
 *
 * Their semantic module is not yet known, so they live together here and
 * carry BrSub<VA> names; re-home each one into its real module when the
 * surrounding subsystem is identified.  Every function below diffs byte-exact
 * against orig/BRGlide.dll under /O2 (verified via tools/match_diff.py).
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
extern unsigned char g_4ABB48, g_4ABB44, g_4ABB40;   /* byte flags   */
extern unsigned char g_AC5C00;                        /* byte flag    */
extern int  g_AC5C28;                                 /* dword state  */
extern int  g_AC5CCC, g_AC5C84, g_AC5C94, g_AC5C5C;   /* copy pair    */
extern int  g_5B16E8;                                 /* dword sink   */
extern int  g_17B3248;                                /* dword base   */
extern int  g_AC0810, g_B71290, g_5BC858;             /* object bases */

/* ---- external call targets (addresses are relocations) ---- */
extern void  BrSub1006F5E0(void *fn);                 /* cdecl registrar   */
extern void  BrSub100745E0(void *fn);                 /* cdecl registrar   */
extern void  BR_THISCALL1 BrSub1006E4A0(void *self);    /* thiscall targets  */
extern void  BR_THISCALL1 BrSub10008760(void *self);
extern void  BR_THISCALL1 BrSub10008D60(void *self);
extern void  BrSub10062D00(void);
extern int   BrSub1006E280(void);
extern void  BrSub1006A4D0(void);
extern void  BrSub1006A5A0(void);
extern void  BrSub1006A580(void);
extern void  BrSub1002A940(void);                     /* forward-call target */
extern double BrSub1001DC40(int x);                   /* returns in st0      */

#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

/* forward decls for the in-file functions whose address is taken or that are
 * the tail-call target of another stub here */
void BR_THISCALL1 BrSub100087C0(int **p);
void *BR_THISCALL1 BrSub10062B00(void *self);
void BrSub10019830(void);
void BrSub10032520(void);
void BrSub10062AF0(void);

/* ============================ byte setters ============================ */
/* @implements 0x10016800 glide BrSub10016800 */
void BrSub10016800(void){ g_4ABB48 = 1; }
/* @implements 0x10016810 glide BrSub10016810 */
void BrSub10016810(void){ g_4ABB48 = 0; }
/* @implements 0x10016820 glide BrSub10016820 */
void BrSub10016820(void){ g_4ABB40 = 0; }
/* @implements 0x10016830 glide BrSub10016830 */
void BrSub10016830(void){ g_4ABB44 = 2; }
/* @implements 0x10016840 glide BrSub10016840 */
void BrSub10016840(void){ g_4ABB44 = 0; }
/* @implements 0x10016850 glide BrSub10016850 */
void BrSub10016850(void){ g_4ABB44 = 1; }

/* byte setter that also returns 1 (eax loaded before/after the store) */
/* @implements 0x10037ED0 glide BrSub10037ED0 */
int BrSub10037ED0(void){ return g_AC5C00 = 1; }
/* @implements 0x10037EE0 glide BrSub10037EE0 */
int BrSub10037EE0(void){ g_AC5C00 = 0; return 1; }

/* ==================== dword setters returning 1 ==================== */
/* @implements 0x1003A820 glide BrSub1003A820 */
int BrSub1003A820(void){ g_AC5C28 = 0; return 1; }
/* @implements 0x1003A830 glide BrSub1003A830 */
int BrSub1003A830(void){ return g_AC5C28 = 1; }
/* @implements 0x1003A840 glide BrSub1003A840 */
int BrSub1003A840(void){ g_AC5C28 = 2; return 1; }
/* @implements 0x1003A850 glide BrSub1003A850 */
int BrSub1003A850(void){ g_AC5C28 = 3; return 1; }

/* ==================== copy-one-global-then-clear ==================== */
/* @implements 0x100403A0 glide BrSub100403A0 */
int BrSub100403A0(void){ g_AC5C5C = g_AC5CCC; return 0; }
/* @implements 0x10040410 glide BrSub10040410 */
int BrSub10040410(void){ g_AC5C5C = g_AC5C84; return 0; }
/* @implements 0x100404A0 glide BrSub100404A0 */
int BrSub100404A0(void){ g_AC5C5C = g_AC5C94; return 0; }

/* ==================== argument stores / getters ==================== */
/* @implements 0x10018A40 glide BrSub10018A40 */
int BrSub10018A40(int x){ return g_5B16E8 = x; }

/* float field getter: fld [arg+0xc] */
struct BrTinyF { char pad[0xc]; float f; };
/* @implements 0x1000DEC0 glide BrSub1000DEC0 */
float BrSub1000DEC0(struct BrTinyF *p){ return p->f; }

/* clear three dwords through a pointer, high offset first */
/* @implements 0x10034710 glide BrSub10034710 */
void BrSub10034710(int *p){ p[2] = 0; p[1] = 0; p[0] = 0; }

/* fastcall: clear two dwords through ecx */
/* @implements 0x1006CDD0 glide BrSub1006CDD0 */
void BR_THISCALL1 BrSub1006CDD0(int *p){ p[0] = 0; p[1] = 0; }

/* ==================== thiscall tail-forwarders ==================== */
/* @implements 0x10019830 glide BrSub10019830 */
void BrSub10019830(void){ BrSub1006E4A0(&g_5BC858); }
/* @implements 0x10032500 glide BrSub10032500 */
void BrSub10032500(void){ BrSub10008760(&g_AC0810); }
/* @implements 0x10032520 glide BrSub10032520 */
void BrSub10032520(void){ BrSub100087C0((int **)&g_AC0810); }
/* @implements 0x10062AD0 glide BrSub10062AD0 */
void BrSub10062AD0(void){ BrSub10062B00(&g_B71290); }
/* @implements 0x10062AF0 glide BrSub10062AF0 */
void BrSub10062AF0(void){ BrSub10008D60(&g_B71290); }

/* ==================== cdecl registrar forwarders ==================== */
/* @implements 0x10019820 glide BrSub10019820 */
void BrSub10019820(void){ BrSub1006F5E0((void *)BrSub10019830); }
/* @implements 0x10032510 glide BrSub10032510 */
void BrSub10032510(void){ BrSub100745E0((void *)BrSub10032520); }
/* @implements 0x10062AE0 glide BrSub10062AE0 */
void BrSub10062AE0(void){ BrSub100745E0((void *)BrSub10062AF0); }
/* @implements 0x1006A570 glide BrSub1006A570 */
void BrSub1006A570(void){ BrSub100745E0((void *)BrSub1006A580); }

/* ==================== remaining one-offs ==================== */
/* fastcall ctor: write a vtable/const, advance ecx, tail-call */
/* @implements 0x100087C0 glide BrSub100087C0 */
void BR_THISCALL1 BrSub100087C0(int **p){ *p = (int *)0x10077150; BrSub10008D60(p + 1); }

/* C++ virtual dispatch: (*this->vtbl[8])(); return 1 */
struct BrTinyVt { void (*m[16])(void); };
struct BrTinyOb { struct BrTinyVt *vp; };
/* @implements 0x10041D00 glide BrSub10041D00 */
int BR_THISCALL1 BrSub10041D00(struct BrTinyOb *o){ o->vp->m[8](); return 1; }

/* fastcall: preserve this across a call, return this */
/* @implements 0x10062B00 glide BrSub10062B00 */
void *BR_THISCALL1 BrSub10062B00(void *self){ BrSub10062D00(); return self; }

/* call, subtract a global, return */
/* @implements 0x1006A310 glide BrSub1006A310 */
int BrSub1006A310(void){ return BrSub1006E280() - g_17B3248; }

/* two calls, return 1 */
/* @implements 0x1006A4C0 glide BrSub1006A4C0 */
int BrSub1006A4C0(void){ BrSub1006A4D0(); BrSub1006A5A0(); return 1; }

/* plain forwarder that keeps an EBP frame: this one was built /Od, so it
 * calls (not tail-jumps) inside a frame.  Force /Od for it alone. */
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
/* @implements 0x1002A932 glide BrSub1002A932 */
void BrSub1002A932(void){ BrSub1002A940(); }
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

/* sqrt of a call result: the split temporaries make VC5 emit fsqrt before the
 * cdecl stack cleanup, matching the original's instruction order. */
/* @implements 0x1001DC80 glide BrSub1001DC80 */
float BrSub1001DC80(int x){ double d = BrSub1001DC40(x); float r = (float)sqrt(d); return r; }
