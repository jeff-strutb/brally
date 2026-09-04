/* br_texblit.c -- drawing: copying texture rows into place.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_19.c, an address batch and not a module.  The inner
 * loop of the texture uploader: a rectangular block of rows walked from
 * source to destination with the caller's stride.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

#ifdef BR_MATCHING_BUILD

void FUN_100746b4(void *d, void *s, unsigned n);

/* WHAT IT DOES: copy a rectangular block of texture rows from source to
 * destination, walking row by row with the caller's stride. The inner loop
 * of the texture uploader. */
/* @implements 0x1002E5B9 glide FUN_1002e5b9 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1002e5b9(int param_1,int param_2,int param_3,int param_4)

{
  /* /Od: one 0x20 struct, fields in address order (ebp-0x20 .. ebp-4). */
  struct {
    int end;
    int n;
    int row;
    int len;
    int src;
    int dest;
    int k3;
    int sbyte;
  } s;
  
  s.end = 0;
  s.k3 = 3;
  s.src = 0;
  s.dest = 0;
  param_2 = param_2;
  for (s.row = 0; s.row < param_4; s.row = s.row + 1) {
    s.dest = 0;
    FUN_100746b4(&s.len,(void *)(param_3 + s.src),4);
    s.src = s.src + 4;
    s.end = s.src + s.len;
    while (s.src < s.end) {
      s.sbyte = (int)*(char *)(param_3 + s.src);
      s.src = s.src + 1;
      if (s.sbyte < 0) {
        for (s.n = -s.sbyte; s.n != 0; s.n = s.n + -1) {
          *(char *)(param_1 + s.dest) = *(char *)(param_3 + s.src);
          s.src = s.src + 1;
          s.dest = s.dest + param_4;
        }
      }
      else {
        s.n = s.sbyte + s.k3;
        s.sbyte = (int)*(char *)(param_3 + s.src);
        s.src = s.src + 1;
        for (; s.n != 0; s.n = s.n + -1) {
          *(char *)(param_1 + s.dest) = (char)s.sbyte;
          s.dest = s.dest + param_4;
        }
      }
    }
    param_1 = param_1 + 1;
  }
  return s.dest;
}

#endif /* BR_MATCHING_BUILD */
