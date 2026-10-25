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
  TgrAddr fn;  /* void (*)(void) */
  TgrAddr seq;          /* unsigned short * -- newest button first, 0xFFFF ends; 0 ends the table */
} BrCheat;
extern BrCheat D_8028DF48[12];         /* eleven and the end */
extern TgrAddr D_8028DF4C;              /* D_8028DF48[0].seq (unsigned short *) */
typedef struct BrPadHistory {   /* a controller's button history */
  unsigned int pressed;         /* 0x00  buttons held this frame: the low half (the
                                 * N64 read it as the halfword at 0x02) */
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
  TGR_PTR(BrSeason *, D_8031B760[0].season)->xce |= 0x1f;
}

/* WHAT IT DOES: Cheat: set bits 0x1FF of player 1's season record's unlock mask. */
/* @implements 0x80254F70 tgr BrCheatUnlockCars */
void BrCheatUnlockCars(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x1ff;
}

/* WHAT IT DOES: Cheat: set bits 0x200 of player 1's season record's unlock mask. */
/* @implements 0x80254F88 tgr BrCheatUnlock0200 */
void BrCheatUnlock0200(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x200;
}

/* WHAT IT DOES: Cheat: set bits 0x1000 of player 1's season record's unlock mask. */
/* @implements 0x80254FA0 tgr BrCheatUnlock1000 */
void BrCheatUnlock1000(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x1000;
}

/* WHAT IT DOES: Cheat: set bits 0x800 of player 1's season record's unlock mask. */
/* @implements 0x80254FB8 tgr BrCheatUnlock0800 */
void BrCheatUnlock0800(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x800;
}

/* WHAT IT DOES: Cheat: set bits 0x400 of player 1's season record's unlock mask. */
/* @implements 0x80254FD0 tgr BrCheatUnlock0400 */
void BrCheatUnlock0400(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x400;
}

/* WHAT IT DOES: Cheat: set bits 0x3E0 of player 1's season record's second unlock mask. */
/* @implements 0x80254FE8 tgr BrCheatUnlockCE3E0 */
void BrCheatUnlockCE3E0(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->xce |= 0x3e0;
}

/* WHAT IT DOES: Cheat: set bits 0x8000 of player 1's season record's unlock mask. */
/* @implements 0x80255000 tgr BrCheatUnlock8000 */
void BrCheatUnlock8000(void)
{
  TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 0x8000;
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
 * The sequence is indexed (seq[k]) rather than walked with a second
 * pointer, so seq stays live across the match loop and interferes with the
 * history and the code read from it, which puts it in a2 as the ROM has it;
 * IDO still strength-reduces the index to a pointer. */
/* @implements 0x80255048 tgr BrCheatInput */
void BrCheatInput(BrPadHistory *pad)
{
  BrCheat *c;
  be16_t *seq;                  /* the sequences are .data the lift does not reach:
                                   big-endian, as the cartridge has them */
  unsigned short *h;
  unsigned int i;
  int k;

  if ((unsigned short)pad->pressed != pad->hist[pad->pos]) {
    pad->pos = (pad->pos - 1) & 0x7f;
    pad->hist[pad->pos] = (unsigned short)pad->pressed;
    seq = TGR_PTR(be16_t *, D_8028DF4C);
    c = D_8028DF48;
    while (seq != 0) {
      i = pad->pos;
      h = pad->hist;
      k = 0;
      while (BE16(seq[k]) != 0xffff) {
        if (BE16(seq[k]) != h[i]) {
          goto next;
        }
        k++;
        i = (i + 1) & 0x7f;
      }
      TGR_FN(void (*)(void), c->fn)();
    next:
      c++;
      seq = TGR_PTR(be16_t *, c->seq);
    }
  }
}
