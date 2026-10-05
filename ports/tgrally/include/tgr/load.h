/* load.h -- reading the cartridge: packed files, streamed unpacks, ROM files
 * (native game state; what they load is cartridge data, see track.h,
 * model.h).
 */
#ifndef TGR_LOAD_H
#define TGR_LOAD_H

typedef struct BrUnpack {       /* a streamed unpack in flight (BrStreamInit starts one) */
    unsigned int pos;           /* 0x00  ROM position of the next chunk */
    TgrAddr dst;         /* unsigned char * -- 0x04  0 until the first call */
    TgrAddr buf;         /* unsigned char * -- 0x08  2 x 16000-byte chunk buffer */
    unsigned int half;          /* 0x0C  which half the next chunk goes to */
    unsigned int total;         /* 0x10  packed length */
    unsigned int size;          /* 0x14  unpacked length */
    unsigned int left;          /* 0x18  packed bytes still to inflate */
    unsigned int len;           /* 0x1C  length of the chunk in flight */
} BrUnpack;

typedef struct BrStream {       /* a streamed car-model load in flight (0x28 bytes) */
    BrUnpack u;                 /* 0x00 */
    int slot;                   /* 0x20  model slot it fills */
    int car;                    /* 0x24  car it loads */
} BrStream;

typedef struct BrRomFile {      /* a ROM file and where it was loaded */
    int start;                  /* 0x00  ROM range */
    int end;                    /* 0x04 */
    TgrAddr data;                 /* void * -- 0x08  where it was loaded */
} BrRomFile;

void BrRomRead(void *dst, unsigned int rom, int len);
int BrRomReadSize(int rom);
int BrRomReadWord(int rom);
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, BrUnpack *s);
void BrStreamInit(BrUnpack *s, unsigned char *buf);
OSIoMesg *BrRomDmaSlot(void);
void BrRomWaitAll(void);
void BrRomFileLoad(BrRomFile *f, unsigned int (*alloc)(int size));
void BrRomFileUnpack(BrRomFile *f, unsigned int (*alloc)(int size));
#endif
