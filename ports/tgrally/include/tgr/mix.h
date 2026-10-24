/* mix.h -- the software mixer's voices (0x18 bytes each on the N64): up to
 * sixteen for the music at 0x802A4798, six for the effects at 0x802A4920;
 * the effect voices' samples, and the car sounds they play.  One definition
 * for every part of the game; where the parts named a field differently, the
 * names share it.
 */
#ifndef TGR_MIX_H
#define TGR_MIX_H

/* A voice's position is its current 8-bit sample and a 32-bit fraction past
 * it: on the N64 one 32.32 accumulator whose high word was the address. */
typedef struct BrMixVoice {
    union {                     /* 0x00  the current sample */
        signed char *pos;
        signed char *p;
    };
    union {                     /* 0x04  the fraction past it */
        unsigned int frac;
        int x4;
    };
    union {                     /* 0x08  32.32 samples per output sample, 0 = silent */
        unsigned long long rate;
        unsigned long long pitch;
        long long x8;
    };
    union {                     /* 0x10  scaled volume */
        int vol;
        int x10;
    };
    union {                     /* 0x14  left << 16 | right */
        int baseVol;
        unsigned int level;
        int x14;
    };
} BrMixVoice;

/* An effect voice's sample (0x0C bytes on the N64). */
typedef struct BrSampleLoop {
    signed char *x0;            /* 0x00  its start */
    unsigned int x4;            /* 0x04  its length */
    unsigned int loop;          /* 0x08  0 = the sample does not loop */
} BrSampleLoop;

/* A car sound sample (0x18 bytes on the N64). */
typedef struct BrSfxSrc {
    union {                     /* 0x00  where it was unpacked */
        unsigned char *data;
        unsigned char *start;
    };
    int rom;                    /* 0x04  its packed file */
    int x8;
    union {                     /* 0x0C  unpacked size */
        unsigned int size;
        unsigned int len;
    };
    unsigned int loop;          /* 0x10  loop start (-1 in the table: the end) */
    union {                     /* 0x14  bytes copied past the end for the loop (-1: all) */
        unsigned int loopLen;
        int x14;
    };
} BrSfxSrc;

extern BrMixVoice D_802A4798[16];   /* the music voices (as many as the module has channels) */
extern BrMixVoice D_802A4920[6];    /* the effect voices */
extern BrSampleLoop D_80378F50[6];
extern BrSfxSrc D_8028BC04[16];

void BrSfxVoiceStart(short v, void *start, unsigned int len, unsigned int loop);
void BrSfxVoicePlay(short v, void *start, unsigned int len, unsigned int loop);

#endif
