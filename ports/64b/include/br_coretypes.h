/* br_coretypes.h: types the original globals are made of that used to be
 * private to one file (they live here so br_globals.h can declare them). */
#ifndef BR_CORETYPES_H
#define BR_CORETYPES_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif
#include <stdint.h>
#include <stdio.h>
#include "br_vec.h"
#include "br_mat.h"
typedef struct BrNetSession {
    uint8_t player[200];                     /* +0x0000  the enumerated session's 200-byte player block */
    int32_t guid[4];                         /* +0x00C8  the session GUID */
    uint32_t cbPayload;                      /* +0x00D8 */
    void *pPayload;                          /* +0x00DC  GlobalLock'd copy of the session payload */
} BrNetSession;
typedef struct BrLivery {
    void * apBmp[4];                         /* +0x0000  the scheme's RGBA bitmap and its three damage copies */
    int32_t id;                              /* +0x0010 */
    int32_t rect[4];                         /* +0x0014  source rectangle (all zero: the whole bitmap) */
    char *pszName;                           /* +0x0024  the bitmap's file name */
} BrLivery;

/* BR_CORETYPES_BODY */
/* One texture upload request (the texture cache record's body). */
typedef struct BrTexReq272 {
    unsigned int fTmu2;                      /* +0x0000 */
    unsigned char lod;                       /* +0x0004 */
    uint8_t _pad0005[0x3];
    int w;                                   /* +0x0008 */
    int h;                                   /* +0x000C */
    int fmt;                                 /* +0x0010 */
    int f14;                                 /* +0x0014 */
    int aspect0;                             /* +0x0018 */
    int aspect1;                             /* +0x001C */
    int f20;                                 /* +0x0020 */
    unsigned int fClampS;                    /* +0x0024 */
    unsigned int fClampT;                    /* +0x0028 */
    int f2c;                                 /* +0x002C */
    int f30;                                 /* +0x0030 */
    int f34;                                 /* +0x0034 */
    int f38;                                 /* +0x0038 */
    int cbTotal;                             /* +0x003C */
    int wPow;                                /* +0x0040 */
    int hPow;                                /* +0x0044 */
    unsigned char *p1;                       /* +0x0048 */
    unsigned char *p2;                       /* +0x004C */
    int p8;                                  /* +0x0050 */
    int p9;                                  /* +0x0054 */
    int iLevel;                              /* +0x0058 */
    int f5c;                                 /* +0x005C */
    int lv[8][16];                           /* +0x0060 */
    int f260;                                /* +0x0260 */
    int f264;                                /* +0x0264 */
    int f268;                                /* +0x0268 */
    int f26c;                                /* +0x026C */
    int f270;                                /* +0x0270 */
    int f274;                                /* +0x0274 */
    int f278;                                /* +0x0278 */
    uint16_t * apMip[4];                     /* +0x027C  expanded mip levels, live when non-NULL */
    int f028C;                               /* +0x028C */
    char b290;                               /* +0x0290 */
    char b291;                               /* +0x0291 */
    char b292;                               /* +0x0292 */
    char b293;                               /* +0x0293 */
    char b294;                               /* +0x0294 */
    char b295;                               /* +0x0295 */
    char b296;                               /* +0x0296 */
    char b297;                               /* +0x0297 */
    int f298;                                /* +0x0298 */
    int cb29c;                               /* +0x029C */
    int w2a0;                                /* +0x02A0 */
    int h2a4;                                /* +0x02A4 */
} BrTexReq272;

struct Metric12 {
    short          advance;     /* +0x00 -- signed: the pen advance is movsx'd */
    unsigned short height;      /* +0x02 */
    short          sprite;      /* +0x04 */
    unsigned short f06;
    unsigned short f08;
    unsigned short f0a;
};
typedef struct Metric12 Metric12;

struct BrCfgRec39580 {
    unsigned int key;           /* +0x00 */
    char         szText[0x20];  /* +0x04 */
};
typedef struct BrCfgRec39580 BrCfgRec39580;

typedef struct BrLevelRec {
  char aIdx[2];
  short mask;
  unsigned char aPair[20];
} BrLevelRec;

struct BrRec24 { int f00; char pad04[20]; };
typedef struct BrRec24 BrRec24;

struct Metric12B {
    short          advance;     /* +0x00 -- signed: movsx before the fild */
    unsigned short height;      /* +0x02 */
    short          sprite;      /* +0x04 */
    unsigned short f06;
    unsigned short f08;
    unsigned short f0a;
};
typedef struct Metric12B Metric12B;

struct BrBind39870 {
    unsigned int key;           /* +0x00 */
    int          f04;           /* +0x04 */
};
typedef struct BrBind39870 BrBind39870;

struct KeyEnt {
    int id;
    int f04;
};
typedef struct KeyEnt KeyEnt;

#define BR_PENDLIST_MAX 30
typedef struct BrPendList {
    int32_t  aItems[BR_PENDLIST_MAX];   /* ctx+0x04 .. ctx+0x78, texture ids */
    int32_t  count;                     /* ctx+0x7C */
} BrPendList;
struct FlagObj;
struct Node;
struct Obj400;
typedef struct BrSnapTailC { int32_t a[0x802]; } BrSnapTailC;

typedef struct BrSnapTailA { int32_t a[0x16]; }  BrSnapTailA;

typedef struct BrDriverCar BrSnapCar;

typedef struct BrDriver BrSnapDrv;   /* the same object as BrDriver */

typedef struct BrAiPathPt {
    BrVec3 left;                 /* +0x00 */
    BrVec3 centre;               /* +0x0C */
    BrVec3 right;                /* +0x18 */
    float  arc;                  /* +0x24  distance still to run this lap  */
} BrAiPathPt;

struct FlagObj {
    int pad[7];
    int f1C;
};
typedef struct FlagObj FlagObj;

typedef struct BrObj2C { int32_t _[11]; int32_t f2C; int32_t f30; } BrObj2C;



struct Phase;
typedef void (*PhaseEnterFn)(struct Phase *);


typedef struct PwSeg { BrVec2 a, b; int f10; } PwSeg;

/* A node of the track's path ring: a record in the loaded track image, so
 * its links are the file's 32-bit addresses (rebased by BrTrackFixupSegRec
 * to br_addr32 values); read them through BR_PTR32. */
typedef struct BrAiPathNode {
    uint32_t  aNext;             /* +0x00  BrAiPathNode */
    uint32_t  aSib;              /* +0x04  BrAiPathNode */
    uint32_t  a08, a0C;          /* +0x08 */
    uint8_t   f10;               /* +0x10 */
    uint8_t   f11;               /* +0x11  cleared by 0x10061660 */
    uint8_t   f12[2];            /* +0x12 */
    uint16_t  count;             /* +0x14 */
    uint16_t  flags;             /* +0x16  bit 0 = skip this node          */
    uint8_t   a18[0x40 - 0x18];
    BrAiPathPt aPt[1];           /* +0x40 */
} BrAiPathNode;

typedef struct { float m[16]; } BrDlMtx;

/* 0x106E86B8, stride 0x58: one per screen view, two in split screen.
 * BrFrameBeginDl sets the rectangle, the race start sets the car each view
 * follows, and the race start's texture pass keeps the view's handles. */
typedef struct BrView {
    int32_t rect[4];          /* +0x00  x, y, w, h */
    int32_t iCar;             /* +0x10  the car this view follows */
    int32_t ahTex[16];        /* +0x14 */
    int32_t hTexB;            /* +0x54 */
} BrView;

/* BrSnap: slice3_41.h (it embeds whole cars) */

typedef struct BrSndBankCarSlot {
    int32_t iName;
    int32_t pad;
} BrSndBankCarSlot;

struct Dim {
    int w;
    int h;
};
typedef struct Dim Dim;

typedef struct BrAiPathPt RcPoint;   /* the same object as BrAiPathPt */
typedef void (*BrCheatFn)(void);
/* One entry of a peer record's +0xD0 table: only its leading time is read. */
typedef struct BrPeerSub {
    float time;                              /* +0x0000 */
    uint8_t _pad0004[0x9C];
} BrPeerSub;

/* One networking peer record, 0x96C bytes: the union of what br_peerpump.c
 * and br_peerrank.c read of it. */
typedef struct BrPeerRec {
    void *hMutex;                            /* +0x0000 */
    int f004;                                /* +0x0004 */
    int f008;                                /* +0x0008  stamped with the current tick */
    uint8_t _pad000C[0x20];
    int f02C;                                /* +0x002C */
    int f030;                                /* +0x0030 */
    unsigned char f034;                      /* +0x0034 */
    unsigned char f035;                      /* +0x0035 */
    unsigned char f036;                      /* +0x0036 */
    unsigned char f037;                      /* +0x0037 */
    uint8_t _pad0038[0x98];
    BrPeerSub aSub[7];                       /* +0x00D0 */
    uint8_t _pad0530[0x28];
    int f558;                                /* +0x0558 */
    char szName[1024];                       /* +0x055C */
    int f95C;                                /* +0x095C */
    int f960;                                /* +0x0960 */
    short f964;                              /* +0x0964 */
    uint8_t _pad0966[0x2];
    unsigned int f968;                       /* +0x0968 */
} BrPeerRec;

/* One entrant slot of the 0x80-byte driver table seen from its car pointer
 * (the driver record's +0x60). */
struct BrDriverCar;
typedef struct BrDriverSlot {
    struct BrDriverCar *pCar;
    unsigned char       pad[0x7C];
} BrDriverSlot;

/* A player's 0x2B68-byte state record. */
typedef struct BrPlayerState {
    int  *pBlock;                                /* +0x000 the option block */
    char  rest[0x2B68 - 4];
} BrPlayerState;
typedef struct Img Img;

typedef int (*funcptr)();

typedef struct { int a, b, c, d; } BrDpGuid;

typedef struct BrHitRect { int32_t l, t, r, b; } BrHitRect;

typedef struct BrLightHist {
    unsigned char body[0x10];
    char          bx, by, bz;     /* +0x10 */
    unsigned char pad[5];
} BrLightHist;

typedef struct BrRaceSelEntry {
    int32_t  f00;          /* car +0x11C */
    int32_t  nSel;         /* byte out of the season record's grid */
    int32_t  nTbl;         /* g_tblBrA9560[nSel] */
    int32_t  f0C;          /* car +0x160 */
    int32_t  a10[12];      /* car +0x128 .. +0x158 */
} BrRaceSelEntry;

typedef struct BrRaceSel {
    int32_t  mode;         /* +0x00  g_brRaceMode */
    int32_t  b4;           /* +0x04  record byte +4 */
    int32_t  b5;           /* +0x08  record byte +5 */
    int32_t  rec0;         /* +0x0C  record dword +0 */
    int32_t  track;        /* +0x10 */
    int32_t  weather;      /* +0x14 */
    BrRaceSelEntry e[4];   /* +0x18 */
} BrRaceSel;

typedef struct BrGlyphMetric12 {
    uint16_t advance;   /* +0x00 */
    uint16_t height;    /* +0x02 */
    uint16_t s4, s6, s8, sA;
} BrGlyphMetric12;

typedef struct {
    BrCheatFn  fn;
    char      *text;
} BrCheatEntry;



typedef struct BrEarMixEvent {
    short f00;            /* +0x00  0x10220C50 */
    short f02;            /* +0x02 */
    int   track;          /* +0x04  0x10220C54 */
    int   result;         /* +0x08  0x10220C58 */
    char  pad0C[0x10];
    int   flags;          /* +0x1C  0x10220C6C */
    char  pad20[0x22];
    short word42;         /* +0x42  0x10220C92 */
} BrEarMixEvent;

typedef struct BrNetSlot BrNetSlot978;   /* the same object as BrNetSlot */

typedef struct {
    int key;         /* what the ack quotes back */
    int tSent;       /* local time the ping went out */
} BrPingSlot;

typedef struct BrTrailSeg {
    float    x1;                    /* 0x10273690 */
    float    y1;                    /* 0x10273694 */
    float    z1;                    /* 0x10273698 */
    float    x2;                    /* 0x1027369C */
    float    y2;                    /* 0x102736A0 */
    float    z2;                    /* 0x102736A4 */
    uint32_t flags;                 /* 0x102736A8 */
} BrTrailSeg;

typedef struct BrVisCell {
    unsigned char col;
    unsigned char row;
    short         dist;
} BrVisCell;

typedef int (__stdcall *BrCdVolumeSetFn)(int, int);

typedef int (__stdcall *BrEarMixEventFn)(void *pEvent);

typedef int (__stdcall *BrEarShutdownChannelFn)(int);

typedef int (__stdcall *BrEarClearChannelFn)(int channel, int flags);

/* Lives inside the loaded track file (0x106EECCC points at it), so its
 * layout is the file's: the queued items are 32-bit texture ids. */
typedef struct BrPendCtx {
    uint32_t unused0;
    int32_t  aItems[BR_PENDLIST_MAX];
    int32_t  count;
} BrPendCtx;

typedef struct {
    int   f00;
    int   f04;
    int   f08;
    int   f0C;
    int  *f10;
    int   f14;
    int  *f18;
    int   f1C;
    int  *f20;
    int   f24;
    int   f28;
    int   f2C;
    int   f30;
    int   f34;
    int   pad38[2];
} BrFrameTask;

typedef struct Br63Race {
    char    _a[0x0FA8];
    int32_t cLaps;              /* +0x0FA8 */
    char    _b[0x0FB0 - 0x0FAC];
    uint32_t timeA;             /* +0x0FB0 */
    char    _c[0x0FE4 - 0x0FB4];
    uint32_t timeB;             /* +0x0FE4 */
    char    _d[0x0FEC - 0x0FE8];
    uint32_t timeC;             /* +0x0FEC */
} Br63Race;

typedef struct BrCamera {
    BrVec3        dir;            /* +0x00 */
    unsigned char pad[0x24];
    BrVec3        pos;            /* +0x30 */
} BrCamera;

struct Img {
    void *surf;
    char *path;
};

typedef struct BrAiPathNode RcNode;   /* the same object as BrAiPathNode */

typedef struct BrPeerRankEnt {
    int   idx;                           /* peer index */
    float score;
} BrPeerRankEnt;

typedef struct BrInJoy {
    int32_t  lX, lY, lZ;               /* +0x00 +0x04 +0x08 */
    int32_t  lRx, lRy, lRz;            /* +0x0C +0x10 +0x14 */
    int32_t  rglSlider[2];             /* +0x18              */
    uint32_t rgdwPOV[4];               /* +0x20              */
    uint8_t  rgbButtons[128];          /* +0x30              */
    uint8_t  pad[0x110 - 0xB0];        /* +0xB0 velocities.. */
} BrInJoy;

typedef struct BrInDiDev     BrInDiDev;

typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(int32_t)) ? 1 : -1];






























#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif



























/* BR_GLOBALS_BEGIN: generated by ports/64b/tools/unify.py */
#ifdef __cplusplus
extern "C" {
#endif
#pragma push_macro("DAT_10078978")
#undef DAT_10078978
extern BrDpGuid DAT_10078978;  /* 0x10078978 */
#pragma pop_macro("DAT_10078978")
#pragma push_macro("DAT_10078998")
#undef DAT_10078998
extern BrDpGuid DAT_10078998;  /* 0x10078998 */
#pragma pop_macro("DAT_10078998")
#pragma push_macro("DAT_100789b8")
#undef DAT_100789b8
extern BrDpGuid DAT_100789b8;  /* 0x100789B8 */
#pragma pop_macro("DAT_100789b8")
#pragma push_macro("DAT_100789d8")
#undef DAT_100789d8
extern BrDpGuid DAT_100789d8;  /* 0x100789D8 */
#pragma pop_macro("DAT_100789d8")
#pragma push_macro("DAT_100789f8")
#undef DAT_100789f8
extern BrDpGuid DAT_100789f8;  /* 0x100789F8 */
#pragma pop_macro("DAT_100789f8")
#pragma push_macro("g_BrVisLightHist")
#undef g_BrVisLightHist
extern BrLightHist g_BrVisLightHist[4];  /* 0x100A5CA8 */
#pragma pop_macro("g_BrVisLightHist")
#pragma push_macro("g_BrVisLightTemplate")
#undef g_BrVisLightTemplate
extern BrLightHist g_BrVisLightTemplate;  /* 0x100A9FF0 */
#pragma pop_macro("g_BrVisLightTemplate")
#pragma push_macro("g_aKeyEnt0AAAD0")
#undef g_aKeyEnt0AAAD0
extern KeyEnt g_aKeyEnt0AAAD0[21];  /* 0x100AAAD0 */
#pragma pop_macro("g_aKeyEnt0AAAD0")
#pragma push_macro("g_brBindAAAD4")
#undef g_brBindAAAD4
extern BrBind39870 g_brBindAAAD4[21];  /* 0x100AAAD4 */
#pragma pop_macro("g_brBindAAAD4")
#pragma push_macro("g_hot1")
#undef g_hot1
extern BrHitRect g_hot1;  /* 0x100AABB8 */
#pragma pop_macro("g_hot1")
#pragma push_macro("g_hot2")
#undef g_hot2
extern BrHitRect g_hot2;  /* 0x100AABC8 */
#pragma pop_macro("g_hot2")
#pragma push_macro("g_hot0")
#undef g_hot0
extern BrHitRect g_hot0;  /* 0x100AABE8 */
#pragma pop_macro("g_hot0")
#pragma push_macro("g_aBrCheatCode")
#undef g_aBrCheatCode
extern BrCheatEntry g_aBrCheatCode[];  /* 0x100ABE48 */
#pragma pop_macro("g_aBrCheatCode")
#pragma push_macro("g_BrGlyphFontA12")
#undef g_BrGlyphFontA12
extern Metric12 g_BrGlyphFontA12[95];  /* 0x100ABE84 */
#pragma pop_macro("g_BrGlyphFontA12")
#pragma push_macro("g_BrGlyphFontB12")
#undef g_BrGlyphFontB12
extern Metric12B g_BrGlyphFontB12[95];  /* 0x100AC2FC */
#pragma pop_macro("g_BrGlyphFontB12")
#pragma push_macro("g_tab")
#undef g_tab
extern struct Dim g_tab[7];  /* 0x100AC908 */
#pragma pop_macro("g_tab")
#pragma push_macro("g_aBrLivery")
#undef g_aBrLivery
extern BrLivery g_aBrLivery[16][30];  /* 0x100AD7D8 */
#pragma pop_macro("g_aBrLivery")
#pragma push_macro("g_brTbl0B3020")
#undef g_brTbl0B3020
extern BrRec24 g_brTbl0B3020[];  /* 0x100B3020 */
#pragma pop_macro("g_brTbl0B3020")
#pragma push_macro("DAT_100b3024")
#undef DAT_100b3024
extern BrLevelRec DAT_100b3024[];  /* 0x100B3024 */
#pragma pop_macro("DAT_100b3024")
#pragma push_macro("g_aBrCtlNameKey")
#undef g_aBrCtlNameKey
extern const BrCfgRec39580 g_aBrCtlNameKey[120];  /* 0x100B3B40 */
#pragma pop_macro("g_aBrCtlNameKey")
#pragma push_macro("g_0B6540")
#undef g_0B6540
extern BrSndBankCarSlot g_0B6540[];  /* 0x100B5D48 */
#pragma pop_macro("g_0B6540")
#pragma push_macro("g_0B6C00")
#undef g_0B6C00
extern BrSndBankCarSlot g_0B6C00[];  /* 0x100B6408 */
#pragma pop_macro("g_0B6C00")
#pragma push_macro("g_0B6C48")
#undef g_0B6C48
extern BrSndBankCarSlot g_0B6C48[];  /* 0x100B6450 */
#pragma pop_macro("g_0B6C48")
#pragma push_macro("g_a220B20")
#undef g_a220B20
extern BrRaceSel g_a220B20;  /* 0x1021C650 */
#pragma pop_macro("g_a220B20")
#pragma push_macro("g_brEarEvent")
#undef g_brEarEvent
extern BrEarMixEvent g_brEarEvent;  /* 0x1021C780 */
#pragma pop_macro("g_brEarEvent")
#pragma push_macro("g_aBrPing")
#undef g_aBrPing
extern BrPingSlot g_aBrPing[8];  /* 0x102265E0 */
#pragma pop_macro("g_aBrPing")
#pragma push_macro("DAT_10273690")
#undef DAT_10273690
extern BrTrailSeg DAT_10273690[];  /* 0x10273690 */
#pragma pop_macro("DAT_10273690")
#pragma push_macro("g_BrVisCells")
#undef g_BrVisCells
extern BrVisCell g_BrVisCells[193];  /* 0x1035F7E8 */
#pragma pop_macro("g_BrVisCells")
#pragma push_macro("g_575454")
#undef g_575454
extern BrCdVolumeSetFn g_575454;  /* 0x104B15FC */
#pragma pop_macro("g_575454")
#pragma push_macro("g_pfn57546C")
#undef g_pfn57546C
extern BrEarMixEventFn g_pfn57546C;  /* 0x104B1614 */
#pragma pop_macro("g_pfn57546C")
#pragma push_macro("g_575470")
#undef g_575470
extern BrEarShutdownChannelFn g_575470;  /* 0x104B1618 */
#pragma pop_macro("g_575470")
#pragma push_macro("g_pfn575480")
#undef g_pfn575480
extern BrEarClearChannelFn g_pfn575480;  /* 0x104B1628 */
#pragma pop_macro("g_pfn575480")
#pragma push_macro("DAT_105ccd50")
#undef DAT_105ccd50
extern BrDlMtx DAT_105ccd50[];  /* 0x105CCD50 */
#pragma pop_macro("DAT_105ccd50")
#pragma push_macro("DAT_106e8618")
#undef DAT_106e8618
extern BrFrameTask DAT_106e8618[];  /* 0x106E8618 */
#pragma pop_macro("DAT_106e8618")
#pragma push_macro("g_pBr63Race")
#undef g_pBr63Race
extern Br63Race *g_pBr63Race;  /* 0x106E9D88 */
#pragma pop_macro("g_pBr63Race")
#pragma push_macro("g_BrCamera")
#undef g_BrCamera
extern BrCamera *g_BrCamera;  /* 0x106ED520 */
#pragma pop_macro("g_BrCamera")
#pragma push_macro("g_aBrNetSession")
#undef g_aBrNetSession
extern BrNetSession g_aBrNetSession[16];  /* 0x10AC3080 */
#pragma pop_macro("g_aBrNetSession")
#pragma push_macro("g_img")
#undef g_img
extern Img g_img[145];  /* 0x10AC53E8 */
#pragma pop_macro("g_img")
#pragma push_macro("g_pBrAiScanBestNode")
#undef g_pBrAiScanBestNode
extern BrAiPathNode *g_pBrAiScanBestNode;  /* 0x10AC67D4 */
#pragma pop_macro("g_pBrAiScanBestNode")
#pragma push_macro("g_brRacePathNode")
#undef g_brRacePathNode
extern RcNode *g_brRacePathNode;  /* 0x10B1CBEC */
#pragma pop_macro("g_brRacePathNode")
#pragma push_macro("g_aBrCtlNameMouse")
#undef g_aBrCtlNameMouse
extern BrCfgRec39580 g_aBrCtlNameMouse[10];  /* 0x10B71B08 */
#pragma pop_macro("g_aBrCtlNameMouse")
#pragma push_macro("g_aBrCtlNameJoy")
#undef g_aBrCtlNameJoy
extern BrCfgRec39580 g_aBrCtlNameJoy[134];  /* 0x10B71C70 */
#pragma pop_macro("g_aBrCtlNameJoy")
#pragma push_macro("g_aBrPeer71")
#undef g_aBrPeer71
extern BrPeerRec g_aBrPeer71[16];  /* 0x117A9B88 */
#pragma pop_macro("g_aBrPeer71")
#pragma push_macro("g_aBr178FEF8")
#undef g_aBr178FEF8
extern BrPeerRec g_aBr178FEF8[16][16];  /* 0x117B3258 */
#pragma pop_macro("g_aBr178FEF8")
#pragma push_macro("g_aBrPeerRank")
#undef g_aBrPeerRank
extern BrPeerRankEnt g_aBrPeerRank[16];  /* 0x11849EB0 */
#pragma pop_macro("g_aBrPeerRank")
#pragma push_macro("g_brInJoy")
#undef g_brInJoy
extern BrInJoy g_brInJoy[];  /* 0x118EEBF8 */
#pragma pop_macro("g_brInJoy")
#ifdef __cplusplus
}
#endif
/* BR_GLOBALS_END */
#endif
