/* model.h -- models and their morph animations: cartridge data, unpacked
 * into a buffer and kept big-endian as loaded (tgr_core.h).  Their stored
 * offsets are rebased in place to original addresses (BrModelRebase), read
 * through BEPTR.
 *
 * One definition for the views the decomp's files declared: BrModel with
 * BrModelDl (romread.c, boot.c) and with BrModelPart (modeldraw.c) is the
 * one header; BrModelParts/BrModelPart (boot.c's rebase) are the animation
 * list and BrAnim.
 */
#ifndef TGR_MODEL_H
#define TGR_MODEL_H

/* one morph animation of a model part */
typedef struct BrAnim {
    be32_t n;                   /* 0x00  vertices */
    be32_t out;                 /* 0x04  the part's vertices (Vtx, 16 bytes each) */
    be32_t x8;                  /* 0x08  rebased too */
    be32_t nKeys;               /* 0x0C */
    be16_t flags;               /* 0x10  bit 0 repeat, bit 1 bounce, bit 2 running backwards */
    be16_t key;                 /* 0x12  the key the search starts from */
    bef_t start;                /* 0x14 */
    bef_t end;                  /* 0x18 */
    bef_t time;                 /* 0x1C */
    be32_t keys[1];             /* 0x20  each: its time, n xyz shorts, n rgb bytes */
} BrAnim;

typedef struct BrAnimList {
    be32_t n;
    be32_t anim[1];             /* BrAnim, n of them */
} BrAnimList;

/* a model part: its display list and where it sits (0x14 bytes) */
typedef struct BrModelDl {
    be32_t dl;                  /* 0x00  its display list, 0 for none */
    be16_t flags;               /* 0x04  8: drawn in the second (blended) pass,
                                 *       0x400: lit in the colour flags & 3 picks,
                                 *       4: cull front faces, 0x80: not fogged */
    be16_t pad06;
    bef_t pos[3];               /* 0x08  where it sits in the model */
} BrModelDl;

/* a model: its parts after a count, and its animations */
typedef struct BrModel {
    be32_t nDl;                 /* 0x00 */
    be32_t anims;               /* 0x04  BrAnimList, 0 for none */
    BrModelDl dls[1];           /* 0x08  nDl of them */
} BrModel;

/* the animation set BrAnimUpdate and friends take is the model itself */
typedef BrModel BrAnimSet;
#endif
