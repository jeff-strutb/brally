/* rmregion.c (port): what Top Gear Rally Remastered takes from the other
 * cartridges, as its Remaster Options choose (ports/remastered/platform/
 * options.h).  Remastered builds this game from the USA cartridge and from
 * Japan's (its PAL/Japan menus); each library carries a little of the
 * other from ROM 0x800000 (ports/remastered/tools/regionextras.py):
 *
 *   the front-end tune: Music Top Gear Rally (USA) plays the USA
 *   cartridge's, Top Gear Rally (PAL/Japan) Japan's (Europe's is the same
 *   tune; the races' pieces are the same in all three)
 *   the other cartridge's three cars (below)
 */
#include "tgr/common.h"

/* -- declarations -- */
int  rm_option(int which);              /* the app's (ports/remastered/platform/options.h) */
void BrMusicStop(void);
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, void *s);
void BrMusicStart(int buf, int voices);
extern char D_802AC400[];               /* the music's voices */
/* -- end declarations -- */

enum { RM_OPT_MUSIC = 1 };
enum { RM_MUSIC_PC, RM_MUSIC_N64, RM_MUSIC_N64_PAL };

#define RM_ROM_TUNE 0x800000            /* the other cartridge's tune */

/* the front-end tune's ROM offset, for the Music in force */
int rm_menu_tune(void)
{
  int pal = rm_option(RM_OPT_MUSIC) == RM_MUSIC_N64_PAL;
#if TGR_REGION_JPN
  return pal ? TGR_VA(000EBC00) : RM_ROM_TUNE;
#else
  return pal ? RM_ROM_TUNE : TGR_VA(000EBC00);
#endif
}

/* the front-end tune again, from its start: Music changed on the options
 * screen between the two cartridges' */
void rm_menu_tune_restart(void)
{
  BrMusicStop();
  BrRomUnpack(TGR_PTR(unsigned char *, TGR_VA(80025C00)), rm_menu_tune(), 0);
  BrMusicStart(TGR_VA(80025C00), tgr_addr32(D_802AC400));
}

/* ---- the other cartridge's three cars --------------------------------------
 * Japan's cartridge replaced three cars with its own: Type-PJ for Type-RS
 * (kind 3), Type-TR for Type-M3 (6), Type-MN for the Helmet (9).  Remastered
 * offers both of each pair, the other cartridge's beside this one's and
 * unlocked with it: the game keeps the kind it knows (its tables, its saves,
 * its unlocks are by kind) and each player's choice says which of the two
 * cars that kind is.  The twin's record and model come from the other
 * cartridge (regionextras.py, ROM 0x840000).  Boss Rally, which runs the
 * races, has all six: Type-RS, M3 and the Helmet its cars 3, 6 and 9, Type-PJ,
 * TR and MN its 13, 14 and 15. */
#include "tgr/car.h"

void BrRomRead(void *dst, unsigned int rom, int n);
extern BrCarModelRec D_8028AE0C[];      /* car n's record */
extern int D_8028AE08;                  /* cars */
int BrCarSelectable(int n);

#define RM_ROM_TWINS 0x840000
#define RM_TWNAMES 0x807F0000u          /* their names, in the arena's top (the game never goes there) */
#define RM_TW 3

static struct { int kind; BrCarModelRec rec; } s_tw[RM_TW];
static int s_ntw = -1;
static BrCarModelRec s_saved;
static int s_saved_kind = -1;
unsigned char rm_twin_car[4];             /* each player's: the other cartridge's car of its kind */
static unsigned char rm_slot_twin[16];    /* a model buffer holds the other cartridge's car of its kind */

static unsigned int rd32(const unsigned char *b) { return (unsigned int)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]; }

static void twin_load(void)
{
  unsigned char buf[4 + RM_TW * (4 + 0x60 + 16)];
  int i, k;

  if (s_ntw >= 0) {
    return;
  }
  BrRomRead(buf, RM_ROM_TWINS, sizeof buf);
  s_ntw = (int)rd32(buf);
  if (s_ntw < 0 || s_ntw > RM_TW) {
    s_ntw = 0;                          /* (no extras: a library built without them) */
  }
  for (i = 0; i < s_ntw; i++) {
    const unsigned char *e = buf + 4 + i * (4 + 0x60 + 16);
    unsigned char *r = (unsigned char *)&s_tw[i].rec;
    char *name = TGR_PTR(char *, RM_TWNAMES + i * 16);

    s_tw[i].kind = (int)rd32(e);
    memcpy(r, e + 4, 0x60);
    /* the record as the cartridge has it, turned native: words but for its
       bytes at 0x28..0x33 (the dial's and the lamps' sizes and places) */
    for (k = 0; k < 0x60; k += 4) {
      if (k >= 0x28 && k < 0x34) {
        continue;
      }
      *(unsigned int *)(r + k) = rd32(e + 4 + k);
    }
    memcpy(name, e + 4 + 0x60, 16);
    s_tw[i].rec.name = RM_TWNAMES + i * 16;
  }
}

static BrCarModelRec *twin_rec(int kind)
{
  int i;

  twin_load();
  for (i = 0; i < s_ntw; i++) {
    if (s_tw[i].kind == kind) {
      return &s_tw[i].rec;
    }
  }
  return 0;
}

/* player p's car's record: the twin's when that is the player's choice */
BrCarModelRec *rm_car_rec(int p, int kind)
{
  BrCarModelRec *j = rm_twin_car[p & 3] ? twin_rec(kind) : 0;
  return j ? j : &D_8028AE0C[kind];
}

/* while the game loads player p's car of `kind` (its model streamed or read
 * by the kind's record), that record is the twin's if the player chose it */
void rm_car_rec_begin(int p, int kind, int slot)
{
  BrCarModelRec *j = rm_twin_car[p & 3] ? twin_rec(kind) : 0;
  rm_slot_twin[slot & 15] = j != 0;
  if (j) {
    s_saved = D_8028AE0C[kind];
    s_saved_kind = kind;
    D_8028AE0C[kind] = *j;
  }
}

void rm_car_rec_end(int p, int kind)
{
  (void)p;
  if (s_saved_kind == kind) {
    twin_rec(kind)->size = D_8028AE0C[kind].size;
    D_8028AE0C[kind] = s_saved;
    s_saved_kind = -1;
  }
}

/* the record of the car in model buffer `slot` (of kind k) */
BrCarModelRec *rm_slot_rec(int slot, int k)
{
  BrCarModelRec *j = rm_slot_twin[slot & 15] ? twin_rec(k) : 0;
  return j ? j : &D_8028AE0C[k];
}

/* the car select's left and right: the next selectable car, the twin after
 * this cartridge's; 1 when it moved */
int rm_car_step(BrCar *car, int p, int dir)
{
  int n = D_8028AE08, i;
  int k0 = car->x2058, j0 = rm_twin_car[p & 3];
  int k = k0, j = j0;

  for (i = 0; i < 2 * n; i++) {
    if (dir > 0) {
      if (!j && twin_rec(k)) {
        j = 1;
      } else {
        j = 0;
        k = (k + 1) % n;
      }
    } else {
      if (j) {
        j = 0;
      } else {
        k = (k + n - 1) % n;
        j = twin_rec(k) != 0;
      }
    }
    if (BrCarSelectable(k)) {
      break;
    }
  }
  tgr_print("car step: player %d %d%s -> %d%s\n", p, k0, j0 ? " (twin)" : "", k, j ? " (twin)" : "");   /* (TGR_LOG) */
  car->x2058 = k;
  rm_twin_car[p & 3] = j;
  return k != k0 || j != j0;
}

/* the car Boss Rally races for player p of `kind`: kinds 3, 6 and 9 are its
 * 3, 6 and 9 on the USA's cartridge and its 13, 14 and 15 on Japan's */
int rm_car_br_code(int p, int kind)
{
  int japans = (kind == 3 || kind == 6 || kind == 9) &&
#if TGR_REGION_JPN
               !(rm_twin_car[p & 3] && twin_rec(kind));
#else
               rm_twin_car[p & 3] && twin_rec(kind);
#endif
  return japans ? 13 + (kind / 3 - 1) : kind;
}
