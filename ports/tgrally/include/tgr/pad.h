/* pad.h -- the per-controller pad record (0x15C bytes), four of them at
 * 0x8036A8E0.
 */
#ifndef TGR_PAD_H
#define TGR_PAD_H

typedef struct BrPadRec {
    unsigned int pressed;       /* 0x00  buttons that went down this frame */
    unsigned int held;          /* 0x04  buttons already acted on */
    int repeat[4];              /* 0x08  stick auto-repeat timers */
    float axis[3];              /* 0x18  stick x and y, and the steering */
    signed char steer;          /* 0x24  the steering, -128..127 */
    union {                     /* 0x25  the control layout, 0..4 (from the season, or the ghost's header) */
        unsigned char x25;
        unsigned char layout;
    };
    char pad26[2];
    int absent;                 /* 0x28  non-zero while no controller answers on this port */
    unsigned char *rec[2];      /* 0x2C  the lap being recorded, per player */
    int recLen[2];              /* 0x34  bytes recorded so far */
    union {                     /* 0x3C  recLen kept when a replay starts; room for */
        int recKeep[2];
        int recMax[2];
    };
    unsigned char *ghost;       /* 0x44  the recording being played back, 0 for none */
    int ghostPos;               /* 0x48  bytes played */
    int ghostLen;               /* 0x4C  bytes in it */
    unsigned short hist[128];   /* 0x50  ring of button changes (the cheats) */
    unsigned int histPos;       /* 0x150 its newest entry */
    int index;                  /* 0x154 */
    TgrPad *cont;               /* 0x158 its controller data */
} BrPadRec;

extern BrPadRec D_8036A8E0[4];

#endif
