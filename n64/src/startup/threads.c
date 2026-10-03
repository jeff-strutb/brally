/* threads.c -- the event threads: RSP done, RDP done and vertical retrace,
 * each forwarding its event to the queue the game waits on.
 *
 * TU POSITION: IDO pads an infinite loop's dead epilogue to 32 bytes from
 * the start of the object's .text, so the nops after each loop depend on
 * where the function sits in its original file.  The display-list patchers
 * 0x8021B72C and 0x8021B97C sit here (this object's .data has 0x8028ADE8,
 * their flag, directly before the retrace counter), but the object does not
 * start with them: objects are 16-aligned and 0x8021BBDC's epilogue needs a
 * start at 16 mod 32.  The nearest candidates are 0x80219DF0 and 0x80219470
 * (viewport, frame begin/end, pad reading and camera functions interleave
 * with these up to here), so 0x8021BBDC stays T2 until that run is one file.
 */
#include "tgr/common.h"
#include "tgr/car.h"

typedef struct BrFlagObj {      /* a track object (0x54 bytes) */
  char pad00[0x44];
  unsigned int *dl;             /* 0x44  its display list */
  char pad48[0x4C - 0x48];
  unsigned short flags;         /* 0x4C  4: keeps the colour override; 8: set when patched */
  char pad4e[0x54 - 0x4E];
} BrFlagObj;
typedef struct BrFlagHdr {      /* the loaded track's header */
  char pad00[0x60];
  BrFlagObj *objs;              /* 0x60 */
  int nObjs;                    /* 0x64 */
} BrFlagHdr;
extern BrFlagHdr D_80025C00;

/* -- declarations -- */
void osCreateMesgQueue(int *mq, int *msgs, int count);
void osSetEventMesg(int event, int *mq, int msg);
void osViSetEvent(int *mq, int msg, int retraceCount);
int osRecvMesg(int *mq, int *msg, int flag);
int osSendMesg(int *mq, int msg, int flag);
void BrPerfMark(int bar, int r, int g, int b, int a);
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
void osCreateThread(void *t, int id, void (*entry)(void *), void *arg, void *sp, int pri);
void osStartThread(void *t);
int osContInit(int *mq, unsigned char *bitpattern, BrContStatus *status);
void func_802607AC(void);
int func_80265CD0(int *mq, BrPfs *pfs, int channel);
int func_80262370(int *mq, BrPfs *pfs, int channel);
void func_80261F20(BrPfs *pfs);
extern void *D_8028A848;
extern char D_80272D68[];
extern void *D_8031AA28[2];
extern unsigned short D_801B5000[];
extern unsigned short D_801DA800[];
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
void func_80208570(void);
void func_802534DC(void);
int BrPakCheckFiles(void);
int osPfsIsPlug(int *mq, unsigned char *pattern);
int osPfsInitPak(int *mq, BrPfs *pfs, int channel);
void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
void BrPadConsume(void *pad, int button);
extern short D_802A4BE8;
extern void (*D_8031B318)(void);
extern char D_80270840;
extern int D_802724F0;
extern char D_803163E0[];
extern char D_80316400[];
extern unsigned char D_80316420;
extern unsigned char D_80316421;
extern unsigned int D_8036A8E0;          /* pad 1's pressed buttons (the head of its record) */
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
/* -- end declarations -- */

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
      switch ((unsigned char)(dl[0] >> 24)) {
      case 0xB8:
        goto done;
      case 0xB9:
        for (i = 0; i < 6; i++) {
          if (dl[0] == table[i][0][0] && dl[1] == table[i][0][1]) {
            dl[0] = table[i][kind][0];
            dl[1] = table[i][kind][1];
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
            if (dl[0] == D_8028AB88[i].w0 && dl[1] == D_8028AB88[i].w1) {
              dl[0] = D_8028AB88[i].to0;
              dl[1] = D_8028AB88[i].to1;
              break;
            }
          }
        }
        if (D_8028AA84 != 0 && D_8028ADE8 != 0) {
          for (i = 0; i < 2; i++) {
            if (dl[0] == D_8028AB98[i][0] && dl[1] == D_8028AB98[i][1]) {
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
          dl[1] = 0x60789000;
        }
        break;
      case 0xFB:
        if (tint && D_8028AA84 != 0) {
          dl[1] = 0x8C9CA800;
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

  flag = D_8028B940 != 2 && D_8028B940 != 7;
  for (i = 0; i < D_80025C00.nObjs; i++) {
    if ((D_80025C00.objs[i].flags & 4) == 0) {
      D_8028ADE8 = flag;
    }
    if (BrDlRaceKindPatch(D_80025C00.objs[i].dl, D_8028ABA8)) {
      D_80025C00.objs[i].flags |= 8;
    }
  }
  for (n = 0; n < D_8028B7F4; n++) {
    for (j = 0; j < 3; j++) {
      if (D_8031B760[n].colour[3] == 2) {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl[j][i], D_8028AC68);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl2[j][i], D_8028AD28);
        }
      } else {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl[j][i], D_8028ABA8);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl2[j][i], D_8028ABA8);
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
  static int retraceCount = 0;    /* 0x8028ADEC */

  osCreateMesgQueue(D_8031A3B0, D_8031A3C8, 1);
  osViSetEvent(D_8031A3B0, D_8031A3CC, 1);
  for (;;) {
    osRecvMesg(D_8031A3B0, 0, 1);
    osSendMesg(D_8031A390, D_8031A3CC, 1);
    retraceCount = (retraceCount + 1) & 0xf;
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
  D_8031AA28[0] = D_801B5000;
  D_8031AA28[1] = D_801DA800;
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
  func_802607AC();
  for (i = 0; i < 4; i++) {
    D_8031B1E8[i] = 0;
    if ((bits >> i & 1) && !(D_8031A3D0[i].errno & 8) && (D_8031A3D0[i].type & 4) &&
        (D_8031A3D0[i].status & 1)) {
      r = func_80265CD0(D_80272D48, &D_8031A3F8[i], i);
      if (r != 0 && (r == 10 || r == 11) && func_80262370(D_80272D48, &D_8031A3F8[i], i) == 0) {
        D_8031B1E8[i] = 1;
        func_80261F20(&D_8031A3F8[i]);
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
 * RESIDUE (47): ours computes pad 1's address once for the B test and the
 * consume call; the ROM loads the word through lui/lw and builds the
 * address again in the branch. */
/* @t4-pass 0x8021C188 1 2026-10-03 compiles 119 best 47 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021C188 2 2026-10-03 compiles 118 best 47 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021C188 */
/* @implements 0x8021C188 tgr BrBootCheck */
void BrBootCheck(void)
{
  static unsigned char pakInit = 0;   /* 0x8028ADF0 */
  unsigned char plugged;

  BrPadPollAll();
  D_802A4BE8 = 0;
  if (D_8036A908 != 0) {
    BrModeSet(func_80208570);
    D_80270840 = 0;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  }
  osPfsIsPlug(D_80272D48, &plugged);
  if (!(plugged & 1)) {
    BrModeSet(func_80208570);
    D_80270840 = 1;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  } else {
    if (pakInit == 0) {
      pakInit = 1;
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[0], 0);
    }
    if (D_802724F0 != 0) {
      if (D_802724F0 == 10) {
        func_80262370(D_80272D48, &D_8031A3F8[0], 0);
      }
      BrModeSet(func_80208570);
      D_80270840 = 1;
      while (D_8031B318 == func_80208570) {
        func_80208570();
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
        func_80262370(D_80272D48, &D_8031A3F8[1], 1);
      }
    } else {
      memcpy(D_80316400, D_80369EC0[1].raw + 0xc, 0x20);
      D_80316421 = 1;
    }
  }
  if (D_8036A8E0 & 0x4000) {
    BrPadConsume(&D_8036A8E0, 0x4000);
    BrModeSet(func_802534DC);
    while (D_8031B318 == func_802534DC) {
      func_802534DC();
    }
  }
  if (BrPakCheckFiles() == 0) {
    BrModeSet(func_80208570);
    D_80270840 = 2;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  }
  D_802A4BE8 = 1;
}
