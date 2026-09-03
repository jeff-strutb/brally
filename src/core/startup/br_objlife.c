/* br_objlife.c -- startup.  See br_objlife.h.
 *
 * Port-build stand-ins for untranscribed callees live here (one definition
 * for the whole tree).  Matching builds define only the ones this TU calls.
 */
#include "br_objlife.h"

#include <stddef.h>
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern uint32_t g_67D550, g_0A81C8, g_AC300, g_690A14;
extern uint32_t g_690A24, g_690A28;
extern uint32_t g_0B3A68, g_0B39B0, g_1826BD0;
extern uint32_t *g_A9D008, *g_57543C;
extern uint32_t g_6C7C44, g_6C7C38, g_18AC2D0;
void BrExt_1001BAD0(void);
void BrExt_10008B80(void);
void BrExt_10067880(void *, void *, int);
void BrExt_10067900(void *, void *);
#ifdef _MSC_VER
void __stdcall BrExt_1007F560(void *, int, int, void (*)(void));
void __stdcall BrExt_1007F680(void *, int, int, void (*)(void), void (*)(void));
#else
void BrExt_1007F560(void *, int, int, void (*)(void));
void BrExt_1007F680(void *, int, int, void (*)(void), void (*)(void));
#endif
void BrExt_10073B40(void);
void BrExt_1003DA90(void *, void *);
void BrExt_1007E8B0(void (*)(void));
void BrExt_10038EB0(void);
void BrExt_10069A80(void);
void BrExt_10035585(void *, int, int);
void BrExt_1001BAD0(void) {}
void BrExt_10008B80(void) {}
void BrExt_10067880(void *a, void *b, int n) { (void)a; (void)b; (void)n; }
void BrExt_10067900(void *a, void *b) { (void)a; (void)b; }
#ifdef _MSC_VER
void __stdcall BrExt_1007F560(void *a, int b, int c, void (*d)(void))
{ (void)a; (void)b; (void)c; (void)d; }
void __stdcall BrExt_1007F680(void *a, int b, int c, void (*d)(void), void (*e)(void))
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
#else
void BrExt_1007F560(void *a, int b, int c, void (*d)(void))
{ (void)a; (void)b; (void)c; (void)d; }
void BrExt_1007F680(void *a, int b, int c, void (*d)(void), void (*e)(void))
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
#endif
void BrExt_10073B40(void) {}
void BrExt_1003DA90(void *a, void *b) { (void)a; (void)b; }
void BrExt_1007E8B0(void (*p)(void)) { (void)p; }
void BrExt_10038EB0(void) {}
void BrExt_10069A80(void) {}
void BrExt_10035585(void *p, int a, int b) { (void)p; (void)a; (void)b; }
#else
uint32_t g_67D550, g_0A81C8, g_AC300, g_690A14;
uint32_t g_690A24, g_690A28;
uint32_t g_0B3A68, g_0B39B0, g_1826BD0;
uint32_t *g_A9D008, *g_57543C;
uint32_t g_6C7C44, g_6C7C38, g_18AC2D0;

void BrExt_1001BAD0(void) {}
/* Declared, not defined: slice6_74.c carries the transcribed body of
 * BrExt_10008B80.  The original really is an empty function -- that is the
 * shipped behaviour, not a gap -- so a stand-in here looks harmless and is
 * not: it collides with the real definition at link time.  This TU still
 * passes it as a callback below, hence the declaration. */
void BrExt_10008B80(void);
void BrExt_10067880(void *a, void *b, int n) { (void)a; (void)b; (void)n; }
void BrExt_10067900(void *a, void *b) { (void)a; (void)b; }
void BrExt_1007F560(void *a, int b, int c, void (*d)(void))
{ (void)a; (void)b; (void)c; (void)d; }
void BrExt_1007F680(void *a, int b, int c, void (*d)(void), void (*e)(void))
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
void BrExt_10073B40(void) {}
void BrExt_1003DA90(void *a, void *b) { (void)a; (void)b; }
void BrExt_1007E8B0(void (*p)(void)) { (void)p; }
void BrExt_10038EB0(void) {}
void BrExt_10069A80(void) {}
void BrExt_10035585(void *p, int a, int b) { (void)p; (void)a; (void)b; }
/* No stub for BrExt_10043E70: slice5_63.c carries the transcribed body.  The
 * stub also had the wrong signature -- void * where slice5_63.h and
 * slice2_26.h both declare int32_t. */
void BrExt_10045BC0(void *p) { (void)p; }
void BrExt_100451E0(void *p) { (void)p; }
void BrExt_10046790(void) {}
void BrExt_10046750(void) {}
void BrExt_10046910(void) {}
void BrExt_10046950(void) {}
void BrExt_100469F0(void) {}
void BrExt_10046A30(void) {}
void BrExt_10046BB0(void) {}
void BrExt_10002660(void *p) { (void)p; }
void BrExt_100025F0(void *p) { (void)p; }
void BrExt_10072B30(void *a, int b, int c) { (void)a; (void)b; (void)c; }
void BrExt_10072A90(void *a, int b, int c, int d)
{ (void)a; (void)b; (void)c; (void)d; }
void BrExt_10024460(void) {}
void BrExt_1002A640(void) {}
int  BrExt_10075020(void) { return 0; }
#endif

/* WHAT IT DOES: drop a live object pointer and retarget a function slot. */
/* @implements 0x1002B950 d3d BrFlagInit_1002B950 */
void BrFlagInit_1002B950(void)
{
    g_67D550 = 0;
#ifdef BR_MATCHING_BUILD
    g_0A81C8 = 0x104B16E8u;  /* Glide VA */
#else
    g_0A81C8 = 0x10575540u;
#endif
}

/* WHAT IT DOES: turn on the gate that skips "part 2", dispatch slot 4. */
/* @implements 0x1002F690 d3d BrFlagInit_1002F690 */
void BrFlagInit_1002F690(void)
{
    g_AC300 = 1;
    g_690A14 = 4;
}

/* WHAT IT DOES: install a constructor and a destructor for a heap object. */
/* @d3donly 0x1001BAE0 BrInstall_1001BAE0 -- config/shared.csv pairs this with
 * Glide 0x1001E080, and that pairing is FALSE. The D3D function is 26 bytes:
 * two pointer stores and `mov eax,1; ret`, which is exactly the body below.
 * Glide 0x1001E080 is 173 bytes of 3dfx bring-up (grGlideInit /
 * grSstQueryHardware / grSstSelect and a board-type switch) and is
 * transcribed as BrGlInstall in src/core/drawing/br_dlglide.c. They occupy
 * the same renderer slot and share nothing else; verified by disassembling
 * both binaries at both addresses. Tagged @implements here until 2026-09-03,
 * which put a 26-byte body against a 173-byte original in the report and had
 * two names claiming one address. */
int BrInstall_1001BAE0(void)
{
    g_690A24 = (uint32_t)(uintptr_t)&BrExt_1001BAD0;
    g_690A28 = (uint32_t)(uintptr_t)&BrExt_10008B80;
    return 1;
}

/* WHAT IT DOES: select dispatch slot 2 for the next jump through that table. */
/* @d3donly 0x1002F6E0 BrSet_1002F6E0 -- glide twin 0x1001CDA0 COMDAT-folded onto br_boot.c:BrAppStateEnterRun */
int BrSet_1002F6E0(void)
{
    g_690A14 = 2;
    return 1;
}

/* WHAT IT DOES: fill a 64-byte named buffer. */
/* @implements 0x10067980 d3d BrWrap_10067980 */
void BrWrap_10067980(void)
{
#ifdef BR_MATCHING_BUILD
    BrExt_10067880(&g_0B3A68, (void *)(uintptr_t)0x10B1CBA8, 0x40);  /* Glide VA */
#else
    BrExt_10067880(&g_0B3A68, (void *)(uintptr_t)0x10AF9848, 0x40);
#endif
}

/* WHAT IT DOES: bind that 64-byte buffer without filling it. */
/* @implements 0x100679A0 d3d BrWrap_100679A0 */
void BrWrap_100679A0(void)
{
#ifdef BR_MATCHING_BUILD
    BrExt_10067900(&g_0B3A68, (void *)(uintptr_t)0x10B1CBA8);  /* Glide VA */
#else
    BrExt_10067900(&g_0B3A68, (void *)(uintptr_t)0x10AF9848);
#endif
}

/* WHAT IT DOES: bind the same kind of buffer inside the caller's object. */
/* @implements 0x10067960 d3d BrWrap_10067960 */
void BrWrap_10067960(void *p)
{
    BrExt_10067900(&g_0B39B0, (char *)p + 0x7080);
}

/* WHAT IT DOES: fill that per-object block (about 90 KB). */
/* @implements 0x10067940 d3d BrWrap_10067940 */
void BrWrap_10067940(void *p)
{
    BrExt_10067880(&g_0B39B0, (char *)p + 0x7080, 0x15F88);
}

/* WHAT IT DOES: destroy the array of 16 C++ objects that 0x100715E0
 * constructed. */
/* @implements 0x10071610 d3d BrWrap_10071610 */
void BrWrap_10071610(void)
{
    BrExt_1007F560(&g_1826BD0, 0x214, 0x10, BrExt_10008B80);
}

/* WHAT IT DOES: construct that array of 16 objects in place. */
/* @implements 0x100715E0 d3d BrWrap_100715E0 */
void BrWrap_100715E0(void)
{
    BrExt_1007F680(&g_1826BD0, 0x214, 0x10, BrExt_10073B40, BrExt_10008B80);
}

/* WHAT IT DOES: register that destructor with atexit so the array is
 * torn down at process exit if nobody destroyed it sooner. */
/* @implements 0x1006A570 glide BrAtexit_10071600 */
void BrAtexit_10071600(void)
{
    BrExt_1007E8B0(BrWrap_10071610);
}

/* WHAT IT DOES: arrange for one object's destructor to run at process exit. */
/* @implements 0x10032510 glide BrAtexit_10038EA0 */
void BrAtexit_10038EA0(void)
{
    BrExt_1007E8B0(BrExt_10038EB0);
}

/* WHAT IT DOES: the same atexit registration for a different object. */
/* @implements 0x10062AE0 glide BrAtexit_10069A70 */
void BrAtexit_10069A70(void)
{
    BrExt_1007E8B0(BrExt_10069A80);
}

void BrWrap_1003DAE0(void)
{
    uint32_t *p = g_A9D008;

    if (p != 0 && p[2] != 0)
        BrExt_1003DA90(p, (void *)(uintptr_t)p[2]);
}

void BrTableCopySlot_10024AB0(int dst, int src)
{
    struct Rec { char pad[696]; } *p = (struct Rec *)g_57543C;
    *(uint32_t *)&p[dst] = *(uint32_t *)&p[src];
}

void BrTableSetField_10025800(int idx, uint32_t v)
{
    struct Rec { char pad[696]; } *p = (struct Rec *)g_57543C;
    *(uint32_t *)((char *)&p[idx] + 0x27C) = v;
}

/* WHAT IT DOES: set a one-shot "this path has already run" flag. */
/* @implements 0x100378A0 d3d BrArm_100378A0 */
void BrArm_100378A0(void)
{
    g_6C7C44 = 1;
}

/* WHAT IT DOES: write a packed sentinel into a related status word. */
/* @d3donly 0x10036020 BrSet_10036020 -- glide twin 0x1002F6C0 COMDAT-folded onto br_racestart.c:BrRaceSub1002F6C0 */
void BrSet_10036020(void)
{
    g_6C7C38 = 0x80096400u;
}

/* WHAT IT DOES: remember one pointer a later CRT/error path will read. */
/* @d3donly 0x10086B80 BrStore_10086B80 -- glide twin 0x100168B0 COMDAT-folded onto slice6_78.c:BrTextSetSize */
void BrStore_10086B80(uint32_t v)
{
    g_18AC2D0 = v;
}

void BrWrap_10035610(void *p)
{
    BrExt_10035585(p, 1, 2);
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_106e7714;
extern int DAT_106e9a2c;
int FUN_1001dfb0();
int __stdcall grSstWinOpen(int,int,int,int,int,int,int);
int BrGlideResOpen(int param_1,int param_2);
int FUN_10032500();
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_106b7ab4;
extern funcptr DAT_10b73528;
extern funcptr DAT_10b7352c;
extern int DAT_10b73644;
extern funcptr DAT_118ed1e8;
extern funcptr PTR_FUN_100b849c;
int BrPodNop();
int BrTexInit();
int br_dl_clip_reset();

/* WHAT IT DOES: map the current width/height globals to a Glide resolution constant and
 * open the 3dfx window with it; unknown sizes skip the open. Either way, push the size
 * into the clip layer (0x1001DFB0). The identical grSstWinOpen call in every branch is
 * what the original's cross-jumped push chains demand -- a shared call through a variable
 * pushes a register, not the per-branch constants. Params are pushed by callers but
 * unused: the sizes are read from the globals. */
/* @implements 0x1001DD80 glide BrGlideResOpen */

#define BR_TRY_RES(W,H,R) if (DAT_100a7514 == (W) && DAT_100a7518 == (H)) { if (grSstWinOpen(0,(R),0,2,1,2,1) == 0) return 0; } else

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
  FUN_1001dfb0(DAT_100a7514,DAT_100a7518);
  return 1;
}

/* WHAT IT DOES: change the Glide framebuffer resolution, falling back to 640x480 on failure.
 * The caller (0x10063970) pushes four words; the last two are never read here. */
/* @implements 0x1001E130 glide BrGlideResSet */

int BrGlideResSet(int param_1,int param_2,int param_3,int param_4)

{
  int iVar1;
  int uVar2;
  
  if ((param_1 == DAT_100a7514) && (param_2 == DAT_100a7518)) {
    DAT_106e7714 = param_1;
    DAT_100a7514 = param_1;
    DAT_106e9a2c = param_2;
    DAT_100a7518 = param_2;
    FUN_1001dfb0(param_1,param_2);
    return 1;
  }
  grSstWinClose();
  DAT_106e7714 = param_1;
  DAT_100a7514 = param_1;
  DAT_106e9a2c = param_2;
  DAT_100a7518 = param_2;
  iVar1 = BrGlideResOpen(param_1,param_2);
  if (iVar1 == 0) {
    DAT_106e7714 = 0x280;
    DAT_100a7514 = 0x280;
    DAT_106e9a2c = 0x1e0;
    DAT_100a7518 = 0x1e0;
    uVar2 = BrGlideResOpen(0x280,0x1e0);
    return uVar2;
  }
  return 1;
}

/* WHAT IT DOES: initialize the object-lifecycle subsystem and register its atexit handler. */
/* @implements 0x100324F0 glide BrObjLifeInit */

int BrObjLifeInit(void)

{
  FUN_10032500();
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

/* WHAT IT DOES: record the render state; state 3 installs the object-ctor, display-list
 * clip-reset and texture-init hooks into their three runtime slots. */
/* @implements 0x10063940 glide BrRenderStateSet */

void BrRenderStateSet(int param_1)

{
  DAT_10b73644 = param_1;
  if (param_1 == 3) {
    DAT_106b7ab4 = BrInstall_1001BAE0;
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
    (*DAT_118ed1e8)();
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

#endif /* BR_MATCHING_BUILD */
