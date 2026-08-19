/* slice3_31.h -- BRD3D.dll, another module's packet.
 *
 * WHAT THIS PACKET IS
 * ===================
 * The packet nominally spans 0x10008750-0x100478C0, but forty of its 125
 * functions are already implemented elsewhere in-tree (br_pod.h, br_seg.h,
 * br_vec.h, br_vecd.h, br_mat.h, br_bits.h, br_span.h, br_state.h). What is
 * left is one function of the POD reader plus a single coherent subsystem:
 *
 *   the continuation of slice2_26's PHASE (screen/mode) SWITCHER,
 *   0x10045780-0x10047610.
 *
 * slice2_26.h describes that mechanism in full and this file does not repeat
 * it. Read slice2_26.h first; everything below uses its vocabulary, its
 * types, and its shapes:
 *
 *   ACTIVATE   lazily build a 0xC8-byte BrPhase, publish it in a per-phase
 *              global and in the "current phase" global 0x10AA2904.
 *   LEAVE      drive the game object's +0x2AE8 sub-object through vtable
 *              slot +0x1C, notify the current phase through ITS slot +0x00
 *              with the argument 1, clear a few globals, repoint 0x10AA2904.
 *   HOOK       run an ACTIVATE, then poke a LEAVE routine into some phase's
 *              +0x08 field.
 *
 * This packet contributes twelve HOOK installers, nine ACTIVATE routines,
 * thirty-five LEAVE routines and a scattering of small helpers (a keyboard
 * ring buffer, six "set a flag and change mode" callbacks, a per-frame
 * state-machine step).
 *
 * NAMING
 * ======
 * Types, the phase object and the cross-slice helper names all come from
 * slice2_26.h / slice2_25.h; nothing is redefined here. Five addresses in
 * this packet were already *declared* (but not implemented) by those two
 * headers, and they keep the exact names and signatures they were given
 * there, even where that loses a return value:
 *
 *     0x10045C90  BrExt_10045C90   (slice2_26.h)
 *     0x10046400  BrSub10046400    (slice2_25.h)
 *     0x10046CD0  BrExt_10046CD0   (slice2_26.h)
 *     0x10046DC0  BrExt_10046DC0   (slice2_26.h)
 *     0x10047360  BrSub10047360    (slice2_25.h)
 *
 * Everything else follows slice2_26's scheme: BrPhase<Shape>_<ADDRESS>.
 * Globals keep address-derived names.
 *
 * HOW STATE IS REACHED  (the one structural DEVIATION -- see the .c)
 * =================================================================
 * slice2_26 gathers its globals into a BrPhaseCtx passed as an added first
 * argument. This packet cannot do that: five of its functions are already
 * declared with fixed signatures, and eight more are stored into BrPhase's
 * +0x08 slot, whose type (BrPhaseHookFn) admits exactly one void* argument.
 * So instead the two context blocks are installed once, module-wide, with
 * BrPhase31SetCtx(), and every function below keeps the original's own
 * argument list. Call BrPhase31SetCtx() before anything else in this module.
 */
#ifndef SLICE3_31_H
#define SLICE3_31_H

#include <stdint.h>

#include "br_pod.h"      /* BrPod, BrPodRead -- for 0x10008850            */
#include "slice2_25.h"   /* BrGameObj, BrGameSub, BrSub10046400/10047360  */
#include "slice2_26.h"   /* BrPhase, BrPhaseCtx, BrOperatorNew, BrExt_*   */

/* ==========================================================================
 * 0x10008850 -- the one POD routine br_pod.h does not already cover
 * ==========================================================================
 *
 * The original is `LoadPod(int i, void *pv)`, an overload of the
 * `LoadPod(int)` that br_pod.h calls BrPodLoad (0x10008810): where that one
 * allocates, this one reads into storage the caller supplies. It reports an
 * out-of-range index and then calls ReadPod anyway; br_pod.c's BrPodRead
 * already refuses the same indices, so nothing is read.
 *
 * Returns pvBuffer -- ALWAYS, including on a rejected index and on I/O
 * failure. The original discards ReadPod's status too.
 */
void *BrPodLoadInto(BrPod *pPod, int iEntry, void *pvBuffer);

/* ==========================================================================
 * The phase vtable -- RESOLVED, and `BrPhaseVtblExt` is GONE
 * ==========================================================================
 *
 * There used to be a `BrPhaseVtblExt` overlay here: slice2_26.h's BrPhaseVtbl
 * modelled only slot +0x00, because that is all its own range calls, while
 * 0x10046FD0 and 0x10047290 in THIS range also call slot +0x1C. Rather than
 * coin a second competing definition of BrPhaseVtbl -- which the contract
 * forbids -- the extra slot was reached through
 *
 *     struct BrPhaseVtblExt { BrPhaseVtbl base; void *aReserved[6];
 *                             void (*f1C)(BrPhase *); };
 *
 * and slice3_31.c cast `p->pVtbl` to it. That was sound arithmetic while
 * `base` was one pointer wide: 4 + 6*4 == 0x1C, so f1C landed on slot +0x1C.
 *
 * It STOPS being sound the moment BrPhaseVtbl is the real nine-slot table,
 * and that is now the case -- slice2_26.h aliases it to br_phase.h's
 * BrPhaseVtbl_. `base` is then nine pointers, aReserved another six, and
 * `f1C` lands 0x1C bytes PAST THE END of the actual vtable at 0x1008F700 --
 * which is .rdata belonging to BrTextBox. A wild read followed by an indirect
 * call through whatever it found.
 *
 * The overlay is deleted rather than re-padded, because there is nothing left
 * for it to do: slot +0x1C (0x10048AA0) is an ordinary member of
 * BrPhaseVtbl_, and slice3_31.c reaches it as `p->pVtbl->f1C(p)` with no cast
 * at all. This is the good outcome of the merge -- the reason the overlay
 * existed was that no single type held both slots, and now one does.
 */

/* ==========================================================================
 * Foreign objects this range reaches that slice2_25/26 do not model
 * ========================================================================== */

/* *(void **)0x10AA2E80. 0x10047360 tests four consecutive dwords. */
typedef struct BrObjAA2E80 {
    unsigned char pad00[0x2C];
    int32_t       f2C;    /* +0x2C */
    int32_t       f30;    /* +0x30 */
    int32_t       f34;    /* +0x34 */
    int32_t       f38;    /* +0x38 */
} BrObjAA2E80;

/* The object slice2_25.h models as BrGameObj is BIGGER than that model:
 * 0x10047360 reaches +0x1E20C and +0x3850, and the constructor at
 * 0x100476C0 (skipped -- see the report) writes as far as +0x3838. The
 * highest byte any of them touches is +0x1E20D (the second byte of the
 * uint16 counter), so storage handed to BrSub10047360 must be at least this
 * many bytes. The fields past sizeof(BrGameObj) are reached by byte offset.
 * (slice2_25.h's BrObj29D4 shows the same pattern: one 16-bit field at
 * +0x1E164 of an otherwise unmodelled object.) */
#define BR_GAMEOBJ31_MIN_SIZE 0x1E210u

#define BR_GAMEOBJ_OFF_FLAGS  0x001Cu   /* uint32 */
#define BR_GAMEOBJ_OFF_COUNT  0x1E20Cu  /* uint16, incremented by 0x10047360 */
#define BR_GAMEOBJ_OFF_STATE  0x2B64u   /* uint8, the state byte it selects  */
#define BR_GAMEOBJ_OFF_F3850  0x3850u   /* uint32, one more veto flag        */

/* Bits of BR_GAMEOBJ_OFF_FLAGS that 0x10047360 tests. Named for their value
 * only -- the disassembly does not say what they mean. */
#define BR_GAMEOBJ_FLAG_10       0x00000010u  /* set -> do nothing          */
#define BR_GAMEOBJ_FLAG_100      0x00000100u  /* set -> run the step        */
#define BR_GAMEOBJ_FLAG_1000000  0x01000000u  /* set -> do nothing          */

/* 0x10047340 zeroes exactly 32 bytes at 0x10A9D618, which is also one of the
 * two destinations of the name copy in the eight "reset the name" LEAVE
 * routines. 32 is therefore the only buffer size the code itself attests to,
 * and it is used for all three name buffers. */
#define BR_NAME31_LEN 32

/* ==========================================================================
 * The globals this range owns that slice2_26's BrPhaseCtx does not carry
 * ==========================================================================
 *
 * INTEGRATION GOTCHA -- two addresses are typed differently here
 * than in slice2_26.h, because this range uses them differently:
 *
 *   0x10AA2950  slice2_26 has `int32_t nAA2950` (it only ever clears it).
 *               0x10046400 loads it and installs it AS THE CURRENT PHASE,
 *               so it is a BrPhase *.
 *   0x10AA29AC  slice2_26 has `int32_t nAA29AC` (again, only cleared).
 *               0x10046260 writes a LEAVE routine into its +0x08 field, so
 *               it too is a BrPhase *.
 *
 * They are declared as BrPhase * below. When the two contexts are merged,
 * the pointer typing is the correct one; slice2_26's uses (stores of 0) are
 * unaffected by the change.
 */
typedef struct BrPhaseCtx31 {
    /* --- phase singletons ------------------------------------------------ */
    BrPhase *pAA2910;   /* 0x10AA2910  built by 0x10046170 */
    BrPhase *pAA291C;   /* 0x10AA291C  built by 0x10045900 */
    BrPhase *pAA2924;   /* 0x10AA2924  built by 0x10045AF0 */
    BrPhase *pAA2928;   /* 0x10AA2928  built by 0x10045BC0 */
    BrPhase *pAA292C;   /* 0x10AA292C  built by 0x10045C90 */
    BrPhase *pAA2930;   /* 0x10AA2930  built by 0x10045DC0 */
    BrPhase *pAA2934;   /* 0x10AA2934  built by 0x10045EA0 */
    BrPhase *pAA2938;   /* 0x10AA2938  built by 0x10045F70 */
    BrPhase *pAA293C;   /* 0x10AA293C  built by 0x100460A0 */
    BrPhase *pAA2950;   /* 0x10AA2950  see the GOTCHA above */
    BrPhase *pAA2958;   /* 0x10AA2958 */
    BrPhase *pAA296C;   /* 0x10AA296C */
    BrPhase *pAA2970;   /* 0x10AA2970 */
    BrPhase *pAA2974;   /* 0x10AA2974  the SECOND object 0x10045C90 builds */
    BrPhase *pAA2978;   /* 0x10AA2978  the SECOND object 0x10045F70 builds */
    BrPhase *pAA2998;   /* 0x10AA2998 */
    BrPhase *pAA29AC;   /* 0x10AA29AC  see the GOTCHA above */
    BrPhase *pAA29C8;   /* 0x10AA29C8  target of six of the HOOK installers */

    /* --- scalars --------------------------------------------------------- */
    int32_t  n0AB3F4;   /* 0x100AB3F4  set to -1 by every name reset        */
    int32_t  nA9CFF8;   /* 0x10A9CFF8  (reserved -- not written here)       */
    int32_t  nAA284C;   /* 0x10AA284C  gate in 0x10047360                   */
    int32_t  nAA2854;   /* 0x10AA2854  set to 2 or 3 by the mode callbacks  */
    int32_t  nAA285C;   /* 0x10AA285C */
    int32_t  nAA28A4;   /* 0x10AA28A4  copied to nAA28AC by 0x10045DC0      */
    int32_t  nAA28AC;   /* 0x10AA28AC */
    int32_t  nAA28B0;   /* 0x10AA28B0  first branch of 0x10047290           */
    int32_t  nAA28B4;   /* 0x10AA28B4  second branch of 0x10047290          */
    int32_t  nAA28C4;   /* 0x10AA28C4 */
    int32_t  nAA28E0;   /* 0x10AA28E0  cleared by 0x10046E10 only           */
    int32_t  nAA28E4;   /* 0x10AA28E4  cleared by the other name resets     */
    int32_t  nAA28F0;   /* 0x10AA28F0  set by 0x100474D0                    */
    int32_t  nAA28F4;   /* 0x10AA28F4  set by 0x100475C0                    */
    int32_t  nAA28F8;   /* 0x10AA28F8  set by 0x10047500                    */
    int32_t  nAA28FC;   /* 0x10AA28FC  set by 0x10047530                    */
    int32_t  nAA29C0;   /* 0x10AA29C0 */
    int32_t  nAA29CC;   /* 0x10AA29CC */
    int32_t  nAA29E0;   /* 0x10AA29E0 */
    int32_t  nAA29E4;   /* 0x10AA29E4 */
    int32_t  nAA29EC;   /* 0x10AA29EC */
    int32_t  nAA29F0;   /* 0x10AA29F0 */
    int32_t  nAA2A40;   /* 0x10AA2A40  set by 0x10047590                    */
    int32_t  nAA2AD4;   /* 0x10AA2AD4  picks the branch in 0x10047210/50    */
    int32_t  nACED34;   /* 0x10ACED34 */
    int32_t  nAD0984;   /* 0x10AD0984 */
    uint16_t n0AC6A4;   /* 0x100AC6A4  set to 0x7FFF by 0x10047560          */
    uint8_t  b680738;   /* 0x10680738  set to 0xFF by 0x10046260            */

    /* --- the keyboard ring ------------------------------------------------ */
    int32_t  nAA33E4;         /* 0x10AA33E4  the pending key, 0 = none      */
    int32_t  nAA2A48;         /* 0x10AA2A48  write index, wraps at 32       */
    int32_t  aA9E150[32];     /* 0x10A9E150  the ring itself                */

    /* --- the block 0x10047120 conditionally clears ------------------------ */
    int32_t  nAA26F0;         /* 0x10AA26F0  must be > 0 for the clear      */
    uint8_t  bAA26F4;         /* 0x10AA26F4  must be 0 for the clear        */
    uint8_t  bAA26F5;         /* 0x10AA26F5  must be 0 for the clear        */
    uint8_t  aAA26F6[24];     /* 0x10AA26F6   6 dwords */
    uint8_t  aAA270E[48];     /* 0x10AA270E  12 dwords */
    uint8_t  aAA2740[96];     /* 0x10AA2740  24 dwords */

    /* --- the three name buffers ------------------------------------------- */
    char     sz39B720[BR_NAME31_LEN];  /* 0x1039B720  the SOURCE (in .bss)  */
    char     szAA2518[BR_NAME31_LEN];  /* 0x10AA2518  destination 1         */
    char     szA9D618[BR_NAME31_LEN];  /* 0x10A9D618  destination 2         */

    /* --- foreign objects --------------------------------------------------- */
    BrObjAA2E80 *pAA2E80;   /* 0x10AA2E80 -- dereferenced without a guard   */
    void        *pB4DF30;   /* 0x10B4DF30 -- `this` for BrExt_1006A4A0      */
    void        *pB4FBE8;   /* 0x10B4FBE8 -- its one argument               */
} BrPhaseCtx31;

/* Install the two context blocks. DEVIATION (portability): the original
 * addresses all of this as absolute globals. Both pointers must be non-NULL
 * and must stay alive for as long as any function below is called. */
void BrPhase31SetCtx(BrPhaseCtx *pBase, BrPhaseCtx31 *pExt);

/* ==========================================================================
 * Cross-slice callees. The integration wires these; stand-ins live in
 * port/tests/test_slice3_31.c and NOWHERE else. Callees already declared by
 * slice2_25.h / slice2_26.h are NOT redeclared -- they are used as declared.
 * ========================================================================== */

/* --- enter hooks stored into BrPhase.pfn04 -------------------------------- */
/* XSLICE 0x10049C20 */ extern void BrExt_10049C20(BrPhase *pSelf);
/* XSLICE 0x10049F40 */ extern void BrExt_10049F40(BrPhase *pSelf);
/* XSLICE 0x1004A260 */ extern void BrExt_1004A260(BrPhase *pSelf);
/* XSLICE 0x1004F2B0 */ extern void BrExt_1004F2B0(BrPhase *pSelf);
/* XSLICE 0x1004F700 */ extern void BrExt_1004F700(BrPhase *pSelf);
/* XSLICE 0x100509F0 */ extern void BrExt_100509F0(BrPhase *pSelf);
/* XSLICE 0x10050060 */ extern void BrExt_10050060(BrPhase *pSelf);
/* XSLICE 0x10052030 */ extern void BrExt_10052030(BrPhase *pSelf);
/* XSLICE 0x10052F50 */ extern void BrExt_10052F50(BrPhase *pSelf);
/* XSLICE 0x10053CF0 */ extern void BrExt_10053CF0(BrPhase *pSelf);
/* XSLICE 0x10054B50 */ extern void BrExt_10054B50(BrPhase *pSelf);

/* --- plain callees --------------------------------------------------------- */

/* XSLICE 0x10045A00 */
/* Guard at the head of 0x10045900: zero means "refuse", and the refusal path
 * reports through BrExt_10074030 + BrExt_100419D0 and returns 0. */
extern int32_t BrExt_10045A00(void);

/* XSLICE 0x10074030 */
/* Takes a small integer id (0xD here) and yields the pointer that
 * BrExt_100419D0 is then handed -- a message-table lookup, most likely. */
extern void *BrExt_10074030(int32_t nId);

/* XSLICE 0x10072AF0 */
/* Two arguments, __cdecl. Every call site in this range passes 2 or 3 as the
 * first and the constant 0x00200020 as the second. */
extern void BrExt_10072AF0(int32_t a, uint32_t b);

/* XSLICE 0x1003E0E0 */
/* A predicate. NOT br_state.h's BrIsAnyActive (0x1003E080) -- a different
 * address, deliberately not folded into it. */
extern int32_t BrExt_1003E0E0(void);

/* XSLICE 0x1003E310 */ extern void BrExt_1003E310(void);
/* XSLICE 0x10079550 */ extern void BrExt_10079550(void);
/* XSLICE 0x1005FBC0 */ extern void BrExt_1005FBC0(int32_t a);
/* XSLICE 0x10041A00 */ extern int32_t BrExt_10041A00(void *pArg);
/* XSLICE 0x10041AC0 */ extern int32_t BrExt_10041AC0(void *pArg);
/* XSLICE 0x10042410 */ extern int32_t BrExt_10042410(void *pArg);
/* XSLICE 0x100424D0 */ extern int32_t BrExt_100424D0(void *pArg);
/* XSLICE 0x10043260 */ extern void BrExt_10043260(void *pArg);
/* XSLICE 0x10043330 */ extern void BrExt_10043330(void *pArg);

/* XSLICE 0x1006A4A0 */
/* __thiscall. `this` is 0x10B4DF30 and the single argument is 0x10B4FBE8. */
extern void BrExt_1006A4A0(void *pThis, void *pArg);

/* XSLICE 0x10047660 */
/* Called by 0x10047610 once a key has been pushed into the ring. */
extern void BrExt_10047660(void);

/* ==========================================================================
 * 0x10045780-0x100458E0 -- twelve HOOK installers
 * ==========================================================================
 *
 * All twelve have the identical body:
 *
 *     <activate>(pArg);            // pArg is passed and then ignored
 *     <slot>->pfn08 = <leave>;
 *     return 1;
 *
 * The even-numbered ones run BrPhaseActivate_100451E0 (slice2_26) and poke
 * pAA29C8; the odd-numbered ones run BrPhaseActivate_10045BC0 (below) and
 * poke pAA29F4. GOTCHA: the slot pointer is loaded and dereferenced with no
 * NULL check, even though the activate it just ran can fail and leave the
 * slot unset -- and unlike slice2_26's installers, none of these touch
 * n0AA010. They return 1 whatever the activate returned.
 */
int BrPhaseHook_10045780(void *pArg);   /* -> pAA29C8, leave 0x10046750 */
int BrPhaseHook_100457A0(void *pArg);   /* -> pAA29F4, leave 0x10046790 */
int BrPhaseHook_100457C0(void *pArg);   /* -> pAA29C8, leave 0x10046830 */
int BrPhaseHook_100457E0(void *pArg);   /* -> pAA29F4, leave 0x10046870 */
int BrPhaseHook_10045800(void *pArg);   /* -> pAA29C8, leave 0x10046910 */
int BrPhaseHook_10045820(void *pArg);   /* -> pAA29F4, leave 0x10046950 */
int BrPhaseHook_10045840(void *pArg);   /* -> pAA29C8, leave 0x100469F0 */
int BrPhaseHook_10045860(void *pArg);   /* -> pAA29F4, leave 0x10046A30 */
int BrPhaseHook_10045880(void *pArg);   /* -> pAA29C8, leave 0x10046AD0 */
int BrPhaseHook_100458A0(void *pArg);   /* -> pAA29F4, leave 0x10046B10 */
int BrPhaseHook_100458C0(void *pArg);   /* -> pAA29C8, leave 0x10046BB0 */
int BrPhaseHook_100458E0(void *pArg);   /* -> pAA29F4, leave 0x10046BF0 */

/* ==========================================================================
 * ACTIVATE routines
 * ==========================================================================
 *
 * Shape, prologue, epilogue and the three-outcome return are exactly as
 * slice2_26.h documents, including its load-order GOTCHA: the CURRENT-phase
 * global is re-read after the enter hook runs and again between the f0C and
 * f68 stores, so a hook that repoints it has its own flags set instead.
 */

/* 0x10045900  Guarded: if BrExt_10045A00() is 0 it reports
 * (BrExt_100419D0(BrExt_10074030(0xD))) and returns 0 without building
 * anything. Otherwise BrExt_100419D0(p0AD300), then slot 0x10AA291C with
 * enter hook 0x1004F2B0. */
int BrPhaseActivate_10045900(void);

/* 0x10045AF0  No prologue. Slot 0x10AA2924, enter hook 0x1004F700. */
int BrPhaseActivate_10045AF0(void);

/* 0x10045BC0  No prologue. Slot 0x10AA2928, enter hook 0x10050060. */
int BrPhaseActivate_10045BC0(void);

/* 0x10045C90  TWO objects. First slot 0x10AA292C with enter hook 0x100509F0
 * and both flags; then, on the just-built path only, a second 0xC8 object in
 * slot 0x10AA2974 with enter hook 0x10049F40 and f0C only -- no f68.
 * Either allocation failing returns 0. Its argument is ignored.
 * Pre-declared by slice2_26.h; the original returns 1/0 and this declaration
 * discards that, which is what all four call sites do anyway. */
/* implemented here: void BrExt_10045C90(void *p) -- see slice2_26.h */

/* 0x10045DC0  Prologue: nAA28AC = nAA28A4. Slot 0x10AA2930, enter hook
 * 0x10052030. */
int BrPhaseActivate_10045DC0(void);

/* 0x10045EA0  No prologue. Slot 0x10AA2934, enter hook 0x10052F50. */
int BrPhaseActivate_10045EA0(void);

/* 0x10045F70  The twin of 0x10045C90: slot 0x10AA2938 / hook 0x10053CF0,
 * then slot 0x10AA2978 / hook 0x1004A260 with f0C only. */
int BrPhaseActivate_10045F70(void);

/* 0x100460A0  No prologue. Slot 0x10AA293C, enter hook 0x10054B50. */
int BrPhaseActivate_100460A0(void);

/* 0x10046170  Prologue: BrExt_100419D0(p0AD300), BrExt_10072AF0(3,0x200020),
 * nAA2854 = 3 -- and that last store happens on BOTH paths, including the
 * already-built one. Slot 0x10AA2910, enter hook 0x10049C20. */
int BrPhaseActivate_10046170(void);

/* 0x10046260  The heaviest one. Prologue: n0AA010 = 2, BrExt_1003E680(),
 * nACED34 = 0, nAD0984 = 1, n0AA010 = 2 (again), n0AC304 = 1, b680738 = 0xFF
 * -- all on both paths; n0AC304 = 1 is then repeated on the not-yet-built
 * path. Slot 0x10AA290C, enter hook 0x1004B430. Epilogue, just-built path
 * only: BrExt_10008B80() (a stub in this build), BrExt_1003DFC0(),
 * BrExt_1003E510(), then pAA29AC->pfn08 = slice2_26's leave 0x10044CB0. */
int BrPhaseActivate_10046260(void);

/* ==========================================================================
 * HOOK installers that are not part of the twelve
 * ========================================================================== */

/* 0x10045AA0  n0AA010 = 0, BrExt_1003E680(), nACED34 = 0,
 * BrExt_10045C90(pArg), pAA29B0->pfn08 = leave 0x10046D70, n0AA010 = 0
 * again, BrExt_1003E510(). Returns 1. */
int BrPhaseHook_10045AA0(void *pArg);

/* 0x10046380  The twin of slice2_26's BrPhaseHook_10045050: n0AC304 = 0,
 * BrPhaseActivate_10045110, n0AC304 = 1, pAA29B4->pfn08 = leave 0x10046D20,
 * n0AA010 = 2. GOTCHA: 0x10045050 installs 0x10046CD0 and finishes with
 * n0AA010 = 0; this one installs 0x10046D20 and finishes with n0AA010 = 2.
 * Returns 1. */
int BrPhaseHook_10046380(void *pArg);

/* ==========================================================================
 * LEAVE routines
 * ==========================================================================
 *
 * Every one of them:
 *
 *     pEntity->pSub->pVtbl->pfnSlot7(pEntity->pSub);   // +0x2AE8, slot +0x1C
 *     if (pAA2904) pAA2904->pVtbl->f00(pAA2904, 1);
 *     <clear some globals>
 *     pAA2904 = <the next phase>;
 *
 * GOTCHA (load order): the next phase is READ OUT of its global BEFORE the
 * clears and written to pAA2904 AFTER them. No routine here clears the
 * global it is about to read, so the two orders agree -- but the original's
 * order is reproduced anyway.
 *
 * RETURN VALUE.  This block used to read "All of them return 0 in the
 * original. They are declared void ... because no caller anywhere looks at
 * the value." The first sentence is right and the last clause was WRONG, and
 * the wrong clause is what set the type.
 *
 * The +0x08 slot is the ACTION hook and 0x10048180 TESTS its result:
 *
 *     10048280  ff5608   call dword ptr [esi + 8]
 *     10048286  85c0     test eax, eax
 *     10048288  7508     jne  0x10048292      ; zero -> return 0 immediately
 *
 * A zero makes BrUiFrame_10048180 return 0 and skip the `[0x10AA33E4] = 0`
 * store, the `flags &= ~2` clear, the child loop and the vtable +0x08 draw.
 * 0x1004CECC is one install site, seen in the D3D build:
 *
 *     1004CECC  c7470860650410  mov dword ptr [edi + 8], 0x10046560
 *     1004CED3  66c7870ce201000300  mov word ptr [edi + 0x1e20c], 3
 *
 * -- a UI object of exactly the shape 0x10048180 frames.
 *
 * So every routine below returns int32_t, and BrPhaseHookFn_ (br_phase.h) is
 * `int32_t (*)(void *)`. The value is 0 for ALL FORTY-THREE of them; each was
 * disassembled rather than assumed, and the address of the instruction that
 * leaves eax zero is given per routine so the claim can be re-checked one
 * line at a time. Two shapes produce it:
 *
 *   - an explicit `xor eax, eax` in the return slot (35 routines), and
 *   - the leftover scan character of the second inlined strcpy (the 8 that
 *     reset the player name; the `xor` is ~20 bytes earlier and nothing
 *     between it and the `ret` writes eax).
 */
/* 0x100463F5 */
int32_t BrPhaseLeave_100463C0(void *pEntity); /* -> pAA2958; clears pAA2940 */
/* 0x10046400 is BrSub10046400 (slice2_25.h): -> pAA2950; clears pAA2954,
 * nAA29E4, nAA29E0, nAA285C. */
/* 1004648F */
int32_t BrPhaseLeave_10046450(void *pEntity); /* -> pAA2908; pAA290C, pAA29AC */
/* 100464D5 */
int32_t BrPhaseLeave_100464A0(void *pEntity); /* -> pAA2908; pAA2910          */
/* 10046515 */
int32_t BrPhaseLeave_100464E0(void *pEntity); /* -> pAA290C; pAA2914          */
/* 10046555 */
int32_t BrPhaseLeave_10046520(void *pEntity); /* -> pAA2908; pAA2918          */
/* 1004659A */
int32_t BrPhaseLeave_10046560(void *pEntity); /* -> pAA297C; pAA2998; then
                                            * BrExt_10079550()             */
/* 100465D5 */
int32_t BrPhaseLeave_100465A0(void *pEntity); /* -> pAA2918; pAA297C          */
/* 10046615 */
int32_t BrPhaseLeave_100465E0(void *pEntity); /* -> pAA2918; pAA2980          */
/* 1004665F */
int32_t BrPhaseLeave_10046620(void *pEntity); /* -> pAA2980; pAA2990, nAA29F0 */
/* 100466AF */
int32_t BrPhaseLeave_10046670(void *pEntity); /* -> pAA2980; pAA2994, nAA29EC */
/* 10046709 */
int32_t BrPhaseLeave_100466C0(void *pEntity); /* -> pAA2918; pAA2984; then
                                            * BrExt_1003E310() and
                                            * BrExt_1006A4A0(pB4DF30,
                                            *                pB4FBE8)      */
/* 10046745 */
int32_t BrPhaseLeave_10046710(void *pEntity); /* -> pAA2918; pAA2988          */
/* 10046785 */
int32_t BrPhaseLeave_10046750(void *pEntity); /* -> pAA292C; pAA2918          */
/* 10046865 */
int32_t BrPhaseLeave_10046830(void *pEntity); /* -> pAA2930; pAA2918          */
/* 10046945 */
int32_t BrPhaseLeave_10046910(void *pEntity); /* -> pAA2934; pAA2918          */
/* 10046A25 */
int32_t BrPhaseLeave_100469F0(void *pEntity); /* -> pAA2938; pAA2918          */
/* 10046B05 */
int32_t BrPhaseLeave_10046AD0(void *pEntity); /* -> pAA293C; pAA2918          */
/* 10046BE5 */
int32_t BrPhaseLeave_10046BB0(void *pEntity); /* -> pAA2914; pAA2918          */
/* 10046CC5 */
int32_t BrPhaseLeave_10046C90(void *pEntity); /* -> pAA2908; pAA291C          */
/* 0x10046CD0 is BrExt_10046CD0 (slice2_26.h): -> pAA2930; clears pAA2914
 * and pAA29B4. */
/* 10046D5F */
int32_t BrPhaseLeave_10046D20(void *pEntity); /* -> pAA295C; pAA2914, pAA29B4 */
/* 10046DB9 */
int32_t BrPhaseLeave_10046D70(void *pEntity); /* -> pAA291C; pAA292C, pAA29B0,
                                            *              pAA2974         */
/* 0x10046DC0 is BrExt_10046DC0 (slice2_26.h): -> pAA2924; clears pAA292C,
 * pAA29B0 and pAA2974 -- the same three as 0x10046D70. */
/* 10047095 */
int32_t BrPhaseLeave_10047060(void *pEntity); /* -> pAA292C; pAA2930          */
/* 100470D5 */
int32_t BrPhaseLeave_100470A0(void *pEntity); /* -> pAA2934; pAA2938          */
/* 10047115 */
int32_t BrPhaseLeave_100470E0(void *pEntity); /* -> pAA2938; pAA293C          */

/* --- the eight LEAVE routines that also reset the player name ------------
 *
 * Same prologue, then:
 *
 *     pAA2928 = NULL; nAA29C0 = nAA29CC = nAA28E4 = 0; n0AB3F4 = -1;
 *     strcpy(szAA2518, sz39B720);
 *     strcpy(szA9D618, sz39B720);
 *
 * The source, 0x1039B720, lives in the zero-initialised tail of .data, so it
 * is the empty string until something else writes it. The original's copies
 * are unbounded inline `rep movs`; see the DEVIATION in the .c.
 */
/* 10046818 */
int32_t BrPhaseLeaveNamed_10046790(void *pEntity);  /* -> pAA292C */
/* 100468F8 */
int32_t BrPhaseLeaveNamed_10046870(void *pEntity);  /* -> pAA2930 */
/* 100469D8 */
int32_t BrPhaseLeaveNamed_10046950(void *pEntity);  /* -> pAA2934 */
/* 10046AB8 */
int32_t BrPhaseLeaveNamed_10046A30(void *pEntity);  /* -> pAA2938 */
/* 10046B98 */
int32_t BrPhaseLeaveNamed_10046B10(void *pEntity);  /* -> pAA293C */
/* 10046C78 */
int32_t BrPhaseLeaveNamed_10046BF0(void *pEntity);  /* -> pAA2914 */
/* 10046F38 */
int32_t BrPhaseLeaveNamed_10046EB0(void *pEntity);  /* -> pAA2934 */

/* 0x10046E10  The odd one out: it clears pAA2924 and nAA28E0 where the other
 * seven clear pAA2928, nAA29C0, nAA29CC and nAA28E4. It still sets
 * n0AB3F4 = -1 and still does both name copies. -> pAA291C. */
/* 10046E8C */
int32_t BrPhaseLeaveNamed_10046E10(void *pEntity);

/* --- LEAVE routines with a different shape -------------------------------- */

/* 0x10046F50 / 0x10046FC0 / 0x10047050  Three bodies of one statement:
 * pAA2904 = <some phase>. They read no argument at all -- the original takes
 * none -- and return 0. */
/* 10046F5A */
int32_t BrPhaseGoto_10046F50(void);   /* pAA2904 = pAA2974 */
/* 10046FCA */
int32_t BrPhaseGoto_10046FC0(void);   /* pAA2904 = pAA292C */
/* 1004705A */
int32_t BrPhaseGoto_10047050(void);   /* pAA2904 = pAA293C */

/* 0x10046F60  Standard prologue, then it clears pAA2904 and pAA2974, and if
 * the phase it saved out of pAA292C first is non-NULL it notifies THAT one
 * too (slot +0x00 with 1) and clears pAA292C. Finally pAA2904 = pAA2908.
 * GOTCHA: pAA2904 is stored as NULL and then immediately overwritten -- the
 * NULL is visible only to the second notify's callee. */
/* 10046FB7 */
int32_t BrPhaseLeave_10046F60(void *pEntity);

/* 0x10046FD0  Tears down pAA2934, pAA2938 and pAA293C through the phase
 * vtable's slot +0x1C (see BrPhaseVtblExt) and NULLs each, then the standard
 * prologue, then pAA2974 = NULL and pAA2904 = pAA2908. */
/* 10047043 */
int32_t BrPhaseLeave_10046FD0(void *pEntity);

/* 0x10047120  BrExt_10045C90(pEntity); then, only if nAA26F0 > 0 and both
 * bAA26F4 and bAA26F5 are zero, wipes three blocks (0x10AA26F6 24 bytes,
 * 0x10AA270E 48 bytes, 0x10AA2740 96 bytes); nAA28C4 = 0; the entity's
 * +0x2AE8 slot +0x1C; then it notifies and clears pAA296C -- NOT pAA2904,
 * which it never touches. */
/* 100471A0 */
int32_t BrPhaseLeave_10047120(void *pEntity);

/* 0x100471B0  BrExt_10045C90(pEntity), the entity's slot +0x1C, then notify
 * and clear pAA2970. Again pAA2904 is untouched. */
/* 100471E3 */
int32_t BrPhaseLeave_100471B0(void *pEntity);

/* 0x10047290  BrExt_1005FBC0(1); tear down pAA2934 and pAA2938 through slot
 * +0x1C; then exactly ONE of three branches --
 *     nAA28B0 != 0 -> BrExt_10043260(pEntity), nAA28B0 = 0
 *     nAA28B4 != 0 -> BrExt_10043330(pEntity), nAA28B4 = 0
 *     otherwise    -> BrExt_10045C90(pEntity)
 * -- then the entity's slot +0x1C, then notify and clear pAA293C. */
/* 1004732D */
int32_t BrPhaseLeave_10047290(void *pEntity);

/* ==========================================================================
 * Small helpers
 * ========================================================================== */

/* 0x100471F0  If BrExt_1003E0E0() is non-zero, run BrPhaseLeave_10047120 and
 * return -1; otherwise return 1 and do nothing. */
int BrPhaseGuard_100471F0(void *pEntity);

/* 0x10047210 / 0x10047250  Identical but for the pair of callees:
 *
 *     if (nAA2AD4)              <a>(pArg);
 *     else if (BrExt_1003E0E0()) <b>(pArg);
 *     else                       return 1;
 *     nAA33E4 = 0;
 *     return -1;
 *
 * GOTCHA: nAA33E4 (the pending key) is cleared on both acting branches but
 * not on the "return 1" branch, and the callee's own return value is thrown
 * away -- the result is the constant -1. */
int BrPhaseEdit_10047210(void *pArg);   /* a = 0x10041A00, b = 0x10041AC0 */
int BrPhaseEdit_10047250(void *pArg);   /* a = 0x10042410, b = 0x100424D0 */

/* 0x10047340  Zero the 32-byte name buffer at 0x10A9D618, nAA28A4 and
 * bAA26F5. Returns 1. NOTE: it clears only ONE of the two name buffers the
 * LEAVE routines write. */
int BrPhaseNameClear_10047340(void);

/* 0x10047360 is BrSub10047360 (slice2_25.h) -- one step of the game object's
 * countdown state machine:
 *
 *   flags = obj[+0x1C]
 *   if (flags & 0x10)                        return   (original: 0)
 *   if (flags & 0x1000000)                   return   (original: 0)
 *   if (obj[+0x3850] & 0x1000000)            return   (original: 0)
 *   if (nAA284C && any of pAA2E80->f2C/f30/f34/f38)
 *        { obj[+0x2B64] = 4; return; }       (original: 1)   <-- flags NOT
 *                                                                written back
 *   if (!(flags & 0x100))                    return   (original: 1)
 *   n = ++(uint16)obj[+0x1E20C]
 *   flags &= ~0x100
 *   switch (n) {                                     // from the jump table
 *      case 2:  obj[+0x2B64] = 0; break;             // at 0x1004745C and the
 *      case 3:  obj[+0x2B64] = 1; break;             // index bytes at
 *      case 4:  obj[+0x2B64] = 2; break;             // 0x10047470
 *      case 52: obj[+0x2B64] = 4; break;
 *      default: (uint16)obj[+0x1E20C] = 2; break;    // 5..51 and everything
 *   }                                                // outside 2..52
 *   obj[+0x1C] = flags
 *
 * GOTCHA: the counter is compared as (int16)n - 2 against 0x32 UNSIGNED, so
 * n = 0 or 1 also takes the default. GOTCHA: the pAA2E80 dereference has no
 * NULL guard in the original and none is added here. */

/* 0x100474B0  BrSub10047360(pObj); return 1. */
int BrPhaseTick_100474B0(BrGameObj *pObj);

/* 0x100475F0  BrPhaseKeyPush_10047610(); BrSub10047360(pObj); return 1. */
int BrPhaseTick_100475F0(BrGameObj *pObj);

/* 0x10047610  If nAA33E4 is non-zero, lowercase it (A-Z only, tested on the
 * LOW BYTE with SIGNED comparisons, so anything >= 0x80 is left alone),
 * sign-extend that byte and push it into the 32-entry ring at 0x10A9E150,
 * advance and wrap nAA2A48, call BrExt_10047660() and clear nAA33E4.
 * GOTCHA: the index wraps AFTER the store, so a full ring overwrites entry 0
 * next. GOTCHA: the value stored is sign-extended, so key 0xE9 lands as
 * -23, not 233. */
void BrPhaseKeyPush_10047610(void);

/* 0x100474D0-0x100475C0  Six "arm a flag and change mode" callbacks. Each
 * sets one global to 1 (or, for 0x10047560, n0AC6A4 to 0x7FFF), calls
 * BrExt_10072AF0(n, 0x00200020) and then sets nAA2854 = n. They leave no
 * return value at all in the original -- eax simply carries whatever
 * BrExt_10072AF0 left there -- so they are declared void. */
void BrPhaseMode_100474D0(void);   /* nAA28F0 = 1;      n = 2 */
void BrPhaseMode_10047500(void);   /* nAA28F8 = 1;      n = 2 */
void BrPhaseMode_10047530(void);   /* nAA28FC = 1;      n = 2 */
void BrPhaseMode_10047560(void);   /* n0AC6A4 = 0x7FFF; n = 3 */
void BrPhaseMode_10047590(void);   /* nAA2A40 = 1;      n = 2 */
void BrPhaseMode_100475C0(void);   /* nAA28F4 = 1;      n = 2 */

#endif /* SLICE3_31_H */
