/* br_carwheelfx.c -- drawing: per-wheel effect state for the car effects.
 *
 * BrCarWheelFx works out, once a frame for each of a car's four wheels, which
 * effect (dust, spray, water) the wheel shows, its direction and its source
 * point, and writes them out packed for the effect drawing to read.
 *
 * Filed out of the address batch slice2_21.c; the preamble is slice2_21.c's,
 * carried whole. That file's header note:
 *
 * Every x87 sequence below was traced instruction by instruction through its
 * fxch/fsubr/fdivr chain; the reversed operand forms (fsubr, fdivr, fsubp
 * st(n)) are spelled out in the arithmetic here rather than "tidied", because
 * `fsubr m32` is `st0 = m32 - st0` and getting that backwards is silent.
 */
#define BrSpanTestPoint BrSpanTestPoint_port
#define BrPfxReset      BrPfxReset_port
#include "slice2_21.h"
#include "br_cartypes.h"
#include "slice3_41.h"
#undef BrSpanTestPoint
#undef BrPfxReset
/* BrSpanTestPoint: prototype in br_funcs.h */
/* BrPfxReset: prototype in br_funcs.h */
/* BrSpanContains: prototype in br_funcs.h */

#include <string.h>

/* --------------------------------------------------------------------------
 * Constants, all read straight out of .rdata rather than guessed.
 * -------------------------------------------------------------------------- */
#define K_0            0.0f                    /* 0x1008F62C, 0x1008F59C */
#define K_1            1.0f                    /* 0x1008F628, 0x1008F588 */
#define K_EPS_REL      1.0000000036274937e-15f /* 0x1008F63C */
#define K_PI           3.1415927410125732f     /* 0x1008F640 */
#define K_PI_2         1.5707963705062866f     /* -0x1008F644 */
#define K_PI_4         0.7853981852531433f     /* -0x1008F648, +0x1008F658 */
#define K_PI_8         0.39269909262657166f    /* 0x1008F64C */
#define K_PI_16        0.19634954631328583f    /* 0x1008F650 */
#define K_SIN_TOL      0.004999999888241291f   /* 0x1008F654 */
#define K_CELL_RECIP   0.03125f                /* 0x1008F61C, 0x1008F608 */
#define K_CELL         32.0f                   /* 0x1008F624 */
#define K_65280_RECIP  1.5318628356908448e-05f /* 0x1008F5FC = 1/65280 */
#define K_65536_RECIP  1.5259021893143654e-05f /* 0x1008F57C = 1/65536 */

/* --------------------------------------------------------------------------
 * 6. Car-driven effects
 * -------------------------------------------------------------------------- */


/* The four wheel records, in the order both routines index them. */
static const unsigned aWheelOff[4] = { 0x994u, 0x57Cu, 0x370u, 0x788u };



/* 0x10039200 */
/* WHAT IT DOES: works out, once a frame and for each of a car's four wheels,
 * what kind of effect that wheel should be showing and which way it should be
 * throwing it -- the direction and the point it comes from, both scaled by how
 * fast the car is going and how much it is sliding sideways. Water is treated
 * differently from dust, a wheel keeps emitting for three frames after it
 * leaves the ground, and the results are written out in the packed form the
 * effect drawing later reads. */
/* @t4-pass 0x10032880 1 2026-09-21 probes 10 bytes 1467 insns 405 regions 12 rows 54 census yes  (hand, fn.py variants: scaled-index vs walked induction pointers, indexed apW[i] vs walked ppW, dead vecB/vecC zero-init removal, vecB component temps; slot census orig-vs-recomp balanced) */
/* @t4-pass 0x10032880 2 2026-09-21 probes 10 bytes 1467 insns 405 regions 12 rows 54 census yes  (hand, fn.py variants: typed vs char* pointer strides, induction-pointer init reorder, pF subtraction operand spellings; none move the ppW/pSoa CSE merge or the fsub/fsubr operand-role fork) */
/* @t3 0x10032880 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1467/1464 insns 405/405 rows 27+27 regions 12 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Behaviour is the original's: the differential oracle agrees on the return,
 * every touched global and every side effect across 64 seeded car states
 * (tools/t3b_verify.py EQUIVALENT), and the instruction count is exact
 * (405/405).  Residue is register allocation plus the x87 operand-role wall:
 *   1. The original walks TWO separate stride-4 induction pointers -- the
 *      wheel-record pointer array (apW, [esp+0x28]) and the SoA cursor (pSoa,
 *      [esp+0x2c]).  VC5 here CSE-merges them (keeps `&apW - pSoa` constant and
 *      rebuilds apW as `[(&apW-pSoa)+pSoa]`), which costs two extra dword slots
 *      (frame sub esp,0x78 vs 0x70) and shifts every esp-relative offset -- the
 *      bulk of the masked diffs are that positional shift, not changed logic.
 *   2. The three packed-position writes `(vecB.n - pCar->f26Cn) * 127` come out
 *      `fld [pCar field] / fsubr [vecB.n]` where the original does
 *      `fld [vecB.n] / fsub [pCar field]` -- same value, the known VC5
 *      commutative-subtract operand-role fork (docs: x87 wall).  Neither
 *      component temps nor statement reordering moves it.
 * Minor: one int->float uses fild qword where the original uses fild dword, and
 * one zeroing is sub r,r vs xor r,r.  Slot census (tools/slotcensus.py) shows
 * matched per-slot write/read balance -- no dropped store.  Do not reopen
 * before the end-grind. */
/* @implements 0x10039200 d3d BrCarWheelFx */
/* The Glide twin (0x10032880) is __fastcall(pCar): everything the port passes
 * as pEnv->* and pSeed is a file-scope global here, and the wheel effect state
 * lives on the car object reached through ecx.  Same algorithm as the port
 * body in the #else arm below -- only the inputs move.  The truncations go
/* _ftol: the original function is ftol */
