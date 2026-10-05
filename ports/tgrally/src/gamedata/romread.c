/* romread.c -- reading words and packed assets out of cartridge ROM, and fixing up models loaded from it
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrRomRead(void *dst, unsigned int rom, int n);
extern int D_8031B328;
extern int D_8031B330;
void BrModelRebase(void *);
typedef struct BrUnpack {      /* a streamed unpack in flight (BrStreamInit starts one) */
  unsigned int pos;             /* 0x00  ROM position of the next chunk */
  TgrAddr dst;           /* unsigned char * -- 0x04  0 until the first call */
  TgrAddr buf;           /* unsigned char * -- 0x08  2 x 16000-byte chunk buffer */
  unsigned int half;            /* 0x0C  which half the next chunk goes to */
  unsigned int total;           /* 0x10  packed length */
  unsigned int size;            /* 0x14  unpacked length */
  unsigned int left;            /* 0x18  packed bytes still to inflate */
  unsigned int len;             /* 0x1C  length of the chunk in flight */
} BrUnpack;
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, BrUnpack *s);
int BrInflate(unsigned char *dst, int *dstLen, unsigned char *src, unsigned int srcLen);
extern unsigned long long osClockRate;
extern unsigned char D_802F7F00[];
extern void *D_80368AC0;
extern void *D_80368AC4;
extern char D_80324550[];
extern char D_8033CBF0[];
OSIoMesg *BrRomDmaSlot(void);
void BrRomWaitAll(void);
extern OSMesgQueue D_80319F88;     /* the ROM transfers' queue */
extern int D_80272D44;
extern int D_80272D40;
void BrDlRebaseWord(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void BrDlRebase(unsigned int *dl, unsigned int lo, unsigned int hi, int base);
extern int D_8021DC94;
extern int D_8021DC98;
extern OSIoMesg D_8031A020[32];

/* -- end declarations -- */

/* WHAT IT DOES: Read the unpacked size of a packed asset in ROM: the second
 * word of its header. Callers check it against the buffer they allocated. */
/* @implements 0x8021C814 tgr BrRomReadSize */
int BrRomReadSize(int param_1)

{
  BrRomRead((void *)&D_8031B328,param_1 + 4,4);
  D_8031B328 = tgr_be32(D_8031B328);      /* the ROM's word, big-endian */
  return D_8031B328;
}

/* WHAT IT DOES: Read one 32-bit word from cartridge ROM. */
/* @implements 0x8021C848 tgr BrRomReadWord */
int BrRomReadWord(int param_1)

{
  BrRomRead((void *)&D_8031B330,param_1,4);
  D_8031B330 = tgr_be32(D_8031B330);      /* the ROM's word, big-endian */
  return D_8031B330;
}

/* WHAT IT DOES: Run-length encode stride interleaved byte planes of src
 * (len bytes) into dst, the format BrRlePlanesUnpack reads: per plane a
 * 4-byte length then packets, a negative count c followed by c literal
 * bytes, or a count c >= 0 followed by one byte repeated c + 3 times; runs
 * of three or more become repeats, and runs and literal stretches are
 * capped so their counts fit a byte.  Answers the length written, or -1
 * when dst (max bytes) would overflow.  The PC twin's BrRleEncode
 * (br_texblit.c) is the same goto-shaped source; here the chars are signed,
 * the header check is against sizeof (an unsigned compare), and the locals
 * are declared so that cur, the plane counter and the length word land in
 * the ROM's frame slots. */
/* @implements 0x8021C878 tgr BrRleEncode */
int BrRleEncode(char *dst, int dstMax, signed char *src, int srcLen, int stride)
{
  int b;
  int c;
  int d;
  int e;
  int h;
  int i;
  int j;
  int a;
  int k;
  int g;
  int l;
  int f;

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
        if (d + 1 + (i - k) / stride > dstMax) {
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
        memcpy(dst + j, &f, sizeof(f));
        g = g + 1;
        src = src + 1;
        if (g >= stride) {
          return d;
        }
newchan:
        if (d + sizeof(f) > dstMax) {
          return -1;
        }
        j = d;
        d = d + 4;
        e = 0;
        k = 0;
        i = 0;
        h = 0xffffff00;
      }
    } else {
      i = e;
      if (e >= srcLen) goto flush;
    }
  } else {
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

/* WHAT IT DOES: Copy len bytes from cartridge ROM into RAM: invalidate the
 * data cache over the destination, then DMA it across in 4 KB pieces, each
 * queued on the ROM message queue. */
/* @implements 0x8021C748 tgr BrRomRead */
void BrRomRead(void *dst, unsigned int rom, int len)
{
  OSIoMesg *mb;
  
  osInvalDCache(dst,len);
  for (; 0x1000 < len; len = len + -0x1000) {
    mb = BrRomDmaSlot();
    osPiStartDma(mb,0,0,rom,dst,0x1000,(&D_80319F88));
    rom = rom + 0x1000;
    dst = (char *)dst + 0x1000;
  }
  mb = BrRomDmaSlot();
  osPiStartDma(mb,0,0,rom,dst,len,(&D_80319F88));
  BrRomWaitAll();
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
void BrStreamInit(BrUnpack *s, unsigned char *buf)
{
  s->dst = 0;
  s->buf = tgr_addr32(buf);
}

/* WHAT IT DOES: Unpack a compressed ROM file (packed length, unpacked
 * length, then 2-aligned chunks each led by its length) into dst: the next
 * chunk is DMA'd into one half of a 32000-byte buffer while the previous is
 * inflated from the other.  Without a stream it runs to the end; with one
 * it does one chunk per call, keeping its place in the stream.  Returns the
 * unpacked length.  The ROM's 0x88 frame holds every declared local in
 * declaration order (register ones too); the timing is computed and dropped,
 * as the ROM still makes its 64-bit multiply and divide calls.  The length
 * reads go through a `void *` prototype so the static length can live in a
 * register, and the read position steps past each length word before its
 * DMA, as the ROM does; the size matches, which keeps the car-sound unpack
 * at frame 2917 of the races in step with the ROM.  Two of the even-size
 * masks are taken on an int, so the ~1 constant is two smaller webs that
 * the allocator rematerialises; the heap pointers are cleared in one chain,
 * which gives the heap end the use count that ranks it into s4. */
/* @implements 0x8021CD30 tgr BrRomUnpack */
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, BrUnpack *s)
{
  extern unsigned int D_8031B348;          /* 0x8031B348 */
  unsigned int total;
  unsigned int size;
  unsigned int left;
  int out;
  unsigned int half;
  int first;
  unsigned int prev;
  unsigned char *buf;
  unsigned int t0;

  if (s != 0 && s->dst != 0) {
    rom = s->pos;
    dst = TGR_PTR(unsigned char *, s->dst);
    left = s->left;
    total = s->total;
    size = s->size;
    buf = TGR_PTR(unsigned char *, s->buf);
    half = s->half;
    D_8031B348 = s->len;
    first = 0;
  } else {
    BrRomRead(&D_8031B348, rom, 4);
    D_8031B348 = tgr_be32(D_8031B348);
    total = D_8031B348;
    left = total;
    rom += 4;
    BrRomRead(&D_8031B348, rom, 4);
    D_8031B348 = tgr_be32(D_8031B348);
    rom += 4;
    size = D_8031B348;
    left -= 8;
    if (s != 0) {
      buf = TGR_PTR(unsigned char *, s->buf);
    } else {
      buf = D_802F7F00;
    }
    half = 0;
    first = 1;
    D_8031B348 = 0;
  }
  t0 = osGetCount();
  for (;;) {
    prev = D_8031B348;
    if (((int)(D_8031B348 + 5) & ~1) < left) {
      if (rom & 1) {
        rom++;
      }
      BrRomRead(&D_8031B348, rom, 4);
      D_8031B348 = tgr_be32(D_8031B348);
      rom += 4;
      osInvalDCache(buf + half, 16000);
      osPiStartDma(BrRomDmaSlot(), 0, 0, rom, buf + half, (D_8031B348 + 1) & ~1, &D_80319F88);
      rom += (int)(D_8031B348 + 1) & ~1;
    } else {
      BrRomWaitAll();
    }
    osInvalDCache(buf + half, 16000);
    half ^= 16000;
    if (first) {
      first = 0;
    } else {
      out = 16000;
      if (0 != s) {
        D_80368AC0 = D_80324550;
        D_80368AC4 = 100000 + (char *)D_80368AC0;
      }
      BrInflate(dst, &out, buf + half, prev);
      if (s != 0) {
        D_80368AC0 = D_80368AC4 = 0;
      }
      dst += out;
      left -= (prev + 5) & ~1;
      if (left == 0) {
        break;
      }
    }
    if (s != 0) {
      break;
    }
  }
  t0 = osGetCount() - t0;
  (unsigned long long)t0 * 1000000 / osClockRate;
  if (s != 0) {
    s->pos = rom;
    s->dst = tgr_addr32(dst);
    s->left = left;
    s->total = total;
    s->size = size;
    s->half = half;
    s->buf = tgr_addr32(buf);
    s->len = D_8031B348;
  }
  return size;
}

/* WHAT IT DOES: Take the next of the 32 ROM transfer slots: if all are in
 * use, wait for one transfer to finish first; returns the slot's I/O
 * message block. */
/* @implements 0x802172D0 tgr BrRomDmaSlot */
OSIoMesg *BrRomDmaSlot(void)
{
  if (D_80272D44 == 32) {
    osRecvMesg(&D_80319F88, 0, 1);
  } else {
    D_80272D44++;
  }
  D_80272D40 = (D_80272D40 + 1) % 32;
  return &D_8031A020[D_80272D40];
}
