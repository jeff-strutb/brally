/* boot.c -- power-on threads, fatal errors, and model and track loading
 *
 * One object of the ROM, 0x8021D070-0x8021E61F: display-list and model
 * rebasing, a car's paint and colour, loading and installing car models,
 * model animation playback, loading the track, waiting for retrace, the
 * fatal-error halt, and the fault, idle and boot threads.
 *
 * Why one file: IDO pads an infinite loop's dead epilogue to 32 bytes from
 * the start of the object's .text, so the fault and idle threads only match
 * where they sit in the original object (it starts at 16 mod 32).  Gathered
 * here in ROM order from romread.c, entcolour.c, carselect.c, anim.c,
 * trackload.c, trackselect.c and the old boot.c; BrFaultThread then
 * matches.  Where those files declared one symbol with different types the
 * types are unified (osRecvMesg takes pointers; BrFatal a message).
 */
#include "tgr/common.h"

/* -- declarations -- */
#include "tgr/gbi.h"
#include "tgr/load.h"
#include "tgr/model.h"
#include "tgr/track.h"
extern int D_8031B328;
extern int D_8031B330;
void BrModelRebase(BrModel *m);
int BrInflate(unsigned char *dst, int *dstLen, unsigned char *src, unsigned int srcLen);
extern unsigned char D_802F7F00[];
extern void *D_80368AC0;
extern void *D_80368AC4;
extern char D_80324550[];
extern char D_8033CBF0[];
extern OSMesgQueue D_80319F88;
extern int D_80272D44;
extern int D_80272D40;
void BrDlRebaseWord(be32_t *slot, unsigned int lo, unsigned int hi, unsigned int base);
void BrDlRebase(Gfx *dl, unsigned int lo, unsigned int hi, unsigned int base);
extern OSIoMesg D_8031A020[32];

#include "tgr/car.h"

void BrCarPhysInit(BrCar *car);
void BrCarModelInstall(int slot, int car, void *src);
void BrFatal(char *msg);
void BrEntRebaseModel(BrCarModel *m);
void BrEntSetRecord(void *, int);
void BrEntRefreshColour(void *);
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern unsigned char D_8028B904[][3];
void BrCarResetFrames(BrCar *car);
extern char D_803D5F88[];

int BrCarModelPresent(int param_1);
extern int D_80272070;
#define D_8031C5BC TGR_PTR(char *, *TGR_PTR(TgrAddr *, 0x8031C5BC))   /* car 0\'s season (a member of D_8031B760) */
extern Gfx *D_8028A858;

void BrAnimSetFlags(BrAnimSet *set, int on, int off);
extern float D_8028AAD8;                /* seconds this frame */

#include "tgr/menu.h"

extern char D_80373EB0[];
extern char D_803736B0[];
extern char D_803746D0[];
extern char D_80373ED0[];
extern int D_8028A87C;
extern int D_8028A880;
extern int D_8028AA7C;
extern int D_8028AA88;
void BrMat4RotateDir(float *out, float *v, float (*m)[4]);
float BrVec3Length(float *v);
void BrScenePassRun(void);

int BrTrackIsPresent(int n);
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern int D_80315EE0;

void BrIdleThread(void *arg);
extern OSThread D_80272680;              /* the idle thread */
void BrHaltLoop(int arg0);
extern char *D_8028A88C;                 /* the fatal error's message */
extern OSMesgQueue D_8031A390;           /* the retrace queue */
extern OSMesgQueue D_8031B1B0;           /* the fault queue */
void BrMainThread(void *arg);
typedef struct BrArgs { int argc; TgrAddr argv; } BrArgs;   /* argv: char ** */
extern char *D_80316450[];           /* argv */
extern BrArgs *D_8026FF00;
extern char **D_8026FF04;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern OSViMode D_802A5D70;          /* MPAL video mode */
extern OSViMode D_802A54B0;          /* NTSC video mode */
extern OSMesgQueue D_80319ED0;       /* the PI manager's command queue */
extern OSMesg D_80319EE8[32];
extern OSMesg D_8031B1C8[1];
extern OSThread D_8031AC00;          /* the fault thread */
extern OSThread D_80272830;          /* the main game thread */
extern unsigned long long D_80316CD0[0x400];
extern unsigned long long D_8031ADB0[0x80];   /* the fault thread's stack */
extern unsigned long long D_80318CD0[];
extern unsigned long long D_80318ED0[0x100];
extern unsigned long long D_803196D0[0x100];
/* -- end declarations -- */

/* WHAT IT DOES: Correct one address inside data just loaded from ROM: if it
 * points into the old block [lo, hi) it is moved to the same offset from
 * the new base, otherwise it is left alone. */
/* @implements 0x8021D070 tgr BrDlRebaseWord */
void BrDlRebaseWord(be32_t *slot, unsigned int lo, unsigned int hi, unsigned int base)
{
  unsigned int v;

  v = BE32(*slot);
  if ((lo <= v) && (v < hi)) {
    SET32(*slot, (v - lo) + base);
  }
}


/* WHAT IT DOES: Walk a display list loaded from ROM and correct every
 * address in it that pointed into the old block (vertex and texture-image
 * commands) so it points at the new copy; stops at the end of the list. */
/* @implements 0x8021D098 tgr BrDlRebase */
void BrDlRebase(Gfx *dl, unsigned int lo, unsigned int hi, unsigned int base)
{
  if (dl == 0) {
    return;
  }
  for (;;) {
    switch ((unsigned char)(BE32(dl->words.w0) >> 24)) {
    case 0x04:
    case 0xfd:
      BrDlRebaseWord(&dl->words.w1, lo, hi, base);
      break;
    case 0xb8:
      return;
    }
    dl++;
  }
}

/* WHAT IT DOES: Paint a car model in a colour (5 bits a channel): each of
 * the six paint slots names two parts, and every one of those parts with a
 * paletted texture gets its first two palette entries rewritten, entry 0
 * to the colour and entry 1 to its half-bright shade.  Both keep the alpha
 * bit of a palette entry picked by the slot (entry 2k for the slot's first
 * part, 2k + 1 for its second), as the ROM does. */
/* @implements 0x8021D140 tgr BrEntPaintTexture */
void BrEntPaintTexture(BrCarModel *m, unsigned int r, unsigned int g, int b)
{
  int i;
  BrCarModelPart *parts;
  BrCarModelPart *p;
  BrCarModelPart *p2;
  unsigned char *pal;

  for (i = 0; i < 12; i += 2) {
    parts = BR_CARPARTS(m);
    p = &parts[m->decalPart[i]];
    pal = BEPTR(unsigned char *, p->b);
    if (pal != 0 && (p->fmt & 0xf) == 1) {
      tgr_wr16(pal + 0, (tgr_rd16(pal + 2 * i) & 1) | (r << 11) | (g << 6) | (b << 1));
      tgr_wr16(pal + 2, (tgr_rd16(pal + 2 * i) & 1) | ((r & 0x1e) << 10) | ((g & 0x1e) << 5) | (b & 0x1e));
    }
    p2 = &BR_CARPARTS(m)[m->decalPart[i + 1]];
    pal = BEPTR(unsigned char *, p2->b);
    if (pal != 0 && (p2->fmt & 0xf) == 1) {
      tgr_wr16(pal + 0, (tgr_rd16(pal + 2 * (i + 1)) & 1) | (r << 11) | (g << 6) | (b << 1));
      tgr_wr16(pal + 2, (tgr_rd16(pal + 2 * (i + 1)) & 1) | ((r & 0x1e) << 10) | ((g & 0x1e) << 5) | (b & 0x1e));
    }
  }
}

/* WHAT IT DOES: Take a car's body colour from its model: the first
 * palette entry of the paint part (when that part is paletted), widened
 * from RGBA5551 to 8 bits a channel (each 5-bit value shifted up and its
 * top 3 bits repeated below).  Each channel reads the entry afresh, which
 * puts all three ahead of the stores, as in the ROM. */
/* @implements 0x8021D2A0 tgr BrCarColourFromModel */
void BrCarColourFromModel(BrCar *car, BrCarModel *m)
{
  BrCarModelPart *p = m->decalPart[2] + BR_CARPARTS(m);
  unsigned char *pal = BEPTR(unsigned char *, p->b);
  int r;
  int g;
  int b;

  if (pal != 0 && (p->fmt & 0xf) == 1) {
    r = ((tgr_rd16(pal) >> 8) & 0xf8) | ((tgr_rd16(pal) >> 13) & 7);
    g = ((tgr_rd16(pal) >> 3) & 0xf8) | ((tgr_rd16(pal) >> 8) & 7);
    b = ((tgr_rd16(pal) << 2) & 0xf8) | ((tgr_rd16(pal) >> 3) & 7);
    car->colour[2] = b;
    car->colour[1] = g;
    car->colour[0] = r;
  }
}

/* WHAT IT DOES: Fix up a car model just loaded into its slot: every part's
 * address and display list is moved from the loading area to the slot's own
 * copy.  The rebase helpers are called without prototypes (declared old
 * style), so the two block addresses go in as pointers with no conversion;
 * a conversion each would reserve two spill slots and grow the frame. */
/* @implements 0x8021D32C tgr BrEntRebaseModel */
void BrEntRebaseModel(BrCarModel *m)
{
  int i;
  int j;
  unsigned int lo = tgr_addr32(D_803C8000);
  unsigned int hi = tgr_addr32(D_803D5F88);
  unsigned int base = tgr_addr32(m);
  BrCarModelPart *parts;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 10; j++) {
      BrDlRebaseWord(&m->dl[i][j], lo, hi, base);
      BrDlRebase(BEPTR(Gfx *, m->dl[i][j]), lo, hi, base);
    }
    for (j = 0; j < 3; j++) {
      if (BE32(m->dl2[i][j]) != 0) {
        BrDlRebaseWord(&m->dl2[i][j], lo, hi, base);
        BrDlRebase(BEPTR(Gfx *, m->dl2[i][j]), lo, hi, base);
      }
    }
  }
  BrDlRebaseWord(&m->parts, lo, hi, base);
  BrDlRebaseWord(&m->x90, lo, hi, base);
  BrDlRebaseWord(&m->x94, lo, hi, base);
  parts = BR_CARPARTS(m);
  for (i = 0; i < BES32(m->nParts); i++) {
    BrDlRebaseWord(&parts[i].a, lo, hi, base);
    BrDlRebaseWord(&parts[i].b, lo, hi, base);
  }
  BrDlRebaseWord(&m->x11c, lo, hi, base);
  for (i = 0; i < 12; i++) {
    BrDlRebaseWord(&BEPTR(be32_t *, m->x11c)[i], lo, hi, base);
  }
}


/* WHAT IT DOES: Load car n's model file from ROM into a model buffer,
 * recording its size; a file bigger than a buffer is a fatal error. */
/* @implements 0x8021D534 tgr BrCarModelLoad */
void BrCarModelLoad(void *buf, int car)
{
  char msg[80];

  if ((D_8028AE0C[car].size = BrRomReadSize(D_8028AE0C[car].rom)) > sizeof(BrCarModelBuf)) {
    sprintf(msg, "Car %d too big (%d vs. %d)", car, D_8028AE0C[car].size, sizeof(BrCarModelBuf));
    BrFatal(msg);
  } else {
    osSyncPrintf("Loading car %d (%d / %d)\n", car, D_8028AE0C[car].size, sizeof(BrCarModelBuf));
  }
  BrRomUnpack(buf, D_8028AE0C[car].rom, 0);
}

/* WHAT IT DOES: Put car n's model into a slot's model buffer -- loaded
 * from ROM, or copied from a model already in memory -- rebase its
 * pointers there and record which car the slot holds. */
/* @implements 0x8021D5E4 tgr BrCarModelInstall */
void BrCarModelInstall(int slot, int car, void *src)
{
  if (src == 0) {
    BrCarModelLoad(&D_803C8000[slot], car);
  } else {
    memcpy(&D_803C8000[slot], src, D_8028AE0C[car].size);
  }
  BrEntRebaseModel((BrCarModel *)&D_803C8000[slot]);
  D_8031B238[slot] = car;
}

/* WHAT IT DOES: Tell whether car n has a model record loaded (its entry in
 * the car model table is non-zero). */
/* @implements 0x8021D6B8 tgr BrCarModelPresent */
int BrCarModelPresent(int param_1)
{
  return D_8028AE0C[param_1].present != 0;
}

/* WHAT IT DOES: Set and clear playback-mode bits on every animation in a
 * set (a set with no list is left alone). */
/* @implements 0x8021D6DC tgr BrAnimSetFlags */
void BrAnimSetFlags(BrAnimSet *set, int on, int off)
{
  int i;
  int n;
  BrAnimList *list;
  BrAnim *a;

  off = ~off;
  if (BE32(set->anims) != 0) {
    list = BEPTR(BrAnimList *, set->anims);
    n = BES32(list->n);
    for (i = 0; i < n; i++) {
      a = BEPTR(BrAnim *, list->anim[i]);
      SET16(a->flags, BE16(a->flags) | on);
      SET16(a->flags, BE16(a->flags) & off);
    }
  }
}

/* WHAT IT DOES: Play every animation in the set through once and stop at
 * the end: clears both the repeat and the bounce-back bits. */
/* @implements 0x8021D7E0 tgr BrAnimSetOnce */
void BrAnimSetOnce(BrAnimSet *set)
{
  BrAnimSetFlags(set,0,3);
}

/* WHAT IT DOES: Play every animation in the set on repeat, restarting from
 * the beginning each time round: sets repeat, clears bounce-back. */
/* @implements 0x8021D804 tgr BrAnimSetLoop */
void BrAnimSetLoop(BrAnimSet *set)
{
  BrAnimSetFlags(set,1,2);
}

/* WHAT IT DOES: Play every animation in the set back and forth: sets both
 * the repeat and the bounce-back bits. */
/* @implements 0x8021D828 tgr BrAnimSetPingPong */
void BrAnimSetPingPong(BrAnimSet *set)
{
  BrAnimSetFlags(set,3,0);
}

/* WHAT IT DOES: Advance every animation in the set by the frame time and
 * morph its vertices: find the two keys either side of the time and blend
 * their positions and colours (12-bit fraction).  Before the start the
 * first key is shown; past the end the last key, unless repeating, when the
 * time wraps back (or, bouncing, turns round and runs backwards to the
 * start).  Written in the ROM's block order: the search and blend sit inside
 * the backwards branch; the time doubles as the blend fraction; `* 2` is an
 * integer so IDO keeps the multiply (a float 2.0f becomes x + x).
 * One key index serves the search and the vertex loop, and the vertex loop
 * runs to the animation's vertex count read each pass; with the entry count
 * held in a local this gives the ROM's 1000 bytes.
 * RESIDUE (246): the ROM's frame is 0x28 to our 0x20, which moves every
 * spill slot. */
/* @t3 0x8021D84C */
/* @t4-pass 0x8021D84C 1 2026-10-04 compiles 121 best 246 moved 0  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x8021D84C 2 2026-10-04 compiles 120 best 246 moved 0  (tools/tgrally/n64permute.py) */
/* @implements 0x8021D84C tgr BrAnimUpdate */
void BrAnimUpdate(BrAnimSet *set)
{
  int i;
  int n;
  BrAnimList *list;
  BrAnim *a;
  float t;
  float len;
  float step;
  float lim;
  int k;
  unsigned char *ka;
  unsigned char *kb;
  unsigned char *pa;
  unsigned char *pb;
  signed char *ca;
  signed char *cb;
  unsigned char *out;
  int f;

  if (BE32(set->anims) != 0) {
    list = BEPTR(BrAnimList *, set->anims);
    n = BES32(list->n);
    for (i = 0; i < n; i++) {
      a = BEPTR(BrAnim *, list->anim[i]);
      if (BE16(a->flags) & 4) {
        SETF(a->time, BEF(a->time) - D_8028AAD8);
        t = BEF(a->time);
        if (!(BEF(a->start) <= t)) {
          goto back_to_start;
        }
        if (!(t < BEF(a->end))) {
          continue;
        }
      search:
        for (k = BE16(a->key); k < BES32(a->nKeys); k++) {
          if (t < tgr_rdf(BEPTR(unsigned char *, a->keys[k]))) {
            break;
          }
        }
        kb = BEPTR(unsigned char *, a->keys[k]);
        k--;
        ka = BEPTR(unsigned char *, a->keys[k]);
        t = (t - tgr_rdf(ka)) / (tgr_rdf(kb) - tgr_rdf(ka));
        k = 0;
      blend:
        f = t * 4096.0f;
        pa = ka + 4;
        pb = kb + 4;
        ca = (signed char *)(ka + 4 + BES32(a->n) * 6);
        cb = (signed char *)(kb + 4 + BES32(a->n) * 6);
        out = BEPTR(unsigned char *, a->out);
        for (; k != BES32(a->n); k++) {
          /* a Vtx: x, y, z big-endian at 0, 2, 4; colour bytes at 12 */
          tgr_wr16(out + 0, (short)tgr_rd16(pa + 0) + (((short)tgr_rd16(pb + 0) - (short)tgr_rd16(pa + 0)) * f >> 12));
          tgr_wr16(out + 2, (short)tgr_rd16(pa + 2) + (((short)tgr_rd16(pb + 2) - (short)tgr_rd16(pa + 2)) * f >> 12));
          tgr_wr16(out + 4, (short)tgr_rd16(pa + 4) + (((short)tgr_rd16(pb + 4) - (short)tgr_rd16(pa + 4)) * f >> 12));
          ((signed char *)out)[12] = ca[0] + ((cb[0] - ca[0]) * f >> 12);
          ((signed char *)out)[13] = ca[1] + ((cb[1] - ca[1]) * f >> 12);
          ((signed char *)out)[14] = ca[2] + ((cb[2] - ca[2]) * f >> 12);
          pa += 6;
          pb += 6;
          ca += 3;
          cb += 3;
          out += 16;
        }
        continue;
      back_to_start:
        if (!(BE16(a->flags) & 1)) {
          continue;
        }
        SET16(a->flags, BE16(a->flags) & ~4);
        SET16(a->key, 0);
        t = BEF(a->start) * 2 - t;
        SETF(a->time, t);
        goto search;
      } else {
        SETF(a->time, BEF(a->time) + D_8028AAD8);
        t = BEF(a->time);
        if (t < BEF(a->start)) {
          t = 0.0f;
          ka = kb = BEPTR(unsigned char *, a->keys[0]);
          k = 0;
          goto blend;
        }
        if (t < BEF(a->end)) {
          if (BEF(a->start) <= t) {
            goto search;
          }
          continue;
        }
        if (BE16(a->flags) & 1) {
          len = BEF(a->end) - BEF(a->start);
          step = len;
          if (BE16(a->flags) & 2) {
            step *= 2;
            lim = BEF(a->end) + step;
            while (lim < t) {
              t -= step;
            }
            step *= 0.5f;
            lim -= step;
            if (lim < t) {
              goto wrap;
            }
            SET16(a->flags, BE16(a->flags) | 4);
            t = BEF(a->end) * 2 - t;
            SETF(a->time, t);
          } else {
            lim = BEF(a->end) + step;
          wrap:
            while (lim < t) {
              t -= step;
            }
            t -= len;
            SETF(a->time, t);
          }
          SET16(a->key, 0);
          goto search;
        } else {
          t = 0.0f;
          ka = kb = BEPTR(unsigned char *, a->keys[BES32(a->nKeys) - 1]);
          k = 0;
          goto blend;
        }
      }
    }
  }
}


/* WHAT IT DOES: Turn the offsets stored in a model just loaded from ROM
 * into real addresses: its part table, each part's geometry and each part's
 * display list. */
/* @implements 0x8021DC34 tgr BrModelRebase */
void BrModelRebase(BrModel *m)
{
  int i;
  int j;
  unsigned int base = tgr_addr32(m);
  BrAnimList *list;
  BrAnim *a;

  if (BE32(m->anims) != 0) {
    BrDlRebaseWord(&m->anims, 0, 0x7fffffff, base);
    list = BEPTR(BrAnimList *, m->anims);
    for (i = 0; i < BES32(list->n); i++) {
      BrDlRebaseWord(&list->anim[i], 0, 0x7fffffff, base);
      a = BEPTR(BrAnim *, list->anim[i]);
      BrDlRebaseWord(&a->out, 0, 0x7fffffff, base);
      BrDlRebaseWord(&a->x8, 0, 0x7fffffff, base);
      for (j = 0; j < BES32(a->nKeys); j++) {
        BrDlRebaseWord(&a->keys[j], 0, 0x7fffffff, base);
      }
    }
  }
  for (i = 0; i < BE32(m->nDl); i++) {
    BrDlRebaseWord(&m->dls[i].dl, 0, 0x7fffffff, base);
    if (BE32(m->dls[i].dl) != 0) {
      BrDlRebaseWord(&m->dls[i].dl, 0, 0x7fffffff, base);
      BrDlRebase(BEPTR(Gfx *, m->dls[i].dl), 0, 0x7fffffff, base);
    }
  }
}

/* WHAT IT DOES: Copy an unpacked model straight out of ROM, from start to
 * end, into buf and turn its stored offsets into real addresses. Returns
 * buf. */
/* @implements 0x8021DDFC tgr BrModelReadRaw */
void *BrModelReadRaw(void *buf, int rom, int end)
{
  BrRomRead(buf, rom, end - rom);
  BrModelRebase((BrModel *)buf);
  return buf;
}

/* WHAT IT DOES: Unpack a packed model from ROM into buf and turn its stored
 * offsets into real addresses. Returns buf. */
/* @implements 0x8021DE2C tgr BrModelLoad */
void *BrModelLoad(void *buf, int rom)
{
  BrRomUnpack((unsigned char *)buf, rom, 0);
  BrModelRebase((BrModel *)buf);
  return buf;
}

/* WHAT IT DOES: Load a track: read its packed size (fatal if over 0x171400),
 * unpack it to 0x80025C00, load the two 0x20-byte headed blocks after it,
 * note its ROM offset and texture base, reset the texture state, then give
 * every object its scale (1 / the length of its rotated x axis), flagging
 * the ones whose matrix is unscaled; fatal on too many objects or the wrong
 * header size.  Runs the scene setup pass. */
/* @implements 0x8021DE5C tgr BrTrackLoad */
void BrTrackLoad(int track)
{
  char buf[80];
  float v[3];
  int n;
  float len;
  char buf2[84];
  char buf3[80];
  float m[4][4];
  BrTrackObj *objs;

  SET32(D_80025C00.x00, BrRomReadSize(D_80270854[track].x14));
  osInvalDCache(&D_80025C00, 4);
  if (BE32(D_80025C00.x00) > 0x171400) {
    sprintf(buf, "Track %d too big (%d vs. %d)", track, BE32(D_80025C00.x00), 0x171400);
    BrFatal(buf);
  } else {
    osSyncPrintf("Loading track %d (%d / %d)\n", track, BE32(D_80025C00.x00), 0x171400);
  }
  BrRomUnpack((unsigned char *)&D_80025C00, D_80270854[track].x14, 0);
  osSyncPrintf("hmm = %04x %04x\n", tgr_rd16(TGR_PTR(void *, 0x80029560)), tgr_rd16(TGR_PTR(void *, 0x80029568)));
  osInvalDCache(&D_80025C00, BE32(D_80025C00.x00));
  BrRomRead(D_80373EB0, D_80270854[track].x1c, 0x20);
  BrRomUnpack((unsigned char *)D_803736B0, D_80270854[track].x1c + 0x20, 0);
  BrRomRead(D_803746D0, D_80270854[track].x24, 0x20);
  BrRomUnpack((unsigned char *)D_80373ED0, D_80270854[track].x24 + 0x20, 0);
  D_8028A87C = D_80270854[track].x14;
  D_8028A880 = D_80270854[track].x14 + BrRomReadWord(D_80270854[track].x14);
  D_8028AA7C = -1;
  D_8028AA88 = -1;
  n = 0;
  objs = BR_TRACKOBJS();
  for (track = 0; track < BES32(D_80025C00.nObjs); track++) {
    v[0] = 1.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    br_mat4_from(m, (const bef_t (*)[4])objs[track].m);
    BrMat4RotateDir(v, v, m);
    len = BrVec3Length(v);
    if (len != 0) {
      len = 1.0f / len;
      if (1.0f == m[0][0] * len && 1.0f == m[1][1] * len && 1.0f == m[2][2] * len) {
        n++;
        SET16(objs[track].flags, BE16(objs[track].flags) | 0x2000);
      }
      SETF(objs[track].scale, len);
    }
  }
  osSyncPrintf("Scalars: %d/%d\n", n, BES32(D_80025C00.nObjs));
  if (BES32(D_80025C00.nObjs) > 0x800) {
    sprintf(buf2, "ERROR: instances (%d) > MAX_INSTANCES (%d)", BES32(D_80025C00.nObjs), 0x800);
    BrFatal(buf2);
  }
  if (BES32(D_80025C00.hdrSize) != 0x230) {
    sprintf(buf3, "ERROR: Track header size mismatch! (%d!=%d)", BES32(D_80025C00.hdrSize), 0x230);
    BrFatal(buf3);
  }
  BrScenePassRun();
}


/* WHAT IT DOES: Tell whether track number n exists in this build's track
 * table (below the track count and with a record present). */
/* @implements 0x8021E180 tgr BrTrackIsPresent */
int BrTrackIsPresent(int n)
{
  return n < D_8028AE04 && D_80270854[n].present != 0;
}

/* WHAT IT DOES: Wait for the next vertical retrace: block until the video
 * interrupt posts its message. */
/* @implements 0x8021E1C0 tgr BrWaitRetrace */
void BrWaitRetrace(void)
{
  osRecvMesg(&D_8031A390,0,1);
}

/* WHAT IT DOES: The end of the line after a fatal error: takes the error
 * code and returns straight away in the retail build (its body was compiled
 * out). */
/* @implements 0x8021E1EC tgr BrHaltLoop */
void BrHaltLoop(int arg0)
{
}

/* WHAT IT DOES: Stop the game with an error message: store the message for
 * the error screen and hand over to the halt loop. Used for out-of-memory
 * and overflow checks. */
/* @implements 0x8021E1F4 tgr BrFatal */
void BrFatal(char *msg)
{
  D_8028A88C = msg;
  BrHaltLoop(0);
}

/* WHAT IT DOES: The fault thread: wait for the CPU-fault event, report it
 * on the debug output, and hand the faulted thread to the halt loop.
 * Never returns. */
/* @implements 0x8021E21C tgr BrFaultThread */
void BrFaultThread(void *arg)
{
  extern TgrAddr D_8031B1CC;     /* 0x8031B1CC: the faulted thread */
  extern int D_8031B1D0;
  OSMesg msg;

  osSetEventMesg(12, &D_8031B1B0, (OSMesg)16);
  D_8031B1D0 = 0;
  for (;;) {
    do {
      osRecvMesg(&D_8031B1B0, &msg, 1);
      osSyncPrintf("\n=> faultproc - got a fault message...\n");
      D_8031B1CC = __osGetCurrFaultedThread();
    } while (D_8031B1CC == 0);
    BrHaltLoop(1);
  }
}

/* WHAT IT DOES: The idle thread: read the argument string from the
 * cartridge (0xFFB000) and split it into argc/argv, start the video and PI
 * managers (MPAL or NTSC, 320x240), start the fault thread, paint the
 * thread stacks with a marker pattern, start the main game thread and drop
 * to the lowest priority for good.
 * The first two stack fills walk a pointer alongside the counter (which
 * keeps IDO from unrolling them) and the third indexes the array (which it
 * unrolls by four), as the ROM has them. */
/* @t4-pass 0x8021E2C8 1 2026-10-03 compiles 119 best 34 moved 0  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x8021E2C8 2 2026-10-03 compiles 117 best 34 moved 0  (tools/tgrally/n64permute.py) */
/* @implements 0x8021E2C8 tgr BrIdleThread */
void BrIdleThread(void *arg)
{
  extern BrArgs D_8028B2EC;   /* 0x8028B2EC; argv: D_80316450 */
  char *p;
  unsigned int i;
  unsigned int buf[16];
  unsigned long long *s;

  for (i = 0; i < 16; i++) {
    osPiReadIo(0xFFB000 + i * 4, &buf[i]);
    buf[i] = tgr_be32(buf[i]);          /* the words' bytes in ROM order: a string */
  }
  p = (char *)buf;
  while (*p != 0) {
    while (*p != 0 && *p == ' ') {
      *p++ = 0;
    }
    if (*p != 0) {
      D_80316450[D_8028B2EC.argc] = p;
      D_8028B2EC.argc++;
    }
    while (*p != 0 && *p != ' ') {
      p++;
    }
  }
  D_8026FF04 = TGR_PTR(char **, D_8028B2EC.argv);
  D_8026FF00 = &D_8028B2EC;
  osCreateViManager(0xfe);
  D_8028AAB0 = 320;
  D_8028AAB4 = 240;
  if (osTvType == 2) {
    osViSetMode(&D_802A5D70);
  } else {
    osViSetMode(&D_802A54B0);
  }
  osViBlack(1);
  osCreatePiManager(150, &D_80319ED0, D_80319EE8, 32);
  osCreateMesgQueue(&D_8031B1B0, D_8031B1C8, 1);
  osCreateThread(&D_8031AC00, 5, BrFaultThread, TGR_PTR(char **, D_8028B2EC.argv), D_8031ADB0 + 0x80, 50);
  osStartThread(&D_8031AC00);
  for (i = 0, s = D_80316CD0; i < 0x400; i++, s++) {
    *s = 0x5015A1DBFED15C00ULL;
  }
  for (i = 0, s = D_80318ED0; i < 0x100; i++, s++) {
    *s = 0x5015A1DBFED15C00ULL;
  }
  for (i = 0; i < 0x100; i++) {
    D_803196D0[i] = 0x5015A1DBFED15C00ULL;
  }
  osCreateThread(&D_80272830, 3, BrMainThread, arg, D_80318CD0, 10);
  osStartThread(&D_80272830);
  osSetThreadPri(0, 0);
  for (;;) {
  }
}

/* WHAT IT DOES: The game's entry point after the boot stub clears memory:
 * initialise the N64 operating system, then create and start the idle
 * thread that brings everything else up. */
/* @implements 0x8021E5C4 tgr BrBoot */
void BrBoot(void)
{
  osInitialize();
  osCreateThread(&D_80272680,1,BrIdleThread,0,D_80316CD0,10);
  osStartThread(&D_80272680);
}
