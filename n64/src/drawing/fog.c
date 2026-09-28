/* fog.c -- the weather fog: the RDP fog range and the fog amount at a point
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
extern int D_8028AA78;
extern int D_8028AB38;
extern int D_8028AB3C;
extern float D_8031AA50[4][4];
void BrVec3Normalise(float v[3]);
extern int D_8028AA80;                  /* the view modes that pick the tints */
extern int D_8028AA84;
extern int D_8028AA8C;
extern unsigned char D_8028AB20, D_8028AB24, D_8028AB28;   /* the fog colour r/g/b */
extern unsigned char D_8028AB2C;                           /* the fog alpha */
extern unsigned char D_8028AB40, D_8028AB44, D_8028AB48;   /* tint A r/g/b */
extern unsigned char D_8028AB4C, D_8028AB50, D_8028AB54;   /* tint B r/g/b */
extern unsigned char D_8031B350[4], D_8031B354[4], D_8031B358[4];   /* the ramp, r/g/b */
extern unsigned int D_8028AB58, D_8028AB5C;                /* packed tints A and B */
extern unsigned int D_8031B360[4];                         /* packed ramp */
extern float D_8031B338[3];
/* -- end declarations -- */

/* WHAT IT DOES: How fogged a world point is, 0 to 1: its projected depth
 * through the fog multiplier and offset the RSP uses (0 when the weather has
 * no fog). */
/* @implements 0x80218C8C tgr BrFogAmount */
float BrFogAmount(float v[3])
{
  float f;
  float p[4];

  if (D_8028AA78 == 0) {
    return 0.0f;
  }
  BrMat4TransformPoint4(p, v, D_8031AA50);
  p[2] /= p[3];
  f = (p[2] * D_8028AB3C + D_8028AB38) * 0.00392156862745098;   /* 1/255 */
  if (f < 0.0f) {
    return 0.0f;
  }
  if (f > 1.0f) {
    return 1.0f;
  }
  return f;
}

/* WHAT IT DOES: Set this frame's two tint colours and the four-step colour
 * ramp from the view mode: two modes take fixed colours, one derives both
 * tints from the fog colour by shifts, and the default blends fixed colours
 * with the fog colour by the fog alpha; the ramp is fixed or quarter, half,
 * three-quarter and full steps of the first tint.  All of it is packed into
 * the RGB words the renderer reads.  The PC twin is BrFrameTintSetup
 * (br_framebegin.c).
 * RESIDUE (261 words, 454/453 instructions): the ROM keeps 0xFF in a1 for
 * the whole tint block (one fewer constant load), has a 0x50 frame (0x40
 * here) and spills the ramp shifts one slot higher; everything after the
 * first branch is shifted by that one instruction. */
/* @implements 0x80218D5C tgr BrFrameTintSetup */
void BrFrameTintSetup(void)
{
  int i;

  if (D_8028AA80 == 0) {
    D_8031B338[0] = 15.0f;
    D_8031B338[1] = 10.0f;
    D_8031B338[2] = 20.0f;
    BrVec3Normalise(D_8031B338);
  }
  if (D_8028AA84 != 0 || D_8028AA8C != 0) {
    D_8028AB40 = (D_8028AB20 + 0x2FD) >> 2;
    D_8028AB44 = (D_8028AB24 + 0x2FD) >> 2;
    D_8028AB48 = (D_8028AB28 + 0x264) >> 2;
    D_8028AB4C = (D_8028AB20 * 5) / 8;
    D_8028AB50 = (D_8028AB24 * 5) / 8;
    D_8028AB54 = (D_8028AB28 * 5) / 8;
  } else if (D_8028AA80 != 0) {
    D_8028AB40 = 0xDD;
    D_8028AB44 = 0xEE;
    D_8028AB48 = 0xFF;
    D_8028AB4C = 0x3C;
    D_8028AB50 = 0x39;
    D_8028AB54 = 0x36;
  } else if (D_8028AA78 != 0) {
    D_8028AB40 = (((D_8028AB20 + 0xFF) >> 1) * D_8028AB2C + (0xFF - D_8028AB2C) * 0xFF) / 0xFF;
    D_8028AB44 = (((D_8028AB24 + 0xFF) >> 1) * D_8028AB2C + (0xFF - D_8028AB2C) * 0xFF) / 0xFF;
    D_8028AB48 = (((D_8028AB28 + 0xCC) >> 1) * D_8028AB2C + (0xFF - D_8028AB2C) * 0xCC) / 0xFF;
    D_8028AB4C = (((D_8028AB20 << 2) / 5) * D_8028AB2C + (0xFF - D_8028AB2C) * 0x66) / 0xFF;
    D_8028AB50 = (((D_8028AB24 << 2) / 5) * D_8028AB2C + (0xFF - D_8028AB2C) * 0x66) / 0xFF;
    D_8028AB54 = (((D_8028AB28 << 2) / 5) * D_8028AB2C + (0xFF - D_8028AB2C) * 0x77) / 0xFF;
  } else {
    D_8028AB40 = 0xFF;
    D_8028AB44 = 0xFF;
    D_8028AB48 = 0xCC;
    D_8028AB4C = 0x66;
    D_8028AB50 = 0x66;
    D_8028AB54 = 0x77;
  }

  if (D_8028AA80 != 0) {
    D_8031B350[0] = 0x22;
    D_8031B354[0] = 0x22;
    D_8031B358[0] = 0x22;
    D_8031B350[1] = 0x44;
    D_8031B354[1] = 0x44;
    D_8031B358[1] = 0x44;
    D_8031B350[2] = 0x66;
    D_8031B354[2] = 0x66;
    D_8031B358[2] = 0x66;
    D_8031B350[3] = 0xFF;
    D_8031B354[3] = 0xFF;
    D_8031B358[3] = 0xFF;
  } else if (D_8028AA84 != 0) {
    D_8031B350[0] = 0xD0;
    D_8031B354[0] = 0xD0;
    D_8031B358[0] = 0xF0;
    D_8031B350[1] = 0xE0;
    D_8031B354[1] = 0xE0;
    D_8031B358[1] = 0xFF;
    D_8031B350[2] = 0xF0;
    D_8031B354[2] = 0xF0;
    D_8031B358[2] = 0xFF;
    D_8031B350[3] = 0xFF;
    D_8031B354[3] = 0xFF;
    D_8031B358[3] = 0xFF;
  } else {
    D_8031B350[0] = D_8028AB40 >> 2;
    D_8031B354[0] = D_8028AB44 >> 2;
    D_8031B358[0] = D_8028AB48 >> 2;
    D_8031B350[1] = D_8028AB40 >> 1;
    D_8031B354[1] = D_8028AB44 >> 1;
    D_8031B358[1] = D_8028AB48 >> 1;
    D_8031B350[2] = (D_8028AB40 >> 1) + (D_8028AB40 >> 2);
    D_8031B354[2] = (D_8028AB44 >> 1) + (D_8028AB44 >> 2);
    D_8031B358[2] = (D_8028AB48 >> 1) + (D_8028AB48 >> 2);
    D_8031B350[3] = D_8028AB40;
    D_8031B354[3] = D_8028AB44;
    D_8031B358[3] = D_8028AB48;
  }

  D_8028AB5C = (unsigned int)D_8028AB4C << 24 | (unsigned int)D_8028AB50 << 16 | (unsigned int)D_8028AB54 << 8;
  D_8028AB58 = (unsigned int)D_8028AB40 << 24 | (unsigned int)D_8028AB44 << 16 | (unsigned int)D_8028AB48 << 8;
  for (i = 0; i < 4; i++) {
    D_8031B360[i] = (unsigned int)D_8031B350[i] << 24 | (unsigned int)D_8031B354[i] << 16
                  | (unsigned int)D_8031B358[i] << 8;
  }
}
