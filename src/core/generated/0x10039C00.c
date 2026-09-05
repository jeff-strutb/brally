/* Matching TU for 0x10039C00 -- reload the active control preset.
 *
 * ‼ THIS IS A C++-LANE FUNCTION.  Do not keep grinding it in C.  The four
 * arms each read, in the original:
 *      push 0                       <- the argument, a PLAIN INT IMMEDIATE
 *      mov  ecx, 0x10B71290         <- `this`, set AFTER the push
 *      call 0x10062B10
 * Two independent tells, both already recorded elsewhere in the tree:
 *
 *  1. PUSH BEFORE ECX.  __fastcall always emits the ecx setup first and the
 *     stack argument after it, so the original's order is unreachable from
 *     the C twin.  Same finding as BrVt8A70CallPair in src/core/slice4_52.c
 *     ("the push-before-ecx order is unreachable from the C fastcall twin"),
 *     whose Glide match had to move to src/core/cpp/0x10008A70.cpp.
 *  2. `push imm8` FOR THE ARGUMENT.  The only way a C __fastcall keeps its
 *     second argument off edx is the struct-typed wrapper (br_match.h), and
 *     a struct is a copy, not an immediate: every arm then costs an extra
 *     `xor eax,eax` / `mov eax,N` plus `push eax` (2 bytes -> 7).  That is
 *     the whole of the +31 bytes below.  A real thiscall `int` argument
 *     pushes the constant directly.
 *
 * `tools/corpus.py find --from 0x10039C00 --at 0x11 --len 12` is a MISS: no
 * byte-exact function anywhere in the tree emits any run of three of these
 * instructions, so there is no C spelling to copy -- which is the corpus
 * saying the construct is not reachable from this lane either.
 *
 * Best C state, kept below as documentation of the structure: 120 bytes
 * against 89, REGNORM 17+4, the switch/jump-table shape and all four arms
 * correct.  The jump table itself is right (`ja default` / `jmp [eax*4+T]`
 * with the default arm shared with every case's `mov eax,1; ret`).
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#include "br_match.h"   /* BR_THISCALL1 */

/* The control-config object (0x10B71290) and the preset selector
 * (0x10AC5D64) this reloads from. */
extern int g_brCtrlCfgObj;                     /* 0x10B71290 */
extern int g_brCtrlPreset;                     /* 0x10AC5D64 */

/* 0x10062B10 -- __thiscall with ONE stack argument: `push n; mov ecx,obj;
 * call`.  __fastcall with a struct-typed second argument reproduces that
 * exactly (the struct is never register-eligible, so it stays pushed). */
typedef struct { int v; } BrCtrlPresetArg;
void BR_THISCALL1 BrCtrlCfgLoadPreset(void *pThis, BrCtrlPresetArg n);

/* WHAT IT DOES: re-applies the currently selected control preset (0..3) to
 * the control-config object -- one call per preset value, written out as a
 * four-way switch; any other selector value does nothing.  Always reports
 * success. */
/* @implements 0x10039C00 glide BrCtrlCfgReloadPreset */
int BrCtrlCfgReloadPreset(void)
{
    BrCtrlPresetArg n;

    switch (g_brCtrlPreset) {
    case 0:
        n.v = 0;
        BrCtrlCfgLoadPreset(&g_brCtrlCfgObj, n);
        return 1;
    case 1:
        n.v = 1;
        BrCtrlCfgLoadPreset(&g_brCtrlCfgObj, n);
        return 1;
    case 2:
        n.v = 2;
        BrCtrlCfgLoadPreset(&g_brCtrlCfgObj, n);
        return 1;
    case 3:
        n.v = 3;
        BrCtrlCfgLoadPreset(&g_brCtrlCfgObj, n);
        break;
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
