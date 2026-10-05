/* frameloop.c -- one object of the ROM, 0x80219470-0x8021C46B: beginning and
 * ending each frame's display list (frame buffers, scissor, viewports and
 * outlines, animated textures), reading the pads, the cameras and frustum,
 * the race-flag display-list patchers, the RSP/RDP/retrace event threads,
 * the scheduler set-up and the boot-time pak check.
 *
 * Why one file: IDO pads an infinite loop's dead epilogue to 32 bytes from
 * the start of the object's .text, so the event threads only match where
 * they sit in the original object (it starts at 16 mod 32).  Gathered here
 * from frameblank.c, viewport.c, padread.c, camera.c, threads.c and
 * carcam.c in ROM order; BrSpEventThread then matches.  Where those files
 * declared one symbol with different types the types are unified (pointer
 * prototypes take void *, the track header is one struct).
 */
#include "tgr/car.h"
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */

void BrFrameBegin(int hires);
void BrScissorSet(int x, int y, int w, int h);
void BrFrameStatsReset(void);
void BrScreenClear(int r, int g, int b);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
extern int D_8028A884;
void BrFrameBeginLayout0(void);
extern int D_8028AA08;
extern int D_8028AA0C;
extern int D_8028AA10;
extern int D_8028AA2C;
extern int D_8028AA30;
extern int D_8028AA34;
extern int D_8028AA38;
extern int D_8028AA3C;
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028B740;
extern int D_8028B744;
extern int D_8028B748;
extern int D_8028B74C;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern int D_8028A85C;                 /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];      /* the frame buffers */
#define D_00000400 0x400                 /* the Z-buffer: physical 0x400 (a link-time address) */
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrFramePoolsReset(void);
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AB0C;                  /* number of players */
extern int D_8028A84C;                  /* resolution changed */
extern Gfx (*D_8028A848)[6000];         /* the two display-list buffers */
extern int D_8028AA68;                  /* texture filtering on */
extern int D_8028A898;                  /* the texture filter mode */
extern int D_8028A8A0;                  /* the colour dither mode */
extern int D_8028A89C;                  /* the alpha dither mode */
extern int D_8028AA50;                  /* z-buffering on */
extern int D_8028AA4C;                  /* gamma off */
extern int D_8028A8A4;                  /* the viewport being used */
extern char D_8028A900[];               /* the viewports (0x28 bytes each) */
extern char D_801B5000[];               /* the low-res frame buffers */
extern char D_801DA800[];
#define D_0028A8C0 0x28A8C0        /* a ROM offset */
typedef struct BrTask {         /* an OSTask (0x40 bytes) */
  unsigned int type;
  unsigned int flags;
  TgrAddr ucode_boot;  /* void * */
  unsigned int ucode_boot_size;
  TgrAddr ucode;  /* void * */
  unsigned int ucode_size;
  TgrAddr ucode_data;  /* void * */
  unsigned int ucode_data_size;
  TgrAddr dram_stack;  /* void * */
  unsigned int dram_stack_size;
  TgrAddr output_buff;  /* void * */
  TgrAddr output_buff_size;  /* void * */
  TgrAddr data_ptr;  /* void * */
  unsigned int data_size;
  TgrAddr yield_data_ptr;  /* void * */
  unsigned int yield_data_size;
} BrTask;
extern BrTask D_8031A9A8[2];            /* one graphics task per display list */
extern char rspbootTextStart[];
extern char rspbootTextEnd[];
extern char gspF3DEX_fifoTextStart[];
extern char gspF3DEX_fifoDataStart[];
extern char D_8031A598[];               /* the RSP's DRAM stack */
extern long long *D_8028A860[2];        /* the RDP FIFO: start and end */
extern int D_8028AB84;                  /* FIFO words kept back */
extern int D_8028AB70;                  /* display-list commands this frame */
extern int D_8028AB7C;                  /* the most commands in one frame */
extern int *D_8028C75C;                 /* matrix pool: next and start */
extern int *D_8028C760;
extern int D_8028AB74;                  /* the most matrices in one frame */
typedef struct { char x[16]; } BrVtx16;
extern BrVtx16 *D_8028C764;             /* vertex pool: next and start */
extern BrVtx16 *D_8028C768;
extern int D_8028AB78;                  /* the most vertices in one frame */
extern void (*D_8031B31C)(void);        /* run once the RSP has finished */
extern void (*D_8031B320)(void);
extern int D_8028AA94;                  /* debug: copy the low-memory frame into the display */
extern int D_80000400[];
extern int D_8028AAE0;                  /* CPU time of the last frame */
extern int D_8028AAE4;
extern int D_8028AAE8;
extern int D_8028A854;                  /* resolution the VI is set to */
extern int osTvType;
extern char D_802A5D70[];               /* VI modes: low res MPAL, NTSC; high res MPAL, NTSC */
extern char D_802A54B0[];
extern char D_802A6040[];
extern char D_802A5780[];
extern int D_8028A888;                  /* frames the screen stays blank */
extern int D_8028AA20;
extern int D_8028AA24;
void BrFatal(char *msg);
void BrPerfFrameStart(void);

extern int D_8028AB60;
extern int D_8028AB64;
extern int D_8028AB68;
extern Vp D_80318CD0[32];
extern int D_8028A8A8;
extern int D_8028A8AC;

extern int D_8028AB6C;
extern short D_802A4BE8;
void BrPadStickRepeatPos(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
void BrPadStickRepeatNeg(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
extern float D_802AB414;
extern float D_802AB418;
extern float D_802AB41C;
extern float D_802AB420;
extern int D_8028AADC;
void BrPadStartRead(void);
extern OSContPad D_8031A3E0[4];
void BrPadMapRead(unsigned int *param_1);
void BrPadEdges(unsigned int *param_1);
extern unsigned int D_8036A8E0[4][0x57];

void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt, float yAt, float zAt,
               float xUp, float yUp, float zUp);
void guPerspectiveF(float mf[4][4], unsigned short *perspNorm, float fovy, float aspect, float near,
                    float far, float scale);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
Mtx *BrMtxAlloc(void);
extern int D_8028A868;
extern float D_8028A86C;
extern float D_8028A870;
extern unsigned short D_8028A874;
extern Mtx *D_8028A878;
extern float D_8031AA50[4][4];
extern float D_8031AA90[4][4];
extern float D_8031AAD0[4][4];
extern float D_8031AB50[4][4];
float sinf(float x);
extern float D_8028AAC4;                /* the view frustum's field of view */
extern float D_8028AACC;                /* and its depth */
typedef struct BrFrustum {
  float eye[3];                         /* 0x00 */
  float corner[4][3];                   /* 0x0C  pulled three quarters of the way to the eye */
  float centre[3];                      /* 0x3C  the far plane's centre */
} BrFrustum;
extern BrFrustum D_8031B1F0;
extern float D_8031ABD0[3];             /* half the far plane's width, along the side axis */
extern float D_8031ABE0[3];             /* half its height, along the up axis */
extern float D_8031ABF0[3];             /* its centre */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
extern int D_8028AAEC;                  /* the view being drawn */
extern int D_8028A8A8;                  /* mirror flags: they differ when the view is mirrored */


#include "tgr/track.h"

extern int D_8031A320[6];
extern int D_8031A338[6];
extern int D_8031A354[1];
extern int D_8031A358[6];
extern int D_8031A370[6];
extern int D_8031A38C[1];
extern int D_8031A390[6];
extern int D_8031A3B0[6];
extern int D_8031A3C8[1];
extern int D_8031A3CC;
typedef struct BrContStatus {   /* an OSContStatus */
  unsigned short type;
  unsigned char status;
  unsigned char errno;
} BrContStatus;
typedef struct { char raw[0x68]; } BrPfs;   /* an OSPfs */
void BrStub802607AC(void);
extern char D_80272D68[];
extern int D_80319F88[6];
extern int D_80319FA0[32];
extern int D_8031A350[1];
extern int D_8031A388[1];
extern int D_8031A3A8[1];
extern char D_802729E0[];
extern char D_80272B90[];
extern void *D_8026FF04;
extern char D_803196D0[];
extern char D_80319ED0[];
extern float D_8028A8B0;
extern int D_80272D48[6];
extern int D_80272D60[1];
extern BrContStatus D_8031A3D0[4];
extern BrPfs D_8031A3F8[4];
extern unsigned char D_8031B1E8[4];
void BrPadPollAll(void);
void BrModeSet(void (*fn)(void));
void BrPakWarnScreen(void);
void BrPakManager(void);
int BrPakCheckFiles(void);
void BrPadConsume(void *pad, int button);
extern void (*D_8031B318)(void);
extern char D_80270840;
extern int D_802724F0;
extern char D_803163E0[];
extern char D_80316400[];
extern unsigned char D_80316420;
extern unsigned char D_80316421;
extern int D_8036A908;
extern BrPfs D_80369EC0[2];
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
extern int D_8028ADE8;
extern int D_8028B7F4;
extern int D_8028B940;
extern unsigned int D_8028ABA8[6][4][2];
extern unsigned int D_8028AC68[6][4][2];
extern unsigned int D_8028AD28[6][4][2];

void BrCarPlayerCtl(void *);
extern float D_8031B1D8[];
void BrVec3MulAdd(void *pOut, void *pA, void *pB, float s);
void BrVec3MulAddTo(BrVec3 *pA, BrVec3 *pB, float s);
void BrVec3AddTo(void *pA, void *pB);
void BrVec3SubFrom(void *pA, void *pB);
extern int D_8026FF18;                  /* the game mode */
extern int D_80270788;
void BrVec3Sub(void *out, void *a, void *b);
float BrVec3Length(BrVec3 *v);
void BrVec3Div(BrVec3 *out, BrVec3 *v, float d);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3Scale(void *out, void *v, float s);
void BrVec3ScaleBy(BrVec3 *v, float s);
void BrVec3Lerp(void *out, void *a, void *b, float t);
extern int D_8028AB0C;                  /* the close camera mode */
typedef struct BrCamPlane {     /* a collision triangle's plane (0x20 bytes) */
  BrVec3 n;                     /* its normal */
  float d;
  TgrAddr v0;                   /* BrVec3 * -- 0x10  its corners */
  TgrAddr v1;  /* BrVec3 * */
  TgrAddr v2;  /* BrVec3 * */
  int x1c;
} BrCamPlane;
extern BrCamPlane D_80379F80[][150];    /* each grid cell's collision planes */
extern unsigned short D_8037EA88[];     /* and how many */
extern int D_8028B710;                  /* the camera was pushed out of a wall */
short BrCollGridCellAcquire(float, float);
float BrVec3Length(BrVec3 *v);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
void BrVec3Add(void *out, void *a, void *b);
int BrTriContainsPoint(BrVec3 *pPt, BrVec3 *pA, BrVec3 *pB, BrVec3 *pC, BrVec3 *pRef);
extern int D_8028B7F8;                  /* camera hold timers */
extern int D_8028B7FC;
extern float D_8028AAC0;                /* the lens scale */
void BrCarCamTargetStep(BrCar *car);
void BrCarCamWallPush(BrCar *car, BrCarCam *cam, BrVec3 *prev);
void BrCarCamPlaceBehind(BrCar *car, BrCarCam *cam, float t);
void BrCarCamLookAt(BrCar *car, BrCarCam *cam);
void BrVec3Negate(BrVec3 *out, BrVec3 *v);
void BrVec3Normalise(BrVec3 *v);
void BrVec3DivBy(BrVec3 *v, float d);
float cosf(float x);
typedef struct BrCamView {      /* the lens offsets in a car's model buffer */
  char pad00[0xB0];
  float xb0;                    /* 0xB0 */
  float xb4;                    /* 0xB4 */
  float xb8;                    /* 0xB8 */
} BrCamView;
/* -- end declarations -- */

/* WHAT IT DOES: Start a frame at the given resolution (0 low, 1 high):
 * lay out the players' views, reset the frame counters and pools, choose
 * the frame buffers when the resolution has changed, and open this frame's
 * display list with the fixed RDP and RSP state: pipe sync, full-screen
 * scissor, the render and combine modes, texture filtering and dither,
 * geometry flags, the viewport, texturing off, the colour image and the
 * depth image; then set the VI's dither and gamma features.  The high-res
 * frame buffers sit below the low-res pair, computed from the two link
 * symbols as integers.  The resolution test is a subtraction (as is the
 * changed-flag test below): its value takes a temporary, which the
 * ROM's register numbering counts.  The split-screen width is computed in
 * one step, so the view table base outranks it for v0. */
/* @implements 0x80219470 tgr BrFrameBegin */
void BrFrameBegin(int hires)
{
  int x;
  int w;
  int h;

  if (hires - D_8028A850) {
    D_8028A84C = 1;
    D_8028A850 = hires;
  }
  BrPerfMark(0, 0, 0x82, 0, 0xff);
  switch (D_8028AB0C) {
  case 1:
    x = 8;
    D_8031B2C8[0].x = x;
    D_8031B2C8[0].y = x;
    D_8031B2C8[0].w = D_8028AAB0 - 16;
    D_8031B2C8[0].h = D_8028AAB4 - 16;
    break;
  case 2:
    x = 8;
    D_8031B2C8[1].x = x;
    D_8031B2C8[1].y = (D_8028AAB4 >> 1) + 1;
    w = D_8028AAB0 - 0x60;
    h = (D_8028AAB4 >> 1) - 8;
    D_8031B2C8[1].h = h;
    D_8031B2C8[1].w = w;
    D_8031B2C8[0].x = x;
    D_8031B2C8[0].y = x;
    D_8031B2C8[0].w = w;
    D_8031B2C8[0].h = h;
    break;
  }
  BrFrameStatsReset();
  BrFramePoolsReset();
  if (D_8028A84C != 0) {
    if (!(D_8028A84C - 1)) {
      if (D_8028A850 != 0) {
        D_8031AA28[0] = tgr_addr32(D_801B5000) - (tgr_addr32(D_801DA800) - tgr_addr32(D_801B5000)) * 6;
        D_8031AA28[1] = tgr_addr32(D_801B5000) - (tgr_addr32(D_801DA800) - tgr_addr32(D_801B5000)) * 2;
      } else {
        D_8031AA28[0] = tgr_addr32(D_801B5000);
        D_8031AA28[1] = tgr_addr32(D_801DA800);
      }
    }
  }
  D_8028A858 = D_8028A848[D_8028A85C] + 0x40;
  if (D_8028AA68 != 0) {
    D_8028A898 = 0x2000;
  } else {
    D_8028A898 = 0;
  }
  D_8028A8A0 = 0x40;
  D_8028A89C = 0;
  gRaw(D_8028A858++, 0xbc000006, 0);
  gRaw(D_8028A858++, 0xe7000000, 0);
  BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);
  gRaw(D_8028A858++, 0xfcffffff, 0xfffdf638);
  gRaw(D_8028A858++, 0xba001001, 0);
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001102, 0);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
  gRaw(D_8028A858++, 0xba000c02, D_8028A898);
  gRaw(D_8028A858++, 0xba000903, 0xc00);
  gRaw(D_8028A858++, 0xba000801, 0);
  gRaw(D_8028A858++, 0xb9000002, 1);
  gRaw(D_8028A858++, 0xb900031d, 0xf0a4000);
  gRaw(D_8028A858++, 0xba000602, D_8028A8A0);
  gRaw(D_8028A858++, 0xba000602, D_8028A89C);
  gRaw(D_8028A858++, 0xba001402, 0);
  gRaw(D_8028A858++, 0xf9000000, 0);
  gRaw(D_8028A858++, 0x1020040, D_0028A8C0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xb6000000);
    tgr_wr32(&_g->words.w1, TGR_W1((0x1f3204)));
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xb7000000);
    tgr_wr32(&_g->words.w1, TGR_W1((0x2000)));
  }
  if (D_8028AA50 != 0) {
    gRaw(D_8028A858++, 0xb7000000, 0x800000);
  } else {
    gRaw(D_8028A858++, 0xb6000000, 0x800000);
  }
  gRaw(D_8028A858++, 0x6000000, D_8028A900 + D_8028A8A4 * 0x28);
  gRaw(D_8028A858++, 0xbb000000, 0);
  gDPSetColorImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8028AAB0 << D_8028A850, D_8031AA28[D_8028A85C] + 0x80000000);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xfe000000);
    tgr_wr32(&_g->words.w1, TGR_W1((D_00000400)));
  }
  osViSetSpecialFeatures(0x40);
  osViSetSpecialFeatures(0x10);
  osViSetSpecialFeatures(D_8028AA4C != 0 ? 1 : 2);
}

/* WHAT IT DOES: Start building a new frame using the first of the two
 * screen-buffer layouts. */
/* @implements 0x80219A1C tgr BrFrameBeginLayout0 */
void BrFrameBeginLayout0(void)
{
  BrFrameBegin(0);
}

/* WHAT IT DOES: Start building a new frame using the second of the two
 * screen-buffer layouts. */
/* @implements 0x80219A3C tgr BrFrameBeginLayout1 */
void BrFrameBeginLayout1(void)
{
  BrFrameBegin(1);
}

/* WHAT IT DOES: Does nothing: a variadic debug hook compiled empty (it
 * still spills its register arguments).  The PC twin is BrStub10008B80. */
/* @implements 0x80219A5C tgr BrStub80219A5C */
void BrStub80219A5C(int a0, ...)
{
}

/* WHAT IT DOES: Set the RDP scissor to a w by h box at (x, y), clipped to
 * the current clip rectangle, in 320-wide coordinates doubled on a hi-res
 * screen. */
/* @implements 0x80219A78 tgr BrScissorSet */
void BrScissorSet(int x, int y, int w, int h)
{
  if (x < D_8028B740) {
    w = w - D_8028B740 + x;
    x = D_8028B740;
  }
  if (x + w > D_8028B744) {
    w = D_8028B744 - x;
  }
  if (w < 0) {
    w = 0;
  }
  if (y < D_8028B748) {
    h = h - D_8028B748 + y;
    y = D_8028B748;
  }
  if (y + h > D_8028B74C) {
    h = D_8028B74C - y;
  }
  if (h < 0) {
    h = 0;
  }
  if (D_8028A850 != 0) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetScissor(D_8028A858++, G_SC_NON_INTERLACE, x, y, x + w, y + h);
}

/* WHAT IT DOES: Take the next viewport in the ring and set it to a w by h
 * view at (x, y) (320-wide coordinates, doubled on a hi-res screen),
 * optionally scissoring to it.  A negative width mirrors the view (the
 * mirror flag is kept, and a second flag flips the sign again); load it and
 * keep its depth scale and offset. */
/* @implements 0x80219BF0 tgr BrViewportSet */
void BrViewportSet(int x, int y, int w, int h, int scissor)
{
  D_8028AB60 = (D_8028AB60 + 1) & 0x1f;
  if (scissor != 0) {
    BrScissorSet(x, y, w < 0 ? -w : w, h);
  }
  if (D_8028A850 != 0) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  if (w < 0) {
    if (D_8028A8AC != 0) {
      SET16(D_80318CD0[D_8028AB60].vp.vscale[0], (short)(int)(w * -2));
    } else {
      SET16(D_80318CD0[D_8028AB60].vp.vscale[0], (short)(int)(w * 2));
    }
    w = -w;
    D_8028A8A8 = 1;
  } else {
    if (D_8028A8AC != 0) {
      SET16(D_80318CD0[D_8028AB60].vp.vscale[0], (short)(int)(w * -2));
    } else {
      SET16(D_80318CD0[D_8028AB60].vp.vscale[0], (short)(int)(w * 2));
    }
    D_8028A8A8 = 0;
  }
  SET16(D_80318CD0[D_8028AB60].vp.vscale[1], (short)(int)(h * 2));
  SET16(D_80318CD0[D_8028AB60].vp.vscale[2], (short)(int)(0x1ff));
  SET16(D_80318CD0[D_8028AB60].vp.vscale[3], (short)(int)(0));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[0], (short)(int)((x * 2 + w) * 2));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[1], (short)(int)((y * 2 + h) * 2));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[2], (short)(int)(0x1ff));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[3], (short)(int)(0));
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = BES16(D_80318CD0[D_8028AB60].vp.vscale[2]);
  D_8028AB68 = BES16(D_80318CD0[D_8028AB60].vp.vtrans[2]);
}

/* WHAT IT DOES: Take the next viewport in the ring and set it to the
 * whole 320x240 screen (optionally syncing and scissoring to x, y, w, h
 * first); load it, clear the mirror flag and keep its depth scale and
 * offset. */
/* @implements 0x80219DF0 tgr BrViewportFull */
void BrViewportFull(int x, int y, int w, int h, int scissor)
{
  D_8028AB60 = (D_8028AB60 + 1) & 0x1f;
  if (scissor != 0) {
    gDPPipeSync(D_8028A858++);
    BrScissorSet(x, y, w < 0 ? -w : w, h);
  }
  D_8028A8A8 = 0;
  SET16(D_80318CD0[D_8028AB60].vp.vscale[0], (short)(int)(0x280));
  SET16(D_80318CD0[D_8028AB60].vp.vscale[1], (short)(int)(0x1e0));
  SET16(D_80318CD0[D_8028AB60].vp.vscale[2], (short)(int)(0x1ff));
  SET16(D_80318CD0[D_8028AB60].vp.vscale[3], (short)(int)(0));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[0], (short)(int)(0x280));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[1], (short)(int)(0x1e0));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[2], (short)(int)(0x1ff));
  SET16(D_80318CD0[D_8028AB60].vp.vtrans[3], (short)(int)(0));
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = BES16(D_80318CD0[D_8028AB60].vp.vscale[2]);
  D_8028AB68 = BES16(D_80318CD0[D_8028AB60].vp.vtrans[2]);
}

/* WHAT IT DOES: Load the current viewport into the display list and keep
 * its depth scale and offset for turning depths into z-buffer values. */
/* @implements 0x80219F04 tgr BrViewportApply */
void BrViewportApply(void)
{
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = BES16(D_80318CD0[D_8028AB60].vp.vscale[2]);
  D_8028AB68 = BES16(D_80318CD0[D_8028AB60].vp.vtrans[2]);
}

/* WHAT IT DOES: Draw a one-pixel outline round the w by h rectangle at
 * (x, y): one-cycle mode, the fog colour set to 0xFF, then the top, bottom,
 * left and right edges as fill rectangles.  The three constant commands
 * are written out over several lines (the one-line macros store w1 first). */
/* @implements 0x80219F6C tgr BrViewOutline */
void BrViewOutline(int x, int y, int w, int h)
{
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xb900031d);         /* render mode */
    tgr_wr32(&_g->words.w1, 0x55004240);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xba001402);         /* cycle type: one cycle */
    tgr_wr32(&_g->words.w1, 0);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xf8000000);         /* fog colour */
    tgr_wr32(&_g->words.w1, 0xff);
  }
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + 1);
  gDPFillRectangle(D_8028A858++, x, y + h - 1, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + h);
  gDPFillRectangle(D_8028A858++, x + w - 1, y, x + w, y + h);
}

/* WHAT IT DOES: Frame a view (the rear-view mirror) in three rings: a black
 * two-pixel border in fill mode just outside it, the one-pixel outline on
 * its edge (as BrViewOutline) and a one-pixel line two further out. */
/* @implements 0x8021A0F8 tgr BrViewFrame */
void BrViewFrame(int x, int y, int w, int h)
{
  Gfx *g;

  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xb900031d);         /* render mode */
    tgr_wr32(&_g->words.w1, 0x0f0a4000);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xba001402);         /* cycle type: fill */
    tgr_wr32(&_g->words.w1, 0x300000);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xf7000000);         /* fill colour */
    tgr_wr32(&_g->words.w1, 0x10001);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xf8000000);         /* fog colour */
    tgr_wr32(&_g->words.w1, 0xff);
  }
  gDPFillRectangle(D_8028A858++, x - 1, y - 2, x + w, y - 1);
  gDPFillRectangle(D_8028A858++, x - 1, y + h, x + w, y + h + 1);
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x - 1, 14, 10) |
                    _SHIFTL(y + h, 2, 10)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(x - 2, 14, 10) | _SHIFTL(y - 1, 2, 10)));
  }
  gDPFillRectangle(D_8028A858++, x + w, y - 1, x + w + 1, y + h);
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xb900031d);         /* render mode */
    tgr_wr32(&_g->words.w1, 0x55004240);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xba001402);         /* cycle type: one cycle */
    tgr_wr32(&_g->words.w1, 0);
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, 0xf8000000);         /* fog colour */
    tgr_wr32(&_g->words.w1, 0xff);
  }
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + 1);
  gDPFillRectangle(D_8028A858++, x, y + h - 1, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + h);
  gDPFillRectangle(D_8028A858++, x + w - 1, y, x + w, y + h);
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y - 2, 2, 10)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y - 3, 2, 10)));
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y + h + 2, 2, 10)));
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x - 2, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y - 3, 2, 10)));
  }
  {
    Gfx *_g = D_8028A858++;
    tgr_wr32(&_g->words.w0, (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10)));
    tgr_wr32(&_g->words.w1, (_SHIFTL(x + w + 2, 14, 10) | _SHIFTL(y - 3, 2, 10)));
  }
}

/* WHAT IT DOES: Push two empty black frames through the second screen
 * layout with the video output blanked, so a mode change starts from a
 * clean screen. */
/* @implements 0x8021A4C8 tgr BrScreenFlush2Layout1 */
void BrScreenFlush2Layout1(void)
{
  D_8028A884 = 1;
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  D_8028A884 = 0;
}

/* WHAT IT DOES: Push two empty black frames through the first screen layout
 * with the video output blanked, so a mode change starts from a clean
 * screen. */
/* @implements 0x8021A540 tgr BrScreenFlush2Layout0 */
void BrScreenFlush2Layout0(void)
{
  D_8028A884 = 1;
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  D_8028A884 = 0;
}


extern int D_8028AA84;
extern int D_8028AA88;
extern int D_8028B940;
extern unsigned int D_8028AAD4;
extern int D_8026FF10;
extern int D_8028A880;
extern int D_80319F88[];
void *BrRomDmaSlot(void);

/* WHAT IT DOES: Bring the track's textures up to date: each animated one
 * loads the frame for the current time (with kind 3 at night, the first
 * frame of a 16-colour-bank texture; a day/night pair switches frame when
 * kind 3 is toggled), DMAing its texture and palette from the ROM; a plain
 * texture reloads its palette when kind 3 changed.  Remembers kind 3. */
/* @implements 0x8021A5B8 tgr BrTexAnimUpdate */
void BrTexAnimUpdate(void)
{
  int i;
  BrTexAnim *anim;
  int tex;
  int pal;
  unsigned int period;
  int k;
  int night;
  unsigned int t;
  BrTexSlot *slot;

  night = D_8028AA84 != 0 && D_8028B940 != 2 && D_8028B940 != 7;
  for (i = 0; i < BES32(D_80025C00.nTex); i++) {
    slot = &BEPTR(BrTexSlot *, D_80025C00.tex)[i];
    if (BE32(slot->dst) == 0) {
      continue;
    }
    if (BR_TEXSLOT_ANIMATED(slot)) {
      anim = BEPTR(BrTexAnim *, slot->anim);
      period = BE32(anim->key[BE16(anim->n) - 2].time) - BE32(anim->t0);
      t = D_8028AAD4 - D_8028AAD4 / period * period;
      if (BE16(anim->n) == 2 && BE32(anim->t0) == (unsigned int)-1) {
        if (D_8028AA84 != D_8028AA88) {
          k = D_8028AA84 == 0;
          if (D_8028AA84 == 0) {
            tex = -1;
            goto palette;
          }
          goto frame;
        }
        continue;
      }
      if (D_8026FF10 != 0) {
        continue;
      }
      if (night && BR_TEXSLOT_FMT(slot) == 0xB) {
        k = 1;
      } else {
        for (k = 1; k < BE16(anim->n); k++) {
          if (t < BE32(anim->key[k - 1].time)) {
            break;
          }
        }
      }
      k--;
frame:
      tex = BES32(anim->key[k].tex);
palette:
      pal = BES32(anim->key[k].pal);
      if (BR_TEXSLOT_SIZE(slot) != 0 && tex != -1) {
        osPiStartDma(BrRomDmaSlot(), 0, 0, tex + D_8028A880, BEPTR(void *, slot->dst),
                     BR_TEXSLOT_SIZE(slot), &D_80319F88);
      }
load:
      if (BE32(slot->palDst) != 0 && pal != -1) {
        osPiStartDma(BrRomDmaSlot(), 0, 0, pal + D_8028A880, BEPTR(void *, slot->palDst),
                     BR_TEXSLOT_FMT(slot) == 1 ? 0x20 : 0x200, &D_80319F88);
      }
    } else if (D_8028AA88 != D_8028AA84) {
      pal = (BES32(slot->anim) & 0xFFF) << 5;
      goto load;
    }
  }
  D_8028AA88 = D_8028AA84;
}

/* WHAT IT DOES: Start the next controller read, unless one is already in
 * flight: the result arrives on the controller message queue. */
/* @implements 0x8021A920 tgr BrPadStartRead */
void BrPadStartRead(void)
{
  if (D_8028AB6C == 0) {
    D_8028AB6C = 1;
    D_802A4BE8 = 0;
    osContStartReadData(&D_80272D48);
  }
}

/* WHAT IT DOES: Finish a controller read: start one if none is in flight,
 * wait for it to complete, copy the results into the pad state, and mark
 * fresh input as ready. */
/* @implements 0x8021A964 tgr BrPadRead */
void BrPadRead(void)
{
  BrPadStartRead();
  osRecvMesg(&D_80272D48,0,1);
  osContGetReadData(D_8031A3E0);
  D_802A4BE8 = 1;
  D_8028AB6C = 0;
}

/* WHAT IT DOES: Poll all four controllers: wait for the pending read, then
 * update each pad record (0x15C bytes apiece) and derive this frame's fresh
 * presses from its raw buttons. */
/* @implements 0x8021A9B4 tgr BrPadPollAll */
void BrPadPollAll(void)
{
  int i;

  BrPadRead();
  for (i = 0; i < 4; i++) {
    BrPadMapRead(D_8036A8E0[i]);
    BrPadEdges(D_8036A8E0[i]);
  }
}

/* WHAT IT DOES: Finish the frame and hand it to the RSP: close the display
 * list, fill in this buffer's graphics task (F3DEX FIFO microcode, the
 * list, the RDP FIFO), record the frame's list, matrix and vertex counts
 * (a list over 6000 commands is fatal); unless this is the first frame,
 * wait for the previous task's RSP and RDP to finish (running the pending
 * callback), optionally copy the debug frame, set the VI mode once after a
 * resolution change, swap to the finished frame buffer and wait for it,
 * lift the screen blanking; then start the task and flip buffers.
 * The list size is (list - (buffer + 0x200)) in bytes (the ROM's negu);
 * the count is stored and then tested; boot size uses rspbootTextEnd, a
 * second symbol at the F3DEX text start.
 * RESIDUE (~210): instruction scheduling -- the task's stores, the counter
 * loads and the debug copy's multiply are ordered differently; the
 * instruction multiset matches except about 30 moved ops. */
/* @t4-pass 0x8021AA08 1 2026-10-03 compiles 120 best 209 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021AA08 2 2026-10-03 compiles 120 best 209 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021AA08 */
/* @implements 0x8021AA08 tgr BrFrameEnd */
void BrFrameEnd(void)
{
  extern int D_8028AB80;         /* 0x8028AB80: a frame has been started before */
  BrTask *t;
  int n;
  int m;
  int v;
  int mul;
  int *src;
  int *dst;
  int i;
  void *fb;

  gRaw(D_8028A858++, 0xe9000000, 0);
  gRaw(D_8028A858++, 0xb8000000, 0);
  t = &D_8031A9A8[D_8028A85C];
  t->type = 1;
  t->ucode = tgr_addr32(gspF3DEX_fifoTextStart);
  t->ucode_data = tgr_addr32(gspF3DEX_fifoDataStart);
  t->flags = 6;
  t->ucode_size = 0x1000;
  t->output_buff = tgr_addr32(D_8028A860[0]);
  t->output_buff_size = tgr_addr32(D_8028A860[1] - D_8028AB84);
  t->ucode_data_size = 0x800;
  t->dram_stack = tgr_addr32(TGR_PTR(void *, ((tgr_addr32(D_8031A598) + 0xf) & ~0xf)));
  t->dram_stack_size = 0x400;
  t->ucode_boot = tgr_addr32(rspbootTextStart);
  t->ucode_boot_size = tgr_addr32(rspbootTextEnd) - tgr_addr32(rspbootTextStart);
  t->data_ptr = tgr_addr32(&D_8028A848[D_8028A85C][0x40]);
  t->data_size = (((char *)D_8028A858 - ((char *)D_8028A848[D_8028A85C] + 0x200)) >> 3) << 3;
  n = ((char *)D_8028A858 - ((char *)D_8028A848[D_8028A85C] + 0x200)) >> 3;
  if (D_8028AB7C < n) {
    D_8028AB7C = n;
  }
  m = (D_8028C75C - D_8028C760) >> 1;
  if (D_8028AB74 < m) {
    D_8028AB74 = m;
  }
  v = D_8028C764 - D_8028C768;
  if (D_8028AB78 < v) {
    D_8028AB78 = v;
  }
  if ((D_8028AB70 = n) > 6000) {
    BrFatal("HUGE GLIST ERROR");
  }
  osWritebackDCacheAll();
  if (D_8028AB80 != 0) {
    BrPerfMark(0, 0, 0, 0, 0xff);
    osRecvMesg(D_8031A320, 0, 1);
    BrPerfMark(0, 0xff, 0xff, 0, 0xff);
    if (D_8031B31C != 0) {
      D_8031B31C();
      D_8031B31C = 0;
    }
    BrPerfMark(0, 0, 0, 0, 0xff);
    osRecvMesg(D_8031A358, 0, 1);
    if (D_8028AA94 != 0) {
      mul = D_8028A850 != 0 ? 4 : 1;
      n = mul * D_8028AAB0 * D_8028AAB4 >> 1;
      src = D_80000400;
      dst = TGR_PTR(int *, D_8031AA28[D_8028A85C ^ 1]);
      for (i = 0; i < n; i++) {
        dst[i] = src[i];
      }
    }
    if (D_8031B320 != 0) {
      D_8031B31C();
      D_8031B31C = 0;
    }
    osWritebackDCacheAll();
    D_8028AAE8 = osGetCount();
    D_8028AAE0 = D_8028AAE8 - D_8028AAE4;
    if (D_8028A84C != 0) {
      if (--D_8028A84C == 0) {
        if (D_8028A850 != 0) {
          if (osTvType == 2) {
            osViSetMode(D_802A6040);
          } else {
            osViSetMode(D_802A5780);
          }
        } else {
          if (osTvType == 2) {
            osViSetMode(D_802A5D70);
          } else {
            osViSetMode(D_802A54B0);
          }
        }
        osViBlack(1);
        D_8028A854 = D_8028A850;
      }
    }
    BrPerfMark(1, 0x20, 0x20, 0x20, 0xff);
    BrPerfMark(2, 0x20, 0x20, 0x20, 0xff);
    BrPerfMark(0, 0x20, 0x20, 0x20, 0xff);
    fb = TGR_PTR(void *, D_8031AA28[D_8028A85C ^ 1]);
    osRecvMesg(D_8031A390, 0, 1);
    osViSwapBuffer(fb);
    osRecvMesg(D_8031A390, 0, 1);
    if (osViGetCurrentFramebuffer() != fb) {
      osRecvMesg(D_8031A390, 0, 1);
    }
    if (D_8028A884 == 0 && D_8028A888 == 0) {
      osViBlack(0);
    }
    if (D_8028A888 != 0) {
      D_8028A888--;
    }
    BrPerfFrameStart();
    D_8028AAE4 = osGetCount();
    D_8028AAE8 = osGetCount() - D_8028AAE8;
  } else {
    D_8028AB80++;
  }
  BrPerfMark(0, 200, 0, 200, 0xff);
  D_8028AA24 = D_8028AA20;
  BrPerfMark(2, 200, 0, 0, 0xff);
  BrPerfMark(1, 200, 100, 0, 0xff);
  osSpTaskLoad(t);
  osSpTaskStartGo(t);
  D_8028A85C ^= 1;
}

#define OS_K0_TO_PHYSICAL(x) (tgr_addr32(x) - 0x80000000)

/* WHAT IT DOES: Work out the race camera's view frustum for culling: the
 * far plane's centre (far along the camera's first axis) and its four
 * corners (sin(fov) * far across the side axis, scaled by h / w up the
 * third, halved for a split screen), each corner then moved three quarters
 * of the way back toward the eye; the field of view and depth are kept. */
/* @implements 0x8021B0C4 tgr BrFrustumSet */
void BrFrustumSet(float m[4][4], float fov, float far, float w, float h)
{
  float halfW;
  float halfH;

  halfW = sinf(fov) * far;
  halfH = halfW * h / w;
  if (D_8028AB0C == 2) {
    halfH *= 0.5f;
  }
  D_8031B1F0.eye[0] = m[3][0];
  D_8031B1F0.eye[1] = m[3][1];
  D_8031B1F0.eye[2] = m[3][2];
  BrVec3MulAdd(D_8031ABF0, D_8031B1F0.eye, m[0], far);
  BrVec3Scale(D_8031ABD0, m[1], halfW);
  BrVec3Scale(D_8031ABE0, m[2], halfH);
  D_8031B1F0.centre[0] = D_8031ABF0[0];
  D_8031B1F0.centre[1] = D_8031ABF0[1];
  D_8031B1F0.centre[2] = D_8031ABF0[2];
  BrVec3Add(D_8031B1F0.corner[0], D_8031ABF0, D_8031ABD0);
  BrVec3AddTo(D_8031B1F0.corner[0], D_8031ABE0);
  BrVec3Add(D_8031B1F0.corner[3], D_8031ABF0, D_8031ABD0);
  BrVec3SubFrom(D_8031B1F0.corner[3], D_8031ABE0);
  BrVec3Sub(D_8031B1F0.corner[1], D_8031ABF0, D_8031ABD0);
  BrVec3AddTo(D_8031B1F0.corner[1], D_8031ABE0);
  BrVec3Sub(D_8031B1F0.corner[2], D_8031ABF0, D_8031ABD0);
  BrVec3SubFrom(D_8031B1F0.corner[2], D_8031ABE0);
  BrVec3Lerp(D_8031B1F0.corner[1], D_8031B1F0.corner[1], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[2], D_8031B1F0.corner[2], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[0], D_8031B1F0.corner[0], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[3], D_8031B1F0.corner[3], D_8031B1F0.eye, 0.75f);
  D_8028AACC = far;
  D_8028AAC4 = fov;
}

/* WHAT IT DOES: Set the race camera from a camera matrix: look from its
 * position along its first axis with its third as up, and project with the
 * given field of view (scaled for the viewport's shape, in radians) and far
 * plane, near plane 2.4; the combined matrix goes to a fresh Mtx. */
/* @implements 0x8021B2F8 tgr BrCameraSet */
void BrCameraSet(float m[4][4], float fov, float far, float w, float h)
{
  guLookAtF(D_8031AAD0, m[3][0], m[3][1], m[3][2], m[3][0] + m[0][0], m[3][1] + m[0][1],
            m[3][2] + m[0][2], m[2][0], m[2][1], m[2][2]);
  D_8028A870 = far;
  D_8028A86C = 2.4f;
  guPerspectiveF(D_8031AA90, &D_8028A874, fov * 1.3333334f * (h / w) * 57.295776f, w / h,
                 D_8028A86C, D_8028A870, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AA90, D_8031AA50);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
}

/* WHAT IT DOES: Set the front end's 3D camera: looking straight into a
 * 1024x768 plane from z 1000, 45 degree field of view, 4:3; load it as the
 * projection. */
/* @implements 0x8021B458 tgr BrMenuCameraSet */
void BrMenuCameraSet(float w, float h)
{
  guLookAtF(D_8031AAD0, 512.0f, 384.0f, 1000.0f, 512.0f, 384.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  guPerspectiveF(D_8031AB50, &D_8028A874, 45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AB50, D_8031AA50);
  gSPPerspNormalize(D_8028A858++, D_8028A874);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
  gSPMatrix(D_8028A858++, OS_K0_TO_PHYSICAL(D_8028A878), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
}

/* WHAT IT DOES: Set a 320x240 screen-space camera (from z 290, 45 degrees,
 * 4:3), near 1 and far 500 unless a caller set its own planes for this one
 * frame; load it as the projection. */
/* @implements 0x8021B5A4 tgr BrScreenCameraSet */
void BrScreenCameraSet(float w, float h)
{
  guLookAtF(D_8031AAD0, 160.0f, 120.0f, 290.0f, 160.0f, 120.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  if (D_8028A868 != 0) {
    D_8028A868 = 0;
  } else {
    D_8028A86C = 1.0;
    D_8028A870 = 500.0f;
  }
  guPerspectiveF(D_8031AA90, &D_8028A874, 45.0f, 1.3333334f, D_8028A86C, D_8028A870, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AA90, D_8031AA50);
  gSPPerspNormalize(D_8028A858++, D_8028A874);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
  gSPMatrix(D_8028A858++, OS_K0_TO_PHYSICAL(D_8028A878), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
}

typedef struct {
  unsigned int w0, w1;
  unsigned int to0, to1;
} DlSwap1;

extern DlSwap1 D_8028AB88[1];
extern unsigned int D_8028AB98[2][2];

/* WHAT IT DOES: Walk a display list up to its end command and patch it for
 * the race kind: each other-mode command found in the six-entry table is
 * replaced by that entry's variant for the kind (returns 1 if one of the last
 * three entries matched); a combine command is swapped for its kind-1/2
 * replacement, and with kind 3 set and the list flagged, a combine found in
 * the two-entry list marks the following primitive and environment colours
 * to be overridden. Clears the flag. */
/* @implements 0x8021B72C tgr BrDlRaceKindPatch */
int BrDlRaceKindPatch(unsigned int *dl, unsigned int (*table)[4][2])
{
  int plain;
  int ret;
  int kind;
  int tint;
  int i;

  ret = 0;
  plain = D_8028AA80 == 0 && D_8028AA8C == 0;
  kind = !plain;
  kind = D_8028AA78 + kind + 1;
  tint = 0;
  if (dl != 0) {
    for (;; dl += 2) {
      switch ((unsigned char)(tgr_rd32(&dl[0]) >> 24)) {
      case 0xB8:
        goto done;
      case 0xB9:
        for (i = 0; i < 6; i++) {
          if (tgr_rd32(&dl[0]) == table[i][0][0] && tgr_rd32(&dl[1]) == table[i][0][1]) {
            tgr_wr32(&dl[0], table[i][kind][0]);
            tgr_wr32(&dl[1], table[i][kind][1]);
            if (i >= 3) {
              ret = 1;
            }
            break;
          }
        }
        break;
      case 0xFC:
        if (plain) {
          for (i = 0; i < 1; i++) {
            if (tgr_rd32(&dl[0]) == D_8028AB88[i].w0 && tgr_rd32(&dl[1]) == D_8028AB88[i].w1) {
              tgr_wr32(&dl[0], D_8028AB88[i].to0);
              tgr_wr32(&dl[1], D_8028AB88[i].to1);
              break;
            }
          }
        }
        if (D_8028AA84 != 0 && D_8028ADE8 != 0) {
          for (i = 0; i < 2; i++) {
            if (tgr_rd32(&dl[0]) == D_8028AB98[i][0] && tgr_rd32(&dl[1]) == D_8028AB98[i][1]) {
              break;
            }
          }
          if (i < 2) {
            tint = 1;
          } else {
            tint = 0;
          }
        }
        break;
      case 0xFA:
        if (tint && D_8028AA84 != 0) {
          tgr_wr32(&dl[1], 0x60789000);
        }
        break;
      case 0xFB:
        if (tint && D_8028AA84 != 0) {
          tgr_wr32(&dl[1], 0x8C9CA800);
        }
        break;
      }
    }
  }
done:
  D_8028ADE8 = 0;
  return ret;
}

/* WHAT IT DOES: Re-evaluate everything that depends on the race-kind flags:
 * marks each track object whose condition list now holds, and does the same
 * for every car's model parts. */
/* @implements 0x8021B97C tgr BrRaceFlagsApply */
void BrRaceFlagsApply(void)
{
  int flag;
  int i;
  int n;
  int j;
  BrTrackObj *objs = BR_TRACKOBJS();

  flag = D_8028B940 != 2 && D_8028B940 != 7;
  for (i = 0; i < BES32(D_80025C00.nObjs); i++) {
    if ((BE16(objs[i].flags) & 4) == 0) {
      D_8028ADE8 = flag;
    }
    if (BrDlRaceKindPatch(BEPTR(unsigned int *, objs[i].dl), D_8028ABA8)) {
      SET16(objs[i].flags, BE16(objs[i].flags) | 8);
    }
  }
  for (n = 0; n < D_8028B7F4; n++) {
    for (j = 0; j < 3; j++) {
      if (D_8031B760[n].colour[3] == 2) {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(BEPTR(unsigned int *, TGR_PTR(BrCarModel *, D_8031B760[n].model)->dl[j][i]), D_8028AC68);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(BEPTR(unsigned int *, TGR_PTR(BrCarModel *, D_8031B760[n].model)->dl2[j][i]), D_8028AD28);
        }
      } else {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(BEPTR(unsigned int *, TGR_PTR(BrCarModel *, D_8031B760[n].model)->dl[j][i]), D_8028ABA8);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(BEPTR(unsigned int *, TGR_PTR(BrCarModel *, D_8031B760[n].model)->dl2[j][i]), D_8028ABA8);
        }
      }
    }
  }
}

/* WHAT IT DOES: The RSP event thread: every time the RSP finishes a task,
 * mark the performance meter's first bar and pass the event on to the
 * scheduler's RSP queue. Never returns. */
/* @t4-pass 0x8021BBDC 1 2026-10-03 compiles 121 best 7 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021BBDC 2 2026-10-03 compiles 121 best 7 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021BBDC tgr BrSpEventThread */
void BrSpEventThread(void *arg)
{
  osCreateMesgQueue(D_8031A338, D_8031A354, 1);
  osSetEventMesg(4, D_8031A338, D_8031A3CC);
  for (;;) {
    osRecvMesg(D_8031A338, 0, 1);
    BrPerfMark(1, 0, 0, 0, 0xff);
    osSendMesg(D_8031A320, D_8031A3CC, 1);
  }
}

/* WHAT IT DOES: The RDP event thread: every time the RDP finishes a frame's
 * drawing, mark the performance meter's second bar and pass the event on to
 * the scheduler's RDP queue. Never returns. */
/* @implements 0x8021BCA8 tgr BrDpEventThread */
void BrDpEventThread(void *arg)
{
  osCreateMesgQueue(D_8031A370, D_8031A38C, 1);
  osSetEventMesg(9, D_8031A370, D_8031A3CC);
  for (;;) {
    osRecvMesg(D_8031A370, 0, 1);
    BrPerfMark(2, 0, 0, 0, 0xff);
    osSendMesg(D_8031A358, D_8031A3CC, 1);
  }
}

/* WHAT IT DOES: The retrace thread: on every vertical retrace, pass the
 * event on to the retrace queue the game waits on and step the 16-frame
 * retrace counter. Never returns. */
/* @implements 0x8021BD68 tgr BrRetraceThread */
void BrRetraceThread(void *arg)
{
  extern int D_8028ADEC;    /* 0x8028ADEC */

  osCreateMesgQueue(D_8031A3B0, D_8031A3C8, 1);
  osViSetEvent(D_8031A3B0, D_8031A3CC, 1);
  for (;;) {
    osRecvMesg(D_8031A3B0, 0, 1);
    osSendMesg(D_8031A390, D_8031A3CC, 1);
    D_8028ADEC = (D_8028ADEC + 1) & 0xf;
  }
}

/* WHAT IT DOES: Add to the camera shake for view n: at most 2.5 per call,
 * and the total never goes above 5. */
/* @implements 0x8021BE28 tgr BrCamShakeAdd */
void BrCamShakeAdd(int cam, float amount)
{
  if (amount > 2.5f) {
    amount = 2.5f;
  }
  D_8031B1D8[cam] += amount;
  if (D_8031B1D8[cam] > 5.0f) {
    D_8031B1D8[cam] = 5.0f;
  }
}

/* WHAT IT DOES: Bring up the scheduler side of the game: the two frame
 * buffers, the RSP, RDP and retrace event queues, the RSP and RDP event
 * threads, the controllers (through the SI queue), and for each controller
 * with a pak that answers as a rumble pak, mark it and stop its motor.
 * RESIDUE (14): IDO gives the last callee-saved register to the flag value
 * 1; the ROM gives it to the loop bound 4 and loads the 1 at the store. */
/* @t4-pass 0x8021BE88 1 2026-10-03 compiles 116 best 14 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021BE88 2 2026-10-03 compiles 116 best 14 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021BE88 */
/* @implements 0x8021BE88 tgr BrSchedInit */
void BrSchedInit(void)
{
  int i;
  int q[6];
  int msg;
  unsigned char bits;
  int r;

  D_8028A848 = D_80272D68;
  D_8031AA28[0] = tgr_addr32(D_801B5000);
  D_8031AA28[1] = tgr_addr32(D_801DA800);
  osCreateMesgQueue(D_80319F88, D_80319FA0, 32);
  osCreateMesgQueue(D_8031A320, D_8031A350, 1);
  osSetEventMesg(4, D_8031A320, D_8031A3CC);
  osCreateMesgQueue(D_8031A358, D_8031A388, 1);
  osSetEventMesg(9, D_8031A358, D_8031A3CC);
  osCreateMesgQueue(D_8031A390, D_8031A3A8, 1);
  osViSetEvent(D_8031A390, D_8031A3CC, 1);
  osCreateThread(D_802729E0, 10, BrSpEventThread, D_8026FF04, D_803196D0, 60);
  osStartThread(D_802729E0);
  osCreateThread(D_80272B90, 11, BrDpEventThread, D_8026FF04, D_80319ED0, 60);
  osStartThread(D_80272B90);
  D_8028A8B0 = 46875.0f;
  osCreateMesgQueue(q, &msg, 1);
  osSetEventMesg(5, q, 1);
  osContInit(q, &bits, D_8031A3D0);
  osCreateMesgQueue(D_80272D48, D_80272D60, 1);
  osSetEventMesg(5, D_80272D48, 0);
  BrStub802607AC();
  for (i = 0; i < 4; i++) {
    D_8031B1E8[i] = 0;
    if ((bits >> i & 1) && !(D_8031A3D0[i].errno & 8) && (D_8031A3D0[i].type & 4) &&
        (D_8031A3D0[i].status & 1)) {
      r = osPfsInit(D_80272D48, &D_8031A3F8[i], i);
      if (r != 0 && (r == 10 || r == 11) && osMotorInit(D_80272D48, &D_8031A3F8[i], i) == 0) {
        D_8031B1E8[i] = 1;
        osMotorStop(&D_8031A3F8[i]);
      }
    }
  }
}

/* WHAT IT DOES: The boot-time controller check: with no first controller,
 * or no controller pak in port 1, or a pak that will not initialise (one
 * reporting "no pak file system" is set up as a rumble pak), or no valid
 * save, run the message screen until it is dismissed (its reason in
 * 0x80270840); a pak in port 2 is initialised too; holding B at boot runs
 * the debug screen first.
 * The B test reads pad 1's pressed word through its absent field
 * (0x8036A908, 0x28 further on), so the load has its own base: the ROM
 * loads the word with lui/lw and builds the record's address for the
 * consume call separately, where one symbol for both lets IDO share them. */
/* @implements 0x8021C188 tgr BrBootCheck */
void BrBootCheck(void)
{
  extern unsigned char D_8028ADF0;   /* 0x8028ADF0 */
  unsigned char plugged;

  BrPadPollAll();
  D_802A4BE8 = 0;
  if (D_8036A908 != 0) {
    BrModeSet(BrPakWarnScreen);
    D_80270840 = 0;
    while (D_8031B318 == BrPakWarnScreen) {
      BrPakWarnScreen();
    }
  }
  osPfsIsPlug(D_80272D48, &plugged);
  if (!(plugged & 1)) {
    BrModeSet(BrPakWarnScreen);
    D_80270840 = 1;
    while (D_8031B318 == BrPakWarnScreen) {
      BrPakWarnScreen();
    }
  } else {
    if (D_8028ADF0 == 0) {
      D_8028ADF0 = 1;
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[0], 0);
    }
    if (D_802724F0 != 0) {
      if (D_802724F0 == 10) {
        osMotorInit(D_80272D48, &D_8031A3F8[0], 0);
      }
      BrModeSet(BrPakWarnScreen);
      D_80270840 = 1;
      while (D_8031B318 == BrPakWarnScreen) {
        BrPakWarnScreen();
      }
    } else {
      memcpy(D_803163E0, D_80369EC0[0].raw + 0xc, 0x20);
      D_80316420 = 1;
    }
  }
  if (plugged & 2) {
    D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[1], 1);
    if (D_802724F0 != 0) {
      if (D_802724F0 == 10) {
        osMotorInit(D_80272D48, &D_8031A3F8[1], 1);
      }
    } else {
      memcpy(D_80316400, D_80369EC0[1].raw + 0xc, 0x20);
      D_80316421 = 1;
    }
  }
  if (D_8036A8E0[0][0] & 0x4000) {
    BrPadConsume(D_8036A8E0[0], 0x4000);
    BrModeSet(BrPakManager);
    while (D_8031B318 == BrPakManager) {
      BrPakManager();
    }
  }
  if (BrPakCheckFiles() == 0) {
    BrModeSet(BrPakWarnScreen);
    D_80270840 = 2;
    while (D_8031B318 == BrPakWarnScreen) {
      BrPakWarnScreen();
    }
  }
  D_802A4BE8 = 1;
}
