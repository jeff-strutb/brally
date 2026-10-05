/* pad.h -- the per-controller pad record (0x15C bytes), four of them at
 * 0x8036A8E0.
 */
#ifndef TGR_PAD_H
#define TGR_PAD_H

typedef struct BrPadRec {
    unsigned int pressed;       /* 0x00  buttons that went down this frame */
    unsigned int held;          /* 0x04  buttons already acted on */
    int repeat[4];              /* 0x08  stick auto-repeat timers */
    float axis[2];              /* 0x18  stick axes */
    char pad20[5];
    unsigned char x25;          /* 0x25  from the season (or the ghost's header) */
    char pad26[2];
    int absent;                 /* 0x28  non-zero while no controller answers on this port */
    TgrAddr rec[2];      /* unsigned char * -- 0x2C  the lap being recorded, per player */
    int recLen[2];              /* 0x34  bytes recorded so far */
    int recKeep[2];             /* 0x3C  recLen kept when a replay starts */
    TgrAddr ghost;       /* unsigned char * -- 0x44  the recording being played back, 0 for none */
    int ghostPos;               /* 0x48  bytes played */
    int ghostLen;               /* 0x4C  bytes in it */
    unsigned short hist[128];   /* 0x50  ring of button changes (the cheats) */
    unsigned int histPos;       /* 0x150 its newest entry */
    int index;                  /* 0x154 */
    TgrAddr cont;                 /* void * -- 0x158  its OSContPad */
} BrPadRec;

extern BrPadRec D_8036A8E0[4];

#endif
