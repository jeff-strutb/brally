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

typedef struct BrIoMesg { int words[6]; } BrIoMesg;   /* an OSIoMesg */
void func_8021C748(int param_1,int param_2,int param_3);
extern int D_8031B328;
extern int D_8031B330;
int func_8021DC34();
typedef struct BrUnpack {      /* a streamed unpack in flight (BrStreamInit starts one) */
  unsigned int pos;             /* 0x00  ROM position of the next chunk */
  unsigned char *dst;           /* 0x04  0 until the first call */
  unsigned char *buf;           /* 0x08  2 x 16000-byte chunk buffer */
  unsigned int half;            /* 0x0C  which half the next chunk goes to */
  unsigned int total;           /* 0x10  packed length */
  unsigned int size;            /* 0x14  unpacked length */
  unsigned int left;            /* 0x18  packed bytes still to inflate */
  unsigned int len;             /* 0x1C  length of the chunk in flight */
} BrUnpack;
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, BrUnpack *s);
int BrInflate(unsigned char *dst, int *dstLen, unsigned char *src, unsigned int srcLen);
unsigned int osGetCount(void);
extern unsigned long long osClockRate;
extern unsigned char D_802F7F00[];
extern void *D_80368AC0;
extern void *D_80368AC4;
extern char D_80324550[];
extern char D_8033CBF0[];
BrIoMesg *BrRomDmaSlot(void);
void func_8021735C(void);
int osPiStartDma(void *mb, int pri, int dir, unsigned int devAddr, void *vAddr, unsigned int n, void *mq);
void osInvalDCache(void *p, int n);
extern int D_80319F88;
int osRecvMesg(void *mq, void *msg, int flag);
extern int D_80272D44;
extern int D_80272D40;
void func_8021D070();
void BrDlRebase(unsigned int *dl, unsigned int lo, unsigned int hi, int base);
extern int D_8021DC94;
extern int D_8021DC98;
extern BrIoMesg D_8031A020[32];
typedef struct BrModelPart {
  int x0;
  void *a;                      /* 0x04 */
  void *b;                      /* 0x08 */
  int n;                        /* 0x0C */
  char pad10[0x10];
  void *v[1];                   /* 0x20  n of them */
} BrModelPart;
typedef struct BrModelParts {
  int count;
  BrModelPart *part[1];         /* count of them */
} BrModelParts;
typedef struct BrModelDl {      /* 0x14 bytes */
  unsigned int *dl;
  int pad[4];
} BrModelDl;
typedef struct BrModel {
  unsigned int nDl;             /* 0x00 */
  BrModelParts *parts;          /* 0x04 */
  BrModelDl dls[1];             /* 0x08  nDl of them */
} BrModel;
void *memcpy(void *d, const void *s, int n);

#include "tgr/car.h"

void func_8021D140(int param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80222D54(int param_1);
void BrCarModelInstall(int slot, int car, void *src);
unsigned int BrRomReadSize(int rom);
void osSyncPrintf(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
void BrFatal(char *msg);
void BrEntRebaseModel(BrCarModel *m);
typedef struct BrStream {       /* a streamed ROM read in flight */
  char pad00[0x20];
  int slot;                     /* 0x20  model slot it fills */
  int car;                      /* 0x24  car it loads */
} BrStream;
void BrStreamInit(BrStream *s, char *buf);
void func_802203F0(int param_1,int param_2);
void func_8021D098();
void func_80220398();
unsigned int BrRomUnpack();
void func_8021D32C(int param_1);
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern unsigned char D_8028B904[][3];
void BrCarResetFrames(BrCar *car);
extern char D_803D5F88[];

#include "tgr/gbi.h"

int BrCarModelPresent(int param_1);
extern int D_8020C688;
extern int D_8020C68C;
extern int D_80272070;
extern char *D_8031C5BC;
extern char D_8028AE24;
extern Gfx *D_8028A858;
int sprintf(char *buf, char *fmt, ...);

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

#include "tgr/menu.h"

int BrRomReadWord(int rom);
unsigned int BrRomUnpack();
typedef struct BrLoadObj {      /* a track object (0x54 bytes) */
  float m[4][4];                /* its matrix */
  float scale;                  /* 0x40  1 / the length of its x axis */
  char pad44[0x4C - 0x44];
  unsigned short flags;         /* 0x4C  0x2000: unscaled */
  char pad4e[0x54 - 0x4E];
} BrLoadObj;
typedef struct BrLoadHdr {      /* the loaded track (packed size, then the header) */
  unsigned int size;
  int hdrSize;                  /* 0x04  0x230 */
  char pad08[0x60 - 0x08];
  BrLoadObj *objs;              /* 0x60 */
  int nObjs;                    /* 0x64 */
} BrLoadHdr;
extern BrLoadHdr D_80025C00;
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
void BrRomRead();

int BrTrackIsPresent(int n);
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern int D_80315EE0;

void BrIdleThread(void *arg);
int osCreateThread();
void osStartThread(int param_1);
void osInitialize(void);
extern int D_80272680;
void BrHaltLoop();
extern int D_8028A88C;
extern int D_8031A390;
void osSetEventMesg(int event, int *mq, int msg);
int __osGetCurrFaultedThread(void);
extern int D_8031B1B0[6];
int osPiReadIo(unsigned int devAddr, unsigned int *data);
void osCreateViManager(int pri);
void osViSetMode(void *mode);
void osViBlack(int active);
void osCreatePiManager(int pri, int *mq, int *msgs, int count);
void osCreateMesgQueue(int *mq, int *msgs, int count);
void osSetThreadPri(void *t, int pri);
void BrMainThread(void *arg);
typedef struct BrArgs { int argc; char **argv; } BrArgs;
extern char *D_80316450[];           /* argv */
extern BrArgs *D_8026FF00;
extern char **D_8026FF04;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern char D_802A5D70[];            /* MPAL video mode */
extern char D_802A54B0[];            /* NTSC video mode */
extern int D_80319ED0[];
extern int D_80319EE8[];
extern int D_8031B1C8[];
extern char D_8031AC00[];
extern char D_80272830[];
extern unsigned long long D_80316CD0[0x400];
extern int osTvType;
extern unsigned long long D_8031ADB0[0x80];   /* the fault thread's stack */
extern unsigned long long D_80318CD0[];
extern unsigned long long D_80318ED0[0x100];
extern unsigned long long D_803196D0[0x100];
/* -- end declarations -- */

/* WHAT IT DOES: Correct one address inside data just loaded from ROM: if it
 * points into the old block [lo, hi) it is moved to the same offset from
 * the new base, otherwise it is left alone. */
/* @implements 0x8021D070 tgr BrDlRebaseWord */
void BrDlRebaseWord(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4)

{
  unsigned int uVar1;
  
  uVar1 = *param_1;
  if ((param_2 <= uVar1) && (uVar1 < param_3)) {
    *param_1 = (uVar1 - param_2) + param_4;
  }
}


/* WHAT IT DOES: Walk a display list loaded from ROM and correct every
 * address in it that pointed into the old block (vertex and texture-image
 * commands) so it points at the new copy; stops at the end of the list. */
/* @implements 0x8021D098 tgr BrDlRebase */
void BrDlRebase(unsigned int *dl, unsigned int lo, unsigned int hi, int base)
{
  if (dl == 0) {
    return;
  }
  for (;;) {
    switch ((unsigned char)(dl[0] >> 24)) {
    case 0x04:
    case 0xfd:
      func_8021D070(dl + 1, lo, hi, base);
      break;
    case 0xb8:
      return;
    }
    dl += 2;
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
  unsigned short *pal;

  for (i = 0; i < 12; i += 2) {
    parts = m->parts;
    p = &parts[m->decalPart[i]];
    pal = p->b;
    if (pal != 0 && (p->fmt & 0xf) == 1) {
      pal[0] = (pal[i] & 1) | (r << 11) | (g << 6) | (b << 1);
      pal[1] = (pal[i] & 1) | ((r & 0x1e) << 10) | ((g & 0x1e) << 5) | (b & 0x1e);
    }
    p2 = &m->parts[m->decalPart[i + 1]];
    pal = p2->b;
    if (pal != 0 && (p2->fmt & 0xf) == 1) {
      pal[0] = (pal[i + 1] & 1) | (r << 11) | (g << 6) | (b << 1);
      pal[1] = (pal[i + 1] & 1) | ((r & 0x1e) << 10) | ((g & 0x1e) << 5) | (b & 0x1e);
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
  BrCarModelPart *p = m->decalPart[2] + m->parts;
  int r;
  int g;
  int b;

  if (p->b != 0 && (p->fmt & 0xf) == 1) {
    r = ((p->b[0] >> 8) & 0xf8) | ((p->b[0] >> 13) & 7);
    g = ((p->b[0] >> 3) & 0xf8) | ((p->b[0] >> 8) & 7);
    b = ((p->b[0] << 2) & 0xf8) | ((p->b[0] >> 3) & 7);
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

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 10; j++) {
      func_8021D070((unsigned int *)&m->dl[i][j], D_803C8000, D_803D5F88, (int)m);
      func_8021D098(m->dl[i][j], D_803C8000, D_803D5F88, (int)m);
    }
    for (j = 0; j < 3; j++) {
      if (m->dl2[i][j] != 0) {
        func_8021D070((unsigned int *)&m->dl2[i][j], D_803C8000, D_803D5F88, (int)m);
        func_8021D098(m->dl2[i][j], D_803C8000, D_803D5F88, (int)m);
      }
    }
  }
  func_8021D070((unsigned int *)&m->parts, D_803C8000, D_803D5F88, (int)m);
  func_8021D070((unsigned int *)&m->x90, D_803C8000, D_803D5F88, (int)m);
  func_8021D070((unsigned int *)&m->x94, D_803C8000, D_803D5F88, (int)m);
  for (i = 0; i < m->nParts; i++) {
    func_8021D070((unsigned int *)&m->parts[i].a, D_803C8000, D_803D5F88, (int)m);
    func_8021D070((unsigned int *)&m->parts[i].b, D_803C8000, D_803D5F88, (int)m);
  }
  func_8021D070((unsigned int *)&m->x11c, D_803C8000, D_803D5F88, (int)m);
  for (i = 0; i < 12; i++) {
    func_8021D070((unsigned int *)&m->x11c[i], D_803C8000, D_803D5F88, (int)m);
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
  return *(int *)(&D_8028AE24 + param_1 * 0x60) != 0;
}

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
        k--;
        ka = a->keys[k];
        t = (t - *(float *)ka) / (*(float *)kb - *(float *)ka);
        k = 0;
      blend:
        f = t * 4096.0f;
        pa = (short *)(ka + 4);
        pb = (short *)(kb + 4);
        ca = (signed char *)(ka + 4 + a->n * 6);
        cb = (signed char *)(kb + 4 + a->n * 6);
        out = a->out;
        for (; k != a->n; k++) {
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
          k = 0;
          goto blend;
        }
        if (t < a->end) {
          if (a->start <= t) {
            goto search;
          }
          continue;
        }
        if (a->flags & 1) {
          len = a->end - a->start;
          step = len;
          if (a->flags & 2) {
            step *= 2;
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
        } else {
          t = 0.0f;
          ka = kb = a->keys[a->nKeys - 1];
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

  if (m->parts != 0) {
    func_8021D070((unsigned int *)&m->parts, 0U, 0x7fffffffU, (int)m);
    for (i = 0; i < m->parts->count; i++) {
      func_8021D070((unsigned int *)&m->parts->part[i], 0U, 0x7fffffffU, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->a, 0U, 0x7fffffffU, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->b, 0U, 0x7fffffffU, (int)m);
      for (j = 0; j < m->parts->part[i]->n; j++) {
        func_8021D070((unsigned int *)&m->parts->part[i]->v[j], 0U, 0x7fffffffU, (int)m);
      }
    }
  }
  for (i = 0; i < m->nDl; i++) {
    func_8021D070((unsigned int *)&m->dls[i].dl, 0U, 0x7fffffffU, (int)m);
    if (m->dls[i].dl != 0) {
      func_8021D070((unsigned int *)&m->dls[i].dl, 0U, 0x7fffffffU, (int)m);
      BrDlRebase(m->dls[i].dl, 0U, 0x7fffffffU, (int)m);
    }
  }
}

/* WHAT IT DOES: Copy an unpacked model straight out of ROM, from start to
 * end, into buf and turn its stored offsets into real addresses. Returns
 * buf. */
/* @implements 0x8021DDFC tgr BrModelReadRaw */
int BrModelReadRaw(int param_1,int param_2,int param_3)

{
  func_8021C748(param_1,param_2,param_3 - param_2);
  func_8021DC34(param_1);
  return param_1;
}

/* WHAT IT DOES: Unpack a packed model from ROM into buf and turn its stored
 * offsets into real addresses. Returns buf. */
/* @implements 0x8021DE2C tgr BrModelLoad */
int BrModelLoad(int param_1,int param_2)

{
  BrRomUnpack(param_1,param_2,0);
  func_8021DC34(param_1);
  return param_1;
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

  D_80025C00.size = BrRomReadSize(D_80270854[track].x14);
  osInvalDCache(&D_80025C00, 4);
  if (D_80025C00.size > 0x171400) {
    sprintf(buf, "Track %d too big (%d vs. %d)", track, D_80025C00.size, 0x171400);
    BrFatal(buf);
  } else {
    osSyncPrintf("Loading track %d (%d / %d)\n", track, D_80025C00.size, 0x171400);
  }
  BrRomUnpack((void *)0x80025C00, D_80270854[track].x14, 0);
  osSyncPrintf("hmm = %04x %04x\n", *(unsigned short *)0x80029560, *(unsigned short *)0x80029568);
  osInvalDCache((void *)0x80025C00, D_80025C00.size);
  BrRomRead(D_80373EB0, D_80270854[track].x1c, 0x20);
  BrRomUnpack(D_803736B0, D_80270854[track].x1c + 0x20, 0);
  BrRomRead(D_803746D0, D_80270854[track].x24, 0x20);
  BrRomUnpack(D_80373ED0, D_80270854[track].x24 + 0x20, 0);
  D_8028A87C = D_80270854[track].x14;
  D_8028A880 = D_80270854[track].x14 + BrRomReadWord(D_80270854[track].x14);
  D_8028AA7C = -1;
  D_8028AA88 = -1;
  n = 0;
  for (track = 0; track < D_80025C00.nObjs; track++) {
    v[0] = 1.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    BrMat4RotateDir(v, v, D_80025C00.objs[track].m);
    len = BrVec3Length(v);
    if (len != 0) {
      len = 1.0f / len;
      if (1.0f == D_80025C00.objs[track].m[0][0] * len && 1.0f == D_80025C00.objs[track].m[1][1] * len &&
          1.0f == D_80025C00.objs[track].m[2][2] * len) {
        n++;
        D_80025C00.objs[track].flags |= 0x2000;
      }
      D_80025C00.objs[track].scale = len;
    }
  }
  osSyncPrintf("Scalars: %d/%d\n", n, D_80025C00.nObjs);
  if (D_80025C00.nObjs > 0x800) {
    sprintf(buf2, "ERROR: instances (%d) > MAX_INSTANCES (%d)", D_80025C00.nObjs, 0x800);
    BrFatal(buf2);
  }
  if (D_80025C00.hdrSize != 0x230) {
    sprintf(buf3, "ERROR: Track header size mismatch! (%d!=%d)", D_80025C00.hdrSize, 0x230);
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
  osRecvMesg((&D_8031A390),0,1);
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
  D_8028A88C = (int)msg;
  BrHaltLoop(0);
}

/* WHAT IT DOES: The fault thread: wait for the CPU-fault event, report it
 * on the debug output, and hand the faulted thread to the halt loop.
 * Never returns. */
/* @implements 0x8021E21C tgr BrFaultThread */
void BrFaultThread(void *arg)
{
  static int faulted;         /* 0x8031B1CC: the faulted thread */
  static int x8031B1D0;
  int msg;

  osSetEventMesg(12, D_8031B1B0, 16);
  x8031B1D0 = 0;
  for (;;) {
    do {
      osRecvMesg(D_8031B1B0, &msg, 1);
      osSyncPrintf("\n=> faultproc - got a fault message...\n");
      faulted = __osGetCurrFaultedThread();
    } while (faulted == 0);
    BrHaltLoop(faulted);
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
  static BrArgs args = { 1, D_80316450 };   /* 0x8028B2EC */
  char *p;
  unsigned int i;
  unsigned int buf[16];
  unsigned long long *s;

  for (i = 0; i < 16; i++) {
    osPiReadIo(0xFFB000 + i * 4, &buf[i]);
  }
  p = (char *)buf;
  while (*p != 0) {
    while (*p != 0 && *p == ' ') {
      *p++ = 0;
    }
    if (*p != 0) {
      D_80316450[args.argc] = p;
      args.argc++;
    }
    while (*p != 0 && *p != ' ') {
      p++;
    }
  }
  D_8026FF04 = args.argv;
  D_8026FF00 = &args;
  osCreateViManager(0xfe);
  D_8028AAB0 = 320;
  D_8028AAB4 = 240;
  if (osTvType == 2) {
    osViSetMode(D_802A5D70);
  } else {
    osViSetMode(D_802A54B0);
  }
  osViBlack(1);
  osCreatePiManager(150, D_80319ED0, D_80319EE8, 32);
  osCreateMesgQueue(D_8031B1B0, D_8031B1C8, 1);
  osCreateThread(D_8031AC00, 5, BrFaultThread, args.argv, D_8031ADB0 + 0x80, 50);
  osStartThread(D_8031AC00);
  for (i = 0, s = D_80316CD0; i < 0x400; i++, s++) {
    *s = 0x5015A1DBFED15C00ULL;
  }
  for (i = 0, s = D_80318ED0; i < 0x100; i++, s++) {
    *s = 0x5015A1DBFED15C00ULL;
  }
  for (i = 0; i < 0x100; i++) {
    D_803196D0[i] = 0x5015A1DBFED15C00ULL;
  }
  osCreateThread(D_80272830, 3, BrMainThread, arg, D_80318CD0, 10);
  osStartThread(D_80272830);
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
