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

/* WHAT IT DOES: run-length encode one image, channel by channel, into the
 * format the block copier above decodes: for each of `stride` interleaved
 * channels a 4-byte length then a stream of packets, where a negative
 * count is that many literal bytes and a positive count is a run of the
 * value that follows. A run must be at least three long; runs and literal
 * stretches are capped so their counts fit a byte. Returns the output
 * length, or -1 when the output buffer would overflow. */
/* @implements 0x1002E376 glide BrRleEncode */

int BrRleEncode(char *dst,int dstMax,char *src,int srcLen,int stride)

{
  /* /Od homes locals by name hash; single letters in the original's
   * frame order land in order (docs/VC5-IDIOMS.md). Roles:
   *   a cur      -4    b c3=stride*3   -8    c c128=stride*128  -0xc
   *   d out      -0x10 e pos           -0x14 f len              -0x18
   *   g chan     -0x1c h prev          -0x20 i runStart         -0x24
   *   j hdr      -0x28 k litStart      -0x2c l c132=stride*132  -0x30 */
  int a;
  int b;
  int c;
  int d;
  int e;
  int f;
  int g;
  int h;
  int i;
  int j;
  int k;
  int l;

  b = stride * 3;
  l = stride * 0x84;
  c = stride << 7;
  k = 0;
  i = 0;
  e = 0;
  d = 0;
  h = 0xffffff00;
  g = 0;
  j = 0;
  f = 0;
  if (1) goto again;
scan:
  a = src[e];
  if (!((a == h) && (e - i <= l) && (e - k <= c) && (e < srcLen))) {
    if (e - i >= b) {
      if (k != i) {
flush:
        if (d + (i - k) / stride + 1 > dstMax) {
          return -1;
        }
        dst[d] = (char)(-(i - k) / stride);
        d = d + 1;
        while (k < i) {
          dst[d] = src[k];
          d = d + 1;
          k = k + stride;
        }
      }
      if (i != e) {
        if (d + 2 > dstMax) {
          return -1;
        }
        dst[d] = (char)((e - i - b) / stride);
        d = d + 1;
        dst[d] = (char)h;
        d = d + 1;
      }
      k = e;
      i = e;
      if (e >= srcLen) {
        f = d - (j + 4);
        FUN_100746b4(dst + j,&f,4);
        g = g + 1;
        src = src + 1;
        if (g >= stride) {
          return d;
        }
newchan:
        if (d + 4 > dstMax) {
          return -1;
        }
        j = d;
        d = d + 4;
        e = 0;
        k = 0;
        i = 0;
        h = 0xffffff00;
      }
    }
    else {
      i = e;
      if (e >= srcLen) goto flush;
    }
  }
  else {
    if (e - k > c) {
      i = e;
      goto flush;
    }
  }
  h = a;
  e = e + stride;
  goto scan;
again:
  goto newchan;
}

#endif /* BR_MATCHING_BUILD */
