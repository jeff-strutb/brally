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
extern unsigned long long D_80379568[10][12];
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

