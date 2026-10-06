/* br_texinit.c -- see br_texinit.h. D3D 0x1002A640 / Glide 0x10029B50.
 *
 * BrTexInit's residue is ONE colouring choice and nothing else: at 0x89 the
 * original spells the shared zero in esi and the first span call's result in
 * edi (`xor esi,esi; push esi; call; push esi; mov edi,eax`), we take the
 * same two webs the other way round.  Sizes, instruction counts and the
 * register-blind multiset are all identical (285/285 B, 55/55 insns, 0+0).
 * `corpus.py find --from 0x10029B50 --at 0x89 --len 12` is a MISS -- no run
 * of 3+ of these instructions is proven anywhere in the solved tree, so
 * there is no spelling to copy.
 * @t4-pass 0x10029B50 1 2026-09-10 probes 14 bytes 285 insns 55 regions 1 rows 0 census yes
 * @t4-pass 0x10029B50 2 2026-09-10 probes 11 bytes 285 insns 55 regions 1 rows 0 census no
 * Pass 1 permuted the zero/result webs at the call pair (literal vs named
 * zero, hi/lo temps in both orders, const and unsigned zero, one-expression
 * and split forms); pass 2 permuted the lifetimes around them (the TMU-count
 * block, the free() block's store order, span's type and the tail's store
 * order).  All 25 compiles left the esi/edi assignment exactly where it was.
 */

/* Header prototypes take a host / a texmem argument.  Both originals read
 * and write absolute globals and take no argument. */
#define BrTexInit        BrTexInit_port
#define BrTexChooseLevel BrTexChooseLevel_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_16.h"   /* br_globals: its objects */
#include "br_texinit.h"
#undef BrTexInit
#undef BrTexChooseLevel

#include <stddef.h>
#include <stdlib.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x105CCBD0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x1186C960 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x10226E78 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* 0x1186C95C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x100B8498 */

/* The thirteen stores of 0x10029B52..0x10029BD4, in the original's order.
 * 0x118ED19C is eleventh in the LISTING while being lowest in memory; the
 * order is the original's and is kept. */
static const uint32_t s_aSlot[BR_TEXINIT_NSLOTS] = {
    0x118ED1BC, 0x118ED1C0, 0x118ED1C4, 0x118ED1C8, 0x118ED1CC,
    0x118ED1D0, 0x118ED1D4, 0x118ED1D8, 0x118ED1DC, 0x118ED1E0,
    0x118ED19C, 0x118ED1E4, 0x118ED1E8
};
static const uint32_t s_aValue[BR_TEXINIT_NSLOTS] = {
    0x10023D20, 0x10024E60, 0x100272F0, 0x10027F00, 0x100284E0,
    0x100285E0, 0x10028620, 0x100287E0, 0x10028820, 0x100297F0,
    0x100298C0, 0x100299A0, 0x10029CD0
};

static uint32_t s_aInstalled[BR_TEXINIT_NSLOTS];
static int      s_cInstalled;

/* The tail's effects, recorded so they can be asserted. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
static uint32_t s_aZeroed[5];
static int      s_cZeroed, s_cFree, s_aTail[3];

/* 0x10029C33..0x10029C5A, in the original's order. NOT sorted: 0x10697A58 and
 * 0x10697A5C come first, then 0x10697A50 and 0x10697A48 -- descending, with
 * 0x106B7A7C last. */
static const uint32_t s_aZeroTarget[5] = {
    0x10697A58, 0x10697A5C, 0x10697A50, 0x10697A48, 0x106B7A7C
};

/* (port-only BrTexInitSlotAddr removed) */

/* (port-only BrTexInitSlotValue removed) */

/* (port-only BrTexInitMemory removed) */

/* (port-only BrTexInitLevel removed) */

/* (port-only BrTexInitInstalledCount removed) */

/* (port-only BrTexInitInstalledAt removed) */


/* (port-only BrTexInitGlobal5E1820 removed) */

/* (port-only BrTexInitGlobal5E1808 removed) */

/* (port-only BrTexInitZeroedCount removed) */

/* (port-only BrTexInitZeroedAt removed) */

/* (port-only BrTexInitFreeCalls removed) */

/* (port-only BrTexInitTailCalls removed) */


/* (port-only BrTexInitResetForTest removed) */


/* ------------------------------------------------------------------ *
 * 0x10029B10 -- texture memory to detail level.
 *
 *   10029B1B  cmp eax, ecx          texmem vs [0x1186C960]
 *   10029B1D  jbe 0x10029B45        <=  -> level 2      (UNSIGNED)
 *   10029B1F  cmp [0x10226E78], 0x2000000
 *   10029B29  jbe 0x10029B3A        <=  -> level 1
 *   10029B2B  cmp eax, 0x3D0900
 *   10029B30  sbb eax, eax          -1 when texmem <  0x3D0900
 *   10029B32  neg eax                1 when texmem <  0x3D0900, else 0
 *
 * `sbb/neg` is easy to read backwards: the borrow is SET when the value is
 * LESS, so a SMALL card gets level 1 and a large one gets level 0. Higher
 * level means LESS detail.
 *
 * 0x3D0900 is 4,000,000 -- four million bytes, decimal. A reader who assumes
 * 4 MiB (0x400000) gets a threshold 194,304 bytes low, and no test on typical
 * hardware would notice.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: decides how much texture detail the game will use, from how
 * much texture memory the video card has and how much memory the machine
 * has. A small card, or a machine with 32MB or less, gets less detail. The
 * threshold is four million bytes, decimal -- not four megabytes, which is a
 * different number. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x11829844  available texels */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x11829848  low-memory threshold */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x10575420  nonzero forces level 2 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x100B8C90  chosen level */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x100A7DFC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x100A7E00 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x100A7E04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x100A7E08 */

/* WHAT IT DOES: decide how much texture detail to use from how much
 * texture memory the card has (and how much RAM the machine has). */
/* @implements 0x10029B10 glide BrTexChooseLevel */
/* @implements 0x1002A5A0 d3d BrTexChooseLevel */
void BrTexChooseLevel(void)
{
    /* Orig fall-through is sbb/neg: `jbe` to level 2, `jbe` to level 1.
     * `if (a <= b) x=2; else if ...` inverts to `ja`. */
    if (s_texmem > g_brTexLowThreshold) {
        if (g_brTexSysMem > 0x2000000u) {
            s_level = (s_texmem < 0x3D0900u) ? 1 : 0;
            return;
        }
        s_level = 1;
        return;
    }
    s_level = 2;
}

/* ------------------------------------------------------------------ *
 * 0x1002A640 -- install the thirteen hooks, then measure the card.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: sets the texture system up: installs the thirteen routines
 * the rest of the engine calls to work with textures, then measures how much
 * texture memory the card actually has and picks the detail level to match.
 * The matching body is the D3D original (absolute slots, two free()s, a
 * GetAvailableTextureMem query). The port records the same install order
 * and measures through a host hook so it does not need those addresses. */
/* The thirteen slot stores, in the original's order. 0x118AA084 is eleventh
 * in the listing while being lowest in memory -- same out-of-sequence store
 * as Glide's 0x118ED19C. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0A4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0A8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0AC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0B0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0B4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0B8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0BC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0C0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0C4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA084 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0CC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0D0 */

void  BrTexHook_10024AB0(void);
void  BrTexHook_10025830(void);
void  BrTexHook_10027C60(void);
void  BrTexHook_10028BF0(void);
void  BrTexHook_10028CA0(void);
void  BrTexHook_10028E00(void);
void  BrTexHook_10028E70(void);
void  BrTexHook_10029060(void);
void  BrTexHook_100290E0(void);
void  BrTexHook_1002A280(void);
void  BrTexHook_1002A350(void);
void  BrTexHook_1002A430(void);
void  BrTexHook_1002A7A0(void);

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1057542C, freed then cleared */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x10575424 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x10575428 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1057543C, second free */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x104D51B8, set to -1 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x105553F0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x105553F4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x105553E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x105553E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x10575414 */

uint32_t BrTexQueryAvail(void);       /* 0x1003E220 */
int32_t  BrTexChooseLevelAbs(void);   /* 0x1002A5A0 -- leaves eax live */
void     BrGbiSolidTexBuildAbs(void); /* 0x1002A740 */
void     BrTexCreateMutex(void);      /* 0x10074F20 */

/* WHAT IT DOES: set the texture system up -- install the thirteen
 * routines the rest of the engine calls, measure the card, pick a
 * detail level. */
/* @t3 0x10029B50 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 285/285 insns 55/55 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * The residue is one colouring wall and nothing else: the original spells the
 * shared zero in esi and the first span call's result in edi at 0x89, we take
 * the same two webs the other way round.  Every other byte is positionally
 * identical -- same size, same instruction count, register-blind multiset
 * 0+0.  Dossier and the 25-compile dead list are in this file's header, and a third
 * crank pass of 250 compiles on 2026-09-10 moved nothing; the corpus is a MISS
 * on the 12-instruction run, so no proven spelling exists to copy.  Do not reopen before the end-grind. */
/* @t4-pass 0x10029B50 3 2026-09-10 probes 250 bytes 285 insns 55 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @implements 0x1002A640 d3d BrTexInit */
/* FUN_10023d20: prototype in br_funcs.h */
/* FUN_10024e60: prototype in br_funcs.h */
/* FUN_100272f0: prototype in br_funcs.h */
/* FUN_10027f00: prototype in br_funcs.h */
/* FUN_100284e0: prototype in br_funcs.h */
/* FUN_100285e0: prototype in br_funcs.h */
/* FUN_10028620: prototype in br_funcs.h */
/* FUN_100287e0: prototype in br_funcs.h */
/* FUN_10028820: prototype in br_funcs.h */
/* FUN_100297f0: prototype in br_funcs.h */
/* FUN_100298c0: prototype in br_funcs.h */
/* FUN_100299a0: prototype in br_funcs.h */
/* FUN_10029cd0: prototype in br_funcs.h */
/* FUN_100281c0: prototype in br_funcs.h */
/* FUN_10072a1a: prototype in br_funcs.h */
/* FUN_10072a14: prototype in br_funcs.h */
/* FUN_10029c70: prototype in br_funcs.h */
/* FUN_1006e180: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

void BrTexInit(void)
{
    int span;

    (*(void (**)(void))&g_BrDrawModelDlHook) = BrTex3dRecCopyHead;
    g_18ED1C0 = BrTex3dExpandInto;
    g_pfn18ED1C4 = BrTex3dCreate;
    g_pfn18AA0B0 = BrGbiBlit;
    (*(void (**)(void))&DAT_118ed1cc) = BrTex3dMakeCurrent;
    DAT_118ed1d0 = BrTex3dReDownload;
    DAT_118ed1d4 = BrTexSlotFetchPixels;
    (*(void (**)(void))&g_BrGfxSubmit) = BrTex3dReconvert;
    (*(void (**)(void))&g_pfn18AA0C4) = BrGbiTexScanRun;
    (*(void (**)(void))&g_pfn18AA0C8) = BrGbiTexCreate;
    g_pfn18AA084 = FUN_100298c0;
    DAT_118ed1e4 = BrTexInstallRecords;
    (*(void (**)(void))&g_18ED1E8) = BrTex3dFreeAll;

    FUN_100281c0();

    {
        int z = 0;
        int hi = grTexMaxAddress(z);
        span = hi - grTexMinAddress(z);
    }
    s_texmem = (uint32_t)span;
    if ((DAT_105ccb68[26]) > 1) {
        int hi = grTexMaxAddress(1);
        span = hi - grTexMinAddress(1);
        s_texmem = s_texmem + (uint32_t)span;
    }

    s_g5E1820 = -1;
    BrTexChooseLevel();
    s_g5E1808 = -1;

    {
        void *pFree = DAT_106b7aa0;
        (*(int *)&DAT_10697a58) = 0;
        (*(int *)&DAT_10697a5c) = 0;
        free(pFree);
    }
    DAT_106b7aa0 = 0;
    DAT_10697a50 = 0;
    DAT_10697a48 = 0;
    (*(int *)&g_brTexScan575414) = 0;

    BrGbiSolidTexBuild();
    BrMutexCreateAA0A0();
}
