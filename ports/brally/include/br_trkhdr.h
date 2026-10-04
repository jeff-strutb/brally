/* br_trkhdr.h: the loaded track header, Glide 0x106EECD8, 0x230 bytes.
 *
 * BrTrackLoad (0x100311C0) reads the first 0x230 bytes of tracks/<name>.trk
 * straight into this resident buffer, BrGlTrackHdrRead (0x10031B80)
 * byte-swaps it and rebases its nineteen address fields, and the rest of the
 * file lands in the track image at file offset 0x230 onwards. The original
 * has no per-field store anywhere else: every global between 0x106EECD8 and
 * 0x106EEF07 is a field of this record.
 *
 * It is a file format, so it keeps the file's 32-bit fields. The address
 * fields hold br_addr32 values once BrSegPtrFixup has run (the a* fields
 * below); every read of one goes through br_ptr32. The gate ring and the
 * specials list are part of it, by value. */
#ifndef BR_TRKHDR_H
#define BR_TRKHDR_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

#include "br_vec.h"
#include "br_addr32.h"

/* +0x98, stride 0x14 -- the gate ring BrRaceGateStep (0x1005FF00) walks. The
 * two posts are handed to BrSeg2Intersect, which reads only offsets 0 and 4
 * of each of its four arguments -- so the gate is genuinely a 2-D segment
 * and the driver's Z is never consulted.
 *
 * `tAward` is read once, to be printed ("moved ahead one gate, getting %f
 * seconds"). Nothing in 0x1005FF00 applies it. */
typedef struct BrRaceGate {
    BrVec2 postA;     /* +0x00 */
    BrVec2 postB;     /* +0x08 */
    float  tAward;    /* +0x10 */
} BrRaceGate;

/* +0x164, stride 0xC -- the track's "specials" list, walked at 0x1001A31A.
 * The kind byte at +0x08 is read with `movsx`, has 3 subtracted from it and
 * indexes the five-entry table at 0x1001C664; anything outside 3..7 is
 * skipped by the `ja` at 0x1001A324. */
typedef struct BrRaceSpecial {
    union {                                  /* +0x0000 */
        int32_t f00;  /* the value every arm stores ... */
        int32_t iObj;  /* ... read as an index into the 0x54-byte records */
    };
    union {                                  /* +0x0004 */
        int32_t f04;
        float angle;  /* degrees */
    };
    union {                                  /* +0x0008 */
        int32_t kind;  /* read as a signed byte */
        int8_t axis;
    };
} BrRaceSpecial;

#define BR_TRK_GATE_MAX     10
#define BR_TRK_SPECIAL_MAX  16

typedef struct BrTrkHdr {
    uint32_t heapOff;        /* +0x000  file offset of the heap; -> g_brRcaBlob   */
    uint32_t cbHeader;       /* +0x004  must be 0x230                             */
    int32_t  cFaces;         /* +0x008  set to the largest grid item + 1          */
    uint32_t aFaces;         /* +0x00C  u16[4] per face                           */
    int32_t  cVertices;      /* +0x010                                            */
    uint32_t aVertices;      /* +0x014  BrVec3[]                                  */
    int32_t  cSections;      /* +0x018                                            */
    uint32_t aSections;      /* +0x01C  0x24-byte texture records                 */
    uint32_t aGridItems;     /* +0x020  u16[]                                     */
    uint32_t aGridStart;     /* +0x024  u16[0x1001]                               */
    float    f028, f02C;     /* +0x028  a span the car locator squares            */
    float    f030, f034;     /* +0x030                                            */
    float    fZMin, fZMax;   /* +0x038  height range: fog ramp, respawn floor     */
    BrVec3   startPos;       /* +0x040  the grid's first slot                     */
    float    startYaw;       /* +0x04C                                            */
    uint32_t aDl50;          /* +0x050  display list emitted as segment 6         */
    uint32_t a54, a58;       /* +0x054                                            */
    uint32_t aNames;         /* +0x05C  table of addresses of names               */
    uint32_t aInstances;     /* +0x060  0x54-byte instance records                */
    int32_t  cInstances;     /* +0x064  <= 0x800                                  */
    uint32_t aQueue;         /* +0x068  u16[]                                     */
    uint32_t aGrid16;        /* +0x06C  u16[0x1001]                               */
    uint32_t aPathRoot;      /* +0x070  BrAiPathNode, the path ring root          */
    uint32_t aPath2;         /* +0x074  second entry into the ring                */
    uint32_t aSegList;       /* +0x078  table of addresses of path nodes          */
    int32_t  cSegList;       /* +0x07C                                            */
    uint8_t  rgba[4];        /* +0x080  not byte-swapped                          */
    uint32_t aPayload;       /* +0x084  BrVec3[], always file offset 0x230        */
    int32_t  cPayload;       /* +0x088                                            */
    uint32_t aU16List8C;     /* +0x08C  u16[], zero-terminated                    */
    uint32_t aU16List90;     /* +0x090  u16[]                                     */
    uint32_t aFaceKind;      /* +0x094  u8 per face: surface bits (low three)     */
    BrRaceGate aGate[BR_TRK_GATE_MAX];          /* +0x098 */
    int32_t  nGate;                              /* +0x160 */
    BrRaceSpecial aSpecial[BR_TRK_SPECIAL_MAX]; /* +0x164 */
    int32_t  nSpecial;                           /* +0x224 */
    uint32_t f228, f22C;                         /* +0x228 */
} BrTrkHdr;

typedef char BrTrkHdrSize_[sizeof(BrTrkHdr) == 0x230 ? 1 : -1];

extern BrTrkHdr g_brTrkHdr;                 /* 0x106EECD8 */
/* The file image the header's addresses point into: file offset X is
 * g_abBrTrkImage[X]; the header's own copy at offsets 0..0x22F is unused. */
#define BR_TRK_IMAGE_MAX 4000000
extern uint8_t g_abBrTrkImage[BR_TRK_IMAGE_MAX];   /* 0x106EFF08 */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
