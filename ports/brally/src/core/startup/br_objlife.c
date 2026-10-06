/* br_objlife.c -- startup.  See br_objlife.h.
 *
 * Port-build stand-ins for untranscribed callees live here (one definition
 * for the whole tree).  Matching builds define only the ones this TU calls.
 */
#include "br_dl.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice1_09.h"   /* br_globals: its objects */
#include "br_objlife.h"
#include "slice3_41.h"
#include "br_match.h"   /* BR_THISCALL1 */

#include <stddef.h>
#include <stdint.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrExt_1001BAD0(void);
/* BrExt_10008B80: prototype in br_funcs.h */
/* BrExt_10067880: prototype in br_funcs.h */
/* BrExt_10067900: prototype in br_funcs.h */
/* BrExt_1007F560: prototype in br_funcs.h */
/* BrExt_1007F680: prototype in br_funcs.h */
/* BrExt_10073B40: prototype in br_funcs.h */
void BrExt_1003DA90(void *, void *);
/* BrExt_1007E8B0: prototype in br_funcs.h */
/* BrExt_10038EB0: prototype in br_funcs.h */
/* BrExt_10069A80: prototype in br_funcs.h */
void BrExt_10035585(void *, int, int);
/* (port-only BrExt_1001BAD0 removed) */

/* BrExt_10008B80: the original function is BrPodNop */
/* BrExt_10067880: the original function is BrVarSave */
/* BrExt_10067900: the original function is BrVarLoad */
/* BrExt_1007F560: the original function is BrEhVecDtor */
/* BrExt_1007F680: the original function is BrEhVecCtor */
/* BrExt_10073B40: the original function is FUN_1006cd80 */
/* (port-only BrExt_1003DA90 removed) */

/* BrExt_1007E8B0: the original function is BrCrtAtExit */
/* BrExt_10038EB0: the original function is BrSub10032520 */
/* BrExt_10069A80: the original function is BrSub10062AF0 */
/* (port-only BrExt_10035585 removed) */


/* WHAT IT DOES: drop a live object pointer and retarget a function slot. */
/* @implements 0x1002B950 d3d BrFlagInit_1002B950 */
void BrFlagInit_1002B950(void)
{
    g_67D550 = 0;
    DAT_100a751c = g_afBrVtxOut;   /* 0x104B16E8 */
}

/* WHAT IT DOES: turn on the gate that skips "part 2", dispatch slot 4. */
/* @implements 0x1002F690 d3d BrFlagInit_1002F690 */
void BrFlagInit_1002F690(void)
{
    g_AC300 = 1;
    (*(uint32_t *)&DAT_105ccb68[21]) = 4;
}

/* WHAT IT DOES: install a constructor and a destructor for a heap object. */
/* @d3donly 0x1001BAE0 BrInstall_1001BAE0 -- config/brally/shared.csv pairs this with
 * Glide 0x1001E080, and that pairing is FALSE. The D3D function is 26 bytes:
 * two pointer stores and `mov eax,1; ret`, which is exactly the body below.
 * Glide 0x1001E080 is 173 bytes of 3dfx bring-up (grGlideInit /
 * grSstQueryHardware / grSstSelect and a board-type switch) and is
 * transcribed as BrGlInstall in src/brally/core/drawing/br_dlglide.c. They occupy
 * the same renderer slot and share nothing else; verified by disassembling
 * both binaries at both addresses. Tagged @implements here until 2026-09-03,
 * which put a 26-byte body against a 173-byte original in the report and had
 * two names claiming one address. */
/* @n64 0x80200000 located */
/* (port-only BrInstall_1001BAE0 removed) */


/* WHAT IT DOES: select dispatch slot 2 for the next jump through that table. */
/* @d3donly 0x1002F6E0 BrSet_1002F6E0 -- glide twin 0x1001CDA0 COMDAT-folded onto br_boot.c:BrAppStateEnterRun */
/* (port-only BrSet_1002F6E0 removed) */


/* WHAT IT DOES: fill a 64-byte named buffer. */
/* @implements 0x10067980 d3d BrWrap_10067980 */
void BrWrap_10067980(void)
{
    BrVarSave(g_0B3A68, g_abBrVarBlock40, 0x40);
}

/* WHAT IT DOES: bind that 64-byte buffer without filling it. */
/* @implements 0x100679A0 d3d BrWrap_100679A0 */
void BrWrap_100679A0(void)
{
    BrVarLoad(g_0B3A68, g_abBrVarBlock40);
}

/* WHAT IT DOES: bind the same kind of buffer inside the caller's object. */
/* @implements 0x10067960 d3d BrWrap_10067960 */
void BrWrap_10067960(void *p)
{
    BrVarLoad(g_0B39B0, (char *)p + 0x7080);
}

/* WHAT IT DOES: fill that per-object block (about 90 KB). */
/* @implements 0x10067940 d3d BrWrap_10067940 */
/* @n64 0x8022AED8 located */
void BrWrap_10067940(void *p)
{
    BrVarSave(g_0B39B0, (char *)p + 0x7080, 0x15F88);
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105CCB88  suppresses the save while set   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x100A9360  race mode (2 and 4 skip)        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x100B3858  number of entrants              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100BCBE8  lap count / gate                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AF3BC8  base of the per-entrant records */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x102066C8  base of the entrant save area   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100B382C */
/* BrReplayIsOn: prototype in br_funcs.h */
/* BrSet_1006AA90: prototype in br_funcs.h */

/* T2 (EQUIVALENT, not byte-exact): insn-exact (80/80), oracle EQUIVALENT,
 * gates 0/A1/A2/A4/A5 pass.  Residue is two unpaired rows in the record loop:
 * the original materialises the save-area base 0x102066C8 into a register
 * (`mov ebp,imm`) and accumulates `-idx*0x15F88 + off` onto it (`add ebp,off`),
 * while VC5 folds the base as an immediate addend (`add r,imm`).  The literal-
 * pooling wall -- the base is used only twice, too few for VC5 to pool it.
 * Reassociating and sequencing the pointer arithmetic (probes v1..v3) do not
 * move it. */
/* DEAD 2026-09-13 (fn.py, 6 probes): an `(int)&DAT - idx*K + off` integer
 * form, `&DAT - idx*K + off`, the pointer term parenthesised first, the
 * offset as `(n-1)*0x3840` in either position -- VC5 folds the base into
 * the final `add ebp, imm` from every one; the original's `mov ebp, imm`
 * at the loop top is not reachable from the sum's association.
 * DEAD 2026-09-13 (fn.py, 6 more): `off + (&DAT - idx*K)`, a `char *pB`
 * local for `&DAT - idx*K`, `&DAT - (idx*K - off)`, `&DAT + (off - idx*K)`,
 * an explicit (char *) cast, `(unsigned)off` -- every one 287 B at 2+2,
 * the base folded into the trailing `add ebp, imm`.  End-of-TU placement
 * inert.
 * DEAD 2026-09-26 (~80 compiles): C++ __thiscall member form, struct-array
 * forms (g_lap[-idx].frame[n], [e][4] rows), pointer-local compound steps
 * (p = base; p -= X; p += off), signed/unsigned/int casts on every term,
 * inline helpers taking the base as a PARAMETER, macros, TU padding.  A
 * micro-test shows VC5 emits `sub; add imm` (constant last) for EVERY
 * spelling of base - x*K + y, so the original's `mov ebp,base; sub; add`
 * needs the base not to be a link-time constant at reassociation time.
 * DEAD 2026-09-26 (~110 more): char/char[]/char[][0x15F88]/struct-array
 * declarations with every -idx subscript form; a named base local (char *,
 * int, unsigned, const; top or pre-loop); (T *)(base - idx*K) + n frame-
 * pointer arithmetic with and without the off induction; 31 compiler flag
 * sets.  All emit the same `add ebp, imm` tail.  Corpus MISS (game, crt,
 * ext, ext2) on `mov R,A; sub R,R; ...; add R,R`. */
/* @t4-pass 0x10060A30 1 2026-09-24 probes 18 bytes 287 insns 80 regions 2 rows 4 census no  (hand: loop shapes -- do/while pointer walk, indexed for, inline car-array index -- x five spellings of base - idx*0x15F88 + off, pointer and int arithmetic; VC5 always reassociates to off - idx*K + base) */
/* @t4-pass 0x10060A30 2 2026-09-24 probes 19 bytes 287 insns 80 regions 2 rows 4 census yes  (hand, slot census: every top-level slot of br_objlife.c; residue identical in all 19) */
/* WHAT IT DOES: at the end of a lap, snapshot every entrant's last-lap
 * record into the save area -- but only during an ordinary recorded race
 * (not a replay, not the two excluded modes, and only once the leader is on
 * the final counted lap). For each entrant it clears the record's running
 * fields, stamps the fixed frame budget (0x3840), points the record at its
 * slot in the save area, and hands the finished area to the writer. */
/* @t3 0x10060A30 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 287/286 insns 80/80 rows 2+2 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * RESIDUE: the save-area address is summed base-first in the original
 * (mov ebp,base; sub; add ebp,off) and constant-last by VC5 for every
 * spelling (DEAD lists above) -- the same integer sum; A3 pairs it by the
 * integer commutative-add rule.  Do not reopen before the end-grind.
 */
/* @implements 0x10060A30 glide BrRaceSaveLastLapInfo */
void __fastcall BrRaceSaveLastLapInfo(BrDriverCar *param_1)
{
    int n;
    int *pRec;
    int off;

    if ((*(int *)&DAT_105ccb68[8]) != 0 || (*(int *)&g_brRaceRules.mode) == 2 || (*(int *)&g_brRaceRules.mode) == 4)
        return;
    if (param_1->f140 >= (*(int *)&g_brRaceNEntrant))
        return;
    if (param_1->lap != g_CBE8 - 1 && g_CBE8 > 1)
        return;
    if (BrReplayIsOn() != 0)
        return;

    BrPodNop();
    BrSet_1006AA90();

    n = 0;
    if ((*(int *)&g_brRaceNEntrant) > 0) {
        off = 0;
        do {
            /* the car's control block: length, capacity and buffer of this
             * entrant's replay record (pCtl +0x34 / +0x3C / +0x2C) */
            BrRaceCtl *pc = g_aBrRaceCar[n].pCtl;
            pc->aLen[param_1->f140] = 0;
            pc->aCap[param_1->f140] = 0x3840;
            pc->apRec[param_1->f140] =
                (char *)&g_ab0C12A0[15 * 0x15F88] + off + param_1->f140 * -0x15f88;
            ++n;
            off += 0x3840;
        } while (n < (*(int *)&g_brRaceNEntrant));
    }
    BrWrap_10067940((char *)&g_ab0C12A0[15 * 0x15F88] + param_1->f140 * -0x15f88);
}

/* WHAT IT DOES: destroy the array of 16 C++ objects that 0x100715E0
 * constructed. */
/* @implements 0x10071610 d3d BrWrap_10071610 */
void BrWrap_10071610(void)
{
    BrEhVecDtor(g_aBrPeerMsg, sizeof g_aBrPeerMsg[0], 0x10, (void (*)(void *))BrPodNop);
}

/* WHAT IT DOES: construct that array of 16 objects in place. */
/* @implements 0x100715E0 d3d BrWrap_100715E0 */
/* @n64 0x80214A3C located */
void BrWrap_100715E0(void)
{
    BrEhVecCtor(g_aBrPeerMsg, sizeof g_aBrPeerMsg[0], 0x10, (void (*)(void *))FUN_1006cd80, (void (*)(void *))BrPodNop);
}

/* WHAT IT DOES: register that destructor with atexit so the array is
 * torn down at process exit if nobody destroyed it sooner. */
/* @implements 0x1006A570 glide BrAtexit_10071600 */
void BrAtexit_10071600(void)
{
    BrCrtAtExit(BrWrap_10071610);
}

/* The two fixed global objects the wrappers below hand to their class
 * routines (names follow the original's operand VAs), and those routines --
 * thiscall, passed in ecx via __fastcall on VC5 (br_match.h). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSub10008760: prototype in br_funcs.h */
/* BrSub10008D60: prototype in br_funcs.h */
/* BrObj87Dtor: prototype in br_funcs.h */

/* WHAT IT DOES: constructs the global object at g_AC0810 -- the static
 * initializer the compiler emits for a C++ global; BrObjLifeInit calls it
 * and then registers the matching destructor (below) with atexit. */
/* @implements 0x10032500 glide BrSub10032500 */
void BrSub10032500(void){ BrObj87Ctor(&g_brModelMgr); }

/* WHAT IT DOES: arrange for one object's destructor to run at process exit. */
/* @implements 0x10032510 glide BrAtexit_10038EA0 */
void BrAtexit_10038EA0(void)
{
    BrCrtAtExit(BrSub10032520);
}

/* WHAT IT DOES: destroys the g_AC0810 object at process exit -- the atexit
 * handler BrAtexit_10038EA0 registers. Pairs with BrSub10032500 above. */
/* @implements 0x10032520 glide BrSub10032520 */
void BrSub10032520(void){ BrObj87Dtor(&g_brModelMgr); }

/* WHAT IT DOES: the same atexit registration for a different object. */
/* @implements 0x10062AE0 glide BrAtexit_10069A70 */
void BrAtexit_10069A70(void)
{
    BrCrtAtExit(BrSub10062AF0);
}

/* WHAT IT DOES: destroys the global object at g_B71290 at process exit --
 * the atexit handler BrAtexit_10069A70 registers; that object's destructor
 * is the +4 sub-object's (0x10008D60, an empty body). */
/* @implements 0x10062AF0 glide BrSub10062AF0 */
void BrSub10062AF0(void){ BrPodNop(); }

/* (port-only BrWrap_1003DAE0 removed) */


/* (port-only BrTableCopySlot_10024AB0 removed) */


/* (port-only BrTableSetField_10025800 removed) */


/* WHAT IT DOES: set a one-shot "this path has already run" flag. */
/* @implements 0x100378A0 d3d BrArm_100378A0 */
void BrArm_100378A0(void)
{
    g_6C7C44 = 1;
}

/* WHAT IT DOES: write a packed sentinel into a related status word. */
/* @d3donly 0x10036020 BrSet_10036020 -- glide twin 0x1002F6C0 COMDAT-folded onto br_racestart.c:BrRaceSub1002F6C0 */
/* (port-only BrSet_10036020 removed) */



/* (port-only BrWrap_10035610 removed) */


/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1001dfb0: prototype in br_funcs.h */
/* grSstWinOpen: prototype in br_funcs.h */
/* BrGlideResOpen: prototype in br_funcs.h */
/* FUN_10032500: prototype in br_funcs.h */
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrPodNop: prototype in br_funcs.h */
/* BrTexInit: prototype in br_funcs.h */
int br_dl_clip_reset(void);
/* BrGlInstall: prototype in br_funcs.h */

/* WHAT IT DOES: map the current width/height globals to a Glide resolution constant and
 * open the 3dfx window with it; unknown sizes skip the open. Either way, push the size
 * into the clip layer (0x1001DFB0). The identical grSstWinOpen call in every branch is
 * what the original's cross-jumped push chains demand -- a shared call through a variable
 * pushes a register, not the per-branch constants. Params are pushed by callers but
 * unused: the sizes are read from the globals. */
/* @implements 0x1001DD80 glide BrGlideResOpen */

#define BR_TRY_RES(W,H,R) if (BrGbiRectG_A7514 == (W) && BrGbiRectG_A7518 == (H)) { if (grSstWinOpen(0,(R),0,2,1,2,1) == 0) return 0; } else

int BrGlideResOpen(int param_1,int param_2)

{
  BR_TRY_RES(320,200,0)
  BR_TRY_RES(320,240,1)
  BR_TRY_RES(400,256,2)
  BR_TRY_RES(512,384,3)
  BR_TRY_RES(640,200,4)
  BR_TRY_RES(640,350,5)
  BR_TRY_RES(640,400,6)
  BR_TRY_RES(640,480,7)
  BR_TRY_RES(800,600,8)
  BR_TRY_RES(960,720,9)
  BR_TRY_RES(856,480,10)
  BR_TRY_RES(512,256,11)
  BR_TRY_RES(1024,768,12)
  BR_TRY_RES(1280,1024,13)
  BR_TRY_RES(1600,1200,14)
  BR_TRY_RES(400,300,15)
  { }
  FUN_1001dfb0();
  return 1;
}

/* WHAT IT DOES: change the Glide framebuffer resolution, falling back to 640x480 on failure.
 * The caller (0x10063970) pushes four words; the last two are never read here. */
/* @implements 0x1001E130 glide BrGlideResSet */

int BrGlideResSet(int param_1,int param_2,int param_3,int param_4)

{
  int iVar1;
  int uVar2;
  
  if ((param_1 == BrGbiRectG_A7514) && (param_2 == BrGbiRectG_A7518)) {
    g_scrW4 = param_1;
    BrGbiRectG_A7514 = param_1;
    (*(int *)&g_brRaceCueBase) = param_2;
    BrGbiRectG_A7518 = param_2;
    FUN_1001dfb0();
    return 1;
  }
  grSstWinClose();
  g_scrW4 = param_1;
  BrGbiRectG_A7514 = param_1;
  (*(int *)&g_brRaceCueBase) = param_2;
  BrGbiRectG_A7518 = param_2;
  iVar1 = BrGlideResOpen(param_1,param_2);
  if (iVar1 == 0) {
    g_scrW4 = 0x280;
    BrGbiRectG_A7514 = 0x280;
    (*(int *)&g_brRaceCueBase) = 0x1e0;
    BrGbiRectG_A7518 = 0x1e0;
    uVar2 = BrGlideResOpen(0x280,0x1e0);
    return uVar2;
  }
  return 1;
}

/* WHAT IT DOES: initialize the object-lifecycle subsystem and register its atexit handler. */
/* @implements 0x100324F0 glide BrObjLifeInit */

int BrObjLifeInit(void)

{
  BrSub10032500();
  BrAtexit_10038EA0();
  return;
}

/* WHAT IT DOES: initialize a subsystem wrapper and register its atexit handler. */
/* @implements 0x1006A540 glide BrObjLifeInit6A540 */

int BrObjLifeInit6A540(void)

{
  BrWrap_100715E0();
  BrAtexit_10071600();
  return;
}

/* WHAT IT DOES: record the render state; state 3 installs the Glide bring-up
 * (BrGlInstall, which also sets the frame-flip hooks), the display-list
 * clip-reset and the texture-init hook into their three runtime slots.
 * The first store is `mov [0x106B7AB4], 0x1001E080` in the original -- the
 * Glide installer.  It used to name BrInstall_1001BAE0, the D3D twin that
 * shares only this slot; the bytes matched because the store is a
 * relocation, but the certified image and the Mac port both followed the
 * name to the wrong function and the game never got its flip hook. */
/* @implements 0x10063940 glide BrRenderStateSet */

void BrRenderStateSet(int param_1)

{
  DAT_10b73644 = param_1;
  if (param_1 == 3) {
    DAT_106b7ab4 = BrGlInstall;
    DAT_10b73528 = br_dl_clip_reset;
    PTR_FUN_100b849c = BrTexInit;
  }
  return;
}

/* WHAT IT DOES: (re)start the renderer in a state: tear down the previous one through its
 * hooks, set the state, change resolution, then run the three state hooks. */
/* @implements 0x10063970 glide BrRenderModeStart */

void BrRenderModeStart(int param_1,int param_2,int param_3,int param_4,
                 int param_5)

{
  if (DAT_10b73644 != 0) {
    BrPodNop();
    (*DAT_10b7352c)();
    (*(*(funcptr *)&g_18ED1E8))();
  }
  BrRenderStateSet(param_1);
  BrGlideResSet(param_2,param_3,param_4,param_5);
  (*(funcptr )PTR_FUN_100b849c)();
  (*DAT_10b73528)();
  BrFlagInit_1002B950();
  return;
}

/* WHAT IT DOES: set the render state and run its three hooks without a resolution change. */
/* @implements 0x100639D0 glide BrRenderModeRestart */

void BrRenderModeRestart(int param_1)

{
  BrRenderStateSet(param_1);
  (*DAT_106b7ab4)();
  (*(funcptr )PTR_FUN_100b849c)();
  (*DAT_10b73528)();
  BrFlagInit_1002B950();
  return;
}

