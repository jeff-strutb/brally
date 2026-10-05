/* racehud.c -- the race's frame hook: extra layers drawn over the 3-D view
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void BrSceneAnimate(void);
void BrPerfMark(int param_1,unsigned int param_2,unsigned int param_3,int param_4,int param_5);
void BrSkidAge(void);
extern int D_8026FF10;
float BrVec3Dot(void *, void *);
extern int D_8028B304;                /* laps in the race */
extern char D_8028B308[];             /* "%yyWRONG WAY" */
float BrAtan2(float param_1,float param_2);
void BrTextPrint(unsigned char *s, int x, int y);
extern int D_8023876C;
extern int D_8023877C;
extern int D_80238784;
extern int D_80238798;
extern int D_802387A8;
extern int D_802387B0;
void BrTextHighlightOff(void);
void BrTextAlignRight(void);
void BrTextSetFont(int param_1);
void BrHudTimeDraw(char *label, char *prefix, float t, int x, int y);
extern int D_8026FF08;
extern int D_8026FF18;
extern int D_8028AAEC;                /* the view being drawn */
extern BrCar *D_8028AAF0;             /* the car of the view being drawn */
extern int D_8028AB0C;
extern char D_802AA060[];
extern char D_802AA070[];
extern char D_802AA080[];
extern char D_802AA094[];
extern char D_802AA0A4[];
extern char D_802AA0B8[];
extern char D_802AA0C8[];
extern char D_802AA0D8[];
extern char D_802AA0E8[];
void BrTextAltBlendOn(void);
void BrTextAltBlendOff(void);
void BrTextAlignLeft(void);
int BrTextSetColours();
int BrTextWidth(unsigned char *param_1,int param_2);
extern BrCarCam *D_8028AAF4;           /* the camera being drawn from */
extern char D_802AA0F8[];
extern char D_802AA0FC[];
extern char D_802AA110[];
extern char D_802AA114[];
extern char D_802AA118[];
extern char D_802AA11C[];
extern char D_802AA120[];
typedef struct BrViewRect { int x; int y; int w; int h; int car; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
void BrTextAlignCentre(void);
extern int D_80238EBC;
extern int D_80238F10;
#include "tgr/track.h"
#define D_80025C70 BE32(D_80025C00.path)
void BrScissorSet(int x0, int y0, int x1, int y1);
void BrHudDialDraw(void);
extern int D_802723D8;                 /* speed in mph (else kph) */
extern char D_80361C30[];              /* the speed text */
typedef struct BrHudPanel {     /* one per view, 0x14 bytes */
  short x0;
  unsigned short h;             /* 0x02  height of the view's top panel */
  char pad04[0x14 - 0x04];
} BrHudPanel;
extern BrHudPanel D_8028C7B4[2];
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028A898;                 /* the texture filter mode */
void BrTexSizeBits(int n, int *shift, int *mask);
typedef struct BrHudImg {       /* an image record for the image drawer (0x14 bytes) */
  char pad00[8];
  TgrAddr data;         /* unsigned short * -- 0x08 */
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  char pad10[4];
} BrHudImg;
extern BrHudImg D_8028C7A8[2];         /* each view's rev counter face */
extern BrHudImg D_8028C7D0[2];         /* and its rev-lamp frame */
extern unsigned int D_8028C790[2];     /* the face loaded for each view: car | night << 8 */
extern int D_8028C798[2];              /* the lamp frame loaded for each view */
extern be16_t D_80361530[2][0x100];          /* each view's dial palette, as the RSP reads it */
extern be16_t D_80361930[0x100];     /* a dial palette as read from ROM */
typedef struct BrHudVtx {       /* a Vtx (RSP data: big-endian) */
  be16_t x, y, z;
  be16_t flag;
  be16_t s, t;
  unsigned char r, g, b, a;
} BrHudVtx;
extern BrHudVtx D_80361B30[2][2][4];   /* the needle quad, per frame buffer and view */
extern int D_8028A85C;                 /* the frame buffer being built */
extern int D_8028AA80;                 /* night */
void BrRomRead(void *dst, int rom, int size);
void BrRomImageDraw(BrHudImg *img, int x, int y, int w, int h, int a, int b, int c, int d,
                   int e, int f, int g, int k, int l);
unsigned int BrRandStep(void);
float cosf(float a);
float sinf(float a);
/* -- end declarations -- */

/* WHAT IT DOES: The race's frame hook: unless the race has switched the
 * layers off, draws the extra model layer and the second overlay pass, each
 * after setting the fill colour it needs. */
/* @implements 0x802003E4 tgr BrRaceDrawLayers */
void BrRaceDrawLayers(void)
{
  if (D_8026FF10 == 0) {
    BrPerfMark(0,0x80,0x80,0xf0,0xff);
    BrSceneAnimate();
    BrPerfMark(0,0,0,0xc0,0xff);
    BrSkidAge();
    BrPerfMark(0,0,0x82,0,0xff);
  }
}

/* WHAT IT DOES: Draw the rev counter: the car's dashboard dial (its face and
 * night or day palette loaded from ROM when the car or the light changes), the
 * rev-lamp frame for the gear (lamps lit with the throttle open), both scaled
 * to three quarters with two views; then by the dial's kind either the needle,
 * a quad swept from the rest angle towards the full-revs angle by the RPM
 * (with a little random shake), or a palette bar whose colours light up to
 * the revs, the unlit ones dimmed or ghosted.  PC twin: BrHudDrawDial.
 * RESIDUE (813, 864 of the ROM's 866 instructions): the ROM's frame is 0xF0
 * with x, y, the key and the frame at 0xEC..0xD8 and the needle pointer and
 * radii at 0x7C..0x74 (wide unused gaps between), where ours is 0xD8 with
 * register choices following. */
/* @t4-pass 0x80237980 1 2026-10-03 compiles 120 best 811 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80237980 2 2026-10-03 compiles 120 best 811 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80237980 */
/* @implements 0x80237980 tgr BrHudDialDraw */
void BrHudDialDraw(void)
{
  int x;
  int y;
  unsigned int key;
  int frame;
  unsigned int v;
  float rev;
  float a;
  float tip;
  float base;
  float fx;
  float fy;
  BrHudVtx *q;
  int first;
  int lit;
  int end;
  be16_t *src;
  be16_t *dst;

  y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h - D_8028C7A8[D_8028AAEC].h - 4;
  x = 296 - D_8028C7A8[D_8028AAEC].w;
  frame = 0;
  if (0.0f <= D_8028AAF0->xe38) {
    frame = D_8028AAF0->xe40 + 1;
  }
  key = D_8028AAF0->kind | D_8028AA80 << 8;
  if (key != D_8028C790[D_8028AAEC]) {
    D_8028C7A8[D_8028AAEC].w = D_8028AE0C[D_8028AAF0->kind].dialW;
    D_8028C7A8[D_8028AAEC].h = D_8028AE0C[D_8028AAF0->kind].dialH;
    BrRomRead(TGR_PTR(unsigned short *, D_8028C7A8[D_8028AAEC].data), D_8028AE0C[D_8028AAF0->kind].dialRom + 0x400,
              D_8028C7A8[D_8028AAEC].h * D_8028C7A8[D_8028AAEC].w);
    BrRomRead(D_80361530[D_8028AAEC], D_8028AE0C[D_8028AAF0->kind].dialRom + D_8028AA80 * 0x200,
              0x200);
    D_8028C7D0[D_8028AAEC].w = D_8028AE0C[D_8028AAF0->kind].lampW;
    D_8028C7D0[D_8028AAEC].h = D_8028AE0C[D_8028AAF0->kind].lampH;
    D_8028C790[D_8028AAEC] = key;
  } else if (frame == D_8028C798[D_8028AAEC]) {
    goto draw;
  }
  BrRomRead(TGR_PTR(unsigned short *, D_8028C7D0[D_8028AAEC].data),
            D_8028C7A8[D_8028AAEC].h * D_8028C7A8[D_8028AAEC].w + D_8028AE0C[D_8028AAF0->kind].dialRom +
                D_8028C7D0[D_8028AAEC].h * D_8028C7D0[D_8028AAEC].w * frame + 0x400,
            D_8028C7D0[D_8028AAEC].h * D_8028C7D0[D_8028AAEC].w);
  D_8028C798[D_8028AAEC] = frame;
draw:
  gRaw(D_8028A858++, 0xE7000000, 0);
  gRaw(D_8028A858++, 0xFD100000, D_80361530[D_8028AAEC]);
  gRaw(D_8028A858++, 0xE8000000, 0);
  gRaw(D_8028A858++, 0xF5000100, 0x07000000);
  gRaw(D_8028A858++, 0xE6000000, 0);
  gRaw(D_8028A858++, 0xF0000000, 0x073FC000);
  gRaw(D_8028A858++, 0xE7000000, 0);
  if (D_8028AB0C == 1) {
    BrRomImageDraw(&D_8028C7A8[D_8028AAEC], x, y, D_8028C7A8[D_8028AAEC].w, D_8028C7A8[D_8028AAEC].h,
                  0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
    BrRomImageDraw(&D_8028C7D0[D_8028AAEC], D_8028AE0C[D_8028AAF0->kind].lampX + x,
                  D_8028AE0C[D_8028AAF0->kind].lampY + y, D_8028C7D0[D_8028AAEC].w,
                  D_8028C7D0[D_8028AAEC].h, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
  } else {
    x += D_8028C7A8[D_8028AAEC].w / 4;
    y += D_8028C7A8[D_8028AAEC].h / 4;
    BrRomImageDraw(&D_8028C7A8[D_8028AAEC], x, y, D_8028C7A8[D_8028AAEC].w * 3 / 4,
                  D_8028C7A8[D_8028AAEC].h * 3 / 4, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
    BrRomImageDraw(&D_8028C7D0[D_8028AAEC], D_8028AE0C[D_8028AAF0->kind].lampX * 3 / 4 + x,
                  D_8028AE0C[D_8028AAF0->kind].lampY * 3 / 4 + y, D_8028C7D0[D_8028AAEC].w * 3 / 4,
                  D_8028C7D0[D_8028AAEC].h * 3 / 4, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
  }
  if (D_8026FF10 == 0) {
    v = BrRandStep() & 0x7F;
  } else {
    v = 0x40;
  }
  rev = (float)v + D_8028AAF0->xdf4;
  if (D_8028AE0C[D_8028AAF0->kind].dialMode == 0) {
    q = D_80361B30[D_8028A85C][D_8028AAEC];
    if (2 == D_8028AB0C) {
      x += D_8028AE0C[D_8028AAF0->kind].needleX * 3 / 4;
      y += D_8028AE0C[D_8028AAF0->kind].needleY * 3 / 4;
      tip = 15.0f;
      base = 5.0f;
    } else {
      x += D_8028AE0C[D_8028AAF0->kind].needleX;
      y += D_8028AE0C[D_8028AAF0->kind].needleY;
      tip = 20.0f;
      base = 7.0f;
    }
    a = D_8028AE0C[D_8028AAF0->kind].needleRest;
    if (0.0f < rev) {
      a = a - rev * (a - D_8028AE0C[D_8028AAF0->kind].needleMax) / 8000.0f;
    }
    fx = (float)x;
    fy = (float)(240 - y);
    SET16(q[0].x, (short)(cosf(a - 0.05f) * tip + fx));
    SET16(q[0].z, (short)(0));
    SET16(q[0].y, (short)(sinf(a - 0.05f) * tip + fy));
    SET16(q[1].x, (short)(cosf(a + 0.05f) * tip + fx));
    SET16(q[1].z, (short)(0));
    SET16(q[1].y, (short)(sinf(a + 0.05f) * tip + fy));
    SET16(q[2].x, (short)(cosf(a + 0.3f) * base + fx));
    SET16(q[2].z, (short)(0));
    SET16(q[2].y, (short)(sinf(a + 0.3f) * base + fy));
    SET16(q[3].x, (short)(cosf(a - 0.3f) * base + fx));
    SET16(q[3].z, (short)(0));
    q[0].r = 0;
    q[0].g = 0xFF;
    q[0].b = 0;
    q[0].a = 0xFF;
    q[1].r = 0;
    q[1].g = 0xFF;
    q[1].b = 0;
    q[1].a = 0xFF;
    q[2].r = 0;
    q[2].g = 0xFF;
    q[2].b = 0;
    q[2].a = 0xFF;
    q[3].r = 0;
    q[3].g = 0xFF;
    q[3].b = 0;
    q[3].a = 0xFF;
    SET16(q[3].y, (short)(sinf(a - 0.3f) * base + fy));
    gRaw(D_8028A858++, 0xE7000000, 0);
    gRaw(D_8028A858++, 0xBA001402, 0);
    gRaw(D_8028A858++, 0xFCFFFFFF, 0xFFFE793C);
    gRaw(D_8028A858++, 0xB900031D, 0x00552048);
    gRaw(D_8028A858++, 0xB6000000, 0x00033000);
    gRaw(D_8028A858++, 0xB7000000, 4);
    gRaw(D_8028A858++, 0x0400103F, q);
    gRaw(D_8028A858++, 0xB1000204, 0x0406);
    gRaw(D_8028A858++, 0xE7000000, 0);
  } else if (D_8028AE0C[D_8028AAF0->kind].dialMode == 1 || D_8028AE0C[D_8028AAF0->kind].dialMode == 2) {
    first = (0x100 - D_8028AE0C[D_8028AAF0->kind].needleX) & ~3;
    BrRomRead(&D_80361930[first], D_8028AE0C[D_8028AAF0->kind].dialRom + D_8028AA80 * 0x200 + first * 2,
              (0x100 - first) * 2);
    lit = rev / 8000.0 * (D_8028AE0C[D_8028AAF0->kind].needleX + 1) - 0.5f;
    first = 0x100 - D_8028AE0C[D_8028AAF0->kind].needleX;
    end = first;
    if (lit >= 0) {
      end = lit + first;
    }
    if (end > 0x100) {
      end = 0x100;
    }
    if (first < end) {
      src = &D_80361930[first];
      dst = &D_80361530[D_8028AAEC][first];
      do {
        *dst++ = *src++;
      } while (src < &D_80361930[end]);
    }
    if (D_8028AE0C[D_8028AAF0->kind].dialMode == 1) {
      if (end < 0x100) {
        src = &D_80361930[end];
        dst = &D_80361530[D_8028AAEC][end];
        do {
          SET16(*dst, (BE16(*src) >> 3) & 0x18C6 | 1);
          dst++, src++;
        } while (src < &D_80361930[0x100]);
      }
    } else if (end < 0x100) {
      src = &D_80361930[end];
      dst = &D_80361530[D_8028AAEC][end];
      do {
        SET16(*dst, BE16(*src) & 0xFFFE);
        dst++, src++;
      } while (src < &D_80361930[0x100]);
    }
  }
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept among
 * the race display code. */
/* @implements 0x8023870C tgr BrStub8023870C */
void BrStub8023870C(void)
{
}

/* WHAT IT DOES: Watch whether a car is driving the wrong way: after half a
 * second of facing backwards the WRONG WAY message starts flashing on that
 * player's screen, and it is taken down as soon as the car turns round. */
/* @t4-pass 0x8021F1F0 1 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F1F0 2 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F1F0 3 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021F1F0 tgr BrWrongWayCheck */
void BrWrongWayCheck(BrCar *car)
{
  if (D_80025C70 != 0) {
    if (car->laps < D_8028B304 && car->xf4c == 0 && BrVec3Dot(&car->xf64, (BrVec3 *)car->mtx0[0]) < 0.0  /* the ROM leaves the car in a1: its x axis */) {
      car->wrongWay++;
      if (car->wrongWay >= 32 && (car->wrongWay & 0x10) == 0x10) {
        if (car->msgA == 0) {
          car->msgA = tgr_addr32(D_8028B308);
          car->msgB = 0;
          car->msgATime = 0.25f;
        }
      } else if (car->msgA == tgr_addr32(D_8028B308)) {
        car->msgB = 0;
        car->msgA = 0;
      }
    } else {
      car->wrongWay = 0;
    }
  }
}

/* WHAT IT DOES: Draw the direction arrow for the next turn: picks one of
 * the arrow shapes from the angle of the upcoming bend and draws it tinted
 * for that player. */
/* @t4-pass 0x80233880 1 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233880 2 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233880 3 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80233880 */
/* @implements 0x80233880 tgr BrHudArrowDraw */
/* the car model's display lists and vertices are cartridge data: big-endian */
#define VS(k) ((short)tgr_rd16(psVar2 + (k)))
#define VSET(k, v) tgr_wr16(psVar2 + (k), (unsigned short)(short)(v))
void BrHudArrowDraw(int param_1,float *param_2,short param_3)
{
  short sVar1;
  short *psVar2;
  unsigned int uVar3;
  int iVar4;
  unsigned int *puVar5;
  short sVar7;
  int iVar6;
  unsigned int uVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  int iVar15;
  unsigned int uVar16;
  int iVar17;
  float fVar18;
  int iVar19;
  
  fVar18 = (float)BrAtan2(*param_2,param_2[1]);
  iVar9 = 0x3fff;
  iVar10 = 0xff;
  iVar11 = 0x3fff;
  iVar12 = -0x3fff;
  iVar14 = 0;
  iVar19 = (int)((fVar18 * 180.0f) / 3.1415927f);
  if ((iVar19 < 0x14) || (0x153 < iVar19)) {
    iVar19 = 0;
  }
  else {
    iVar9 = 0x3fff;
    if (iVar19 < 0x32) {
      iVar10 = 0x80;
      iVar11 = 0x3fff;
      iVar12 = 0x40;
      iVar19 = 1;
    }
    else {
      iVar9 = 0x3fff;
      if (iVar19 < 0x82) {
        iVar10 = -0x3fff;
        iVar11 = 0x3fff;
        iVar12 = 0x40;
        iVar19 = 2;
      }
      else {
        iVar9 = -0x80;
        if (iVar19 < 0xa0) {
          iVar10 = -0x3fff;
          iVar11 = 0x3fff;
          iVar12 = 0x40;
          iVar19 = 3;
        }
        else {
          iVar9 = -0xff;
          if (iVar19 < 200) {
            iVar10 = -0x3fff;
            iVar11 = 0x3fff;
            iVar12 = -0x3fff;
            iVar19 = 4;
          }
          else {
            iVar9 = -0x80;
            if (iVar19 < 0xe6) {
              iVar10 = -0x3fff;
              iVar11 = -0x40;
              iVar12 = -0x3fff;
              iVar19 = 5;
            }
            else {
              iVar9 = 0x3fff;
              if (iVar19 < 0x136) {
                iVar9 = 0x3fff;
                iVar10 = -0x3fff;
                iVar11 = -0x40;
                iVar12 = -0x3fff;
                iVar19 = 6;
              }
              else {
                iVar10 = 0x80;
                iVar11 = -0x40;
                iVar12 = -0x3fff;
                iVar19 = 7;
              }
            }
          }
        }
      }
    }
  }
  iVar19 = param_1 + iVar19 * 2;
  sVar7 = *TGR_PTR(short *, (iVar19 + 0x207c));
  if (sVar7 < 0x100) {
    sVar1 = (short)((unsigned int)((int)param_3 << 0x12) >> 0x10);
    *TGR_PTR(short *, (iVar19 + 0x207c)) = sVar7 + sVar1;
    *TGR_PTR(unsigned short *, (param_1 + 0x208c)) = (*TGR_PTR(short *, (param_1 + 0x208c)) + 5U & 7) - 4;
    iVar19 = (int)*TGR_PTR(short *, (param_1 + 0x208c));
    iVar17 = (int)sVar1;
    if (iVar19 < 0) {
      iVar19 = iVar19 + 1;
    }
    fVar18 = param_2[1];
    sVar7 = sVar1 >> 1;
    if (fVar18 <= 1.0f) {
      if (0.0f < fVar18) {
        iVar17 = (int)sVar7;
      }
      else {
        iVar17 = (int)-sVar7;
        if (fVar18 < -1.0f) {
          iVar17 = (int)-sVar1;
        }
      }
    }
    fVar18 = param_2[2];
    if (1.0f < fVar18) {
      iVar4 = (int)(sVar1 >> 2);
    }
    else if (0.0f < fVar18) {
      iVar4 = (int)(sVar1 >> 3);
    }
    else {
      iVar4 = (int)(short)-(sVar1 >> 3);
      if (fVar18 < -1.0f) {
        iVar4 = (int)(short)-(sVar1 >> 2);
      }
    }
    fVar18 = *param_2;
    iVar15 = (int)sVar1;
    if (fVar18 <= 1.25f) {
      if (0.0f < fVar18) {
        iVar15 = (int)sVar7;
      }
      else {
        iVar15 = (int)-sVar1;
        if (fVar18 < -1.25f) {
          iVar15 = (int)-sVar1;
          iVar4 = (iVar4 << 0x11) >> 0x10;
        }
      }
    }
    iVar13 = 0;
LAB_80233be0:
    do {
      if ((iVar13 != 9) &&
         (puVar5 = TGR_PTR(unsigned int *, tgr_rd32(TGR_PTR(void *, (*TGR_PTR(int *, (param_1 + 0x2078)) + iVar14 * 0x28 + iVar13 * 4 + 0x18)))),
         puVar5 != (unsigned int *)0x0)) {
LAB_80233c10:
        do {
          uVar3 = tgr_rd32(puVar5);
          while( 1 ) {
            if (uVar3 >> 0x18 != 4) break;
            uVar3 = uVar3 >> 10 & 0x3f;
            psVar2 = TGR_PTR(short *, tgr_rd32(puVar5 + 1));
            puVar5 = puVar5 + 2;
            if (uVar3 == 0) goto LAB_80233c10;
            do {
              uVar3 = uVar3 - 1;
              sVar7 = VS(0);
              uVar16 = (sVar7 + iVar19) * 0x10000 >> 0x10;
              if ((((iVar10 < (int)uVar16) && ((int)uVar16 < iVar9)) &&
                  (uVar8 = (VS(1) + iVar19) * 0x10000 >> 0x10, iVar12 < (int)uVar8)) &&
                 ((((int)uVar8 < iVar11 &&
                   (iVar6 = (VS(2) + iVar19) * 0x10000 >> 0x10, -0x30 < iVar6)) &&
                  (iVar6 < 0xe0)))) {
                if ((uVar8 & 0x80) == 0) {
                  VSET(0, sVar7 + (short)((int)(iVar15 * ((uVar8 & 0xf) - 0xc)) >> 5));
                }
                else {
                  VSET(0, sVar7 + (short)((int)(iVar15 * (4 - (uVar8 & 0xf))) >> 5));
                }
                if ((uVar16 & 0x80) == 0) {
                  VSET(1, VS(1) + (short)((int)(iVar17 * ((uVar16 & 0xf) - 0xc)) >> 5));
                }
                else {
                  VSET(1, VS(1) + (short)((int)(iVar17 * (4 - (uVar16 & 0xf))) >> 5));
                }
                uVar16 = (int)((uVar16 + uVar8) * 0x10000) >> 0x10;
                if ((uVar16 & 0x80) == 0) {
                  VSET(2, VS(2) + (short)((int)(iVar4 * ((uVar16 & 0xf) - 8)) >> 6));
                }
                else {
                  VSET(2, VS(2) + (short)((int)(iVar4 * (8 - (uVar16 & 0xf))) >> 6));
                }
              }
              psVar2 = psVar2 + 8;
            } while (uVar3 != 0);
            uVar3 = tgr_rd32(puVar5);
          }
          if (uVar3 >> 0x18 == 0xb8) break;
          puVar5 = puVar5 + 2;
        } while( 1 );
      }
      iVar13 = iVar13 + 1;
    } while (iVar13 != 10);
    iVar14 = iVar14 + 1;
    if (iVar14 != 3) {
      iVar13 = 0;
      goto LAB_80233be0;
    }
  }
}
#undef VS
#undef VSET

/* WHAT IT DOES: Draw a race time as minutes, seconds and hundredths under
 * its label.  No named locals: the ROM's frame holds only the buffer, and
 * the seconds and minutes are the one expression CSE'd. */
/* @t4-pass 0x80238714 1 2026-09-26 compiles 17 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 2 2026-09-26 compiles 16 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 3 2026-09-26 compiles 13 best 70 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238714 tgr BrHudTimeDraw */
void BrHudTimeDraw(char *label, char *prefix, float t, int x, int y)
{
  char buf[44];

  sprintf(buf, "%s%d'%02d\"%02d", prefix,
                ((int)(t * 100.0f) / 100) / 60, ((int)(t * 100.0f) / 100) - ((int)(t * 100.0f) / 100) / 60 * 60, (int)(t * 100.0f) - ((int)(t * 100.0f) / 100) * 100);
  BrTextPrint(buf, x, y + 15);
  BrTextPrint(label, x, y);
}

/* WHAT IT DOES: Draw the race times panel by race mode: the total time
 * (single-player layout only), then the lap time, best lap or time left,
 * placed below the top of the player's view. */
/* @t4-pass 0x8023880C 1 2026-09-26 compiles 16 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 2 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 3 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8023880C tgr BrHudTimesDraw */
void BrHudTimesDraw(void)
{
  int y;
  BrCar *car;                   /* declared, never used: its slot is in the frame */
  int dy;

  if (D_8028AB0C == 1) {
    dy = 30;
  } else {
    dy = 0;
  }
  y = D_8031B2C8[D_8028AAEC].y + 20;
  BrTextHighlightOff();
  BrTextAlignRight();
  BrTextSetFont(15);
  switch (D_8026FF18) {
  case 0:
  case 2:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15TOTAL TIME", (char *)&D_802AA060, D_8028AAF0->lapTime, 296, y);
    }
    if (D_8028AAF0->laps >= D_8028B304) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA070, D_8028AAF0->xf98, 296, y + dy);
    } else {
      BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA080, D_8028AAF0->raceTime, 296, y + dy);
    }
    break;
  case 1:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15TOTAL TIME", (char *)&D_802AA094, D_8028AAF0->lapTime, 296, y);
    }
    if (D_8028AAF0->laps >= D_8028B304) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA0A4, D_8028AAF0->xf98, 296, y + dy);
    } else if (D_8026FF08 == 1) {
      BrHudTimeDraw("%15TIME LEFT", (char *)&D_802AA0B8, D_8028AAF0->xfa4, 296, y + dy);
    } else {
      BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA0C8, D_8028AAF0->raceTime, 296, y + dy);
    }
    break;
  case 3:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA0D8, D_8028AAF0->xf98, 296, y);
    }
    BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA0E8, D_8028AAF0->raceTime, 296, y + dy);
    break;
  }
}

/* WHAT IT DOES: Draw the lap counter and the race position.  Outside race
 * mode 3: unless the view is the car's third camera, the lap counter
 * (n/m, or FINISHED once done; shown finished only in the single-player
 * layout) at the top left; then the position number at the bottom left
 * with its st/nd/rd/th suffix after it, sized for the layout. */
/* @t4-pass 0x80238AB8 1 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 2 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 3 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238AB8 tgr BrHudLapDraw */
void BrHudLapDraw(void)
{
  char buf[20];
  char *suffix;
  int x;
  int y;
  int adj;
  int w;

  if (D_8026FF18 != 3) {
    x = D_8031B2C8[0].x + 16;
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      if (D_8028AAF0->laps < D_8028B304 || D_8028AB0C == 1) {
        y = D_8031B2C8[D_8028AAEC].y + 5;
        if (D_8028AAF0->laps < D_8028B304) {
          sprintf(buf, "%%y1%s%d/%d", D_8028AB0C == 2 ? D_802AA0F8 : D_802AA0FC,
                  D_8028AAF0->laps + 1, D_8028B304);
        } else {
          sprintf(buf, "FINISHED");
        }
        y += 15;
        BrTextHighlightOff();
        BrTextAlignLeft();
        BrTextSetFont(15);
        BrTextPrint(buf, x, y);
      }
    }
    y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h - 12;
    x -= 2;
    BrTextAltBlendOn();
    BrTextAlignLeft();
    BrTextSetColours(0xff, 0xf0, 0x7d, 0xff, 0x78, 0);
    sprintf(buf, D_802AA110, D_8028AAF0->xfac + 1);
    adj = 0;
    switch (D_8028AAF0->xfac) {
    case 0:
      suffix = D_802AA114;
      adj = -3;
      break;
    case 1:
      suffix = D_802AA118;
      adj = 1;
      break;
    case 2:
      suffix = D_802AA11C;
      break;
    default:
      suffix = D_802AA120;
      adj = 1;
      break;
    }
    if (D_8028AB0C == 1) {
      BrTextSetFont(40);
      w = BrTextWidth(buf, 40);
      BrTextPrint(buf, x - 1, y - 1);
      BrTextSetFont(20);
      BrTextPrint(suffix, x + adj + w + 3, y - 15);
    } else {
      BrTextSetFont(26);
      w = BrTextWidth(buf, 26);
      BrTextPrint(buf, x, y);
      BrTextSetFont(13);
      BrTextPrint(suffix, x + 3 + adj * 2 / 3 + w, y - 10);
    }
    BrTextAltBlendOff();
  }
}

/* WHAT IT DOES: Draw the car's current message (the first, else the
 * second) centred in the view, a third of the way down, sized for one or
 * two players, unless the display is switched off. */
/* @t4-pass 0x80238DD4 1 2026-09-26 compiles 17 best 75 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 2 2026-09-26 compiles 17 best 76 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 3 2026-09-26 compiles 17 best 76 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238DD4 tgr BrHudPositionDraw */
void BrHudPositionDraw(void)
{
  int x;
  int y;
  int big;
  int small;

  if (D_8026FF10 == 0) {
    if (D_8028AB0C == 1) {
      big = 30;
      small = 20;
    } else {
      big = 20;
      small = 15;
    }
    x = D_8031B2C8[D_8028AAEC].x + D_8031B2C8[D_8028AAEC].w / 2;
    y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h / 3;
    BrTextAlignCentre();
    if (D_8028AAF0->msgA != 0) {
      BrTextSetFont(big);
      BrTextPrint(TGR_PTR(unsigned char *, D_8028AAF0->msgA), x, big / 4 + y);
    } else if (D_8028AAF0->msgB != 0) {
      BrTextSetFont(small);
      BrTextPrint(TGR_PTR(unsigned char *, D_8028AAF0->msgB), x, big * 3 / 16 + y);
    }
  }
}


/* WHAT IT DOES: The race HUD for the view being drawn: clip to the view,
 * the arrow and times panels (not from the car's third camera), the lap
 * counter and position and the car's message, then its speed (never
 * negative) in kph or mph at the bottom right -- smaller in the two-player
 * layout, with the units under it in the one-player one.  (From the third
 * camera in the one-player layout x keeps its first value, 296.) */
/* @implements 0x80238F30 tgr BrHudDraw */
void BrHudDraw(void)
{
  float speed;
  int x;
  int y;

  speed = D_8028AAF0->xfe4[0];
  BrScissorSet(8, D_8031B2C8[D_8028AAEC].y, 312, D_8031B2C8[D_8028AAEC].h);
  if (speed < 0.0) {
    speed = 0.0f;
  }
  if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
    BrHudDialDraw();
    BrHudTimesDraw();
  }
  BrHudLapDraw();
  BrHudPositionDraw();
  BrTextHighlightOff();
  BrTextAlignRight();
  if (D_802723D8 != 0) {
    sprintf(D_80361C30, "%%yw%.0f", speed / 1.609344f);
  } else {
    sprintf(D_80361C30, "%%yw%.0f", speed);
  }
  x = 296;
  y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h - 4;
  if (D_8028AB0C == 2) {
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      y = y - D_8028C7B4[D_8028AAEC].h * 3 / 4 - 1;
    }
    BrTextSetFont(15);
    BrTextPrint((unsigned char *)D_80361C30, x, y);
  } else {
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      x = 266;
      y = y - D_8028C7B4[D_8028AAEC].h;
    }
    BrTextSetFont(20);
    if (D_802723D8 != 0) {
      BrTextPrint((unsigned char *)D_80361C30, x, y - 3);
    } else {
      BrTextPrint((unsigned char *)D_80361C30, x - 3, y - 3);
    }
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      BrTextSetFont(15);
      BrTextAlignLeft();
      if (D_802723D8 != 0) {
        BrTextPrint((unsigned char *)"%wwmph", x, y - 3);
      } else {
        BrTextPrint((unsigned char *)"%wwkph", x - 3, y - 3);
      }
    }
  }
}

/* WHAT IT DOES: Draw a tw by th 4-bit intensity texture as a w by h
 * rectangle at (x, y), upside down (t starts at the bottom row and steps
 * back), tinted red (primitive 255, 88, 88), in 320-wide coordinates
 * doubled on a hi-res screen: the texture is loaded as one block with its
 * wrap masks from BrTexSizeBits, and perspective correction is off for the
 * rectangle and back on after it.  A negative w is taken as its size.  The
 * tile size is written out as a block: the macro's one-line form stores
 * its second word before taking the packet pointer, the ROM after. */
/* @implements 0x80239220 tgr BrTex4bFlipDraw */
void BrTex4bFlipDraw(void *img, int tw, int th, int x, int y, int w, int h)
{
  int ms;
  int mt;
  int ss;
  int st;
  int spare[2];                 /* declared, never used: the frame holds it */

  if (w < 0) {
    w = -w;
  }
  BrTexSizeBits(tw, &ss, &ms);
  BrTexSizeBits(th, &st, &mt);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, ss, st, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0x309861, 0x5532ff7f);
  gDPSetRenderMode(D_8028A858++, 0xf0a4000, 0);
  gDPSetTextureImage(D_8028A858++, 4, G_IM_SIZ_16b, 1, img);
  gDPSetTile(D_8028A858++, 4, G_IM_SIZ_16b, 0, 0, 7, 0, 0, mt, 0, 0, ms, 0);
  gDPLoadSync(D_8028A858++);
  gDPLoadBlock(D_8028A858++, 7, 0, 0, ((tw * th + 3) >> 2) - 1,
               ((1 << 11) + (tw / 16 < 1 ? 1 : tw / 16) - 1) / (tw / 16 < 1 ? 1 : tw / 16));
  gDPPipeSync(D_8028A858++);
  gDPSetTile(D_8028A858++, 4, 0, ((tw >> 1) + 7) >> 3, 0, 0, 0, 0, mt, 0, 0, ms, 0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(0, 12, 12) | _SHIFTL(0, 0, 12));
    tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL((tw - 1) << 2, 12, 12) | _SHIFTL((th - 1) << 2, 0, 12));
  }
  gDPSetTextureLUT(D_8028A858++, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    w <<= 1;
    h <<= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0x58, 0x58, 0xff);
  gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12)));
  }
  gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(0, 16, 16) | _SHIFTL((th - 1) << 5, 0, 16)));
  gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL(((tw << 10) - (1 << 10)) / w, 16, 16) | _SHIFTL(((1 << 10) - (th << 10)) / h, 0, 16)));
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}

/* WHAT IT DOES: BrTex4bFlipDraw with a colour and a mirror: the texture is
 * shaded by the primitive colour (r, g, b) instead of tinted red, and a
 * negative w also mirrors it (s starts at the right column and steps back).
 * Drawn upside down, in 320-wide coordinates doubled on a hi-res screen,
 * the texture loaded as one block with its wrap masks from BrTexSizeBits.
 * The mirror's s start and step are conditionals inside the two RDP half
 * words (IDO branches there, after the command word); the tile size is
 * written out as a block (see BrTex4bFlipDraw). */
/* @implements 0x80239750 tgr BrTex4bFlipDrawRGB */
void BrTex4bFlipDrawRGB(void *img, int tw, int th, int x, int y, int w, int h, int r, int g, int b)
{
  int ms;
  int mt;
  int ss;
  int st;
  int mirror;

  mirror = 0;
  if (w < 0) {
    w = -w;
    mirror = 1;
  }
  BrTexSizeBits(tw, &ss, &ms);
  BrTexSizeBits(th, &st, &mt);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, ss, st, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf2f9);
  gDPSetRenderMode(D_8028A858++, 0x504240, 0);
  gDPSetTextureImage(D_8028A858++, 4, G_IM_SIZ_16b, 1, img);
  gDPSetTile(D_8028A858++, 4, G_IM_SIZ_16b, 0, 0, 7, 0, 0, mt, 0, 0, ms, 0);
  gDPLoadSync(D_8028A858++);
  gDPLoadBlock(D_8028A858++, 7, 0, 0, ((tw * th + 3) >> 2) - 1,
               ((1 << 11) + (tw / 16 < 1 ? 1 : tw / 16) - 1) / (tw / 16 < 1 ? 1 : tw / 16));
  gDPPipeSync(D_8028A858++);
  gDPSetTile(D_8028A858++, 4, 0, ((tw >> 1) + 7) >> 3, 0, 0, 0, 0, mt, 0, 0, ms, 0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(0, 12, 12) | _SHIFTL(0, 0, 12));
    tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL((tw - 1) << 2, 12, 12) | _SHIFTL((th - 1) << 2, 0, 12));
  }
  gDPSetTextureLUT(D_8028A858++, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    w <<= 1;
    h <<= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12)));
  }
  gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(mirror ? (tw - 1) << 5 : 0, 16, 16) | _SHIFTL((th - 1) << 5, 0, 16)));
  gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL(mirror ? ((1 << 10) - (tw << 10)) / w : ((tw << 10) - (1 << 10)) / w, 16, 16) | _SHIFTL(((1 << 10) - (th << 10)) / h, 0, 16)));
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}
