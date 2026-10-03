/* anim.c -- animation playback modes for a model's animated parts
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrAnim {         /* one morph animation of a model part */
  int n;                        /* 0x00  vertices */
  short *out;                   /* 0x04  the part's vertices (16 bytes each) */
  int x8;
  int nKeys;                    /* 0x0C */
  unsigned short flags;         /* 0x10  bit 0 repeat, bit 1 bounce, bit 2 running backwards */
  unsigned short key;           /* 0x12  the key the search starts from */
  float start;                  /* 0x14 */
  float end;                    /* 0x18 */
  float time;                   /* 0x1C */
  char *keys[1];                /* 0x20  each: its time, n xyz shorts, n rgb bytes */
} BrAnim;
typedef struct BrAnimList { int n; BrAnim *anim[1]; } BrAnimList;
typedef struct BrAnimSet { int x0; BrAnimList *list; } BrAnimSet;
void BrAnimSetFlags(BrAnimSet *set, int on, int off);
extern float D_8028AAD8;                /* seconds this frame */
/* -- end declarations -- */

/* WHAT IT DOES: Set and clear playback-mode bits on every animation in a
 * set (a set with no list is left alone). */
/* @implements 0x8021D6DC tgr BrAnimSetFlags */
void BrAnimSetFlags(BrAnimSet *set, int on, int off)
{
  int i;
  int n;
  BrAnim *a;

  off = ~off;
  if (set->list != 0) {
    n = set->list->n;
    for (i = 0; i < n; i++) {
      a = set->list->anim[i];
      a->flags |= on;
      a->flags &= off;
    }
  }
}

/* WHAT IT DOES: Play every animation in the set through once and stop at
 * the end: clears both the repeat and the bounce-back bits. */
/* @implements 0x8021D7E0 tgr BrAnimSetOnce */
void BrAnimSetOnce(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,0,3);
}

/* WHAT IT DOES: Play every animation in the set on repeat, restarting from
 * the beginning each time round: sets repeat, clears bounce-back. */
/* @implements 0x8021D804 tgr BrAnimSetLoop */
void BrAnimSetLoop(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,1,2);
}

/* WHAT IT DOES: Play every animation in the set back and forth: sets both
 * the repeat and the bounce-back bits. */
/* @implements 0x8021D828 tgr BrAnimSetPingPong */
void BrAnimSetPingPong(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,3,0);
}

/* WHAT IT DOES: Advance every animation in the set by the frame time and
 * morph its vertices: find the two keys either side of the time and blend
 * their positions and colours (12-bit fraction).  Before the start the
 * first key is shown; past the end the last key, unless repeating, when the
 * time wraps back (or, bouncing, turns round and runs backwards to the
 * start).  Written in the ROM's block order: the search and blend sit inside
 * the backwards branch; the time doubles as the blend fraction; `* 2` is an
 * integer so IDO keeps the multiply (a float 2.0f becomes x + x).
 * RESIDUE (243): the loop count spills to the stack where the ROM keeps it
 * in a register (one fewer saved register), so 4096.0f is not hoisted and
 * the temporaries shift.  Not yet matched. */
/* @t4-pass 0x8021D84C 1 2026-10-03 compiles 120 best 243 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021D84C 2 2026-10-03 compiles 119 best 243 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021D84C tgr BrAnimUpdate */
void BrAnimUpdate(BrAnimSet *set)
{
  int i;
  int n;
  BrAnim *a;
  float t;
  float len;
  float step;
  float lim;
  int k;
  char *ka;
  char *kb;
  short *pa;
  short *pb;
  signed char *ca;
  signed char *cb;
  short *out;
  int j;
  int f;
  int nv;

  if (set->list != 0) {
    n = set->list->n;
    for (i = 0; i < n; i++) {
      a = set->list->anim[i];
      if (a->flags & 4) {
        a->time -= D_8028AAD8;
        t = a->time;
        if (!(a->start <= t)) {
          goto back_to_start;
        }
        if (!(t < a->end)) {
          continue;
        }
      search:
        for (k = a->key; k < a->nKeys; k++) {
          if (t < *(float *)a->keys[k]) {
            break;
          }
        }
        kb = a->keys[k];
        ka = a->keys[k - 1];
        t = (t - *(float *)ka) / (*(float *)kb - *(float *)ka);
      blend:
        f = t * 4096.0f;
        pa = (short *)(ka + 4);
        pb = (short *)(kb + 4);
        ca = (signed char *)(ka + 4 + a->n * 6);
        cb = (signed char *)(kb + 4 + a->n * 6);
        out = a->out;
        nv = a->n;
        for (j = 0; j < nv; j++) {
          out[0] = pa[0] + ((pb[0] - pa[0]) * f >> 12);
          out[1] = pa[1] + ((pb[1] - pa[1]) * f >> 12);
          out[2] = pa[2] + ((pb[2] - pa[2]) * f >> 12);
          ((signed char *)out)[12] = ca[0] + ((cb[0] - ca[0]) * f >> 12);
          ((signed char *)out)[13] = ca[1] + ((cb[1] - ca[1]) * f >> 12);
          ((signed char *)out)[14] = ca[2] + ((cb[2] - ca[2]) * f >> 12);
          pa += 3;
          pb += 3;
          ca += 3;
          cb += 3;
          out += 8;
        }
        continue;
      back_to_start:
        if (!(a->flags & 1)) {
          continue;
        }
        a->flags &= ~4;
        a->key = 0;
        t = a->start * 2 - t;
        a->time = t;
        goto search;
      } else {
        a->time += D_8028AAD8;
        t = a->time;
        if (t < a->start) {
          t = 0.0f;
          ka = kb = a->keys[0];
          goto blend;
        }
        if (t < a->end) {
          if (!(a->start <= t)) {
            continue;
          }
          goto search;
        }
        if (!(a->flags & 1)) {
          t = 0.0f;
          ka = kb = a->keys[a->nKeys - 1];
          goto blend;
        }
        len = a->end - a->start;
        step = len;
        if (a->flags & 2) {
          step = len * 2;
          lim = a->end + step;
          while (lim < t) {
            t -= step;
          }
          step *= 0.5f;
          lim -= step;
          if (lim < t) {
            goto wrap;
          }
          a->flags |= 4;
          t = a->end * 2 - t;
          a->time = t;
        } else {
          lim = a->end + step;
        wrap:
          while (lim < t) {
            t -= step;
          }
          t -= len;
          a->time = t;
        }
        a->key = 0;
        goto search;
      }
    }
  }
}
