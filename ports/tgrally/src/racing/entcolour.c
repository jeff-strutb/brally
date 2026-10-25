/* entcolour.c -- car artwork records and their paint colours
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
#include "tgr/load.h"
void BrEntPaintTexture(BrCarModel *m, unsigned int r, unsigned int g, int b);
void BrCarPhysInit(BrCar *car);
void BrCarModelInstall(int slot, int car, void *src);
void BrFatal(char *msg);
void BrEntRebaseModel(BrCarModel *m);
extern unsigned char D_802F7F00[][0x8000];
void BrEntSetRecord(BrCar *car, int slot);
void BrEntRefreshColour(BrCar *car);
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern unsigned char D_8028B904[][3];
void BrCarResetFrames(BrCar *car);
extern char D_803D5F88[];
/* -- end declarations -- */

/* WHAT IT DOES: Repaint a car's artwork record in the car's own colour (its
 * red, green and blue reduced to the 5-bit texture format), then refresh
 * what depends on it. */
/* @implements 0x80220398 tgr BrEntRefreshColour */
void BrEntRefreshColour(BrCar *car)
{
  BrEntPaintTexture(TGR_PTR(BrCarModel *, car->model), car->colour[0] >> 3, car->colour[1] >> 3,
                    car->colour[2] >> 3);
  BrCarPhysInit(car);
}

/* WHAT IT DOES: Load a car into its slot and attach that car's artwork
 * record to the entity, repainting it. */
/* @implements 0x80220438 tgr BrEntLoadRecord */
void BrEntLoadRecord(BrCar *car, int slot, int kind)
{
  BrCarModelInstall(slot, kind, 0);
  BrEntSetRecord(car, slot);
}

/* WHAT IT DOES: Start streaming car n's model file from ROM into a slot's
 * model buffer through one of the 32 KB stream buffers, recording its size
 * and, in the stream, which slot and car it is for. */
/* @implements 0x80220474 tgr BrCarModelStream */
void BrCarModelStream(BrStream *s, int slot, int car, int bufIdx)
{
  D_8028AE0C[car].size = BrRomReadSize(D_8028AE0C[car].rom);
  BrStreamInit(&s->u, D_802F7F00[bufIdx]);
  BrRomUnpack((unsigned char *)&D_803C8000[slot], D_8028AE0C[car].rom, &s->u);
  s->slot = slot;
  s->car = car;
}

/* WHAT IT DOES: Tell whether an entity slot is unused (it has no owner
 * yet). */
/* @implements 0x80220534 tgr BrEntIsFree */
int BrEntIsFree(void *p)
{
  return *(int *)((char *)p + 0x18) == 0;
}

/* WHAT IT DOES: Attach one of the car artwork records to a car and repaint
 * it in its own colour. */
/* @implements 0x802203F0 tgr BrEntSetRecord */
void BrEntSetRecord(BrCar *car, int slot)
{
  car->model = tgr_addr32(&D_803C8000[slot]);
  BrEntRefreshColour(car);
}

/* WHAT IT DOES: Load a car's model into its slot when the slot has an
 * owner, remember which model is there, and fix up the model's addresses. */
/* @implements 0x80220544 tgr BrEntLoadModel */
int BrEntLoadModel(BrStream *p)
{
  if (p->u.left != 0) {
    /* the ROM leaves the stream in a2 for the unpack */
    BrRomUnpack((unsigned char *)&D_803C8000[p->slot], D_8028AE0C[p->car].rom, &p->u);
    if (p->u.left == 0) {
      D_8031B238[p->slot] = p->car;
      BrEntRebaseModel((BrCarModel *)&D_803C8000[p->slot]);
    }
  }
  return p->u.left;
}


/* WHAT IT DOES: Give a car its slot's default body colour from the
 * three-byte colour table (0x8028B904) after resetting its model, and clear
 * the colour's fourth byte and two related fields. */
/* @implements 0x802260A0 tgr BrCarDefaultColour */
void BrCarDefaultColour(BrCar *car)
{
  unsigned char r;
  unsigned char g;
  unsigned char b;

  BrCarResetFrames(car);
  r = D_8028B904[car->slot][0];
  g = D_8028B904[car->slot][1];
  b = D_8028B904[car->slot][2];
  car->colour[3] = 0;
  car->x2068 = 0;
  car->xed8 = 0;
  car->colour[0] = r;
  car->colour[1] = g;
  car->colour[2] = b;
}

