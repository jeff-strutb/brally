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
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_17.h"
#undef BrPtrListContains

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

/* g_s17 is the port's gathering of scattered originals.  The matching build
 * keeps the fields used here FILE-STATIC, as slice2_17.c had them: against
 * externs /O2 reschedules every pGfx bump (1768 -> 1632 bytes).  One static
 * per original global, so each name reaches one address; the image resolves
 * the references by function and offset (config/brally/reloc_overrides.csv). */
static uint32_t *s17_pGfx;              /* 0x106E7710 */
static int       s17_f690A1C;           /* 0x105CCBC8 */
static uint32_t  s17_f6C0258;           /* 0x106E72E8 */
static uint32_t  s17_f6C0688;           /* 0x106E7718 */
static uint32_t  s17_f6C0920;           /* 0x106E79B0 */
static uint32_t  s17_f6C3364;           /* 0x106EA3F4 */
static uint32_t  s17_f6C1174;           /* 0x106E8204 */
static int       s17_f0AA880;           /* 0x100AA010 */

/* 0x100AA5D0, 0x106C08A0 and 0x106C0860 are fixed STORAGE in the original,
 * not pointers to storage: BrScenePropsDraw passes their addresses as
 * immediates (`push 0x106e7930`) and indexes the colour table directly
 * (`mov edx,[ecx*4+0x100a9930]`). BrS17State models all three as pointers,
 * which costs a load at every use and rotates the whole allocation.
 *
 * PROPER FIX (header, serialised -- not done here): in src/brally/include/slice2_17.h
 * make them objects --
 *      const uint32_t *pColAA5D0;  ->  const uint32_t colAA5D0[4];
 *      BrMat4 *pLightMtx;          ->  BrMat4 lightMtx;
 *      BrMat4 *pTransMtx;          ->  BrMat4 transMtx;
 * and drop the three assignments in tests/brally/test_slice2_17.c. Until then the
 * matching build models them locally and the PORT IS LEFT EXACTLY AS IT
 * WAS -- do not delete the #else arm. */
/* The four prop brightness levels (64, 128, 192, 255 grey) the lamp and
 * lit props take as their ambient light.  Glide 0x100A9930 (the D3D copy
 * is 0x100AA5D0); the table must carry the values, not only the address. */
static const uint32_t s17_colAA5D0[4] = {   /* 0x100A9930 */
    0x40404000u, 0x80808000u, 0xC0C0C000u, 0xFFFFFF00u
};
static BrMat4 s17_lightMtx;              /* 0x106C08A0 */
static BrMat4 s17_transMtx;              /* 0x106C0860 */
#define BRS17_COL       s17_colAA5D0
#define BRS17_LIGHTMTX  (&s17_lightMtx)
#define BRS17_TRANSMTX  (&s17_transMtx)

/* BrPropItem's f04/f05 are ONE 16-bit flags word in the original: the loop
 * head loads it whole (`mov ax,word ptr [esi]`) and tests the high half with
 * `test ah,4`. Two adjacent unsigned char members do not reproduce that --
 * VC5 narrows each use to its own byte load and the word never gets CSEd.
 *
 * PROPER FIX (header, serialised -- not done here): in src/brally/include/slice2_17.h
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
#define BRS17_THISCALL __fastcall
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
        uint32_t *p_ = s17_pGfx;                                      \
                                                                        \
        s17_pGfx = p_ + 2;                                            \
        p_[0] = (w0_);                                                  \
        p_[1] = (w1_);                                                  \
    } while (0)

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
#define s17_ptrword(p_)   ((uint32_t)(uintptr_t)(const void *)(p_))

/* 0x1002FB20 */
/* The item loop tests the pass bit and the display list in one `if` around
 * the body, not as two `continue`s.  Both false edges then land on the body's
 * end, where the list pointer is reloaded for the count test, so the pointer
 * stays out of a register across the loop: the pass head loads it straight
 * into the item pointer (`add esi,0xC`) and the foot reloads it into edx. */
/* WHAT IT DOES: draws the trackside scenery -- the objects standing around the
 * course. It sets the lighting up from a fixed overhead direction, configures
 * how the objects are shaded and depth-tested, and then walks the list drawing
 * each one. */
/* @implements 0x1002FB20 d3d BrScenePropsDraw */
void BrScenePropsDraw(const BrPropList *pList, const BrMat4 *pViewMtx)
{
    uint32_t *pCombine;
    void *pLights;
    void *pMtx;
    int pass;

    s17_emit(0xE7000000u, 0);
    s17_emit(0xBA001402u, 0x00100000u);

    if (s17_f690A1C != 0) {
        s17_emit(0xB900031Du, 0x0C192008u);
        s17_f690A1C = 0;
    } else {
        s17_emit(0xB900031Du, 0x0C192038u);
    }

    /* The combiner command is reserved first and filled in by 0x1002F900. */
    pCombine = s17_pGfx;
    s17_pGfx = pCombine + 2;
    BrRdpSetCombineLERP((BrGfxWords *)pCombine,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3E8, 0,     0x3EC, 0,
                        0,     0,     0,     0x3E8);

    s17_emit(0xF9000000u, 0);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, s17_f6C0258);
    s17_emit(0xBA000602u, s17_f6C0688);
    s17_emit(0xBA000402u, s17_f6C0920);
    s17_emit(0xB7000000u, 1);
    s17_emit(0xB9000002u, 1);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0x00010000u);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, s17_f6C0258);
    s17_emit(0xB6000000u, 0x00853200u);

    /* `neg / sbb / and 0xFFFFF000 / add 0x2000` -- 0x1000 when the two
     * globals differ, 0x2000 when they are equal. */
    s17_emit(0xB7000000u,
             (((s17_f6C3364 ^ s17_f6C1174) ? 0x1000u : 0x2000u)
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

            if ((bit ^ (uint32_t)(pass == 0)) == 0 && it->dl != 0) {

                if (f & 0x400) {
                    s17_emit(0xBC00000Au, 0);
                    s17_emit(0xBC00040Au, 0);
                    s17_emit(0xBC00200Au, BRS17_COL[it->flags & 3]);
                    s17_emit(0xBC00240Au, BRS17_COL[it->flags & 3]);
                }
                if (it->flags & 4)
                    s17_emit(0xB6000000u, 0x3000u);
                if ((it->flags & 0x80) && s17_f0AA880 != 0)
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

                if ((it->flags & 0x80) && s17_f0AA880 != 0)
                    s17_emit(0xB7000000u, 0x200u);
                if (it->flags & 4)
                    s17_emit(0xB7000000u,
                             (s17_f6C3364 ^ s17_f6C1174) ? 0x1000u : 0x2000u);
                if (it->flags & 0x400) {
                    s17_emit(0xBC00000Au, 0xFFFFFF00u);
                    s17_emit(0xBC00040Au, 0xFFFFFF00u);
                    s17_emit(0xBC00200Au, 0x40404000u);
                    s17_emit(0xBC00240Au, 0x40404000u);
            }
            }
        }
    }

    s17_emit(0xBD000000u, 0);
}
