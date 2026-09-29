/* cheats.c -- the cheat actions (toggles and unlocks) and the button-sequence
 * matcher that runs them from the cheat table
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
extern int D_8028AA94;
extern int D_8026FF20;
extern int D_8028AA68;
typedef struct BrCheat {        /* one button-sequence cheat (8 bytes) */
  void (*fn)(void);
  unsigned short *seq;          /* newest button first, 0xFFFF ends; 0 ends the table */
} BrCheat;
extern BrCheat D_8028DF48[];
extern unsigned short *D_8028DF4C;    /* D_8028DF48[0].seq */
typedef struct BrPadHistory {   /* a controller's button history */
  short pad00;
  unsigned short buttons;       /* 0x02  buttons held this frame */
  char pad04[0x50 - 0x04];
  unsigned short hist[128];     /* 0x50  ring of button changes */
  unsigned int pos;             /* 0x150 newest entry */
} BrPadHistory;
/* -- end declarations -- */

/* WHAT IT DOES: Flip the debug flag at 0x8028AA94 on or off. */
/* @implements 0x80254F40 tgr BrCheatToggleAA94 */
void BrCheatToggleAA94(void)
{
  D_8028AA94 = !D_8028AA94;
}

/* WHAT IT DOES: Cheat: set bits 0x1F of player 1's season record's second unlock mask. */
/* @implements 0x80254F58 tgr BrCheatUnlockCE1F */
void BrCheatUnlockCE1F(void)
{
  D_8031B760[0].season->xce |= 0x1f;
}

/* WHAT IT DOES: Cheat: set bits 0x1FF of player 1's season record's unlock mask. */
/* @implements 0x80254F70 tgr BrCheatUnlockCars */
void BrCheatUnlockCars(void)
{
  D_8031B760[0].season->unlocked |= 0x1ff;
}

/* WHAT IT DOES: Cheat: set bits 0x200 of player 1's season record's unlock mask. */
/* @implements 0x80254F88 tgr BrCheatUnlock0200 */
void BrCheatUnlock0200(void)
{
  D_8031B760[0].season->unlocked |= 0x200;
}

/* WHAT IT DOES: Cheat: set bits 0x1000 of player 1's season record's unlock mask. */
/* @implements 0x80254FA0 tgr BrCheatUnlock1000 */
void BrCheatUnlock1000(void)
{
  D_8031B760[0].season->unlocked |= 0x1000;
}

/* WHAT IT DOES: Cheat: set bits 0x800 of player 1's season record's unlock mask. */
/* @implements 0x80254FB8 tgr BrCheatUnlock0800 */
void BrCheatUnlock0800(void)
{
  D_8031B760[0].season->unlocked |= 0x800;
}

/* WHAT IT DOES: Cheat: set bits 0x400 of player 1's season record's unlock mask. */
/* @implements 0x80254FD0 tgr BrCheatUnlock0400 */
void BrCheatUnlock0400(void)
{
  D_8031B760[0].season->unlocked |= 0x400;
}

/* WHAT IT DOES: Cheat: set bits 0x3E0 of player 1's season record's second unlock mask. */
/* @implements 0x80254FE8 tgr BrCheatUnlockCE3E0 */
void BrCheatUnlockCE3E0(void)
{
  D_8031B760[0].season->xce |= 0x3e0;
}

/* WHAT IT DOES: Cheat: set bits 0x8000 of player 1's season record's unlock mask. */
/* @implements 0x80255000 tgr BrCheatUnlock8000 */
void BrCheatUnlock8000(void)
{
  D_8031B760[0].season->unlocked |= 0x8000;
}

/* WHAT IT DOES: Flip the debug flag at 0x8026FF20 on or off. */
/* @implements 0x80255018 tgr BrCheatToggleFF20 */
void BrCheatToggleFF20(void)
{
  D_8026FF20 = !D_8026FF20;
}

/* WHAT IT DOES: Flip the debug flag at 0x8028AA68 on or off. */
/* @implements 0x80255030 tgr BrCheatToggleAA68 */
void BrCheatToggleAA68(void)
{
  D_8028AA68 = !D_8028AA68;
}

/* WHAT IT DOES: Record a change of the held buttons in the controller's
 * 128-entry history ring, and run every cheat whose button sequence
 * matches the newest entries.
 * RESIDUE (11): register naming only -- the ROM keeps the sequence
 * pointer in a2, ours in a0 (and one move/addiu pair swaps with it).  The
 * head's decrement-and-store through pad->pos and the compare-then-advance
 * loop took it from 33; declaration order, statement order in the loop
 * head, the table start spelling and 295 permuter compiles leave 11. */
/* @implements 0x80255048 tgr BrCheatInput */
void BrCheatInput(BrPadHistory *pad)
{
  BrCheat *c;
  unsigned short *seq;
  unsigned short *p;
  unsigned short *h;
  unsigned int i;

  if (pad->buttons != pad->hist[pad->pos]) {
    pad->pos = (pad->pos - 1) & 0x7f;
    pad->hist[pad->pos] = pad->buttons;
    seq = D_8028DF4C;
    c = D_8028DF48;
    while (seq != 0) {
      i = pad->pos;
      h = pad->hist;
      p = seq;
      while (*p != 0xffff) {
        if (*p != h[i]) {
          goto next;
        }
        p++;
        i = (i + 1) & 0x7f;
      }
      c->fn();
    next:
      c++;
      seq = c->seq;
    }
  }
}
