/* br_sceneprops.c -- drawing: the trackside prop display list.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * 0x1001D1B0 sets up lighting, shading and depth state for the objects
 * standing around the course and emits one display-list call per prop.
 *
 * Filed out of the address batch slice2_17.c.  That file's preamble is
 * carried verbatim (its #ifdef blocks, includes, .rdata constants,
 * cross-slice declarations and small helpers -- the helper set changes
 * /O2 register choice, so it is kept whole); its state block g_s17 is
 * declared in slice2_17.h and defined there.
 */
#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* .rdata constants, read from the DLL.                                */

/* 0x1008F448  dword 0x400F5C29 -- 2.24f, the m/s -> mph factor. */
#define BR_MPH_PER_MS      2.24f

/* 0x1008F4E0  0x3F91DF46A2529D39 -- pi/180. */
#define BR_DEG_TO_RAD      0.017453292519943295

/* 0x1008F4B0  0xC0545F30B4E4E30A and 0x1008F4B8 0xBFD45F30B4E4E30A.
 * Exactly 256x apart. Neither is the correctly rounded -256/pi or -1/pi;
 * the shipped values are used verbatim. */
#define BR_ANG_K256      (-81.48734781601175)
#define BR_ANG_K1        (-0.3183099524062959)

/* ------------------------------------------------------------------ */
/* Module state.                                                       */

/* g_s17 is declared in slice2_17.h and defined in slice2_17.c. */

#ifdef BR_MATCHING_BUILD
/* The matched bytes need the state block FILE-STATIC in this TU, as it was
 * in slice2_17.c: against an external g_s17, /O2 reschedules every pGfx
 * bump (1776 -> 1648 bytes).  The image resolves each of these references
 * by address (config/reloc_overrides.csv, keyed by function and offset,
 * not by symbol), so the matching arm names a TU-static block of the same
 * layout.  The port has no #else arm to change: it uses the shared g_s17. */
static BrS17State s17_tuState;
#define g_s17 s17_tuState
#endif

/* 0x100AA5D0, 0x106C08A0 and 0x106C0860 are fixed STORAGE in the original,
 * not pointers to storage: BrScenePropsDraw passes their addresses as
 * immediates (`push 0x106e7930`) and indexes the colour table directly
 * (`mov edx,[ecx*4+0x100a9930]`). BrS17State models all three as pointers,
 * which costs a load at every use and rotates the whole allocation.
 *
 * PROPER FIX (header, serialised -- not done here): in include/slice2_17.h
 * make them objects --
 *      const uint32_t *pColAA5D0;  ->  const uint32_t colAA5D0[4];
 *      BrMat4 *pLightMtx;          ->  BrMat4 lightMtx;
 *      BrMat4 *pTransMtx;          ->  BrMat4 transMtx;
 * and drop the three assignments in tests/test_slice2_17.c. Until then the
 * matching build models them locally and the PORT IS LEFT EXACTLY AS IT
 * WAS -- do not delete the #else arm. */
#ifdef BR_MATCHING_BUILD
static const uint32_t s17_colAA5D0[4];   /* 0x100AA5D0 */
static BrMat4 s17_lightMtx;              /* 0x106C08A0 */
static BrMat4 s17_transMtx;              /* 0x106C0860 */
#define BRS17_COL       s17_colAA5D0
#define BRS17_LIGHTMTX  (&s17_lightMtx)
#define BRS17_TRANSMTX  (&s17_transMtx)
#else
#define BRS17_COL       g_s17.pColAA5D0
#define BRS17_LIGHTMTX  g_s17.pLightMtx
#define BRS17_TRANSMTX  g_s17.pTransMtx
#endif

/* BrPropItem's f04/f05 are ONE 16-bit flags word in the original: the loop
 * head loads it whole (`mov ax,word ptr [esi]`) and tests the high half with
 * `test ah,4`. Two adjacent unsigned char members do not reproduce that --
 * VC5 narrows each use to its own byte load and the word never gets CSEd.
 *
 * PROPER FIX (header, serialised -- not done here): in include/slice2_17.h
 * replace
 *      unsigned char f04;  unsigned char f05;
 * with
 *      unsigned short flags;   (bit0..1 colour index, bit2, bit3 pass,
 *                               bit7 gated on 0x100AA880,
 *                               bit10 = old f05 bit2)
 * and this shadow declaration goes away. f04/f05 are read nowhere else in
 * the tree. The reinterpretation is layout-identical on every little-endian
 * target. */
typedef struct S17PropItem {
    uint32_t dl;            /* +0x00 */
    unsigned short flags;   /* +0x04, was f04 | f05 << 8 */
    unsigned char f06, f07; /* +0x06 +0x07 */
    float x, y, z;          /* +0x08 +0x0C +0x10 */
} S17PropItem;

/* ------------------------------------------------------------------ */
/* Cross-slice callees. Stand-ins live in the test file.               */

/* XSLICE 0x10008B80 */  /* a bare `ret` in this build -- see the contract */
extern void BrStub10008B80(intptr_t a0, ...);
/* XSLICE 0x10060E90 */
extern int   BrX10060E90(void);
/* XSLICE 0x100751D0
 * 0x1002C2A0 tail-jumps into it with the object in ecx and nothing on the
 * stack: it is a C++ __thiscall method. MSVC 5.0's C front end cannot spell
 * __thiscall, but for a single pointer argument __fastcall is byte-identical
 * at the call site (arg1 in ecx, no stack cleanup), so that is what the
 * matching build uses. Off MSVC the qualifier vanishes and it is an ordinary
 * one-argument function. */
#if defined(_MSC_VER)
#define BRS17_THISCALL __fastcall
#else
#define BRS17_THISCALL
#endif
extern void BRS17_THISCALL BrX100751D0(void *pThis);
/* XSLICE 0x1002C2C0 */
extern void  BrX1002C2C0(void);
/* XSLICE 0x1003563A */
extern void  BrX1003563A(int a0);
/* XSLICE 0x100397C0 */
extern void  BrX100397C0(void);
/* XSLICE 0x10034C66 */
extern void  BrX10034C66(void (*pfn)(void));
/* XSLICE 0x1002C500 */
extern void  BrX1002C500(void);
/* XSLICE 0x10075F10 */
extern void  BrX10075F10(void *pThis);
/* XSLICE 0x100664C0 */
extern void  BrX100664C0(void *pThis);
/* XSLICE 0x10005DE0 */
extern int   BrX10005DE0(void *pOwner, unsigned char *pb0,
                         unsigned char *pb1, unsigned char *pb2);
/* XSLICE 0x10076AE0 */
extern void  BrX10076AE0(void *pThis, int a0);
/* XSLICE 0x10005E70 */
extern const char *BrX10005E70(void *pOwner);
/* XSLICE 0x10068260 */
extern void  BrX10068260(int i, uint32_t tag);
/* XSLICE 0x10072580 */
extern void  BrX10072580(int a0);
/* XSLICE 0x10042AF0 */
extern void  BrX10042AF0(void *p, int a1, int a2);
/* XSLICE 0x10035BBA */
extern void  BrX10035BBA(const char *psz);
/* XSLICE 0x10069530 */
extern void *BrX10069530(void);
/* XSLICE 0x10069490 */
extern void *BrX10069490(void);
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1).
 * Anything at or above 0x1007CC40 is statically linked MSVC CRT, so the
 * platform's own atexit is used instead of porting it. */
extern int   BrXAtExit(void (*pfn)(void));

/* ------------------------------------------------------------------ */
/* Small helpers. Loads and stores go through memcpy so that the byte
 * offsets recovered from the disassembly stay valid without relying on
 * pointer casts being aligned.                                         */

static uint32_t s17_ld32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_st32(unsigned char *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static float s17_ldf(const unsigned char *p)
{
    float v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_stf(unsigned char *p, float v)
{
    memcpy(p, &v, sizeof v);
}

/* g_6C0680 is advanced by 8 bytes and then the two words are written --
 * the original reads the cursor, bumps the global, and only then stores. */
#define s17_emit(w0_, w1_)                                              \
    do {                                                                \
        uint32_t *p_ = g_s17.pGfx;                                      \
                                                                        \
        g_s17.pGfx = p_ + 2;                                            \
        p_[0] = (w0_);                                                  \
        p_[1] = (w1_);                                                  \
    } while (0)

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
#define s17_ptrword(p_)   ((uint32_t)(uintptr_t)(const void *)(p_))

/* 0x1002FB20 */
/* RESIDUE (12 masked diffs, T3a, instruction count and body length exact --
 * the +8 the scorer reports is trailing alignment nops). Two sites, one
 * cause: where the reloaded `pList` lives. The original homes it in the
 * induction register at the pass head (`mov esi,[esp+0x1C]` then
 * `add esi,0xC`) and in a SCRATCH register at the loop foot (`mov edx,
 * [esp+0x1C]`); ours picks edi both times, so the head reads
 * `lea esi,[edi+0xC]` and the foot swaps `inc eax` with the count load.
 * DO NOT RE-PROBE: an explicit induction pointer (`it` hoisted out of the
 * inner loop, stepped in the third clause) is far worse -- 746 diffs,
 * REGNORM 14+12, +13 bytes.
 *
 * DEAD 2026-09-09 (32 compiles, three passes, all at 12 diffs unless said):
 *   `it` as `pList->items + k`, as byte arithmetic from pList, non-const,
 *   at function scope, declared before `k`; `f`/`bit` at function scope;
 *   `bit` before `f`; the pass test hoisted to a `want` local (35);
 *   `k` at function scope, as `int`, `unsigned short` (+12 B), post-
 *   increment, `count > k` (14); the count without its cast; the two
 *   `continue` tests merged (either order: -8 B when dl first), swapped
 *   (-8 B), `!it->dl`, `bit != want` (13), `pass != 1` (+1 B);
 *   declaration order of the four function locals (all six orders);
 *   `pass` unsigned (13); `bit` int; `f` int or uint32_t (+2 B); the
 *   `(unsigned char)` cast dropped (42); the item matrix at function
 *   scope; the colour read into a local (-16 B); an `items` base local
 *   (+5 B); the inner loop as `while` (+15 B).  The residue is which
 *   register holds the reloaded pList at two sites; no spelling reaches it. */
/* @t4-pass 0x1001D1B0 1 2026-09-09 probes 9 bytes 1768 insns 431 regions 2 rows 2 census yes  (hand: item/count/test spellings; thin, not counted) */
/* @t4-pass 0x1001D1B0 2 2026-09-09 probes 12 bytes 1768 insns 431 regions 2 rows 2 census yes  (hand: scope, widths, declaration order -- zero movement) */
/* @t4-pass 0x1001D1B0 3 2026-09-09 probes 11 bytes 1768 insns 431 regions 2 rows 2 census yes  (hand: loop and test shapes, locals -- zero movement) */
/* WHAT IT DOES: draws the trackside scenery -- the objects standing around the
 * course. It sets the lighting up from a fixed overhead direction, configures
 * how the objects are shaded and depth-tested, and then walks the list drawing
 * each one. */
/* @t3 0x1001D1B0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1768/1768 insns 431/431 rows 1+1 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * Residue: which register holds the reloaded `pList` at the inner loop's
 * head (the original adds 0xC in place in the induction register, ours
 * keeps the base in edi and forms it with `lea`) and at its foot (edx vs
 * edi, which also swaps `inc eax` with the count load).  Size, count and
 * the register-blind multiset are exact; the 1+1 is that lea/add pair.
 * Dossier and the 32-compile dead list: the RESIDUE block above; ledger
 * lines above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1002FB20 d3d BrScenePropsDraw */
void BrScenePropsDraw(const BrPropList *pList, const BrMat4 *pViewMtx)
{
    uint32_t *pCombine;
    void *pLights;
    void *pMtx;
    int pass;

    s17_emit(0xE7000000u, 0);
    s17_emit(0xBA001402u, 0x00100000u);

    if (g_s17.f690A1C != 0) {
        s17_emit(0xB900031Du, 0x0C192008u);
        g_s17.f690A1C = 0;
    } else {
        s17_emit(0xB900031Du, 0x0C192038u);
    }

    /* The combiner command is reserved first and filled in by 0x1002F900. */
    pCombine = g_s17.pGfx;
    g_s17.pGfx = pCombine + 2;
    BrRdpSetCombineLERP((BrGfxWords *)pCombine,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3E8, 0,     0x3EC, 0,
                        0,     0,     0,     0x3E8);

    s17_emit(0xF9000000u, 0);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, g_s17.f6C0258);
    s17_emit(0xBA000602u, g_s17.f6C0688);
    s17_emit(0xBA000402u, g_s17.f6C0920);
    s17_emit(0xB7000000u, 1);
    s17_emit(0xB9000002u, 1);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0x00010000u);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, g_s17.f6C0258);
    s17_emit(0xB6000000u, 0x00853200u);

    /* `neg / sbb / and 0xFFFFF000 / add 0x2000` -- 0x1000 when the two
     * globals differ, 0x2000 when they are equal. */
    s17_emit(0xB7000000u,
             (((g_s17.f6C3364 ^ g_s17.f6C1174) ? 0x1000u : 0x2000u)
              | 0x000A0205u));

    pLights = BrX10069530();
    BrLightDirsFromLookAt(BRS17_LIGHTMTX, (BrLightPair *)pLights,
                          0.0f, -1.0f, 15.0f,
                          0.0f,  0.0f,  0.0f,
                          0.0f,  1.0f,  0.0f);
    s17_emit(0x03840010u, s17_ptrword(pLights));
    s17_emit(0x03820010u, s17_ptrword((unsigned char *)pLights + 0x10));

    pMtx = BrX10069490();
    BrMat4Copy(pViewMtx, (BrMat4 *)pMtx);     /* SOURCE first -- br_mat.h */
    s17_emit(0x01040040u, s17_ptrword(pMtx));

    s17_emit(0xBB000001u, 0xFFFFFFFFu);
    s17_emit(0xB6000000u, 0x000C0000u);
    s17_emit(0xE8000000u, 0);
    s17_emit(0xF5100000u, 0x07000000u);
    s17_emit(0xF50001F0u, 0x06000000u);
    s17_emit(0xF5000100u, 0x05000000u);

    for (pass = 0; pass < 2; ++pass) {
        uint32_t k;

        for (k = 0; k < (uint32_t)pList->count; ++k) {
            const S17PropItem *it = (const S17PropItem *)&pList->items[k];
            unsigned short f = it->flags;
            uint32_t bit = ((uint32_t)(unsigned char)(~f) >> 3) & 1u;

            if ((bit ^ (uint32_t)(pass == 0)) != 0)
                continue;
            if (it->dl == 0)
                continue;

            if (f & 0x400) {
                s17_emit(0xBC00000Au, 0);
                s17_emit(0xBC00040Au, 0);
                s17_emit(0xBC00200Au, BRS17_COL[it->flags & 3]);
                s17_emit(0xBC00240Au, BRS17_COL[it->flags & 3]);
            }
            if (it->flags & 4)
                s17_emit(0xB6000000u, 0x3000u);
            if ((it->flags & 0x80) && g_s17.f0AA880 != 0)
                s17_emit(0xB6000000u, 0x200u);

            s17_emit(0xBB000001u, 0xFFFFFFFFu);
            s17_emit(0xE8000000u, 0);

            {
                void *pItemMtx = BrX10069490();

                BrMat4Translate(BRS17_TRANSMTX, it->x, it->y, it->z);
                BrMat4Copy(BRS17_TRANSMTX, (BrMat4 *)pItemMtx);
                s17_emit(0x01040040u, s17_ptrword(pItemMtx));
            }

            s17_emit(0x06000000u, it->dl);
            s17_emit(0xBD000000u, 0);

            if ((it->flags & 0x80) && g_s17.f0AA880 != 0)
                s17_emit(0xB7000000u, 0x200u);
            if (it->flags & 4)
                s17_emit(0xB7000000u,
                         (g_s17.f6C3364 ^ g_s17.f6C1174) ? 0x1000u : 0x2000u);
            if (it->flags & 0x400) {
                s17_emit(0xBC00000Au, 0xFFFFFF00u);
                s17_emit(0xBC00040Au, 0xFFFFFF00u);
                s17_emit(0xBC00200Au, 0x40404000u);
                s17_emit(0xBC00240Au, 0x40404000u);
            }
        }
    }

    s17_emit(0xBD000000u, 0);
}
