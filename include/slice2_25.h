/* slice2_25.h -- another module's packet, 0x10042880-0x100446D0 (46 functions).
 *
 * WHAT THIS MODULE IS
 * ===================
 * One coherent subsystem: the **race-setup / lobby screen**. Three shapes
 * repeat over and over, and once you have seen them the whole packet reads
 * mechanically:
 *
 *  1. OPTION CYCLERS.  Twenty-odd functions of the form
 *
 *         if (g_brAA33D4) { if (++v > MAX) v = 0;   }
 *         else if (g_brAA33D0) { if (--v < 0) v = MAX; }
 *         <derived global> = <lookup table>[v];
 *         return 1;
 *
 *     0x10AA33D4 and 0x10AA33D0 are the two edit inputs. They are `a1` and
 *     `a0` of br_state.h's BrActiveFlags -- that header names them only
 *     positionally; this packet establishes their roles: **0x10AA33D4 steps
 *     the value UP, 0x10AA33D0 steps it DOWN**, and 33D4 wins when both are
 *     set. That is the one semantic fact this module contributes to them.
 *
 *  2. SCREEN OBJECT INSTALLERS.  Ten functions that lazily create a 0xC8-byte
 *     object (operator new + the thiscall constructor at 0x10048710), poke a
 *     handler function pointer into +0x04, call it, and publish the object in
 *     0x10AA2904. See BrOptEnsureObj.
 *
 *  3. TEARDOWN / TRANSITION HANDLERS.  Short routines that take the game
 *     object, drive its +0x2AE8 sub-object through two virtual slots, and
 *     move 0x10AA2904 on to the next screen.
 *
 * The lobby half (0x10043810, 0x10043A00, 0x100441A0, 0x10044280) talks to
 * **DirectPlay**. That identification is not a guess:
 *
 *   - the object at 0x10277B40 is called through vtable byte offset +0x7C
 *     with three stdcall arguments; slot 31 of IDirectPlay4 is SetSessionDesc
 *   - the buffer handed to it has dwFlags at +0x04 and the code ORs in 0x20,
 *     which is DPSESSION_JOINDISABLED
 *   - the field compared against 1 is at +0x2C, which is DPSESSIONDESC2's
 *     dwCurrentPlayers ("fewer than two players" -> refuse to start)
 *   - the same buffer is released with GlobalHandle/GlobalUnlock/GlobalFree,
 *     which is exactly how a DirectPlay GetSessionDesc wrapper's output is
 *     disposed of.
 *
 * Per the contract the vtable type is therefore named for the interface:
 * BrDPlayVtbl, never a generic name.
 *
 * NAMING
 * ======
 * Globals keep address-derived names: `g_brXXXXXX` where XXXXXX is the
 * hex RVA (address - 0x10000000), matching the annotations in the .asm.
 * Nothing here is given a semantic name that the disassembly does not
 * support. Functions whose *shape* is unambiguous get a shape name
 * (BrOptCycle..., BrOptOpen...); the rest are BrOpt<addr-low-4>.
 *
 * See the bottom of the .c for the list of DEVIATIONs.
 */
#ifndef SLICE2_25_H
#define SLICE2_25_H

#include <stddef.h>
#include <stdint.h>

#include "br_slots.h"   /* BrSlot, BR_SLOT_COUNT, BR_SLOT_EMPTY */
#include "br_phase.h"   /* BrPhase_, BrPhaseVtbl_ -- CANONICAL, see below */

/* ==========================================================================
 * Types
 * ========================================================================== */

/* --- the 0xC8-byte screen object -----------------------------------------
 *
 * Created by `operator new(0xC8)` (0x1007DFE0 -- NOT zeroed, see the
 * contract) followed by the thiscall constructor at 0x10048710, which sets
 * +0x00. The installers then write +0x04 and call it as `pfn04(pThis)`
 * (cdecl: `push this; call [this+4]; add esp,4`).
 *
 * Slot 0 of the vtable is invoked as `push 1; call [[this]]` with `this`
 * still in ecx and no stack cleanup by the caller -- i.e. thiscall with one
 * argument. That is the MSVC scalar deleting destructor; the argument 1
 * means "also free the storage".
 *
 * ---------------------------------------------------------------------------
 * ADJUDICATED: this is br_phase.h's BrPhase_, and it was a LIVE OVERFLOW.
 *
 * `BrOptObj` was the THIRD partial model of the object at 0x10048710 --
 * slice2_26.h's BrPhase and slice3_33.h's BrUiPhase were the others -- and the
 * only one that had already been wired to the real constructor. It named the
 * same five fields the other two did (pVtbl, pfn04, pfn08, f0C, f68) and
 * padded the gaps to reach 0xC8.
 *
 * The padding is what made it dangerous, because it looks like a size
 * guarantee and is not one. `pad10[0x68-0x10]` and `pad6C[0xC8-0x6C]` are
 * FIXED BYTE COUNTS, so on LP64 the three leading pointers widen, f68 stops
 * being at +0x68, and the struct comes to 216 bytes -- while the object the
 * constructor actually writes (BrPhase_) is 304. slice2_25.c's installer
 * allocated `sizeof(BrOptObj)` and then called `BrOptObjCtor`, which resolves
 * at the host link to slice6_73.c's faithful body. That body's last two stores
 * are +0xC0 and +0xC4, i.e. bytes 288..303 of a 216-byte allocation: an
 * 88-byte heap overflow on every phase installation, plus a scattering of
 * fields landing in the wrong place on the way there.
 *
 * The header's own DEVIATION note said `sizeof(BrOptObj)` was used "because on
 * a 64-bit host the three leading pointers make the object larger", and the
 * transcription notes added "anything that assumes 0xC8 (notably the
 * constructor at 0x10048710) must be ported with the same struct". Both are
 * exactly right, and the second is the condition that was not met.
 *
 * So BrOptObj is an alias now, the allocation uses BR_PHASE_ALLOC_SIZE, and
 * the condition is met by construction rather than by remembering.
 *
 * TYPE CHANGES, both adjudicated in br_phase.h from the disassembly:
 *   - the vtable has NINE slots, not one; this range only calls slot +0x00.
 *   - slot +0x00 returns `void *`. This header read it as the MSVC scalar
 *     deleting destructor, which is right, and 0x10048850 ends `mov eax,esi /
 *     ret 4` -- it returns `this`. Callers here discard it, as before.
 *
 * WRONG IF: BrOptObj's five fields are not the five br_phase.h names them.
 * The installer writes +0x04 then calls it with `this`, and sets +0x0C and
 * +0x68 to 1 -- the same three facts slice2_26.h recorded independently for
 * the same object. All three models agree on all five fields.
 * ------------------------------------------------------------------------- */
typedef BrPhase_     BrOptObj;
typedef BrPhaseVtbl_ BrOptObjVtbl;

/* +0x04 and +0x08 hold plain cdecl function pointers, not vtable entries.
 * br_phase.h types +0x08 as `void (*)(void *)` because slice2_26's call site
 * passes the CALLER's argument rather than the phase; this range only ever
 * stores +0x08, so it has no evidence either way and defers. */
typedef BrPhaseEnterFn_ BrOptObjFn;

/* FIELD RENAMES done at the use sites, not with macros -- a `#define pfn04
 * pfnEnter` would textually rewrite the unrelated `pfn04` members of BrUiCtl_
 * and BrTextBoxVtbl the moment two headers met in one TU. Vtable slot 0 was
 * `pfnDeletingDtor` here and is `f00` in br_phase.h; the five object fields
 * are pVtbl / pfnEnter / pfnHook / f0C / f68. */

/* --- the game object every transition handler is passed ------------------
 *
 * Only two fields are touched anywhere in this packet, so only two are
 * modelled. The padding is written so that BOTH original byte offsets are
 * reproduced exactly on a 32-bit and a 64-bit host.
 */
typedef struct BrGameSub BrGameSub;

typedef struct BrGameSubVtbl {
    void *pfnSlot0;                              /* +0x00 */
    void *pfnSlot1;                              /* +0x04 */
    void *pfnSlot2;                              /* +0x08 */
    void *pfnSlot3;                              /* +0x0C */
    void *pfnSlot4;                              /* +0x10 */
    void *pfnSlot5;                              /* +0x14 */
    void (*pfnSlot6)(BrGameSub *pThis, int arg); /* +0x18 */
    void (*pfnSlot7)(BrGameSub *pThis);          /* +0x1C */
} BrGameSubVtbl;

struct BrGameSub {
    const BrGameSubVtbl *pVtbl;                       /* +0x00 */
    unsigned char        pad04[0x68 - sizeof(void *)];
    int32_t              f68;                         /* +0x68 */
};

typedef struct BrGameObj {
    unsigned char pad0000[0x2AE8];
    BrGameSub    *pSub;                                        /* +0x2AE8 */
    unsigned char pad2AEC[0x2F7C - 0x2AE8 - sizeof(void *)];
    int32_t       f2F7C;                                       /* +0x2F7C */
} BrGameObj;

/* --- DirectPlay ---------------------------------------------------------- */

/* DPSESSIONDESC2, as far as this packet uses it. The two offsets that are
 * actually read are dwFlags (+0x04) and dwCurrentPlayers (+0x2C); the rest
 * of the prefix is spelled out because it is what fixes those offsets. */
typedef struct BrDPSessionDesc {
    uint32_t      dwSize;             /* +0x00 */
    uint32_t      dwFlags;            /* +0x04 -- 0x20 = DPSESSION_JOINDISABLED */
    unsigned char aGuids[0x20];       /* +0x08  guidInstance + guidApplication */
    uint32_t      dwMaxPlayers;       /* +0x28 */
    uint32_t      dwCurrentPlayers;   /* +0x2C */
} BrDPSessionDesc;

typedef struct BrDPlay BrDPlay;

typedef struct BrDPlayVtbl {
    /* Slots 0..30 are never called from this packet and are deliberately
     * left untyped rather than given plausible-looking signatures. */
    void *aSlots[31];                                        /* +0x00..+0x78 */
    /* slot 31, +0x7C: IDirectPlay4::SetSessionDesc(lpDesc, dwFlags) */
    long (*pfnSetSessionDesc)(BrDPlay *pThis,
                              BrDPSessionDesc *pDesc, uint32_t dwFlags);
} BrDPlayVtbl;

struct BrDPlay {
    const BrDPlayVtbl *pVtbl;   /* +0x00 */
};

/* --- assorted small objects reached through globals ---------------------- */

/* 0x10A9D008. Only +0x08 is read (compared against 0x100AB3E0, and matched
 * against the slot ids at 0x10AA2538), so only +0x08 is modelled. */
typedef struct BrOptUi {
    int32_t f00, f04;
    int32_t f08;
} BrOptUi;

/* 0x10AA29D8. Only +0x1C, and only ever `&= ~0x10`. */
typedef struct BrOptFlagObj {
    int32_t pad00[7];
    int32_t f1C;        /* +0x1C */
} BrOptFlagObj;

/* 0x10AA29D4. A single 16-bit field a very long way in. */
typedef struct BrObj29D4 {
    unsigned char pad[0x1E164];
    uint16_t      f1E164;     /* +0x1E164 */
} BrObj29D4;

/* Element type of the pointer table at 0x100BD2A8. Only bit 0x10 of the byte
 * at +0x04 is tested (it makes 0x10042EE0 append one extra string). */
typedef struct BrRec2A8 {
    int32_t       f00;
    unsigned char f04;      /* +0x04, bit 0x10 tested */
    unsigned char f05, f06, f07;
} BrRec2A8;

/* ==========================================================================
 * Constants
 * ========================================================================== */

/* Cycler bounds, all taken straight from the compare immediates. */
#define BR_OPT_TRACK_MAX     0x1F   /* 0x100AC654, 0x10042B30 */
#define BR_OPT_AC65C_MAX     7      /* 0x100AC65C, 0x10042C80 */
#define BR_OPT_B4E708_MAX    9      /* 0x10B4E708, 0x10042CF0 */
#define BR_OPT_B4E70C_MAX    9      /* 0x10B4E70C, 0x10042D60 */
#define BR_OPT_AC64C_MAX     2      /* 0x100AC64C, 0x10042DC0 */
#define BR_OPT_AC650_MAX     2      /* 0x100AC650, 0x10042E20 */
#define BR_OPT_AA2A08_MAX    1      /* 0x10AA2A08, 0x10042E80 */
#define BR_OPT_AA2A00_MAX    4      /* 0x10AA2A00, 0x10043180 */
#define BR_OPT_AA2A18_MAX    4      /* 0x10AA2A18, 0x10044600 */
#define BR_OPT_AA2A0C_MAX    3      /* 0x10AA2A0C, 0x10043400 -- 1 is SKIPPED */

/* 0x100BD3E0 is the odd one out: it is 1-BASED. 0x100430B0 wraps 13 -> 1 and
 * 0 -> 12, so 0 is not a legal value of this option. */
#define BR_OPT_BD3E0_MIN     1
#define BR_OPT_BD3E0_MAX     0xC

/* 0x100AC648's upper bound is computed at every step, not constant: 14 when
 * 0x10AA28FC is set, 11 otherwise (`neg/sbb/and 3/add 0xB`). */
#define BR_OPT_AC648_MAX_BASE   0x0B
#define BR_OPT_AC648_MAX_EXTRA  0x0E

/* 0x10AA26F0 receives `rep movsd` of ecx = 0x53 dwords. 0x10AA26F0 + 0x53*4
 * = 0x10AA283C, which lands just below the next referenced global
 * (0x10AA2854) -- that is what fixes the count. */
#define BR_OPT_AA26F0_COUNT  0x53

/* Text buffers. DEVIATION: the original's sizes are not recoverable from the
 * code (nothing bounds any of these writes); 0x104 is used because it is the
 * path-buffer size this game uses elsewhere (see slice1_06.h). */
#define BR_OPT_TEXT_MAX      0x104

/* Frame size of 0x10042880. The two string buffers live INSIDE it at fixed
 * offsets and the port keeps them there -- see the .c, they overlap. */
#define BR_OPT_2880_FRAME    0x108
#define BR_OPT_2880_NUM_OFF  0x10
#define BR_OPT_2880_STR_OFF  0x14

/* Object array at 0x10B4DF30: four records 0xA8 apart (0x10B4DF30, DFD8,
 * E080, E128), which 0x10043400 selects between. */
#define BR_OPT_B4DF30_COUNT   4
#define BR_OPT_B4DF30_STRIDE  0xA8

/* String-table ids used as printf formats / literals. */
#define BR_OPT_STR_LOCKED    0xB0   /* appended when BrRec2A8.f04 & 0x10 */
#define BR_OPT_STR_TRACK     0xB7   /* 0x10042B30 */
#define BR_OPT_STR_CAR       0xB8   /* 0x10042EE0 */
#define BR_OPT_STR_BD3E0     0xB9   /* 0x100430B0 */
#define BR_OPT_STR_AA2A00    0xBA   /* 0x10043180 */
#define BR_OPT_STR_TOOFEW    0xBB   /* 0x10043A00, dwCurrentPlayers <= 1 */
#define BR_OPT_STR_NOTREADY  0xBC   /* 0x10043A00, a slot is not ready */
#define BR_OPT_STR_AA2A18    0xBD   /* 0x10044600 */

/* ==========================================================================
 * Globals owned by this packet
 *
 * Every one is listed with its original address so integration can
 * de-duplicate against other slices mechanically. Several of these are
 * certainly shared (0x10AA2904, 0x10277B40, 0x10A9D008, 0x10AA33D0/D4,
 * 0x1039B720 ...) -- see the report.
 * ========================================================================== */

/* --- edit inputs --------------------------------------------------------- */
extern int32_t g_brAA33D4;      /* 0x10AA33D4  step the option UP   */
extern int32_t g_brAA33D0;      /* 0x10AA33D0  step the option DOWN */

/* --- option indices ------------------------------------------------------ */
extern int32_t g_br0AC648;      /* 0x100AC648 */
extern int32_t g_br0AC64C;      /* 0x100AC64C */
extern int32_t g_br0AC650;      /* 0x100AC650 */
extern int32_t g_br0AC654;      /* 0x100AC654 */
extern int32_t g_br0AC658;      /* 0x100AC658 */
extern int32_t g_br0AC65C;      /* 0x100AC65C */
extern int32_t g_br0BD3E0;      /* 0x100BD3E0 */
extern int32_t g_brAA2A00;      /* 0x10AA2A00 */
extern int32_t g_brAA2A08;      /* 0x10AA2A08 */
extern int32_t g_brAA2A0C;      /* 0x10AA2A0C */
extern int32_t g_brAA2A18;      /* 0x10AA2A18 */
extern int32_t g_brAA2A1C;      /* 0x10AA2A1C */
extern int32_t g_brAA2A20;      /* 0x10AA2A20 */
extern int32_t g_brAA2A24;      /* 0x10AA2A24 */
extern int32_t g_brAA2A28;      /* 0x10AA2A28 */
extern int32_t g_brB4E708;      /* 0x10B4E708 */
extern int32_t g_brB4E70C;      /* 0x10B4E70C */

/* --- values derived from those indices ----------------------------------- */
extern int32_t g_br094350;      /* 0x10094350 */
extern int32_t g_br094354;      /* 0x10094354 */
extern int32_t g_br094358;      /* 0x10094358 */
extern int32_t g_br09435C;      /* 0x1009435C */
extern int32_t g_br0B380C;      /* 0x100B380C */
extern int32_t g_br22B34C;      /* 0x1022B34C */
extern int32_t g_br22B350;      /* 0x1022B350 */
extern int32_t g_brB4E1D0;      /* 0x10B4E1D0 */
extern int32_t g_brB4E1D8;      /* 0x10B4E1D8 */
extern int32_t g_brB4E1DC;      /* 0x10B4E1DC */
extern int32_t g_brB4E1E0;      /* 0x10B4E1E0 */
extern int32_t g_brB4E728;      /* 0x10B4E728 */
extern int32_t g_brB4E7A0;      /* 0x10B4E7A0 */
extern void   *g_brB4E1D4;      /* 0x10B4E1D4 -> &g_aBrB4DF30[k] */

/* --- mode / session state ------------------------------------------------ */
extern int32_t g_br0AA010;      /* 0x100AA010 */
extern int32_t g_br0AB3D8;      /* 0x100AB3D8 */
extern int32_t g_br0AB3E0;      /* 0x100AB3E0 */
extern int32_t g_br0B4050;      /* 0x100B4050 */
extern int32_t g_br22AF18;      /* 0x1022AF18 */
extern int32_t g_br690A18;      /* 0x10690A18 */
extern int32_t g_brA9CFFC;      /* 0x10A9CFFC */
extern int32_t g_brA9D000;      /* 0x10A9D000 */
extern int32_t g_brAA2854;      /* 0x10AA2854 */
extern int32_t g_brAA285C;      /* 0x10AA285C  (br_state.h's `override`) */
extern int32_t g_brAA2878;      /* 0x10AA2878 */
extern int32_t g_brAA287C;      /* 0x10AA287C  0..3, a mode selector */
extern int32_t g_brAA2884;      /* 0x10AA2884  non-zero => networked session */
extern int32_t g_brAA2888;      /* 0x10AA2888 */
extern int32_t g_brAA288C;      /* 0x10AA288C */
extern int32_t g_brAA2890;      /* 0x10AA2890 */
extern int32_t g_brAA2894;      /* 0x10AA2894 */
extern int32_t g_brAA2898;      /* 0x10AA2898 */
extern int32_t g_brAA289C;      /* 0x10AA289C */
extern int32_t g_brAA28D8;      /* 0x10AA28D8  one-shot latch, see BrOptToggle2F7C_A */
extern int32_t g_brAA28E8;      /* 0x10AA28E8 */
extern int32_t g_brAA28FC;      /* 0x10AA28FC  selects 0x100AC648's upper bound */
extern int32_t g_brAA2958;      /* 0x10AA2958 */
extern int32_t g_brAA29A8;      /* 0x10AA29A8 */

/* --- saved settings restored by 0x10042880 ------------------------------- */
extern int32_t     g_brAD0978;  /* 0x10AD0978 */
extern int32_t     g_brAD097C;  /* 0x10AD097C */
extern int32_t     g_brAD0980;  /* 0x10AD0980 */
extern int32_t     g_brAD0984;  /* 0x10AD0984 */
extern int32_t     g_brAD0988;  /* 0x10AD0988 */
extern int32_t     g_brAD098C;  /* 0x10AD098C */
extern signed char g_br680738;  /* 0x10680738 -- read with movsx, SIGNED */
extern signed char g_br68073F;  /* 0x1068073F -- read with movsx, SIGNED */

/* --- objects ------------------------------------------------------------- */
extern BrDPlay      *g_brP277B40;   /* 0x10277B40  IDirectPlay4, may be NULL */
extern BrOptUi      *g_brPA9D008;   /* 0x10A9D008 */
extern void         *g_brP680584;   /* 0x10680584 */
extern const int32_t *g_brPACED34;  /* 0x10ACED34  source of the 0x53-dword copy */
extern BrOptFlagObj *g_brPAA29D8;   /* 0x10AA29D8 */
extern BrObj29D4    *g_brPAA29D4;   /* 0x10AA29D4 */

/* 0x10AA2904, "current screen", is NOT storage of this module's.  It is the
 * same dword br_uinav.h calls BrUiNav::pAA2904 and slice2_26.h calls
 * BrPhaseCtx::pAA2904, and modelling it separately here is why an option
 * screen could publish itself as current and the frame loop never see it.
 * `BrOptObj` is `BrPhase_` (above), so this is a rename, not a cast.
 * The macro keeps every existing `g_brPAA2904` read, write and address-of
 * working unchanged -- see br_phasecur.h. */
#include "br_phasecur.h"        /* BR_PHASE_CUR -- the ONE 0x10AA2904 slot */
#define g_brPAA2904   BR_PHASE_CUR
extern BrOptObj *g_brPAA2908;   /* 0x10AA2908 */
extern BrOptObj *g_brPAA2940;   /* 0x10AA2940 */
extern BrOptObj *g_brPAA2948;   /* 0x10AA2948 */
extern BrOptObj *g_brPAA294C;   /* 0x10AA294C */
extern BrOptObj *g_brPAA2950;   /* 0x10AA2950 */
extern BrOptObj *g_brPAA2954;   /* 0x10AA2954 */
extern BrOptObj *g_brPAA296C;   /* 0x10AA296C */
extern BrOptObj *g_brPAA2970;   /* 0x10AA2970 */
extern BrOptObj *g_brPAA298C;   /* 0x10AA298C */
extern BrOptObj *g_brPAA2998;   /* 0x10AA2998 */
extern BrOptObj *g_brPAA29B8;   /* 0x10AA29B8 */

/* --- buffers ------------------------------------------------------------- */
extern int32_t g_aBrAA26F0[BR_OPT_AA26F0_COUNT];   /* 0x10AA26F0 */
extern char    g_aBrA9CDF0[BR_OPT_TEXT_MAX];       /* 0x10A9CDF0, strlen'd */
extern char    g_aBrA9DD28[BR_OPT_TEXT_MAX];       /* 0x10A9DD28, message text */
extern char    g_aBr39B720[BR_OPT_TEXT_MAX];       /* 0x1039B720, see note */
extern char    g_aBr1782BC8[BR_OPT_TEXT_MAX];      /* 0x11782BC8, see note */

/* 0x10AA2538: the eight 12-byte slots br_slots.h describes. NOTE the layout
 * warning in the .c -- br_slots.h's BrSlotTable is NOT the real memory
 * layout, because 0x10AA288C is 0x2F4 bytes past the end of this array. */
extern BrSlot g_aBrAA2538[BR_SLOT_COUNT];

/* 0x10B4DF30: four 0xA8-byte records; 0x10043400 points 0x10B4E1D4 at one. */
extern unsigned char g_aBrB4DF30[BR_OPT_B4DF30_COUNT][BR_OPT_B4DF30_STRIDE];
/* 0x10B4FBE8: passed by ADDRESS to 0x1006A4A0. */
extern unsigned char g_aBrB4FBE8[];

/* ==========================================================================
 * Lookup tables (read-only data in the DLL; integration supplies them)
 *
 * Sizes are declared incomplete on purpose. The bounds below are derived
 * from the highest index the code can produce and from the next table's
 * address, and are recorded so nobody has to re-derive them:
 *
 *   0x100AC308  <= 24 entries (next referenced table is at 0x100AC368);
 *               indexed by the VALUE of 0x100B380C, not by the option index
 *   0x100AC368  16 entries (0x100AC368..0x100AC3A8); index = track & 0xF
 *   0x100AC3B0  6 entries  (0x100AC3B0..0x100AC3C8); indexed by 0x1022B350
 *   0x100AC3C8  >= 5 used  (indices 0..4)
 *   0x100AC420  32 entries (indices 0..0x1F)
 *   0x100AC4A0  4 entries  (0x100AC4A0..0x100AC4B0); indices 0..2 used
 *   0x100AC4B0  4 entries  (0x100AC4B0..0x100AC4C0); indices 0..2 used
 *   0x100AC4C0  6 entries  (0x100AC4C0..0x100AC4D8); indices 0..4 used
 *   0x100AC4D8  16 entries (0x100AC4D8..0x100AC518); indices 0..0xE used
 *   0x100AC518  2 entries  (0x100AC518..0x100AC520)
 *   0x100AC520  4 entries  (0x100AC520..0x100AC530)
 *   0x100AC530  2 entries  (0x100AC530..0x100AC538)
 *   0x100AC538  2 entries  (0x100AC538..0x100AC540)
 *   0x100AC540  2 entries  (0x100AC540..0x100AC548)
 *   0x100AC548  2 entries  (indices 0..1 used)
 * ========================================================================== */
extern const int32_t g_aBrAC308[];   /* 0x100AC308  -> string ids */
extern const int32_t g_aBrAC368[];   /* 0x100AC368  -> string ids */
extern const int32_t g_aBrAC3B0[];   /* 0x100AC3B0  -> string ids */
extern const int32_t g_aBrAC3C8[];   /* 0x100AC3C8  -> string ids */
extern const int32_t g_aBrAC420[];   /* 0x100AC420 */
extern const int32_t g_aBrAC4A0[];   /* 0x100AC4A0 */
extern const int32_t g_aBrAC4B0[];   /* 0x100AC4B0 */
extern const int32_t g_aBrAC4C0[];   /* 0x100AC4C0 */
extern const int32_t g_aBrAC4D8[];   /* 0x100AC4D8 */
extern const int32_t g_aBrAC518[];   /* 0x100AC518 */
extern const int32_t g_aBrAC520[];   /* 0x100AC520 */
extern const int32_t g_aBrAC530[];   /* 0x100AC530 */
extern const int32_t g_aBrAC538[];   /* 0x100AC538 */
extern const int32_t g_aBrAC540[];   /* 0x100AC540 */
extern const int32_t g_aBrAC548[];   /* 0x100AC548 */

/* 0x100BD2A8: array of pointers to BrRec2A8, indexed by 0x100B380C. */
extern const BrRec2A8 *const g_aBrBD2A8[];

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* XSLICE 0x1008C000 */
extern char *BrItoa(int value, char *pszBuf, int radix);
/* XSLICE 0x1007C830 -- variadic in the original; every call site in this
 * packet passes exactly one extra pointer argument. */
extern int BrSprintf(char *pszDest, const char *pszFmt, ...);
/* XSLICE 0x10074030 -- string table lookup by id. */
extern const char *BrStrGet(int id);

/* XSLICE 0x10038F30 */ extern void BrSub10038F30(int a);
/* XSLICE 0x1003BF60 */ extern void BrSub1003BF60(void);
/* XSLICE 0x1003C020 */ extern void BrSub1003C020(void);
/* XSLICE 0x1003C150 */ extern void BrSub1003C150(void);
/* XSLICE 0x1003C1E0 */ extern void BrSub1003C1E0(void);
/* XSLICE 0x1003C230 */ extern void BrSub1003C230(void);
/* XSLICE 0x1003C260 */ extern int  BrSub1003C260(void);
/* XSLICE 0x1003CDA0 */ extern void BrSub1003CDA0(void);
/* XSLICE 0x1003CE80 */ extern void BrSub1003CE80(void);
/* XSLICE 0x1003D0B0 -- fills *ppDesc with a Global-allocated session desc. */
extern void BrSub1003D0B0(BrDPlay *pDPlay, BrDPSessionDesc **ppDesc);
/* XSLICE 0x1003D210 */ extern void BrSub1003D210(void *a, BrOptUi *b, int c);
/* XSLICE 0x1003D950 */ extern int32_t BrSub1003D950(BrOptUi *pUi, int a);
/* XSLICE 0x1003D9F0 */ extern int32_t BrSub1003D9F0(BrOptUi *pUi);
/* XSLICE 0x1003DA40 */ extern void BrSub1003DA40(BrOptUi *pUi, int a);
/* XSLICE 0x1003E310 */ extern void BrSub1003E310(void);
/* XSLICE 0x1003E680 */ extern void BrSub1003E680(void);
/* XSLICE 0x1003F2B0 -- non-zero => index 0x100AC648 is selectable. */
extern int  BrSub1003F2B0(int index);
/* XSLICE 0x1003F320 -- non-zero => index 0x100AC654 is selectable. */
extern int  BrSub1003F320(int index);
/* XSLICE 0x10041B50 */ extern void BrSub10041B50(void);
/* XSLICE 0x10043BF0 */ extern void BrSub10043BF0(BrGameObj *p);
/* XSLICE 0x10044540 */ extern void BrSub10044540(void);
/* XSLICE 0x10046400 */ extern int32_t BrSub10046400(BrGameObj *p);  /* 0 @ 0x10046446 */
/* XSLICE 0x10047360 */ extern void BrSub10047360(BrGameObj *p);
/* XSLICE 0x10048710 -- thiscall constructor, returns pThis. */
extern BrOptObj *BrOptObjCtor(BrOptObj *pThis);
/* XSLICE 0x10058700 */ extern int  BrSub10058700(void);
/* XSLICE 0x100586A0 -- resets the eight slots at 0x10AA2538. This is
 * br_slots.h's BrSlotsReset; the original takes no argument because the
 * table is a global. See the layout warning in the .c. */
extern void BrSub100586A0(void);
/* XSLICE 0x1005FCF0 */ extern void BrSub1005FCF0(void);
/* XSLICE 0x10060D90 */ extern void BrSub10060D90(void);
/* XSLICE 0x1006A4A0 -- thiscall(pThis, pArg). */
extern void BrSub1006A4A0(void *pThis, void *pArg);
/* XSLICE 0x10071130 */ extern void BrSub10071130(int a, int b);
/* XSLICE 0x10072AF0 */ extern void BrSub10072AF0(int a, int b);

/* Handler functions installed into BrOptObj::pfn04. */
/* XSLICE 0x1004CAC0 */ extern void BrOptFn1004CAC0(BrOptObj *pThis);
/* XSLICE 0x10051990 */ extern void BrOptFn10051990(BrOptObj *pThis);
/* XSLICE 0x10051D30 */ extern void BrOptFn10051D30(BrOptObj *pThis);
/* XSLICE 0x100558A0 */ extern void BrOptFn100558A0(BrOptObj *pThis);
/* XSLICE 0x10056A10 */ extern void BrOptFn10056A10(BrOptObj *pThis);
/* XSLICE 0x10056FF0 */ extern void BrOptFn10056FF0(BrOptObj *pThis);
/* XSLICE 0x100575F0 */ extern void BrOptFn100575F0(BrOptObj *pThis);
/* XSLICE 0x10057C10 */ extern void BrOptFn10057C10(BrOptObj *pThis);
/* XSLICE 0x10058750 */ extern void BrOptFn10058750(BrOptObj *pThis);
/* Handler functions installed into BrOptObj::pfn08 (never called here). */
/* ADJUDICATED: these two take an ENTITY RECORD, not the screen object.
 *
 * They were declared `void (BrOptObj *pThis)` here, on the reasonable-looking
 * grounds that this range only ever STORES them -- into the +0x08 slot of a
 * BrOptObj -- and never calls them. Merging BrOptObj into BrPhase_ made the
 * conflict a compile error instead of a silent one, because br_phase.h types
 * +0x08 `void (*)(void *pEntity)`.
 *
 * br_phase.h is right, and the disassembly is not close:
 *
 *   0x100450F0, the dispatcher, does
 *       mov eax,[esp+4]              ; the CALLER's own argument
 *       mov ecx,[0x10AA29F4]         ; the phase
 *       push eax / call [ecx+8]      ; +0x08 receives eax, NOT ecx
 *   so the argument is whatever the caller was handed, never the phase.
 *
 *   0x10044970 itself then does
 *       mov esi,[esp+8]              ; that argument
 *       mov ecx,[esi+0x2AE8]         ; ... and reads +0x2AE8 out of it
 *       call [[ecx]+0x18] / call [[ecx]+0x1C]
 *   +0x2AE8 is slice2_26.h's BR_ENTITY_OFF_SUB, to the byte. A 0xC8-byte
 *   screen object has no +0x2AE8; reading one would be a wild read 10KB past
 *   its end.
 *
 * This is br_ui.h ADJ-8's rule again -- between a header that only watched a
 * STORE and one that watched the CALL, the call site decides.
 *
 * WRONG IF: +0x2AE8 is a field of the screen object after all. The object is
 * 0xC8 bytes, established by its own `operator new` literal and by its
 * constructor's last store landing on +0xC4. */
/* XSLICE 0x10044970 */ extern int32_t BrOptFn10044970(void *pEntity);
/* XSLICE 0x10044A30 */ extern int32_t BrOptFn10044A30(void *pEntity);

/* KERNEL32 imports, used verbatim by 0x10043810 and 0x10043A00 to dispose of
 * the session descriptor DirectPlay handed back. Supplied by the platform
 * layer; on a non-Windows host GlobalHandle can be the identity, GlobalUnlock
 * a no-op and GlobalFree free(). */
extern void *BrGlobalHandle(void *pMem);
extern int   BrGlobalUnlock(void *hMem);
extern void *BrGlobalFree(void *hMem);

/* ==========================================================================
 * The packet, in address order
 * ========================================================================== */

/* 0x10042880  Restore the saved race settings and build the time-attack ghost
 * filename "TimeAttack<n>.grf" into 0x11782BC8. `pUnused` is the first
 * argument and is genuinely never read; `pIndex` is the SECOND argument and
 * is dereferenced for the number. Returns 0 when (signed char)0x10680738 is
 * negative, 1 otherwise. */
int BrOptBeginTimeAttack(void *pUnused, const int32_t *pIndex);

/* 0x10042A90, 0x10042AC0, 0x10042B00. Three byte-identical copies. Each
 * toggles pGame->f2F7C, but only the FIRST call of the three ever does
 * anything: they share the latch at 0x10AA28D8 and nothing in this packet
 * clears it. Always return 1. */
int BrOptToggle2F7C_A(BrGameObj *pGame);
int BrOptToggle2F7C_B(BrGameObj *pGame);
int BrOptToggle2F7C_C(BrGameObj *pGame);

/* 0x10042B30  Track select: step 0x100AC654 over 0..0x1F, skipping indices
 * BrSub1003F320 rejects, and give up after a full circle. */
int BrOptCycleTrack(void);

/* 0x10042C80 */ int BrOptCycleAC65C(void);
/* 0x10042CF0 */ int BrOptCycleB4E708(void);
/* 0x10042D60 */ int BrOptCycleB4E70C(void);
/* 0x10042DC0 */ int BrOptCycleAC64C(void);
/* 0x10042E20 */ int BrOptCycleAC650(void);
/* 0x10042E80 */ int BrOptCycleAA2A08(void);

/* 0x10042EE0  Vehicle select: step 0x100AC648 over 0..(11 or 14), skipping
 * indices BrSub1003F2B0 rejects. */
int BrOptCycleCar(void);

/* 0x100430B0  1-based cycler over 0x100BD3E0, range 1..12. */
int BrOptCycleBD3E0(void);

/* 0x10043180 */ int BrOptCycleAA2A00(void);

/* 0x10043260 */ int BrOptOpen296C(BrGameObj *pUnused);
/* 0x10043330 */ int BrOptOpen2970(BrGameObj *pUnused);

/* 0x10043400  Cycler over 0x10AA2A0C that SKIPS the value 1 in both
 * directions, then selects one of the four 0x10B4DF30 records. */
int BrOptCycleAA2A0C(void);

/* 0x100434C0 */ int BrOptOpen2998(BrGameObj *pUnused);
/* 0x10043590 */ int BrOptCycleAA2A1C(void);
/* 0x100435F0 */ int BrOptCycleAA2A28(void);
/* 0x10043650 */ int BrOptCycleAA2A20(void);
/* 0x100436B0 */ int BrOptCycleAA2A24(void);

/* 0x10043760  Leave to the front end. Returns 0. */
int BrOpt3760(BrGameObj *pGame);
/* 0x100437B0  Push 0x100AB3E0 into the UI object if it differs. Returns 1. */
int BrOpt37B0(void);
/* 0x100437D0  Returns 1. */
int BrOpt37D0(BrGameObj *pGame);
/* 0x10043810  Lobby tick / leave. Returns 0 on the paths that transition, 1
 * on the path that stays put. */
int BrOpt3810(BrGameObj *pGame);
/* 0x10043A00  "Start the race" from the lobby. Always returns 1. */
int BrOpt3A00(void);

/* 0x10043CD0 */ int BrOptOpen2940(BrGameObj *pUnused);
/* 0x10043DA0 */ int BrOptOpen298C(BrGameObj *pUnused);
/* 0x10043E70 */ int BrOptOpen2948(BrGameObj *pUnused);

/* 0x10043F50 */ int BrOpt3F50(BrGameObj *pGame);
/* 0x10043FA0 */ int BrOpt3FA0(BrGameObj *pGame);
/* 0x10043FC0 */ int BrOpt3FC0(BrGameObj *pGame);

/* 0x10044010, 0x10044030, 0x10044050, 0x10044070, 0x10044090, 0x100440B0.
 * Six one-liners: set 0x10AA287C then enter one of two screens. */
int BrOpt4010(BrGameObj *pGame);
int BrOpt4030(BrGameObj *pGame);
int BrOpt4050(BrGameObj *pGame);
int BrOpt4070(BrGameObj *pGame);
int BrOpt4090(BrGameObj *pGame);
int BrOpt40B0(BrGameObj *pGame);

/* 0x100440D0 */ int BrOptOpen294C(BrGameObj *pUnused);
/* 0x100441A0  Tear the lobby down and go back. Returns nothing -- see the
 * DEVIATION note in the .c. */
void BrOpt41A0(void);
/* 0x10044280  Open 0x10AA2950 in single-player form (0x10AA2884 = 0). */
int BrOptOpen2950A(BrGameObj *pUnused);
/* 0x100443E0  Open 0x10AA2950 in networked form (0x10AA2884 = 1). */
int BrOptOpen2950B(BrGameObj *pUnused);
/* 0x100444C0 */ int BrOpt44C0(BrGameObj *pGame);
/* 0x10044600 */ int BrOptCycleAA2A18(void);
/* 0x100446D0 */ int BrOptOpen2954(BrGameObj *pUnused);

#endif /* SLICE2_25_H */
