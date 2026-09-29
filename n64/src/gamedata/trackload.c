/* trackload.c -- loading a track: its data, its two headed blocks and its
 * objects' scales
 */
#include "tgr/common.h"
#include "tgr/menu.h"

/* -- declarations -- */
int BrRomReadSize(int rom);
int BrRomReadWord(int rom);
unsigned int BrRomUnpack();
typedef struct BrLoadObj {      /* a track object (0x54 bytes) */
  float m[4][4];                /* its matrix */
  float scale;                  /* 0x40  1 / the length of its x axis */
  char pad44[0x4C - 0x44];
  unsigned short flags;         /* 0x4C  0x2000: unscaled */
  char pad4e[0x54 - 0x4E];
} BrLoadObj;
typedef struct BrLoadHdr {      /* the loaded track (packed size, then the header) */
  unsigned int size;
  int hdrSize;                  /* 0x04  0x230 */
  char pad08[0x60 - 0x08];
  BrLoadObj *objs;              /* 0x60 */
  int nObjs;                    /* 0x64 */
} BrLoadHdr;
extern BrLoadHdr D_80025C00;
extern char D_80373EB0[];
extern char D_803736B0[];
extern char D_803746D0[];
extern char D_80373ED0[];
extern int D_8028A87C;
extern int D_8028A880;
extern int D_8028AA7C;
extern int D_8028AA88;
void osInvalDCache(void *p, int n);
void BrFatal(char *msg);
int sprintf(char *buf, const char *fmt, ...);
void BrMat4RotateDir(float *out, float *v, float (*m)[4]);
float BrVec3Length(float *v);
void BrScenePassRun(void);
void BrRomRead();
void osSyncPrintf(char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: Load a track: read its packed size (fatal if over 0x171400),
 * unpack it to 0x80025C00, load the two 0x20-byte headed blocks after it,
 * note its ROM offset and texture base, reset the texture state, then give
 * every object its scale (1 / the length of its rotated x axis), flagging
 * the ones whose matrix is unscaled; fatal on too many objects or the wrong
 * header size.  Runs the scene setup pass. */
/* @implements 0x8021DE5C tgr BrTrackLoad */
void BrTrackLoad(int track)
{
  char buf[80];
  float v[3];
  int n;
  float len;
  char buf2[84];
  char buf3[80];

  D_80025C00.size = BrRomReadSize(D_80270854[track].x14);
  osInvalDCache(&D_80025C00, 4);
  if (D_80025C00.size > 0x171400) {
    sprintf(buf, "Track %d too big (%d vs. %d)", track, D_80025C00.size, 0x171400);
    BrFatal(buf);
  } else {
    osSyncPrintf("Loading track %d (%d / %d)\n", track, D_80025C00.size, 0x171400);
  }
  BrRomUnpack((void *)0x80025C00, D_80270854[track].x14, 0);
  osSyncPrintf("hmm = %04x %04x\n", *(unsigned short *)0x80029560, *(unsigned short *)0x80029568);
  osInvalDCache((void *)0x80025C00, D_80025C00.size);
  BrRomRead(D_80373EB0, D_80270854[track].x1c, 0x20);
  BrRomUnpack(D_803736B0, D_80270854[track].x1c + 0x20, 0);
  BrRomRead(D_803746D0, D_80270854[track].x24, 0x20);
  BrRomUnpack(D_80373ED0, D_80270854[track].x24 + 0x20, 0);
  D_8028A87C = D_80270854[track].x14;
  D_8028A880 = D_80270854[track].x14 + BrRomReadWord(D_80270854[track].x14);
  D_8028AA7C = -1;
  D_8028AA88 = -1;
  n = 0;
  for (track = 0; track < D_80025C00.nObjs; track++) {
    v[0] = 1.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    BrMat4RotateDir(v, v, D_80025C00.objs[track].m);
    len = BrVec3Length(v);
    if (len != 0) {
      len = 1.0f / len;
      if (1.0f == D_80025C00.objs[track].m[0][0] * len && 1.0f == D_80025C00.objs[track].m[1][1] * len &&
          1.0f == D_80025C00.objs[track].m[2][2] * len) {
        n++;
        D_80025C00.objs[track].flags |= 0x2000;
      }
      D_80025C00.objs[track].scale = len;
    }
  }
  osSyncPrintf("Scalars: %d/%d\n", n, D_80025C00.nObjs);
  if (D_80025C00.nObjs > 0x800) {
    sprintf(buf2, "ERROR: instances (%d) > MAX_INSTANCES (%d)", D_80025C00.nObjs, 0x800);
    BrFatal(buf2);
  }
  if (D_80025C00.hdrSize != 0x230) {
    sprintf(buf3, "ERROR: Track header size mismatch! (%d!=%d)", D_80025C00.hdrSize, 0x230);
    BrFatal(buf3);
  }
  BrScenePassRun();
}
