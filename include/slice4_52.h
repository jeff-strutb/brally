/* slice4_52.h -- BRD3D.dll, a later pass (slice 4 "close the link" packet).
 *
 * Every function here is one that an already-ported module declares `extern`
 * but nobody implements.  Names and signatures are taken verbatim from the
 * `; ===== WANTED AS:` lines of work/slice4/agent52.asm and from the modules
 * listed there; where two modules disagree the conflict is spelled out in the
 * comment above the declaration and repeated in the report.
 *
 * ---------------------------------------------------------------------------
 * READ THIS FIRST -- the packet listing is mis-paired
 *
 * Six of the seventeen `WANTED AS` blocks in work/slice4/agent52.asm carry the
 * disassembly of a DIFFERENT function than the address in the name:
 *
 *   BrSub1003CE80  <- body of 0x1003C260      BrSub1003C020  <- 0x10038F30
 *   BrOptFn10051990<- body of 0x1004CAC0      BrSub1003CDA0  <- 0x1003C230
 *   BrSub1003D9F0  <- body of 0x1003D210      BrSub1005F530  <- 0x10060260
 *   BrSub10060260  <- body of 0x1003E310
 *
 * Everything below was decompiled from the REAL listing for the wanted
 * address, taken out of asm/1003*.asm / asm/1005*.asm / asm/1006*.asm.  Do not
 * cross-check this file against the packet .asm; check it against asm/.
 * ---------------------------------------------------------------------------
 *
 * This header deliberately includes almost nothing.  slice2_25.h and
 * slice3_33.h CANNOT be included in the same translation unit today
 * (slice1_06.h and slice2_25.h define `BrDPlayVtbl` incompatibly), and five of
 * the declarations below belong to each side, so every type that crosses the
 * boundary is named as an incomplete `struct` tag that matches the owning
 * header's tag exactly.  That keeps slice4_52.h includable next to either one.
 */
#ifndef SLICE4_52_H
#define SLICE4_52_H

#include <stddef.h>
#include <stdint.h>

#include "br_bits.h"   /* BrHandleLookup, BR_HANDLE_MIN / BR_HANDLE_MAX */

/* Types owned by other packets; only ever used through a pointer here. */
struct BrScrPt;      /* slice2_14.h */
struct BrUiScreen;   /* slice3_33.h */
struct BrOptObj;     /* slice2_25.h */
struct BrOptUi;      /* slice2_25.h -- the same storage slice2_22.h models as
                      * BrDPlayLink; see the DEVIATION on BrSub1003D9F0 */
struct BrErrHost;    /* slice1_06.h */

/* ==========================================================================
 * 0x10074030  BrStrGet -- string-table lookup by id
 * ==========================================================================
 *
 * The body is already ported: br_bits.h's BrHandleLookup is this function with
 * the hardcoded table address turned into a parameter.  BrStrGet is the
 * global-table wrapper the three callers actually want.
 *
 * NAMING: this one address now carries FOUR names in port/include --
 * BrStrGet (slice2_23.h, slice2_25.h, slice3_33.h), BrStringById
 * (slice2_24.h) and BrHandleLookup (br_bits.h).  BrStrGet has three of the
 * four votes and is the WANTED name, so it is what is defined.
 *
 * Valid ids are 1..0x12E inclusive; 0 is the reserved "none" and returns NULL,
 * and the range test is UNSIGNED, so a negative id is rejected too. */
#define BR_STR_TABLE_COUNT  (BR_HANDLE_MAX + 1)   /* 0x12F */

/* 0x11829370.  Filled by whoever loads the localised string resource. */
extern void *g_apBrStrTable[BR_STR_TABLE_COUNT];

const char *BrStrGet(int id);

/* ==========================================================================
 * 0x10010960 / 0x10010980  BrPolyDistX / BrPolyDistY
 * ==========================================================================
 *
 * The two lower clip-plane distance functions slice2_13.h describes: the
 * scissor rectangle is 0 <= f0C <= 1024 and 0 <= f10 <= 1024 and these are the
 * `>= 0` halves.  They are a bare field load -- no clamp, no guard, and NaN is
 * propagated (slice2_13's BrPolyClipPlane treats NaN as OUTSIDE). */
float BrPolyDistX(const struct BrScrPt *pPt);   /* returns pPt->f0C */
float BrPolyDistY(const struct BrScrPt *pPt);   /* returns pPt->f10 */

/* ==========================================================================
 * 0x1003BD50  BrRandom
 * ==========================================================================
 *
 * slice2_22.h already ports the arithmetic as BrDPlayRandStep(&seed); this is
 * the no-argument form slice2_15.h calls, over the original's own state word.
 *
 * GOTCHA: the modulus is 2^27, NOT 2^31-1.  Despite the 16807 multiplier this
 * is not the Lehmer minimal-standard generator and the sequences are
 * unrelated.  The result is therefore always in [0, 0x7FFFFFF] and NEVER
 * negative, even though slice2_15.h types it `int`.
 * GOTCHA: seed 0 is absorbing and nothing guards against it. */
extern uint32_t g_brA9BFD0;    /* 0x10A9BFD0 -- the generator state */

int BrRandom(void);

/* ==========================================================================
 * 0x1005FF30  BrMenuSub1005FF30
 * ==========================================================================
 *
 * Three `rep stosd` of 0x40 dwords over 0x10AA3288, 0x10AA2A80 and 0x10AA2E88,
 * in that order.  slice1_07.h ports the body as BrTables64Clear; this is the
 * global-array wrapper slice2_24.h calls.
 *
 * GOTCHA (already recorded by slice3_39.h, repeated because it is easy to
 * "fix"): 0x40 dwords is the WHOLE of the 256-byte key-state buffer but only
 * the first 64 entries of the two 256-entry DWORD arrays.  Entries 64..255 of
 * g_BrDikEdge and g_BrDikPrev are never cleared. */
void BrMenuSub1005FF30(void);

/* ==========================================================================
 * 0x10048470  BrUiScreenCtor -- __thiscall, returns `this`
 * ==========================================================================
 *
 * Zeroes +0x04..+0x0C, +0x10, +0x14 (word), the 200-pointer array at +0x18,
 * +0x338, +0x33C, +0x340, +0x344 and +0x346 (words), and installs the vtable
 * 0x1008F6F8 at +0x00.
 *
 * DEVIATION: slice3_33.h's BrUiScreen models the object from +0x10 onwards
 * only -- it has no vtable slot, no +0x04/+0x08/+0x0C and no +0x346.  Those
 * four stores therefore have nowhere to go and are NOT performed.  Nothing in
 * the tree reads a BrUiScreen vtable today, but a caller that starts to will
 * find it uninitialised: `operator new` (0x1007DFE0) does not zero.  See the
 * report -- this needs BrUiScreen extended, which is not this packet's file. */
struct BrUiScreen *BrUiScreenCtor(struct BrUiScreen *pThis);

/* ==========================================================================
 * 0x10060260  BrSub10060260
 * ==========================================================================
 *
 * SIGNATURE NOTE: slice3_32.h declares this `void BrSub10060260(void *pThis)`
 * and comments it "thiscall", and slice3_32.c passes `pG->pAA2900`.  The
 * original takes NO argument at all: it loads `this` from *0x10AA2E80 and the
 * one stack argument from *0x10680584, both inside the function.  The
 * parameter is accepted to keep the link, and IGNORED -- which is exactly what
 * the original does with whatever the caller left in ecx. */
/* XSLICE 0x100603A0 -- __thiscall(this, pArg); in slice3_39.h's skipped list. */
extern void BrSub100603A0(void *pThis, void *pArg);
/* XSLICE -- 0x10680584, also declared by slice2_25.h with this exact name. */
extern void *g_brP680584;

void BrSub10060260(void *pThis);

/* ==========================================================================
 * 0x1005F530  BrSub1005F530 -- release every loaded UI asset
 * ==========================================================================
 *
 * The table is the runtime side of the one slice1_06.h describes at
 * 0x10A9E360: BR_UIASSET_COUNT records of BR_UIASSET_STRIDE bytes, whose +0x70
 * holds the asset path.  This function only touches +0x00, an object pointer,
 * and calls its vtable slot 2 (byte +0x08) -- a Release -- then nulls the slot.
 *
 * GOTCHAS:
 *  - gated twice: it does nothing at all unless g_brA9D070 is non-zero, and
 *    the count is read as a WORD for the guard.
 *  - the loop bound is RE-READ from g_brAA28D4 on every iteration, so a
 *    Release that lowers the count shortens the walk.
 *  - a null slot is skipped without clearing anything. */
typedef struct BrUiAssetObj BrUiAssetObj;

typedef struct BrUiAssetObjVtbl {
    void *aSlots[2];                          /* +0x00, +0x04 -- untouched */
    void (*pfnRelease)(BrUiAssetObj *pThis);  /* +0x08, __thiscall, no args */
} BrUiAssetObjVtbl;

struct BrUiAssetObj {
    const BrUiAssetObjVtbl *pVtbl;   /* +0x00 */
};

/* One record of the 0x10A9E360 table.  Only +0x00 is modelled; the padding
 * reproduces the original stride so the record count stays meaningful. */
typedef struct BrUiAssetRec {
    BrUiAssetObj *pObj;                                   /* +0x00 */
    unsigned char pad[0x74 - sizeof(BrUiAssetObj *)];     /* +0x04..+0x73 */
} BrUiAssetRec;

#define BR_UIASSET_REC_COUNT 145      /* slice1_06.h's BR_UIASSET_COUNT */

extern BrUiAssetRec g_aBrUiAssetRec[BR_UIASSET_REC_COUNT];  /* 0x10A9E360 */
extern int32_t      g_brA9D070;    /* 0x10A9D070 -- master gate            */
extern uint32_t     g_brAA28D4;    /* 0x10AA28D4 -- count, low 16 bits only */

void BrSub1005F530(void);

/* ==========================================================================
 * 0x1003D9F0  BrSub1003D9F0
 * ==========================================================================
 *
 * slice2_22.h ports the body as BrDPlaySendTag3(pLink, fGate).  This is the
 * global-gate wrapper slice2_25.h calls.
 *
 * DEVIATION / INTEGRATION ACTION: slice2_25.h types the object at 0x10A9D008
 * `BrOptUi { int32_t f00, f04, f08; }` and slice2_22.h types the same storage
 * `BrDPlayLink { void *pIface; void *f04; uint32_t f08; uint32_t f0C; }`.
 * They coincide on a 32-bit host and DO NOT on a 64-bit one.  The wrapper
 * casts, because the declaration it has to match is slice2_25.h's.  One of the
 * two models has to go. */
extern int32_t g_brAA288C;   /* 0x10AA288C -- also br_slots.h's slot count */

int32_t BrSub1003D9F0(struct BrOptUi *pUi);

/* ==========================================================================
 * 0x100709A0  BrMenuSub100709A0 -- write the season save file
 * ==========================================================================
 *
 * fopen(0x11782CD0, "wb") then, in order: the four magic bytes at 0x100B5D94,
 * the adler32 of the 0x200-byte block at *0x10ACED34, that block, five loose
 * dwords, and 0x80 bytes at 0x10AD0990.  Always fclose()s a file it opened.
 *
 * DEVIATION: the original returns a bool in al (1 = written).  slice2_24.h --
 * the only declarer -- types it `void`, so the result is dropped.  A caller
 * cannot tell a failed save from a successful one.
 *
 * GOTCHAS:
 *  - the fwrite argument order is NOT uniform.  The magic, the checksum, the
 *    0x200 block and the 0x80 block go out as (ptr, 1, n) and are checked
 *    against n; the five loose dwords go out as (ptr, 4, 1) and are NOT
 *    checked at all.
 *  - the checksum covers ONLY the 0x200 block; nothing else in the file is
 *    protected, and the checksum itself is written before the data.
 *  - the magic is copied out of a writable global, not a literal.
 *  - adler32 is seeded by calling it once with (0, NULL, 0), which returns 1.
 *  - the very first failure (fopen) returns without fclose, correctly; every
 *    later one fcloses first. */
#define BR_SEASON_BLOCK_SIZE  0x200   /* *0x10ACED34 */
#define BR_SEASON_TAIL_SIZE   0x80    /* 0x10AD0990  */

extern char          g_brB5D94[];        /* 0x100B5D94 -- "RSea"            */
extern unsigned char g_brAD0990[BR_SEASON_TAIL_SIZE];   /* 0x10AD0990       */
/* Declared exactly as slice2_25.h declares them. */
extern const int32_t *g_brPACED34;   /* 0x10ACED34 -> BR_SEASON_BLOCK_SIZE bytes */
extern int32_t g_brAA2A08;   /* 0x10AA2A08 */
extern int32_t g_br0AC64C;   /* 0x100AC64C */
extern int32_t g_br0AC650;   /* 0x100AC650 */
extern int32_t g_br0AC654;   /* 0x100AC654 */
extern int32_t g_br0AC65C;   /* 0x100AC65C */

void BrMenuSub100709A0(void);

/* ==========================================================================
 * 0x10038F30  BrSub10038F30 -- process shutdown
 * ==========================================================================
 *
 * Twenty-odd calls in a fixed order and then exit(code).  Nothing here is
 * arithmetic, so the whole thing is injected: the host below is one field per
 * callee, in call order, and the three nullable entries are function-pointer
 * GLOBALS in the original that are tested before use.
 *
 * GOTCHAS:
 *  - the object at 0x10AA2904 is re-read from the global between the field
 *    store and the virtual call.
 *  - pfn10008B80 is a bare `ret` in this build (see CONTRACT); it is kept as a
 *    slot so the call order stays one-for-one.
 *  - the function does not return: 0x1007CC00 is `exit`. */
typedef struct BrShutObj BrShutObj;

typedef struct BrShutObjVtbl {
    void *aSlots[6];                        /* +0x00..+0x14 -- untouched */
    void (*f18)(BrShutObj *pThis, int a);   /* +0x18, __thiscall         */
} BrShutObjVtbl;

/* The storage slice2_25.h models twice, as BrOptObj and as BrGameSub.  Neither
 * of those exposes BOTH +0x68 and vtable slot 6, which is what this function
 * needs, so it is modelled here under its own name rather than by widening
 * somebody else's struct. */
struct BrShutObj {
    const BrShutObjVtbl *pVtbl;                     /* +0x00 */
    unsigned char        pad04[0x68 - sizeof(void *)];
    int32_t              f68;                       /* +0x68 */
};

typedef struct BrShutdownHost {
    BrShutObj **ppAA2904;      /* 0x10AA2904 */
    int32_t    *pn0AC300;      /* 0x100AC300 -- gates the first block only  */
    int32_t    *pn22AF18;      /* 0x1022AF18 */
    int32_t    *pn0940A4;      /* 0x100940A4 */

    void (*pfn1002C4A0)(void);
    void (*pfn10016990)(void);   /* slice2_14.h BrLruShutdown, global form */
    void (*pfnB501CC)(void);     /* *0x10B501CC, nullable                  */
    void (*pfn10079550)(void);
    void (*pfn10078BC0)(void);
    void (*pfn10078DB0)(void);
    void (*pfn10073730)(void);
    void (*pfn10005BE0)(int);    /* only when *pn22AF18 != 0, argument 1   */
    void (*pfn1003BFD0)(void);
    void (*pfn1003BF60)(void);
    void (*pfn10002CF0)(void);   /* only when *pn0940A4 != 0               */
    void (*pfn10008B80)(void);   /* bare `ret` in this build               */
    void (*pfn18AA0D0)(void);    /* *0x118AA0D0, nullable                  */
    void (*pfn690A28)(void);     /* *0x10690A28, nullable                  */
    void (*pfn10061620)(void);
    void (*pfn10008970)(void);   /* __thiscall on 0x10A99780               */
    void (*pfn1002AEA0)(void);
    void (*pfn10074050)(void);
    void (*pfnCoUninitialize)(void);
    void (*pfnExit)(int);        /* 0x1007CC00 */
} BrShutdownHost;

extern const BrShutdownHost *g_pBrShutdownHost;

void BrSub10038F30(int code);

/* ==========================================================================
 * 0x10008CF0  BrLogPrint -- fatal message screen, never returns
 * ==========================================================================
 *
 * Puts a 0x8000-byte display list on the stack, points the global write cursor
 * at it, draws one line of text centred horizontally at y = 0xDC, terminates
 * the list with G_ENDDL (0xB8000000, 0) and submits it, then spins forever
 * polling VK_ESCAPE and sleeping 1 ms.  ESC calls BrSub10038F30(1), i.e.
 * exit(1); if that ever returned the spin would simply continue.
 *
 * GOTCHAS:
 *  - the x coordinate is a SIGNED halving of the screen width (`cdq/sub/sar`),
 *    not an unsigned shift.
 *  - the key test is on the LOW 16 BITS of GetAsyncKeyState's result, so both
 *    the "down" and the "pressed since last call" bits count.
 *  - the cursor global is advanced by 8 BYTES, and the two words are written
 *    through the pre-advance value.
 *
 * DEVIATION: the original's buffer is `_alloca(0x8000)` (0x1007E170 is the
 * stack probe).  A plain local array is used instead; the function never
 * returns, so the lifetime is identical. */
#define BR_LOGPRINT_DL_BYTES  0x8000
#define BR_LOGPRINT_TEXT_Y    0xDC
#define BR_LOGPRINT_VK_ESCAPE 0x1B

typedef struct BrLogHost {
    void (*pfn10016990)(void);      /* slice2_14.h BrLruShutdown, global form */
    void (*pfn10019260)(void);      /* slice2_15.h BrSub_10019260 */
    void (*pfn10019270)(void);      /* slice2_15.h BrSub_10019270 */
    void (*pfn100192F0)(int);       /* slice2_15.h BrSub_100192F0, arg 0x14 */
    void (*pfnTextDraw)(const char *psz, int x, int y);   /* 0x10019300 */
    void (*pfnSubmit)(void *pDl);   /* *0x10B501D0 */
    void (*pfnShutdown)(int);       /* 0x10038F30 */
    int  (*pfnKeyAsync)(int vk);    /* USER32 GetAsyncKeyState */
    void (*pfnSleep)(unsigned ms);  /* KERNEL32 Sleep */
    int32_t   *pnScreenW;           /* 0x100A81C0 */
    uint32_t **ppDlCursor;          /* 0x106C0680 */
} BrLogHost;

extern const BrLogHost *g_pBrLogHost;

void BrLogPrint(const void *p);

/* ==========================================================================
 * 0x10051990  BrOptFn10051990 -- build one menu screen
 * ==========================================================================
 *
 * The sixth member of the family slice3_33.c ports as BrExt_1004A580 ..
 * BrExt_1004CAC0: allocate a 0x348 screen, then six 0x1E214 controls.
 *
 * SIGNATURE NOTE: slice2_25.h is the only declarer and types the argument
 * `BrOptObj *`; slice3_33.h types the SAME 0xC8-byte object `BrUiPhase *`.
 * The declaration below keeps slice2_25.h's spelling and the body casts, which
 * is safe: the two are alternative views of one allocation, and this function
 * only ever touches the BrUiPhase fields (+0x10, +0x12, +0x14, +0x6C).
 * The original returns 1; the declared type discards it.
 *
 * GOTCHAS:
 *  - fX is 195.0 and fY is 130.0, the same literals as the five twins.
 *  - controls 2 and 3 are placed at ABSOLUTE coordinates (0,29) and (13,7),
 *    not relative to fX/fY -- the only two in the family that are.
 *  - only the last control is selectable: cSel ends at 1 while cCtl ends at 6.
 *  - the last control sets f1E20C to 2.  Every control in slice3_33's five
 *    twins sets 3 (once 5).  Do not "normalise" it.
 *  - f34's third argument is 1 and its FOURTH is 0 here; slice3_33's sites all
 *    pass 1 and 1.
 *  - the two literals 2 and 5 are f38's fourth and fifth arguments at every
 *    call site, here as there. */
typedef struct BrUi51990Ctx {
    /* DEVIATION, inherited from slice1_06.h: 0x1003E260 takes only an index in
     * the original; the port injects its host. */
    const struct BrErrHost *pErrHost;

    const void *p0AB438;            /* 0x100AB438 -- the one style block */

    /* Installed into control slots, never called here.  The signature is
     * slice3_33.h's BrUiCtlFn, spelled out so this header need not include it. */
    void (*p1003F440)(void *);      /* -> control 4's +0x04 */
    void (*p1003F540)(void *);      /* -> control 5's +0x04 */
    void (*p100471F0)(void *);      /* -> control 6's +0x04 */
    void (*p10047120)(void *);      /* -> control 6's +0x08 */
    void (*p10047360)(void *);      /* -> control 6's +0x0C */
} BrUi51990Ctx;

extern const BrUi51990Ctx *g_pBrUi51990Ctx;

void BrOptFn10051990(struct BrOptObj *pThis);

/* ==========================================================================
 * NOT IMPLEMENTED -- see the report
 * ==========================================================================
 *
 *   0x1003C020  BrSub1003C020    0x1003CDA0  BrSub1003CDA0
 *   0x1003CE80  BrSub1003CE80    0x1003D210  BrFn1003D210
 *
 * All four are the DirectPlay module slice2_22.h already declares mostly
 * unportable, and each has a concrete blocker recorded in the report (a callee
 * whose HRESULT the existing declaration throws away, a session-descriptor
 * layout no header models past +0x2C, or a signature that cannot hold a
 * pointer).  They are deliberately left for integration to stub. */

#endif /* SLICE4_52_H */
