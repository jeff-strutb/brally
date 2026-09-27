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
/* -- end declarations -- */

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

