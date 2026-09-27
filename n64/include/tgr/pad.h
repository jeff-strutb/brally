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
    char pad20[8];
    int absent;                 /* 0x28  non-zero while no controller answers on this port */
    char pad2c[0x154 - 0x2C];
    int index;                  /* 0x154 */
    void *cont;                 /* 0x158  its OSContPad */
} BrPadRec;

extern BrPadRec D_8036A8E0[4];

#endif
