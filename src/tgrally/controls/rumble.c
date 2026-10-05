/* rumble.c -- Rumble Pak output
 */
#include "tgr/car.h"
#include "tgr/common.h"

/* -- declarations -- */
typedef struct { char raw[0x68]; } BrPfs;   /* an OSPfs */
void BrRaceTick(void);
int BrModeIs(void (*mode)(void));
int func_80262370(int *mq, BrPfs *pfs, int channel);   /* osMotorInit */
void func_80261F20(BrPfs *pfs);                          /* osMotorStop */
void func_80262088(BrPfs *pfs);                          /* osMotorStart */
extern int D_8026FF10;                  /* the race is paused */
extern int D_8026FF18;
extern int D_80272D48[6];               /* the controller message queue */
extern short D_802A4BE8;                /* rumble is on */
extern short D_802A4BEC[2];             /* each player's effect is running */
extern short D_802A4BF0[2];             /* the motor is on */
extern short D_802A4BF4[2];             /* frames on */
extern short D_802A4BF8[2];             /* frames off */
extern short D_802A4BFC[2];             /* frames left in this phase */
extern short D_802A4C00[2];             /* frames left in the effect */
extern short D_802A4C04;
extern int D_802A4C08;                  /* frame count for the re-detect */
extern char D_8031B1E8[2];              /* a Rumble Pak is in */
extern BrPfs D_8031A3F8[4];
/* -- end declarations -- */

/* WHAT IT DOES: Run the Rumble Paks: re-detect them outside a race (or
 * paused, or when an effect is held off), and in a race pulse each player's
 * motor on and off at the rates the current effect asks for, while the
 * player's pad is live, stopping it when the effect runs out; every 64
 * frames look for paks again. */
/* @t4-pass 0x80260490 1 2026-10-03 compiles 30 best 170 moved 0  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x80260490 2 2026-10-03 compiles 30 best 170 moved 0  (tools/tgrally/n64permute.py) */
/* @t3 0x80260490 */
/* @implements 0x80260490 tgr BrRumbleUpdate */
void BrRumbleUpdate(int arg0)
{
  int i;

  if (!BrModeIs(BrRaceTick) || D_8026FF18 == 5 || D_802A4C04 != 0 || D_8026FF10 != 0) {
    for (i = 0; i != 2; i++) {
      if (D_802A4BE8 != 0) {
        D_802A4BEC[i] = 0;
        D_8031B1E8[i] = 0;
        if (func_80262370(D_80272D48, &D_8031A3F8[i], i) == 0) {
          D_8031B1E8[i] = 1;
          func_80261F20(&D_8031A3F8[i]);
        }
      }
    }
  }
  if (D_802A4BE8 != 0 && D_802A4C04 == 0 && D_8026FF10 == 0) {
    for (i = 0; i != 2; i++) {
      if (D_8031B1E8[i] != 0 && D_802A4BEC[i] != 0 && D_8031B760[i].pad[0x11] == 0) {
        if (D_802A4BFC[i] == 0) {
          if (D_802A4BF0[i] != 0) {
            D_802A4BFC[i] = D_802A4BF8[i];
            func_80261F20(&D_8031A3F8[i]);
          } else {
            D_802A4BFC[i] = D_802A4BF4[i];
            func_80262088(&D_8031A3F8[i]);
          }
          D_802A4BF0[i] ^= 1;
        } else {
          D_802A4BFC[i]--;
        }
        if (D_802A4C00[i] == 0) {
          func_80261F20(&D_8031A3F8[i]);
          D_802A4BF0[i] = 0;
          D_802A4BFC[i] = 0;
          D_802A4BEC[i] = 0;
        } else {
          D_802A4C00[i]--;
        }
      }
    }
  }
  D_802A4C08 = (D_802A4C08 + 1) & 0x3f;
  if (D_802A4C08 == 0 && D_802A4BE8 != 0) {
    for (i = 0; i != 2; i++) {
      D_8031B1E8[i] = 0;
      if (func_80262370(D_80272D48, &D_8031A3F8[i], i) == 0) {
        D_8031B1E8[i] = 1;
      }
    }
  }
}

/* WHAT IT DOES: Does nothing: an empty function the rumble code ends with,
 * called once from the game's startup. */
/* @implements 0x802607AC tgr BrStub802607AC */
void BrStub802607AC(void)
{
}
