/* raceresult.c -- keeping the players' race results for the results screen
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
/* The human players' results, copied out of their car records when a race
 * ends; separate arrays, one entry per player. */
extern int D_80315D60[2];               /* car xfac */
extern float D_80315D68[2];             /* the lap clock */
extern float D_80315D70[2];             /* car xf98 */
extern float D_80315D78[2][5];          /* each lap's time */
extern int D_80315DA0[2];               /* car xf9c */
extern int D_80315DA8[2];               /* laps completed */
extern int D_8026FF08;                  /* human players */
extern int D_8028B304;                  /* laps in the race */
extern int D_80270790;                  /* results are ready */
extern int D_8026FF18;                  /* the game mode */
extern unsigned char D_802707AC[];      /* season points per finishing position */
extern char *D_80315DB0[];              /* the results screen's message lines */
extern int D_802707C0;                  /* and how many */
/* -- end declarations -- */

/* WHAT IT DOES: Copy each human player's race numbers (laps done, lap
 * times and a few more) out of their car into the results record, then
 * flag the results ready. */
/* @implements 0x80205FDC tgr BrRaceResultSave */
void BrRaceResultSave(void)
{
  int i;
  int j;

  for (i = 0; i < D_8026FF08; i++) {
    D_80315D60[i] = D_8031B760[i].xfac;
    D_80315D68[i] = D_8031B760[i].lapTime;
    D_80315D70[i] = D_8031B760[i].xf98;
    D_80315DA0[i] = D_8031B760[i].xf9c;
    D_80315DA8[i] = D_8031B760[i].laps;
    for (j = 0; j < D_8028B304; j++) {
      D_80315D78[i][j] = D_8031B760[i].lapTimes[j];
    }
  }
  D_80270790 = 1;
}

/* WHAT IT DOES: Put the saved race results back into the players' cars
 * (laps, lap times and the rest); in a season race with results ready, first
 * add each player's points for their finishing position to the round and
 * record the position and lap clock for the race. */
/* @implements 0x802060DC tgr BrRaceResultRestore */
void BrRaceResultRestore(void)
{
  int i;
  int j;

  if (D_8026FF18 == 0 && D_80270790 != 0) {
    for (i = 0; i < D_8026FF08; i++) {
      TGR_PTR(BrSeason *, D_8031B760[i].season)->points[TGR_PTR(BrSeason *, D_8031B760[i].season)->round] += D_802707AC[D_80315D60[i]];
      TGR_PTR(BrSeason *, D_8031B760[i].season)->place[TGR_PTR(BrSeason *, D_8031B760[i].season)->round][TGR_PTR(BrSeason *, D_8031B760[i].season)->race] = D_80315D60[i];
      TGR_PTR(BrSeason *, D_8031B760[i].season)->times[TGR_PTR(BrSeason *, D_8031B760[i].season)->round][TGR_PTR(BrSeason *, D_8031B760[i].season)->race] = D_80315D68[i];
      osSyncPrintf("points = %d\n", TGR_PTR(BrSeason *, D_8031B760[i].season)->points[TGR_PTR(BrSeason *, D_8031B760[i].season)->round]);
    }
  }
  for (i = 0; i < D_8026FF08; i++) {
    for (j = 0; j < D_8028B304; j++) {
      D_8031B760[i].lapTimes[j] = D_80315D78[i][j];
    }
    D_8031B760[i].laps = D_80315DA8[i];
    D_8031B760[i].xf9c = D_80315DA0[i];
    D_8031B760[i].lapTime = D_80315D68[i];
    D_8031B760[i].xfac = D_80315D60[i];
  }
}

/* WHAT IT DOES: After a season race, count it for each player: when the
 * round's races are done, either the player lacks the round's points (try
 * again) or the round's cars unlock and the next round begins -- after the
 * sixth, the season is over: winning every race of the last round adds the
 * Mine track (the mirrored one on a mirror season), and each season adds a
 * bonus car (milk truck, helmet, Cupra, beach ball, then the mirrored cars)
 * before the next season starts.  The round's points are cleared for the
 * new round, and the track and weather just raced are marked seen.  The
 * messages go to the results screen's lines, a blank line between groups.
 * Each message line is stored with its own n++ (two in a row for the
 * bonus-track pair), which keeps the message pointer ahead of the last
 * round's race count in the ROM's register order. */
/* @implements 0x802063A4 tgr BrSeasonRaceDone */
void BrSeasonRaceDone(void)
{
  int n;
  int i;
  int j;
  int k;

  n = 0;
  if (D_8026FF18 == 0 && D_80270790 != 0) {
    for (i = 0; i < D_8026FF08; i++) {
      TGR_PTR(BrSeason *, D_8031B760[i].season)->race++;
      if (TGR_PTR(BrSeason *, D_8031B760[i].season)->race == D_8028B944[TGR_PTR(BrSeason *, D_8031B760[i].season)->round].x8) {
        TGR_PTR(BrSeason *, D_8031B760[i].season)->race = 0;
        if (TGR_PTR(BrSeason *, D_8031B760[i].season)->points[TGR_PTR(BrSeason *, D_8031B760[i].season)->round] >= D_8028B944[TGR_PTR(BrSeason *, D_8031B760[i].season)->round].xc) {
          TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= D_8028B944[TGR_PTR(BrSeason *, D_8031B760[i].season)->round].kindMask;
          TGR_PTR(BrSeason *, D_8031B760[i].season)->round++;
          if (TGR_PTR(BrSeason *, D_8031B760[i].season)->round == 6) {
            D_80315DB0[n++] = "ALL SEASONS COMPLETED!";
            for (j = 0; j < D_8028B944[5].x8; j++) {
              if (TGR_PTR(BrSeason *, D_8031B760[i].season)->place[5][j] != 0) {
                goto bonus;
              }
            }
            if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state & 1) {
              if (!(TGR_PTR(BrSeason *, D_8031B760[i].season)->xce & 0x100)) {
                if (n) {
                  D_80315DB0[n++] = "";
                }
                D_80315DB0[n++] = "You Won all races this season!";
                D_80315DB0[n++] = "Bonus: Mirror Mine Track Added!";
                TGR_PTR(BrSeason *, D_8031B760[i].season)->xce |= 0x100;
              }
            } else {
              if (!(TGR_PTR(BrSeason *, D_8031B760[i].season)->xce & 8)) {
                if (n) {
                  D_80315DB0[n++] = "";
                }
                D_80315DB0[n++] = "You Won all races this season!";
                D_80315DB0[n++] = "Bonus: Mine Track Added!";
                TGR_PTR(BrSeason *, D_8031B760[i].season)->xce |= 8;
              }
            }
bonus:
            if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state == 0) {
              if (n) {
                D_80315DB0[n++] = "";
              }
              D_80315DB0[n++] = "Bonus: Milk Truck Added!";
              TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= 0x400;
            } else if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state == 1) {
              if (n) {
                D_80315DB0[n++] = "";
              }
              D_80315DB0[n++] = "Bonus: Helmet Car Added!";
              TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= 0x200;
            } else if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state == 2) {
              if (n) {
                D_80315DB0[n++] = "";
              }
              D_80315DB0[n++] = "Bonus: Cupra Car Added!";
              TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= 0x800;
            } else if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state == 3) {
              if (n) {
                D_80315DB0[n++] = "";
              }
              D_80315DB0[n++] = "Bonus: Beach Ball Car Added!";
              TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= 0x1000;
            } else if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state == 4) {
              if (n) {
                D_80315DB0[n++] = "";
              }
              D_80315DB0[n++] = "Bonus: Mirrored Cars Added!";
              TGR_PTR(BrSeason *, D_8031B760[i].season)->unlocked |= 0x8000;
            }
            TGR_PTR(BrSeason *, D_8031B760[i].season)->round = 0;
            TGR_PTR(BrSeason *, D_8031B760[i].season)->state++;
          } else {
            D_80315DB0[n++] = "SEASON COMPLETED!";
            D_80315DB0[n++] = "";
            D_80315DB0[n++] = "PROGRESS TO THE NEXT SEASON";
          }
        } else {
          D_80315DB0[n++] = "Sorry, you don't have enough points to";
          D_80315DB0[n++] = "pass this season.  You'll have to try again.";
        }
        TGR_PTR(BrSeason *, D_8031B760[i].season)->points[TGR_PTR(BrSeason *, D_8031B760[i].season)->round] = 0;
      }
      if (TGR_PTR(BrSeason *, D_8031B760[i].season)->state & 1) {
        k = 5;
      } else {
        k = 0;
      }
      TGR_PTR(BrSeason *, D_8031B760[i].season)->xce |= 1 << (D_8028B944[TGR_PTR(BrSeason *, D_8031B760[i].season)->round].races[TGR_PTR(BrSeason *, D_8031B760[i].season)->race][0] + k);
      TGR_PTR(BrSeason *, D_8031B760[i].season)->xd0 |= 1 << D_8028B944[TGR_PTR(BrSeason *, D_8031B760[i].season)->round].races[TGR_PTR(BrSeason *, D_8031B760[i].season)->race][1];
    }
  }
  D_802707C0 = n;
}
