/* music.c -- the module music player
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrModChan {      /* one module channel, 0x18 bytes */
  unsigned long long pos;       /* 0x00 */
  char pad08[5];
  unsigned char xd;             /* 0x0D */
  unsigned char xe;             /* 0x0E */
  char pad0f[9];
} BrModChan;
typedef struct BrMixVoice {     /* one mixer voice, 0x18 bytes */
  unsigned int pos;             /* 0x00 */
  int x4;
  unsigned long long rate;      /* 0x08  0 = silent */
  int vol;                      /* 0x10  scaled volume */
  int baseVol;                  /* 0x14 */
} BrMixVoice;
typedef struct BrModState {
  short x0;
  short x2;                     /* 0x02 */
  short x4;                     /* 0x04 */
  char pad06[10];
  unsigned char x10;            /* 0x10 */
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
void func_80256720(int param_1,char *param_2);
void BrModReset(void);
void osSyncPrintf();
extern int D_802A49C0;
extern int D_80378F98;
extern float D_802A49D4[12];
typedef struct BrSample {       /* a module instrument's sample header; 8-bit data follows at +0x28 */
  unsigned int len;             /* 0x00 */
  int x4;
  unsigned int loopLen;         /* 0x08 */
  char pad0c[2];
  unsigned char loops;          /* 0x0E */
} BrSample;
extern BrSample *D_803787D0[];  /* by instrument number - 1 */
extern unsigned long long D_80379568[10][12];
extern int D_802A4918;
extern int D_802A491C;
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
void func_8025721C(void);
void BrMixMusicVoice(short *buf, int bytes, int voice);
void BrMixSfx(short *buf, unsigned int bytes);
void BrSfxLoopSamples(void);
void BrRumbleUpdate(int);
/* -- end declarations -- */

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
    D_80378DD0[i].xd = 0;
    D_80378DD0[i].xe = 0x40;
    D_80378DD0[i].pos = 0;
  }
}


/* WHAT IT DOES: Bring up the music player: set the audio interface to
 * 21998 Hz, start a 10 ms timer on the music queue, silence every music and
 * sound voice, time one test mix, build the instrument and note-rate
 * tables, reset the module player, queue the first buffer and start the
 * mixer thread.
 * RESIDUE (39): IDO unrolls the six-voice loop with its stores in another
 * order and one temp register later, and keeps 0xFFFEFFFE in v0 where the
 * ROM uses t6.  Loop tests, store order in the body and the constant's
 * spelling (chained, comma, one line) do not reach it. */
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
  func_80256720(param_1, param_2);
  osSyncPrintf("Creating Note Frequency entries\n");
  BrNoteRatesInit();
  osSyncPrintf("Starting Mod\n");
  BrModReset();
  D_80378F98 = 1;
  osAiSetNextBuffer(D_803747D0, 0x4000);
  osCreateThread(D_80378FB8, 7, func_80257D3C, 0, D_80379568, 0x7f);
  osStartThread(D_80378FB8);
}

/* WHAT IT DOES: Start a piece of music: builds its instrument entries,
 * starts the module player and sets every channel to its starting volume. */
/* @implements 0x80257964 tgr BrMusicStart */
void BrMusicStart(int param_1,int param_2)
{
  int i;

  osSyncPrintf("Creating I entries\n");
  func_80256720(param_1,param_2);
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
 * RESIDUE (26): temporaries one register later than the ROM's, and the
 * loop/stop arms laid the other way round. */
/* @implements 0x80256D3C tgr BrMusicLoopSamples */
void BrMusicLoopSamples(void)
{
  int i;
  unsigned char n;
  BrSample *smp;

  for (i = 0; i < D_802A49C0; i++) {
    n = D_80378DD0[i].xd;
    if (n != 0) {
      smp = D_803787D0[n - 1];
      if (smp->len + (unsigned int)smp + 0x28 < D_802A4798[i].pos) {
        if (smp->loops != 0) {
          D_802A4798[i].pos -= smp->loopLen;
        } else {
          D_802A4798[i].rate = 0;
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
      func_8025721C();
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
    BrMixSfx(D_803747D0, n);
    BrSfxLoopSamples();
    sfxPos += n;
    if (sfxPos > 0x4000) {
      sfxPos -= 0x4000;
    }
    BrRumbleUpdate(0);
  }
}
