/* br_framedrive.c -- drawing: the per-frame SCENE DRIVER, 0x10011FA0.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * ONE function, 4,500 bytes: the thing the race loop calls once per frame
 * to draw everything.  It takes a slot index (five 0x2E0F0-byte slot
 * records, the LRU cache slice2_14.h describes; the four bases below are
 * all `base + slot*0x2E0F0`) and for every view in that slot builds the
 * camera, the scene display list (two passes through BrSceneDlBuild), the
 * cars (opaque, body, translucent), the environment, then the overlays --
 * FPS, split-screen captions, the pause menu, the attract-mode credits --
 * and finally closes the frame.
 *
 * Everything it calls already has a name in the tree except seven leaves
 * that are still Ghidra-named (FUN_1006ec30, FUN_1002af17, FUN_1002b480,
 * FUN_10014e00, FUN_1002cb49, FUN_1002cee9 and the two generated TUs);
 * the d3d twins of three of them are slice2_18's BrFogUpdate /
 * BrHudColorsUpdate / BrFrameEnd.
 *
 * WHAT THE BYTES PIN (read before re-deriving any of it):
 *  - 0x10008D60 is a bare `ret` debug logger, called K&R-style with 0, 1
 *    or 5 arguments.  Its one-argument call pushes 0x3EA8F5C3 (0.33f) as
 *    FOUR bytes; a float through an unprototyped call would be promoted
 *    to a double and pushed as eight, so the source value is spelled as
 *    the int bits.
 *  - the per-view record is the 0x58-byte BrHudView (x,y,w,h, then the
 *    view's car index at +0x10); `&aViews[i]` is the `lea [ebp+ecx*8]`
 *    with ecx = 11*i.
 *  - `g_brCViews > 1` (cmp 1 / jle), NOT `>= 2`: the global compare at the
 *    end (`cmp [g],2; jl`) shows VC5 does not canonicalise `>= 2`.
 *  - three `x >> 1` / `>> 2` sites are bare `sar` with no cdq fix-up:
 *    they are shifts in the source, the `/ 4`, `/ 16`, `/ 8`, `/ 11`
 *    sites are real signed divisions.
 *  - `g - 0x1E` as an ARGUMENT compiles to `add r,-0x1E`; `y -= 5` as a
 *    statement compiles to `sub r,5`.  The credits title's `y - 0x14`
 *    is the former.
 *  - the 32-byte records at 0x100A9368 are the credits pages: a title,
 *    4.0f, six line pointers; a line starting with '`' is a sub-heading
 *    drawn in the small font with the mark stripped.
 *  - 0x100A6B60 = "%ry" and 0x100A6B64 = "%y1": text-markup colour
 *    escapes, the pause menu's "selected / not selected" prefixes.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Records                                                            */
/* ------------------------------------------------------------------ */
/* One view of the split screen: slice2_15.h's BrHudView, 0x58 bytes. */
typedef struct BrHudView {
    int32_t  x, y, w, h;        /* +0x00 .. +0x0C */
    int32_t  iCar;              /* +0x10  the car this view follows      */
    uint32_t aDial[16];         /* +0x14 .. +0x50                        */
    uint32_t dlOverlay;         /* +0x54                                 */
} BrHudView;

/* A camera object: eye position at +0x30, field of view at +0x40. */
typedef struct BrCamObj {
    uint8_t  pad00[0x30];
    float    pos[4];            /* +0x30 */
    float    fov;               /* +0x40 */
} BrCamObj;

#define BR_SLOT_STRIDE   0x2E0F0    /* one slot record (slice2_14.h)      */
#define BR_CAR_STRIDE    0x2B68     /* one car record (br_drawcar.h)      */
#define CAR_PCAM(p)      (*(BrCamObj **)((p) + 0x2734))   /* active camera */
#define CAR_KIND(p)      ((p)[0x29AF])                    /* 2 = translucent */

/* The slot's car-pointer table: 0x80-byte entries from +0x60, first
 * dword is the car (br_drawcar.h). */
#define HDR_CAR(pHdr, k) (*(uint8_t **)((pHdr) + 0x60 + (k) * 0x80))

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
void  BrRecHdrLatch_10010F80(uint8_t *pRec);          /* 0x10010F80 */
void  BrFadeTick(void);                               /* 0x100186E0 */
void  BrFrameBeginRec(BrHudView *aViews);             /* 0x1002BF24 */
int   BrPodNop();                                     /* 0x10008D60, bare ret */
void  BrViewBuffersRebase(void);                      /* 0x1000CB20 */
void  BrGfxFillRect(int ulx, int uly, int w, int h,   /* 0x1002AD39 */
                    int r, int g, int b);
void  BrGfxClearScreen(int r, int g, int b);          /* 0x1002AB99 */
void  FUN_1006ec30(int a, int b, float *pEye,         /* 0x1006EC30 */
                   void *p4, void *p5, void *p6, void *p7, void *p8,
                   void *p9);
void  BrDlRectCmdEmit(int x, int y, int w, int h, int f); /* 0x1002C0F3 */
void  BrCamFrustumBuild(BrCamObj *pCam, float fov, float far_,
                        float w, float h);            /* 0x1002D362 */
void  BrCamMatrixSetup(BrCamObj *pCam, float fov, float far_,
                       float w, float h);             /* 0x1002D534 */
void  FUN_1002af17(void);                             /* 0x1002AF17, d3d BrFogUpdate */
void  FUN_1002b480(void);                             /* 0x1002B480, d3d BrHudColorsUpdate */
void  BrSceneSetupFrame(BrHudView *aViews);           /* 0x10015630 */
void  BrSceneAccumReset(void);                        /* 0x10017F60 */
void  BrSpanBuildHull(void);                          /* 0x10034010 */
void  BrCarVisibilityUpdate(uint8_t *pCar);           /* 0x10009FC0 */
void  BrSceneDlBuild(BrHudView *aViews, int pass,     /* 0x1000EAF0 */
                     uint8_t *pHdr, uint8_t *pCars);
void  BrCarDrawVehicle(uint8_t *pCar, int lodBias);   /* 0x1000A110 */
int   BrNop6E590();                                   /* 0x1006E590 */
void  FUN_10011650(BrHudView *aViews);                /* 0x10011650 */
void  FUN_10011d20(void);                             /* 0x10011D20 */
void  BrCarDrawBody(uint8_t *pCar);                   /* 0x1000BEB0 */
void  FUN_100119c0(BrHudView *aViews, uint8_t *pHdr); /* 0x100119C0 */
void  BrEnvEmit(void);                                /* 0x10017110 */
void  BrFadeDrawSprite(BrHudView *aViews, float alpha); /* 0x10017F80 */
void  BrCamMatrixSetupOrtho(float w, float h);        /* 0x1002D72E */
void  BrDlScreenRectEmit(int x, int y, int w, int h, int f); /* 0x1002C2E9 */
void  BrNop_1002AB94();                               /* 0x1002AB94 */
void  BrDlBorderEmit(int x, int y, int w, int h);     /* 0x1002C50E */
void  BrFpsReadout();                                 /* 0x10011EA0 */
int   BrTextFlag358Clear();                           /* 0x10016820 */
int   BrSet_10019270();                               /* 0x10016830 */
int   BrSetGlobal_ABB30(int v);                       /* 0x100168B0 */
const char *BrStrGet(int id);                         /* 0x1006D280 */
void  BrTextDraw(const char *psz, int x, int y);      /* 0x100168C0 */
void  BrSub_10019280(void);                           /* 0x10016840 */
void  BrSub_10019290(void);                           /* 0x10016850 */
void  FUN_10014e00(BrHudView *aViews, uint8_t *pCars); /* 0x10014E00 */
void  BrHudDrawViewMessage(BrHudView *aViews);        /* 0x10014D00 */
void  BrHudDraw(BrHudView *aViews, uint8_t *pCars);   /* 0x10015300 */
void  BrNop_1002C509();                               /* 0x1002C509 */
void  BrHudDrawViewCentreText(BrHudView *aViews);     /* 0x10014C00 */
void  BrNop_1002AB8F(void);                           /* 0x1002AB8F */
void  BrSub_1003289F(int a, int b, int c, int d);     /* 0x1002BF50 */
void  BrHudTextListDraw(BrHudView *aViews);           /* 0x10013140 */
void  BrTextSetColors(int a1, int a2, int a3,         /* 0x10016860 */
                      int a4, int a5, int a6);
void  BrFadeDrawBars(void);                           /* 0x100183B0 */
void  FUN_1002cb49(void);                             /* 0x1002CB49 */
void  FUN_1002cee9(void);                             /* 0x1002CEE9, d3d BrFrameEnd */

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
extern uint8_t   DAT_10396f4c[];    /* slot +0x004: view header, car table at +0x60 */
extern uint8_t   DAT_10397950[];    /* slot +0xA08: 16 car records, 0x2B68 each     */
extern uint8_t   DAT_103c2fd0[];    /* slot +0x2C088: BrHudView aViews[]            */
extern uint8_t   DAT_103c302c[];    /* slot +0x2C0E4: the record header latched     */
extern int       DAT_100a64b8;      /* frames left to clear the whole screen        */
extern int       DAT_100a64bc;      /* view count the clear counter was armed for   */
extern int       DAT_106ed698;
extern int       g_brCViews;        /* 0x100AA044 */
extern int       g_brIView;         /* 0x106EC798 */
extern uint8_t  *DAT_106e9d88;      /* the car the current view follows             */
extern BrCamObj *DAT_106ed520;      /* its active camera                            */
extern float     DAT_100aa040;      /* 400.0f, the far plane                        */
extern uint8_t   DAT_106ed528[], DAT_106e8a18[], DAT_106ed590[];
extern uint8_t   DAT_106b7ac4[], DAT_106e728c[], DAT_106ec780[];
extern int       DAT_106ed6ac;
extern int       DAT_10273688;
extern int       g_BrCarCount;      /* 0x100B2F04 */
extern int       DAT_106ed6b0;
extern int       DAT_100b3014;
extern int       g_brRaceNDriver;   /* 0x100B2F00 */
extern float     g_4B16A0;          /* 0x104B16A0 */
extern float     g_4B16AC;          /* 0x104B16AC */
extern int       DAT_100aa018;      /* mirror size selector 1/2/3               */
extern int       DAT_100a9360;      /* game mode                                */
extern int       DAT_106e9a2c;      /* screen height                            */
extern int       DAT_106e7714;      /* screen width                             */
extern int       DAT_100aa024;
extern int       DAT_105ccb88;
extern int       g_brRaceBeginStage; /* 0x105BC760 */
extern int       DAT_105ccb5c;      /* pause state                              */
extern int       DAT_105bc8dc;      /* pause-menu cursor                        */
extern int       g_brRaceNet;       /* 0x10226A48 */
extern char      DAT_100a6b60[];    /* "%ry" */
extern char      DAT_100a6b64[];    /* "%y1" */
extern int       g_brB4E70C;        /* 0x10B71A6C */
extern int       g_brB4E708;        /* 0x10B71A68 */
extern char      DAT_10396f28[];    /* sprintf scratch                          */
extern int       DAT_105bc768;      /* credits page                             */
extern const char *DAT_100a9368[][8]; /* credits: title, 4.0f, six lines        */
extern float     DAT_105bc884;      /* credits page timer                       */
extern int       DAT_106ed6bc;

static const float kF728C = 0.6499999761581421f;   /* 0x1007728C */
static const float kF7290 = 0.699999988079071f;    /* 0x10077290 */
static const float kF7294 = -0.5f;                 /* 0x10077294 */
static const float kF7298 = 0.20000000298023224f;  /* 0x10077298 */
static const float kF729C = 0.30000001192092896f;  /* 0x1007729C */
static const float kF72A0 = 0.5f;                  /* 0x100772A0 */

/* WHAT IT DOES: draws one whole frame for a slot of the split screen.  For
 * every view it builds that view's camera and scene display list, draws
 * the cars in three passes (opaque, bodies, translucent), the track
 * environment and the fade sprite, then -- for a single-view race with the
 * rear-view mirror on -- repeats the scene into the mirror rectangle with
 * the car's second camera before restoring the first.  After the views it
 * lays the overlays on top: the FPS readout, the split-screen captions
 * ("wait for player" style prompts by game mode), the pause menu with its
 * highlighted entry, the attract-mode credits page, the fade bars, and
 * finally closes the frame.  Debug colour markers bracket every stage;
 * their callee is a bare `ret`. */
/* @implements 0x10011FA0 glide BrFrameDraw */
void BrFrameDraw(int iSlot)
{
    uint8_t   *pCars, *pHdr, *pCar;
    BrHudView *aViews, *pV;
    BrCamObj  *pCamSave;
    const char *psz;
    int        off, i, k, n, x, y;
    int        wMir, hMir, xMir, yMir;

    off    = iSlot * BR_SLOT_STRIDE;
    pCars  = DAT_10397950 + off;
    pHdr   = DAT_10396f4c + off;
    aViews = (BrHudView *)(DAT_103c2fd0 + off);
    BrRecHdrLatch_10010F80(DAT_103c302c + off);
    BrFadeTick();
    BrFrameBeginRec(aViews);
    BrPodNop(0, 0, 0x82, 0, 0xff);
    BrViewBuffersRebase();
    BrPodNop();

    if (DAT_100a64b8 == 0 && DAT_106ed698 == 0) {
        if (aViews->w < 0x130)
            BrGfxFillRect(aViews->x + aViews->w, 8, 0x130 - aViews->w, 0xe0,
                          0, 0, 0);
    } else {
        BrGfxClearScreen(0, 0, 0);
        if (DAT_100a64b8 != 0)
            DAT_100a64b8 = DAT_100a64b8 - 1;
    }
    if (g_brCViews != DAT_100a64bc) {
        DAT_100a64b8 = 2;
        DAT_100a64bc = g_brCViews;
    }

    for (i = 0; i < g_brCViews; i++) {
        BrPodNop(0, 0, 0, 0, 0xff);
        pV = &aViews[i];
        DAT_106e9d88 = pCars + pV->iCar * BR_CAR_STRIDE;
        DAT_106ed520 = CAR_PCAM(DAT_106e9d88);
        g_brIView = i;
        FUN_1006ec30(0, 0, DAT_106ed520->pos, DAT_106ed528, DAT_106e8a18,
                     DAT_106ed590, DAT_106b7ac4, DAT_106e728c, DAT_106ec780);
        BrDlRectCmdEmit(pV->x, pV->y, pV->w, pV->h, 1);
        if (g_brCViews > 1) {
            BrCamFrustumBuild(DAT_106ed520, DAT_106ed520->fov * kF7290,
                              DAT_100aa040 * kF728C, (float)pV->w, (float)pV->h);
            BrCamMatrixSetup(DAT_106ed520, DAT_106ed520->fov * kF7290,
                             DAT_100aa040 * kF728C, (float)pV->w, (float)pV->h);
        } else {
            BrCamFrustumBuild(DAT_106ed520, DAT_106ed520->fov, DAT_100aa040,
                              (float)pV->w, (float)pV->h);
            BrCamMatrixSetup(DAT_106ed520, DAT_106ed520->fov, DAT_100aa040,
                             (float)pV->w, (float)pV->h);
        }
        FUN_1002af17();
        FUN_1002b480();
        BrPodNop(0, 0, 0x82, 0, 0xff);
        BrSceneSetupFrame(aViews);
        BrSceneAccumReset();
        BrSpanBuildHull();
        DAT_10273688 = (DAT_106ed6ac != 0) + 1;
        for (k = 0; k < g_BrCarCount; k++)
            BrCarVisibilityUpdate(pCars + k * BR_CAR_STRIDE);
        BrSceneDlBuild(aViews, 0, pHdr, pCars);
        if (DAT_106ed6b0 == 0 || DAT_100b3014 == 2 || DAT_100b3014 == 8) {
            FUN_1002af17();
            FUN_1002b480();
            BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
            for (k = 0; k < g_brRaceNDriver; k++) {
                pCar = HDR_CAR(pHdr, k);
                if (pCar != 0 && CAR_KIND(pCar) != 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrPodNop(0, 0x80, 0x80, 0, 0xff);
            BrNop6E590(aViews);
            FUN_10011650(aViews);
        }
        BrPodNop(0, 0x40, 0x40, 0x40, 0xff);
        FUN_10011d20();
        BrSceneDlBuild(aViews, 1, pHdr, pCars);
        if (DAT_106ed6b0 != 0 && DAT_100b3014 != 2 && DAT_100b3014 != 8) {
            FUN_1002af17();
            FUN_1002b480();
            BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
            for (k = 0; k < g_brRaceNDriver; k++) {
                pCar = HDR_CAR(pHdr, k);
                if (pCar != 0 && CAR_KIND(pCar) != 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrPodNop(0, 0x80, 0x80, 0, 0xff);
            BrNop6E590(aViews);
            FUN_10011650(aViews);
        }
        BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
        for (k = 0; k < g_brRaceNDriver; k++) {
            pCar = HDR_CAR(pHdr, k);
            if (pCar != 0)
                BrCarDrawBody(pCar);
        }
        BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
        for (k = 0; k < g_brRaceNDriver; k++) {
            pCar = HDR_CAR(pHdr, k);
            if (pCar != 0 && CAR_KIND(pCar) == 2)
                BrCarDrawVehicle(pCar, 0);
        }
        BrPodNop(0, 0x80, 0x80, 0, 0xff);
        FUN_100119c0(aViews, pHdr);
        BrPodNop(0, 0, 0xff, 0xff, 0xff);
        BrEnvEmit();
        BrPodNop(0, 0, 0x82, 0, 0xff);
        BrFadeDrawSprite(aViews, g_4B16AC - g_4B16A0 * kF7294);
        BrCamMatrixSetupOrtho((float)pV->w, (float)pV->h);
        BrDlScreenRectEmit(pV->x, pV->y, pV->w, pV->h, 1);

        /* The rear-view mirror: single view, the car's first camera active
         * and a mirror size selected.  The scene is drawn again into a
         * strip along the top through the car's second camera. */
        if (DAT_106ed520 == (BrCamObj *)(DAT_106e9d88 + 0x27C4)
            && g_brCViews == 1 && DAT_100aa018 != 0) {
            if (DAT_100aa018 == 1)
                wMir = pV->w / 4;
            else if (DAT_100aa018 == 2)
                wMir = (pV->w * 5) / 16;
            else
                wMir = (pV->w * 3) / 8 + 2;
            xMir = ((pV->w - wMir) >> 1) + pV->x;
            yMir = pV->h / 16 + pV->y;
            pCamSave = DAT_106ed520;
            CAR_PCAM(DAT_106e9d88) = (BrCamObj *)(DAT_106e9d88 + 0x2890);
            hMir = wMir >> 2;
            DAT_106ed520 = CAR_PCAM(DAT_106e9d88);
            BrDlRectCmdEmit(xMir, yMir, -wMir, hMir, 1);
            BrNop_1002AB94(xMir, yMir, wMir, hMir);
            BrCamFrustumBuild(DAT_106ed520, DAT_106ed520->fov,
                              DAT_100aa040 * kF7298, (float)wMir, (float)hMir);
            BrCamMatrixSetup(DAT_106ed520, DAT_106ed520->fov,
                             DAT_100aa040 * kF729C, (float)wMir, (float)hMir);
            FUN_1002af17();
            FUN_1002b480();
            BrPodNop(0, 0, 0x82, 0, 0xff);
            BrSceneSetupFrame(aViews);
            BrSceneAccumReset();
            BrSpanBuildHull();
            BrPodNop(0, 0, 0x82, 0, 0xff);
            for (k = 0; k < g_BrCarCount; k++)
                BrCarVisibilityUpdate(pCars + k * BR_CAR_STRIDE);
            BrSceneDlBuild(aViews, 0, pHdr, pCars);
            if (DAT_106ed6b0 == 0 || DAT_100b3014 == 2 || DAT_100b3014 == 8) {
                FUN_1002af17();
                FUN_1002b480();
                BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
                for (k = 0; k < g_brRaceNDriver; k++) {
                    pCar = HDR_CAR(pHdr, k);
                    if (pCar != 0 && CAR_KIND(pCar) != 2)
                        BrCarDrawVehicle(pCar, 0);
                }
                BrPodNop(0, 0x80, 0x80, 0, 0xff);
                BrNop6E590(aViews);
                FUN_10011650(aViews);
            }
            BrSceneDlBuild(aViews, 1, pHdr, pCars);
            if (DAT_106ed6b0 != 0 && DAT_100b3014 != 2 && DAT_100b3014 != 8) {
                FUN_1002af17();
                FUN_1002b480();
                BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
                for (k = 0; k < g_brRaceNDriver; k++) {
                    pCar = HDR_CAR(pHdr, k);
                    if (pCar != 0 && CAR_KIND(pCar) != 2)
                        BrCarDrawVehicle(pCar, 0);
                }
            }
            BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
            for (k = 0; k < g_brRaceNDriver; k++) {
                pCar = HDR_CAR(pHdr, k);
                if (pCar != 0)
                    BrCarDrawBody(pCar);
            }
            BrPodNop(0, 0x80, 0x80, 0x80, 0xff);
            for (k = 0; k < g_brRaceNDriver; k++) {
                pCar = HDR_CAR(pHdr, k);
                if (pCar != 0 && CAR_KIND(pCar) == 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrFadeDrawSprite(aViews, g_4B16A0 + g_4B16AC);
            BrPodNop(0, 0, 0x82, 0, 0xff);
            CAR_PCAM(DAT_106e9d88) = pCamSave;
            DAT_106ed520 = pCamSave;
            BrDlRectCmdEmit(pV->x, pV->y, pV->w, pV->h, 1);
            BrDlBorderEmit(xMir, yMir, wMir, hMir);
        }

        BrFpsReadout(aViews);

        /* The split-screen captions, by game mode. */
        if (DAT_100a9360 == 5) {
            BrTextFlag358Clear();
            BrSet_10019270();
            BrSetGlobal_ABB30(0x28);
            if (*(char *)(*(uint8_t **)(pCars + 0xE8C) + 4) != 0)
                BrTextDraw(BrStrGet(0xF0), DAT_106e7714 / 2, DAT_106e9a2c - 0x1E);
            else
                BrTextDraw(BrStrGet(0xF1), DAT_106e7714 / 2, DAT_106e9a2c - 0x1E);
        } else if (DAT_100aa024 != 0) {
            if (DAT_100a9360 != 4) {
                if (DAT_105ccb88 != 0) {
                    BrTextFlag358Clear();
                    BrSub_10019280();
                    BrSetGlobal_ABB30(0xF);
                    BrTextDraw(BrStrGet(0xF2), 0x1C, 0x20);
                    if (DAT_100a9360 == 6)
                        FUN_10014e00(aViews, pCars);
                    BrSub_10019290();
                    BrTextDraw(BrStrGet(0xF4), DAT_106e7714 - 0x1C,
                               DAT_106e9a2c - 0x18);
                } else if (DAT_106ed520 != (BrCamObj *)(DAT_106e9d88 + 0x2808)) {
                    /* The original's branch is `je` into the message arm, so
                     * the HUD arm is the FALLTHROUGH: the test is `!=` and
                     * this arm comes first in the source. */
                    BrPodNop(0, 0xff, 0, 0, 0xff);
                    BrDlScreenRectEmit(pV->x, pV->y, pV->w, pV->h, 1);
                    BrHudDraw(aViews, pCars);
                    FUN_10014e00(aViews, pCars);
                    BrPodNop(0, 0, 0x82, 0, 0xff);
                    if (DAT_106ed520 != (BrCamObj *)(DAT_106e9d88 + 0x27C4))
                        BrPodNop();
                } else if ((*(uint8_t **)(DAT_106e9d88 + 0xF00))[0x68] & 2) {
                    BrHudDrawViewMessage(aViews);
                }
            } else if (DAT_100aa024 != 0 && DAT_100a9360 == 4
                       && g_brRaceBeginStage == 0) {
                BrTextFlag358Clear();
                BrSetGlobal_ABB30(0xF);
                BrSub_10019290();
                BrTextDraw(BrStrGet(0xF4), DAT_106e7714 - 0x1C,
                           DAT_106e9a2c - 0x18);
            }
        }
        BrNop_1002C509(pV->x, pV->y, pV->w, pV->h);
        BrHudDrawViewCentreText(aViews);
    }

    BrNop_1002AB8F();
    BrSub_1003289F(0, 0, DAT_106e7714, DAT_106e9a2c);

    /* The pause menu. */
    if (DAT_105ccb5c == 2) {
        x = (aViews->w >> 1) + aViews->x;
        BrPodNop(0x3EA8F5C3);           /* 0.33f as its bits: a 4-byte push */
        BrTextFlag358Clear();
        BrSet_10019270();
        BrSetGlobal_ABB30(0x28);
        BrTextDraw(BrStrGet(0xF5), x, (DAT_106e9a2c * 5) / 16);
        y = (DAT_106e9a2c * 5) / 11;
        BrSetGlobal_ABB30(0x14);
        BrTextDraw(BrStrGet(0xF6), x, y);
        y += 0x28;
        if (DAT_105bc8dc == 0)
            psz = BrStrGet(0xF7);
        else
            psz = BrStrGet(0xF8);
        BrTextDraw(psz, x, y);
        y += 0x14;
        if (DAT_105bc8dc == 1)
            psz = BrStrGet(0xF9);
        else
            psz = BrStrGet(0xFA);
        BrTextDraw(psz, x, y);
    } else if (DAT_105ccb5c != 0) {
        x = (aViews->w >> 1) + aViews->x;
        BrPodNop(0x3EA8F5C3);
        BrTextFlag358Clear();
        BrSet_10019270();
        BrSetGlobal_ABB30(0x28);
        BrTextDraw(BrStrGet(0xF5), x, (DAT_106e9a2c * 5) / 16);
        y = (DAT_106e9a2c * 5) / 11;
        BrSetGlobal_ABB30(0x14);
        if (DAT_105bc8dc == 0)
            psz = BrStrGet(0xFB);
        else
            psz = BrStrGet(0xFC);
        BrTextDraw(psz, x, y);
        y += 0x14;
        if (g_brRaceNet == 0) {
            if (DAT_105bc8dc == 1)
                psz = BrStrGet(0xFF);
            else
                psz = BrStrGet(0x100);
            BrTextDraw(psz, x, y);
        }
        y += 0x14;
        sprintf(DAT_10396f28, BrStrGet(0x101),
                DAT_105bc8dc == 2 ? DAT_100a6b64 : DAT_100a6b60, g_brB4E70C);
        BrTextDraw(DAT_10396f28, x, y);
        y += 0x14;
        sprintf(DAT_10396f28, BrStrGet(0x102),
                DAT_105bc8dc == 3 ? DAT_100a6b64 : DAT_100a6b60, g_brB4E708);
        BrTextDraw(DAT_10396f28, x, y);
        y += 0x14;
        if (DAT_100a9360 == 0) {
            if (DAT_105bc8dc == 4)
                psz = BrStrGet(0x103);
            else
                psz = BrStrGet(0x104);
        } else {
            if (DAT_105bc8dc == 4)
                psz = BrStrGet(0x105);
            else
                psz = BrStrGet(0x106);
        }
        BrTextDraw(psz, x, y);
        y += 0x14;
        if (DAT_105bc8dc == 5)
            psz = BrStrGet(0x107);
        else
            psz = BrStrGet(0x108);
        BrTextDraw(psz, x, y);
    }

    /* Attract mode: the text list, or a credits page once its timer runs. */
    if (DAT_100a9360 == 4 && g_brRaceBeginStage == 2) {
        BrHudTextListDraw(aViews);
    } else if (DAT_100a9360 == 4 && g_brRaceBeginStage == 1
               && DAT_100a9368[DAT_105bc768][0] != 0
               && DAT_105bc884 > kF72A0) {
        BrTextFlag358Clear();
        BrSet_10019270();
        BrTextSetColors(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
        for (n = 0; n < 6; n++) {
            if (DAT_100a9368[DAT_105bc768][2 + n] == 0)
                break;
        }
        y = DAT_106e9a2c / 2 - (n * 40) / 4 + 10;
        if (DAT_100a9368[DAT_105bc768][0][0] == 0)
            y -= 5;
        for (n--; n >= 0; n--) {
            if (DAT_100a9368[DAT_105bc768][2 + n][0] == '`') {
                BrSetGlobal_ABB30(0xF);
                BrTextDraw(DAT_100a9368[DAT_105bc768][2 + n] + 1,
                           DAT_106e7714 / 2, (n * 40) / 2 + y);
            } else {
                BrSetGlobal_ABB30(0x14);
                BrTextDraw(DAT_100a9368[DAT_105bc768][2 + n],
                           DAT_106e7714 / 2, (n * 40) / 2 + y);
            }
        }
        BrSetGlobal_ABB30(0xF);
        BrTextDraw(DAT_100a9368[DAT_105bc768][0], DAT_106e7714 / 2, y - 0x14);
    }

    BrFadeDrawBars();
    if (DAT_106ed6bc >= 2) {
        BrDlRectCmdEmit(0, 0, DAT_106e7714, DAT_106e9a2c, 1);
        BrPodNop();
    }
    BrPodNop(0, 0xff, 0xe0, 0, 0xff);
    FUN_1002cb49();
    BrPodNop(0, 0, 0x82, 0, 0xff);
    FUN_1002cee9();
}

#endif /* BR_MATCHING_BUILD */
