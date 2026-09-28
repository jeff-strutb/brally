/* romread.c -- reading words and packed assets out of cartridge ROM, and fixing up models loaded from it
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
int osRecvMesg(int param_1,int *param_2,int param_3);
extern int D_80272D44;
extern int D_80272D40;
void func_8021D070(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
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
/* -- end declarations -- */

/* WHAT IT DOES: Read the unpacked size of a packed asset in ROM: the second
 * word of its header. Callers check it against the buffer they allocated. */
/* @implements 0x8021C814 tgr BrRomReadSize */
int BrRomReadSize(int param_1)

{
  func_8021C748(&D_8031B328,param_1 + 4,4);
  return D_8031B328;
}

/* WHAT IT DOES: Read one 32-bit word from cartridge ROM. */
/* @implements 0x8021C848 tgr BrRomReadWord */
int BrRomReadWord(int param_1)

{
  func_8021C748(&D_8031B330,param_1,4);
  return D_8031B330;
}

/* WHAT IT DOES: Unpack n run-length-coded byte planes into dst, plane k
 * going to every n-th byte from dst + k.  Each plane is a 4-byte length and
 * then runs: a negative count -c copies the next c bytes, a count c >= 0
 * repeats the next byte c + 3 times.  Answers the last plane's output
 * offset; max is not checked. */
/* @implements 0x8021CB4C tgr BrRlePlanesUnpack */
int BrRlePlanesUnpack(unsigned char *dst, int max, signed char *data, int n)
{
  int out;
  int in;
  int i;
  int end;
  int c;
  int count;
  int v;
  int len;
  signed char *src;

  out = 0;
  in = 0;
  i = 0;
  if (n > 0) {
    src = data;
    do {
      out = 0;
      memcpy(&len, src, 4);
      in += 4;
      i++;
      src += 4;
      end = in + len;
      while (in < end) {
        c = *src;
        in++;
        src++;
        if (c < 0) {
          for (count = -c; count != 0; count--) {
            dst[out] = *src;
            out += n;
            in++;
            src++;
          }
        } else {
          count = c + 3;
          c = *src;
          in++;
          src++;
          for (; count != 0; count--) {
            dst[out] = c;
            out += n;
          }
        }
      }
      dst++;
    } while (i != n);
  }
  return out;
}

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

/* WHAT IT DOES: Copy len bytes from cartridge ROM into RAM: invalidate the
 * data cache over the destination, then DMA it across in 4 KB pieces, each
 * queued on the ROM message queue. */
/* @implements 0x8021C748 tgr BrRomRead */
void BrRomRead(int param_1,int param_2,int param_3)
{
  int uVar1;
  
  osInvalDCache(param_1,param_3);
  for (; 0x1000 < param_3; param_3 = param_3 + -0x1000) {
    uVar1 = BrRomDmaSlot();
    osPiStartDma(uVar1,0,0,param_2,param_1,0x1000,(&D_80319F88));
    param_2 = param_2 + 0x1000;
    param_1 = param_1 + 0x1000;
  }
  uVar1 = BrRomDmaSlot();
  osPiStartDma(uVar1,0,0,param_2,param_1,param_3,(&D_80319F88));
  func_8021735C();
}

/* WHAT IT DOES: Wait until every ROM transfer still outstanding has
 * finished, taking one completion message off the ROM queue for each. */
/* @implements 0x8021735C tgr BrRomWaitAll */
void BrRomWaitAll(void)
{
  for (; D_80272D44 != 0; D_80272D44 = D_80272D44 + -1) {
    osRecvMesg((&D_80319F88),0,1);
  }
}

/* WHAT IT DOES: Reset a load stream: nothing consumed yet, and the given
 * source position. */
/* @implements 0x8021CD24 tgr BrStreamInit */
void BrStreamInit(int param_1,int param_2)
{
  *(int *)(param_1 + 4) = 0;
  *(int *)(param_1 + 8) = param_2;
}

/* WHAT IT DOES: Unpack a compressed ROM file (packed length, unpacked
 * length, then 2-aligned chunks each led by its length) into dst: the next
 * chunk is DMA'd into one half of a 32000-byte buffer while the previous is
 * inflated from the other.  Without a stream it runs to the end; with one
 * it does one chunk per call, keeping its place in the stream.  Returns the
 * unpacked length. */
/* @implements 0x8021CD30 tgr BrRomUnpack */
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, BrUnpack *s)
{
  static unsigned int len;
  unsigned int total;
  unsigned int size;
  unsigned int left;
  unsigned char *buf;
  unsigned int half;
  int first;
  unsigned int prev;
  unsigned char *chunk;
  int out;
  unsigned int t0;
  unsigned long long us;

  if (s != 0 && (dst = s->dst) != 0) {
    rom = s->pos;
    total = s->total;
    left = s->left;
    size = s->size;
    buf = s->buf;
    len = s->len;
    half = s->half;
    first = 0;
  } else {
    BrRomRead(&len, rom, 4);
    total = len;
    rom += 4;
    BrRomRead(&len, rom, 4);
    size = len;
    rom += 4;
    left = total - 8;
    if (s != 0) {
      buf = s->buf;
    } else {
      buf = D_802F7F00;
    }
    half = 0;
    first = 1;
    len = 0;
  }
  t0 = osGetCount();
  do {
    prev = len;
    chunk = buf + half;
    if (((len + 5) & ~1) < left) {
      if (rom & 1) {
        rom++;
      }
      BrRomRead(&len, rom, 4);
      osInvalDCache(chunk, 16000);
      osPiStartDma(BrRomDmaSlot(), 0, 0, rom + 4, chunk, (len + 1) & ~1, &D_80319F88);
      rom += 4 + ((len + 1) & ~1);
    } else {
      BrRomWaitAll();
    }
    osInvalDCache(chunk, 16000);
    half ^= 16000;
    if (first) {
      first = 0;
    } else {
      out = 16000;
      if (s != 0) {
        D_80368AC0 = D_80324550;
        D_80368AC4 = D_8033CBF0;
      }
      BrInflate(dst, &out, buf + half, prev);
      if (s != 0) {
        D_80368AC4 = 0;
        D_80368AC0 = 0;
      }
      left -= (prev + 5) & ~1;
      dst += out;
      if (left == 0) {
        break;
      }
    }
  } while (s == 0);
  us = (unsigned long long)(osGetCount() - t0) * 1000000 / osClockRate;
  if (s != 0) {
    s->pos = rom;
    s->left = left;
    s->dst = dst;
    s->total = total;
    s->half = half;
    s->size = size;
    s->buf = buf;
    s->len = len;
  }
  return size;
}

/* WHAT IT DOES: Take the next of the 32 ROM transfer slots: if all are in
 * use, wait for one transfer to finish first; returns the slot's I/O
 * message block. */
/* @implements 0x802172D0 tgr BrRomDmaSlot */
BrIoMesg *BrRomDmaSlot(void)
{
  if (D_80272D44 == 32) {
    osRecvMesg(&D_80319F88, 0, 1);
  } else {
    D_80272D44++;
  }
  D_80272D40 = (D_80272D40 + 1) % 32;
  return &D_8031A020[D_80272D40];
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


/* WHAT IT DOES: Turn the offsets stored in a model just loaded from ROM
 * into real addresses: its part table, each part's geometry and each part's
 * display list. */
/* @implements 0x8021DC34 tgr BrModelRebase */
void BrModelRebase(BrModel *m)
{
  int i;
  int j;

  if (m->parts != 0) {
    func_8021D070((unsigned int *)&m->parts, 0, 0x7fffffff, (int)m);
    for (i = 0; i < m->parts->count; i++) {
      func_8021D070((unsigned int *)&m->parts->part[i], 0, 0x7fffffff, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->a, 0, 0x7fffffff, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->b, 0, 0x7fffffff, (int)m);
      for (j = 0; j < m->parts->part[i]->n; j++) {
        func_8021D070((unsigned int *)&m->parts->part[i]->v[j], 0, 0x7fffffff, (int)m);
      }
    }
  }
  for (i = 0; i < m->nDl; i++) {
    func_8021D070((unsigned int *)&m->dls[i].dl, 0, 0x7fffffff, (int)m);
    if (m->dls[i].dl != 0) {
      func_8021D070((unsigned int *)&m->dls[i].dl, 0, 0x7fffffff, (int)m);
      BrDlRebase(m->dls[i].dl, 0, 0x7fffffff, (int)m);
    }
  }
}

