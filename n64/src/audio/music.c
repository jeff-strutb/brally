/* music.c -- the module music player
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrModChan {      /* one module channel, 0x18 bytes */
  unsigned long long target;    /* 0x00  the tone portamento's target rate */
  short porta;                  /* 0x08  rate step per tick */
  short slide;                  /* 0x0A  volume step per tick */
  unsigned char note;           /* 0x0C */
  unsigned char smp;            /* 0x0D  the playing instrument, 0 none */
  unsigned char vol;            /* 0x0E  0..64 */
  unsigned char fx;             /* 0x0F */
  unsigned char param;          /* 0x10 */
  unsigned char inst;           /* 0x11  the row's instrument */
  unsigned char arp[3];         /* 0x12  arpeggio notes */
  unsigned char arpIdx;         /* 0x15 */
  unsigned char arpOn;          /* 0x16 */
  char pad17;
} BrModChan;
typedef struct BrMixVoice {     /* one mixer voice, 0x18 bytes */
  unsigned int pos;             /* 0x00 */
  int x4;
  unsigned long long rate;      /* 0x08  0 = silent */
  int vol;                      /* 0x10  scaled volume */
  int baseVol;                  /* 0x14 */
} BrMixVoice;
typedef struct BrModState {
  unsigned short x0;                   /* 0x00  ticks per row */
  unsigned short x2;                   /* 0x02  ticks left in the row */
  unsigned short x4;                   /* 0x04  rows left in the pattern */
  char pad06[2];
  unsigned char *order;         /* 0x08  the order list */
  unsigned char *row;           /* 0x0C  the next packed row */
  unsigned char x10;            /* 0x10  order position */
  unsigned char len;            /* 0x11  orders in the song */
  unsigned char restart;        /* 0x12 */
} BrModState;
extern BrModChan D_80378DD0[];
extern BrModState D_80378FA0;
extern BrMixVoice D_802A4798[];
typedef struct BrSampleLoop {   /* 0x0C bytes */
  unsigned int x0;
  unsigned int x4;
  unsigned int loop;            /* 0 = the sample does not loop */
} BrSampleLoop;
extern BrSampleLoop D_80378F50[];
extern BrMixVoice D_802A4920[6];
void BrModLoad(unsigned char *xm, unsigned char *buf);
unsigned char *BrModRowRead(unsigned char *p);
void BrModReset(void);
void osSyncPrintf();
extern int D_802A49C0;
extern unsigned char D_80378FB2;      /* D_80378FA0.restart, declared on its own */
extern unsigned char *D_803789D0[];     /* the module's patterns (row count at +5, rows at +9) */
extern int D_80378F98;
extern float D_802A49D4[12];
typedef struct BrSample {       /* a module instrument's sample header; 8-bit data follows at +0x28 */
  unsigned int len;             /* 0x00 */
  int x4;
  unsigned int loopLen;         /* 0x08 */
  unsigned char vol;            /* 0x0C  0..64 */
  char pad0d;
  unsigned char loops;          /* 0x0E */
  char pad0f;
  signed char relNote;          /* 0x10 */
} BrSample;
extern BrSample *D_803787D0[];  /* by instrument number - 1 */
extern unsigned long long D_80379568[10][12];
extern int D_802A4918;                 /* the mixer's state block (mixer.s): */
extern unsigned int D_802A491C;        /* its first two words differ in type */
extern char D_803746F0[];               /* the music timer's message queue */
extern char D_80374708[];               /* its two messages */
extern char D_80374710[];               /* the music timer */
extern short D_803747D0[];              /* the music output buffer (0x4000 bytes) */
extern short D_802A4A08[];              /* silence: where idle sound voices point */
extern char D_80378FB8[];               /* the mixer thread */
extern short D_802A4790;
extern unsigned long long osClockRate;
int osAiSetFrequency(unsigned int freq);
void osCreateMesgQueue(void *mq, void *msgs, int count);
int osSetTimer(void *t, unsigned long long countdown, unsigned long long interval, void *mq, void *msg);
unsigned int osGetCount(void);
void BrMixMusic(short *buf, int bytes);
void BrNoteRatesInit(void);
int osAiSetNextBuffer(void *buf, unsigned int size);
void osCreateThread(void *t, int id, void (*entry)(void *), void *arg, void *sp, int pri);
void osStartThread(void *t);
void func_80257D3C(void *arg);
extern unsigned char D_802A49C4;        /* music volume */
extern unsigned char D_802A49C8;        /* music fade */
extern unsigned char D_802A49CC;        /* effects volume */
extern unsigned char D_802A49D0;        /* effects fade */
void osRecvMesg(void *mq, void *msg, int flag);
int osAiGetStatus(void);
int osAiGetLength(void);
void BrMusicLoopSamples(void);
void BrModTick(void);
void BrMixMusicVoice(short *buf, int bytes, int voice);
void BrMixSfx(short *buf, unsigned int bytes, unsigned int pos);   /* pos: the effects write position */
void BrSfxLoopSamples(void);
void BrRumbleUpdate(int);
/* -- end declarations -- */

#define LE16(p) ((p)[1] * 0x100 + (p)[0])
#define LE32(p) ((p)[0] + (p)[1] * 0x100 + (p)[2] * 0x10000 + (p)[3] * 0x1000000)

/* WHAT IT DOES: Load a FastTracker II module into the player: song
 * length, restart, channels and speed from the header, the order list
 * copied to the front of buf, then every pattern (header and packed rows)
 * copied word-aligned behind it with its address in the pattern table,
 * then every instrument's first sample: its 0x28-byte header copied, its
 * delta-coded 8-bit data decoded, and 0x4B0 bytes appended -- silence for
 * a one-shot sample, the loop repeated for a forward loop (16-bit and
 * ping-pong samples are refused).  Prints its progress.
 * The pattern walk has its own pointer, handed to p for the instruments;
 * the order count is re-read from the state struct each pass; the restart
 * byte is stored through its own symbol and read back through the struct;
 * the 0x28-byte header loop counts from its own initialiser (so IDO does
 * not unroll it) and the instrument types are a switch. */
/* @implements 0x80256720 tgr BrModLoad */
void BrModLoad(unsigned char *xm, unsigned char *buf)
{
  unsigned char *src;
  unsigned int i;
  unsigned char *p;
  unsigned char *q;
  unsigned int npat;
  unsigned int ninst;
  unsigned int j;
  unsigned int hdr;
  int acc;
  unsigned char type;
  unsigned int len;
  unsigned char *pat;
  unsigned char *start;

  start = buf;
  D_80378FA0.order = buf;
  D_80378FA0.len = xm[0x40];
  src = xm + 0x50;
  D_80378FA0.x0 = xm[0x4c];
  D_802A49C0 = xm[0x44];
  for (i = 0; i < D_80378FA0.len; i++) {
    *buf++ = *src++;
  }
  buf = (unsigned char *)((unsigned int)(buf + 3) & ~3);
  osSyncPrintf("Length = %d\n", D_80378FA0.len);
  D_80378FB2 = xm[0x42];
  osSyncPrintf("Restartfrom = %d\n", D_80378FA0.restart);
  npat = xm[0x46];
  osSyncPrintf("%d Patterns found\n", npat);
  ninst = xm[0x48];
  osSyncPrintf("%d Instruments found\n", ninst);
  hdr = *(unsigned int *)(xm + 0x3c);
  pat = xm + 0x3c + (((hdr >> 24) & 0xff) + ((hdr >> 16) & 0xff) * 0x100 + ((hdr >> 8) & 0xff) * 0x10000 + ((hdr & 0xff) << 24));
  for (i = 0; i < npat; i++) {
    D_803789D0[i] = buf;
    len = LE16(pat + 7) + LE32(pat);
    src = pat;
    for (j = 0; j < len; j++) {
      *buf++ = *src++;
    }
    pat = pat + (LE16(pat + 7) + LE32(pat));
  }
  p = pat;
  buf = (unsigned char *)((unsigned int)(buf + 3) & ~3);
  osSyncPrintf("Now doSamples");
  for (i = 0; i < ninst; i++) {
    osSyncPrintf(".");
    q = p + LE32(p);
    if (p[0x1b] > 0) {
      len = LE32(q);
      D_803787D0[i] = (BrSample *)buf;
      for (src = q, j = 0; j < 0x28; j++) {
        *buf++ = *src++;
      }
      D_803787D0[i]->len = LE32(q);
      D_803787D0[i]->loopLen = LE32(q + 8);
      src = q + 0x28;
      acc = 0;
      for (j = 0; j < len; j++) {
        acc = *src++ + acc;
        *buf++ = acc;
      }
      type = q[0xe];
      switch (type) {
      case 0:
        for (j = 0; j < 0x4b0; j++) *buf++ = 0;
        break;
      case 1:
        src = buf - LE32(q + 8);
        for (j = 0; j < 0x4b0; j++) {
          *buf++ = *src++;
        }
        break;
      default:
        osSyncPrintf("WANKER fuck off no 16bit, no Ping fucking pong\n");
        break;
      }
      buf = (unsigned char *)((unsigned int)(buf + 3) & ~3);
      p = q + len + 0x28;
    } else {
      D_803787D0[i] = 0;
      p = q;
    }
  }
  osSyncPrintf("\n\nSample Space used = %d bytes\n", buf - start);
}

/* WHAT IT DOES: Build the mixer's note-rate table: for ten octaves of the
 * twelve note frequencies, the 32.32 fixed-point sample step relative to
 * the mixer's base rate. */
/* @implements 0x80256C2C tgr BrNoteRatesInit */
void BrNoteRatesInit(void)
{
  int oct;
  int n;
  double r;

  for (oct = 0; oct < 10; oct++) {
    for (n = 0; n < 12; n++) {
      r = D_802A49D4[n] * (1 << oct) * 0.0625;
      D_80379568[oct][n] = r / 261.7 * 0.3801709246295118 * 4294967296.0;
    }
  }
}


/* WHAT IT DOES: Read one packed pattern row (FastTracker II packing: a
 * byte with the top bit set says which of note, instrument, volume, effect
 * and parameter follow; otherwise all five are there) into every channel:
 * a note restarts its instrument's sample at the note's rate (relative
 * note added), a new instrument without a volume plays at 64; then set up
 * the row's effect -- arpeggio, portamento up/down, tone portamento toward
 * the note (no restart), volume slide, set volume, pattern break, speed --
 * and the voice's volume from the channel's and the instrument's.
 * Returns the next row.  The bytes are chained into the channel first and
 * sign-extended into the locals (lbu, then sll/sra); the channel is
 * indexed, not a pointer, so the count is read once.
 * RESIDUE (241): register allocation -- the ROM holds the four table
 * bases in s0-s3 (frame 0x48), ours the row values; the code is otherwise
 * in the ROM's order (register-blind gap 72). */
/* @t4-pass 0x80256DEC 1 2026-10-03 compiles 25 best 240 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80256DEC 2 2026-10-03 compiles 25 best 240 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80256DEC */
/* @implements 0x80256DEC tgr BrModRowRead */
unsigned char *BrModRowRead(unsigned char *p)
{
  int i;
  signed char flags;
  signed char note;
  unsigned char inst;
  signed char fx;
  signed char param;
  unsigned int smp;
  unsigned long long target;

  for (i = 0; i < D_802A49C0; i++) {
    flags = *p++;
    note = 0;
    inst = 0;
    if (flags < 0) {
      if (flags & 1) {
        note = D_80378DD0[i].note = *p++;
      }
      if (flags & 2) {
        inst = D_80378DD0[i].inst = *p++;
      }
      if (flags & 4) {
        D_80378DD0[i].vol = *p++;
      } else if (note != 0) {
        D_80378DD0[i].vol = 64;
      }
      if (flags & 8) {
        fx = D_80378DD0[i].fx = *p++;
      } else {
        fx = 0;
      }
      if (flags & 0x10) {
        param = D_80378DD0[i].param = *p++;
      } else {
        param = 0;
      }
    } else {
      note = D_80378DD0[i].note = flags;
      inst = D_80378DD0[i].inst = *p++;
      D_80378DD0[i].vol = *p++;
      fx = D_80378DD0[i].fx = *p++;
      param = D_80378DD0[i].param = *p++;
    }
    if (note != 0) {
      D_80378DD0[i].note--;
    }
    if (inst != 0 && D_80378DD0[i].vol == 0) {
      D_80378DD0[i].vol = 64;
    }
    D_80378DD0[i].porta = 0;
    D_80378DD0[i].slide = 0;
    smp = D_80378DD0[i].smp;
    D_80378DD0[i].arpOn = 0;
    switch (fx) {
    case 0:
      if (param != 0) {
        D_80378DD0[i].arp[1] = D_80378DD0[i].note + (param >> 4 & 0xf);
        D_80378DD0[i].arp[2] = D_80378DD0[i].note + (param & 0xf);
        D_80378DD0[i].arpOn = 1;
        D_80378DD0[i].arp[0] = D_80378DD0[i].note;
      }
      break;
    case 1:
      D_80378DD0[i].target = 0;
      D_80378DD0[i].porta = D_80378DD0[i].param;
      break;
    case 2:
      D_80378DD0[i].porta = -D_80378DD0[i].param;
      D_80378DD0[i].target = 0;
      break;
    case 3:
      if (smp != 0) {
        D_80378DD0[i].porta = D_80378DD0[i].param;
        note = 0;
        target = (&D_80379568[0][0])[(D_803787D0[smp - 1]->relNote + D_80378DD0[i].note) & 0xffff];
        D_80378DD0[i].target = target;
        if (D_802A4798[i].rate > target) {
          D_80378DD0[i].porta = -D_80378DD0[i].porta;
        }
      }
      break;
    case 6:
    case 10:
      if (D_80378DD0[i].param & 0xf0) {
        D_80378DD0[i].slide = D_80378DD0[i].param >> 4 & 0xf;
      } else {
        D_80378DD0[i].slide = -(D_80378DD0[i].param & 0xf);
      }
      break;
    case 12:
      D_80378DD0[i].vol = param;
      break;
    case 13:
      D_80378FA0.x4 = 0;
      break;
    case 15:
      D_80378FA0.x0 = D_80378FA0.x2 = D_80378DD0[i].param;
      break;
    }
    if (note != 0) {
      smp = D_80378DD0[i].inst;
      D_80378DD0[i].arpIdx = 0;
      D_802A4798[i].x4 = 0;
      D_80378DD0[i].smp = smp;
      D_802A4798[i].pos = (unsigned int)D_803787D0[smp - 1] + 0x28;
      D_802A4798[i].rate = (&D_80379568[0][0])[(D_803787D0[smp - 1]->relNote + D_80378DD0[i].note) & 0xffff];
    }
    if (smp != 0) {
      D_802A4798[i].baseVol = (D_80378DD0[i].vol * D_803787D0[smp - 1]->vol) >> 6;
    }
  }
  return p;
}

/* -- declarations: BrModTick -- */
extern int D_802A4A04;
/* -- end declarations -- */

/* WHAT IT DOES: One tick of the module player: when the row's ticks run
 * out, start the next row (moving to the next order's pattern when the
 * pattern ends, wrapping to the restart order) and read it; then for every
 * channel step the arpeggio through its three notes, slide the rate by
 * the portamento in period space (period = K / rate, clamped at 1; a tone
 * portamento stops at its target), and slide the volume within 0..64.
 * The arpeggio index is bumped and masked in two stores; the target test
 * puts the clamping arm first.  The arpeggio note is an unsigned short
 * local read through the index in place, so the allocator colours it
 * before anything else in that block (the ROM's v1). */
/* @implements 0x8025721C tgr BrModTick */
void BrModTick(void)
{
  int i;
  unsigned char *pat;
  int idx;                       /* unused: the ROM's frame keeps its home */
  long long per;
  short v;
  unsigned int smp;
  long long r;
  unsigned short n;

  if (--D_80378FA0.x2 == 0) {
    D_80378FA0.x2 = D_80378FA0.x0;
    if (D_80378FA0.x4 == 0) {
      D_802A4A04 = 0;
      if (++D_80378FA0.x10 == D_80378FA0.len) {
        D_80378FA0.x10 = D_80378FA0.restart;
      }
      pat = D_803789D0[D_80378FA0.order[D_80378FA0.x10]];
      D_80378FA0.row = pat + 9;
      D_80378FA0.x4 = pat[5];
    }
    D_80378FA0.x4--;
    D_80378FA0.row = BrModRowRead(D_80378FA0.row);
  }
  for (i = 0; i < D_802A49C0; i++) {
    smp = D_80378DD0[i].smp;
    if (D_80378DD0[i].arpOn != 0 && smp != 0 && D_802A4798[i].rate != 0) {
      n = D_80378DD0[i].arp[D_80378DD0[i].arpIdx] + D_803787D0[smp - 1]->relNote;
      D_802A4798[i].rate = (&D_80379568[0][0])[n];
      D_80378DD0[i].arpIdx++;
      D_80378DD0[i].arpIdx &= 3;
    }
    if (D_80378DD0[i].porta != 0 && D_802A4798[i].rate != 0) {
      per = 0xEE9FCFF0B5ULL / D_802A4798[i].rate;
      per -= D_80378DD0[i].porta;
      if (per < 1) {
        per = 1;
      }
      if (D_80378DD0[i].target != 0) {
        r = 0xEE9FCFF0B5LL / per;
        if (D_80378DD0[i].porta < 0) {
          if (r < D_80378DD0[i].target) {
            r = D_80378DD0[i].target;
          }
        } else if (r > D_80378DD0[i].target) {
          r = D_80378DD0[i].target;
        }
        D_802A4798[i].rate = r;
      } else {
        D_802A4798[i].rate = 0xEE9FCFF0B5LL / per;
      }
    }
    if (D_80378DD0[i].slide != 0) {
      v = D_80378DD0[i].vol + D_80378DD0[i].slide;
      if (v < 0) {
        v = 0;
      } else if (v > 64) {
        v = 64;
      }
      D_80378DD0[i].vol = v;
      if (smp != 0) {
        D_802A4798[i].baseVol = (D_803787D0[smp - 1]->vol * D_80378DD0[i].vol) >> 6;
      }
    }
  }
}

/* WHAT IT DOES: Reset the module player: playback state flags, and every
 * channel's position, two per-channel bytes (0 and 64, a centred pan or
 * volume). */
/* @implements 0x802571AC tgr BrModReset */
void BrModReset(void)
{
  int i;

  D_80378FA0.x2 = 1;
  D_80378FA0.x4 = 0;
  D_80378FA0.x10 = 0xff;
  for (i = 0; i < D_802A49C0; i++) {
    D_80378DD0[i].smp = 0;
    D_80378DD0[i].vol = 0x40;
    D_80378DD0[i].target = 0;
  }
}


/* WHAT IT DOES: Bring up the music player: set the audio interface to
 * 21998 Hz, start a 10 ms timer on the music queue, silence every music and
 * sound voice, time one test mix, build the instrument and note-rate
 * tables, reset the module player, queue the first buffer and start the
 * mixer thread.  The two 0xFFFEFFFE stores go to globals of different
 * types, so they are two constants: one int and one unsigned.  If they were
 * one constant, IDO would hold it in v0 across the entry block, which shifts
 * every register in the voice loop below. */
/* @implements 0x802575C4 tgr BrMusicInit */
void BrMusicInit(int param_1, char *param_2)
{
  int i;
  int j;
  unsigned int t;

  D_802A4918 = 0xfffefffe;
  D_802A491C = 0xfffefffe;
  osSyncPrintf("Real Frequency is = %d\n", osAiSetFrequency(21998));
  osCreateMesgQueue(D_803746F0, D_80374708, 2);
  osSetTimer(D_80374710, (unsigned long long)10000 * osClockRate / 1000000,
             (unsigned long long)10000 * osClockRate / 1000000, D_803746F0, 0);
  for (i = 0; i < D_802A49C0; i++) {
    D_802A4798[i].pos = (unsigned int)D_803747D0;
    D_802A4798[i].rate = 0;
    D_802A4798[i].baseVol = 0x20;
  }
  for (j = 0; j < 6; j++) {
    D_802A4920[j].pos = (unsigned int)D_802A4A08;
    D_802A4920[j].rate = 0;
    D_802A4920[j].baseVol = 0x20;
  }
  D_802A4790 = 0;
  t = -osGetCount();
  BrMixMusic(D_803747D0, 0x4000);
  t += osGetCount();
  osSyncPrintf("%1.7f", (double)t / 46875500.0);
  osSyncPrintf("Creating I entries\n");
  BrModLoad(param_1, param_2);
  osSyncPrintf("Creating Note Frequency entries\n");
  BrNoteRatesInit();
  osSyncPrintf("Starting Mod\n");
  BrModReset();
  D_80378F98 = 1;
  osAiSetNextBuffer(D_803747D0, 0x4000);
  osCreateThread(D_80378FB8, 7, func_80257D3C, 0, D_80379568, 0x7f);
  osStartThread(D_80378FB8);
}

/* WHAT IT DOES: Stop the music: halt the module player and silence every
 * music voice and channel. */
/* @implements 0x802578F4 tgr BrMusicStop */
void BrMusicStop(void)
{
  int i;

  D_80378F98 = 0;
  for (i = 0; i < D_802A49C0; i++) {
    D_802A4798[i].pos = (unsigned int)D_802A4A08;
    D_802A4798[i].rate = 0;
    D_802A4798[i].baseVol = 0;
    D_80378DD0[i].smp = 0;
  }
}

/* WHAT IT DOES: Start a piece of music: builds its instrument entries,
 * starts the module player and sets every channel to its starting volume. */
/* @implements 0x80257964 tgr BrMusicStart */
void BrMusicStart(int param_1,int param_2)
{
  int i;

  osSyncPrintf("Creating I entries\n");
  BrModLoad(param_1,param_2);
  osSyncPrintf("Starting Mod\n");
  BrModReset();
  D_80378F98 = 1;
  for (i = 0; i < D_802A49C0; i++) {
    D_802A4798[i].baseVol = 0x20;
  }
}


/* WHAT IT DOES: Find a free sound-effect voice: the first of the six that
 * is silent, or -1 when all are playing. */
/* @implements 0x802579F4 tgr BrSfxFreeVoice */
short BrSfxFreeVoice(void)
{
  int i;

  for (i = 0; i < 6; i++) {
    if (D_802A4920[i].rate == 0) {
      return i;
    }
  }
  return -1;
}


/* WHAT IT DOES: Start a sample on a sound-effect voice: record the
 * sample's start, length and loop length, and set the voice playing from the
 * start at rate 1.0 (32.32 fixed point), volume 0, both pans 0x20. */
/* @implements 0x80257B04 tgr BrSfxVoiceStart */
void BrSfxVoiceStart(short v, unsigned int start, unsigned int len, unsigned int loop)
{
  D_80378F50[v].x0 = start;
  D_80378F50[v].x4 = len;
  D_80378F50[v].loop = loop;
  D_802A4920[v].vol = 0;
  D_802A4920[v].pos = start;
  D_802A4920[v].x4 = 0;
  D_802A4920[v].baseVol = 0x200020;
  D_802A4920[v].rate = 0x100000000LL;
}


/* WHAT IT DOES: Keep the music voices' samples looping: a channel with an
 * instrument whose voice has run past the end of the sample jumps back by
 * the loop length, or falls silent if the sample does not loop.
 * The instrument number is an int, and the sample is indexed from the
 * table at each use rather than held in a pointer, so the end address is
 * the busier web and is coloured first, as the ROM has it (v1), and the
 * sample address second (a2). */
/* @implements 0x80256D3C tgr BrMusicLoopSamples */
void BrMusicLoopSamples(void)
{
  int i;
  int n;
  unsigned int end;

  for (i = 0; i < D_802A49C0; i++) {
    n = D_80378DD0[i].smp;
    if (n != 0) {
      end = (unsigned int)D_803787D0[n - 1] + D_803787D0[n - 1]->len + 0x28;
      if (end < D_802A4798[i].pos) {
        if (D_803787D0[n - 1]->loops == 0) {
          D_802A4798[i].rate = 0;
        } else {
          D_802A4798[i].pos -= D_803787D0[n - 1]->loopLen;
        }
      }
    }
  }
}


/* WHAT IT DOES: Keep the six sound-effect voices' samples looping, a
 * stereo pair at a time: a playing voice that has run past its sample's end
 * jumps back by the loop length, or stops if the sample does not loop. */
/* @implements 0x80257C44 tgr BrSfxLoopSamples */
void BrSfxLoopSamples(void)
{
  int i;

  for (i = 0; i < 6; i += 2) {
    if (D_802A4920[i].rate != 0 && D_802A4920[i].pos >= D_80378F50[i].x0 + D_80378F50[i].x4) {
      if (D_80378F50[i].loop != 0) {
        D_802A4920[i].pos -= D_80378F50[i].loop;
      } else {
        D_802A4920[i].rate = 0;
      }
    }
    if (D_802A4920[i + 1].rate != 0 && D_802A4920[i + 1].pos >= D_80378F50[i + 1].x0 + D_80378F50[i + 1].x4) {
      if (D_80378F50[i + 1].loop != 0) {
        D_802A4920[i + 1].pos -= D_80378F50[i + 1].loop;
      } else {
        D_802A4920[i + 1].rate = 0;
      }
    }
  }
}


/* WHAT IT DOES: The mixer thread, woken by the music timer: scale every
 * music voice's volume by the music level and fade, and every effect
 * voice's two channel volumes by the effects level and fade; queue the
 * output buffer if the AI can take it; work out how far the AI has read
 * and mix that much music (stepping the module on alternate wake-ups),
 * the remaining voices in pairs, and the effects; then update rumble.
 * The six counters/positions only this thread uses are function-local
 * statics (the ROM re-materialises their addresses).
 * RESIDUE: ours hoists more loop-invariant addresses and constants into
 * saved registers (frame 0x40 vs 0x30); the volume loop keeps a counter
 * and a pointer in the ROM.  Not yet matched. */
/* @t4-pass 0x80257D3C 1 2026-10-04 compiles 31 best 209 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80257D3C 2 2026-10-04 compiles 31 best 209 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80257D3C */
/* @implements 0x80257D3C tgr BrMusicThread */
void BrMusicThread(void *arg)
{
  static int odd = 0;            /* 0x802A4A0C: the module steps on alternate wake-ups */
  static int readPos = 0;        /* 0x802A4A10: where the AI was reading last time */
  static unsigned int sfxPos = 0; /* 0x802A4A14: the effects write position */
  static int bytes;             /* 0x80379930: bytes to mix this time */
  static int aiPos;             /* 0x80379934: where the AI will be reading */
  static int v;                 /* 0x80379938: voice counter */
  int i;
  int len;
  unsigned short save;
  unsigned int n;

  for (;;) {
    odd ^= 1;
    osRecvMesg(D_803746F0, 0, 1);
    for (i = 0; i < D_802A49C0; i++) {
      D_802A4798[i].vol = (unsigned int)(D_802A4798[i].baseVol * D_802A49C4 * D_802A49C8) >> 16;
    }
    for (v = 0; v < 6; v++) {
      D_802A4920[v].vol =
          ((D_802A49CC * ((unsigned int)D_802A4920[v].baseVol >> 16) * D_802A49D0 >> 16) << 16) +
          (D_802A49CC * (D_802A4920[v].baseVol & 0xffff) * D_802A49D0 >> 16);
    }
    bytes = osAiGetStatus();
    if (bytes >= 0) {
      bytes = osAiSetNextBuffer(D_803747D0, 0x4000);
    }
    len = osAiGetLength();
    aiPos = len - 0x1000;
    if (aiPos < 0) {
      aiPos += 0x4000;
    }
    bytes = readPos - aiPos;
    if (bytes < 0) {
      bytes += 0x4000;
    }
    readPos = aiPos;
    BrMusicLoopSamples();
    if (odd != 0 && D_80378F98 != 0) {
      BrModTick();
    }
    save = D_802A4790;
    BrMixMusic(D_803747D0, bytes);
    for (v = 6; v < D_802A49C0; v += 2) {
      D_802A4790 = save;
      BrMixMusicVoice(D_803747D0, bytes, v * sizeof(BrMixVoice) - 0x90);
    }
    n = ((0x5000 - len) & ~7) - sfxPos;
    if (n > 0x4000) {
      n -= 0x4000;
    }
    BrMixSfx(D_803747D0, n, sfxPos);
    BrSfxLoopSamples();
    sfxPos += n;
    if (sfxPos > 0x4000) {
      sfxPos -= 0x4000;
    }
    BrRumbleUpdate(0);
  }
}

/* WHAT IT DOES: Start a sound voice on a sample: record the sample's
 * start, end and loop, point the voice at the start, silent, at the normal
 * rate (1.0 in 32.32). */
/* @implements 0x80257B78 tgr BrSfxVoicePlay */
void BrSfxVoicePlay(short v, unsigned int start, unsigned int end, unsigned int loop)
{
  D_80378F50[v].x0 = start;
  D_80378F50[v].x4 = end;
  D_80378F50[v].loop = loop;
  D_802A4920[v].baseVol = 0;
  D_802A4920[v].vol = 0;
  D_802A4920[v].pos = start;
  D_802A4920[v].x4 = 0;
  D_802A4920[v].rate = 0x100000000ULL;
}

/* WHAT IT DOES: Set a sound voice's volume: two 16-bit channel levels,
 * a level over 32 is masked with 32 (bug preserved: the ROM ANDs, keeping
 * only that bit, rather than clamping). */
/* @implements 0x80257BE4 tgr BrSfxVoiceVolume */
void BrSfxVoiceVolume(short v, unsigned int vol)
{
  if ((vol & 0xffff) > 0x20) {
    vol &= 0xffff0020;
  }
  if ((vol >> 16) > 0x20) {
    vol &= 0x20ffff;
  }
  D_802A4920[v].baseVol = vol;
}
