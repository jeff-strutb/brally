/* WHAT IT DOES: walks one viewport's route-marker list.  Each marker is
 * projected by the view matrix; markers outside a w-depth gate, or whose
 * rescaled screen depth falls outside the visible band, or whose rescaled
 * x/y lands outside the marker's own half-size, are skipped.  A marker that
 * survives every gate has its on-screen extent measured (skipped if it rounds
 * to less than a pixel either way) and is emitted as a small textured
 * billboard: a pipe-sync, the caller's colour with the marker's own alpha
 * byte, the combine mode, the projected 1/w and depth, then a rectangle
 * centred on the marker's screen position (in quarter pixels, offset by the
 * viewport's centre) and clamped at zero. */
/* @implements 0x10011300 glide BrPaceNoteEmit_10011300
 * @cpp_symbol _BrPaceNoteEmit_10011300
 *
 * Hand-transcribed from the Glide bytes.  A plain cdecl free function, but
 * a C++ translation unit: every list read is `[list + n*32 + disp]` with the
 * freshly loaded list pointer as the SIB BASE and the loop-carried n*32 as
 * the index (`add eax, ebp` before the transform call).  The C front end
 * puts the offset first for every spelling tried (DAT[n], n[DAT], char-pointer or int
 * arithmetic, a named offset, an int list) and costs a byte on
 * `lea ecx, [ebp + eax]`; the C++ front end gives the original's order.
 * /Gi is inert on it.  The function is alone in its TU (config/tu_map.csv
 * tu_009), so nothing else moves lanes.
 *
 * Shapes the bytes fix:
 *  - the viewport rect is read x, y, w, h; the centre (x*2 + w)*2 and
 *    (y*2 + h)*2 are computed before the two (float)(w*2) / (float)(h*2)
 *    half-extents (that order is what lets VC5 double w and h in place).
 *  - the list is re-read through the global at every use (the display-list
 *    stores may alias it), so it is DAT[n].field, never a cached pointer.
 *  - the scaled z and x/y are written back into pt[] (the original stores
 *    them over the transform's output) and pt[2] is emitted by its bits.
 *  - the extent is (int)(t * aspect) and (int)t off one float t kept on
 *    the x87 stack; the first failing test pops it (`fstp st(0)`).
 *  - the alpha byte is `(unsigned)(a0 * a1) >> 8 & 0xff` -- VC5's
 *    `xor ebx,ebx; mov bl,dh`.  A (unsigned char) cast gives sar+and.
 *
 * Two tie-breaks, neither a spelling:
 *  - the SIB order above only comes out once the TU declares enough before
 *    the function (dummy-prototype sweep: 0-96 no, 108 and up yes);
 *    <stdio.h> alone lands in the window and gives every byte.  math.h,
 *    stdlib.h, windows.h or combinations of them land elsewhere (2-31 B
 *    of operand roles); no header gives the original's call site at all.
 *  - halfW and halfH are declared AFTER pt[]: that makes them the loaded
 *    operand (`fld halfW; fmul scale`, `fld halfW; fmul pt[0]`,
 *    `fld halfH; fmul pt[1]`).  Declared before it, all three products
 *    load the other factor first.  Writing the products the other way
 *    round is inert.
 *
 * BYTE-EXACT 2026-09-25 under /O2 /GX /MD.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif

#include <stdio.h>     /* the preamble size is a codegen input: see the header */

typedef unsigned int   uint32_t_;
typedef unsigned short uint16_t_;

typedef struct BrPaceNote {
    float         pos[4];     /* +0x00 fed to BrMat4TransformPoint4      */
    float         unk10[2];   /* +0x10                                   */
    float         size;       /* +0x18 world-space marker size           */
    uint16_t_     next;       /* +0x1c next record index, 0 ends         */
    unsigned char a0;         /* +0x1e alpha factors: the colour word's  */
    unsigned char a1;         /* +0x1f low byte is (a0 * a1) >> 8        */
} BrPaceNote;

extern "C" {
extern int         DAT_106ec798;  /* viewport rect index                */
extern BrPaceNote *DAT_10396f04;  /* pace-note list, 0x20-byte records  */
extern float       DAT_10396eb8;  /* the view/proj matrix, 16 floats    */
extern uint32_t_  *DAT_106e7710;  /* display-list write cursor          */

extern float _DAT_1007725c, _DAT_10077260;   /* w near/far gate         */
extern float _DAT_10077264, _DAT_10077268;   /* depth band (1.0, lower) */
extern float _DAT_1007726c, _DAT_10077270;   /* half-size scale/bias    */
extern float _DAT_10077274;                  /* x aspect for the extent */

void BrMat4TransformPoint4(float pOut[4], const float *pV, const float *pM); /* 0x10034920 */
}

#define EMIT(W0, W1) \
    { uint32_t_ *p_ = DAT_106e7710; DAT_106e7710 += 2; \
      p_[0] = (uint32_t_)(W0); p_[1] = (uint32_t_)(W1); }

extern "C"
void BrPaceNoteEmit_10011300(int pViewport, uint32_t_ n, int r, uint32_t_ g, uint32_t_ b)
{
    int  *pRect;
    int   x, y, w, h;
    int   cx, cy;
    float pt[4];        /* [0]=x [1]=y [2]=z [3]=w, BrMat4TransformPoint4's order */
    float invW, scale, half, t;
    int   ex, ey, ix, iy;
    short s;
    unsigned u0, u1;
    float halfW, halfH; /* declared after pt[]: see the header */

    pRect = (int *)(pViewport + DAT_106ec798 * 0x58);
    x = pRect[0]; y = pRect[1]; w = pRect[2]; h = pRect[3];
    cx    = (x * 2 + w) * 2;
    cy    = (y * 2 + h) * 2;
    halfW = (float)(w * 2);
    halfH = (float)(h * 2);
    while (n != 0) {
        BrMat4TransformPoint4(pt, DAT_10396f04[n].pos, &DAT_10396eb8);
        if (pt[3] > _DAT_1007725c || pt[3] < _DAT_10077260) {
            invW  = _DAT_10077264 / pt[3];
            pt[2] = invW * pt[2];
            if (pt[2] < _DAT_10077264 && pt[2] > _DAT_10077268) {
                pt[0] = invW * pt[0];
                scale = DAT_10396f04[n].size * invW * _DAT_1007726c;
                half  = scale - _DAT_10077270;
                if (pt[0] > -half && pt[0] < half) {
                    pt[1] = invW * pt[1];
                    if (pt[1] > -half && pt[1] < half) {
                        t  = halfW * scale;
                        ex = (int)(t * _DAT_10077274);
                        if (ex >= 1) {
                            ey = (int)t;
                            if (ey >= 1) {
                                ix = (int)(halfW * pt[0]) + cx;
                                iy = (int)(halfH * pt[1]) + cy;

                                EMIT(0xe7000000, 0);
                                EMIT(0xfa00ffff,
                                     (((r << 8) | (g & 0xff)) << 8 | (b & 0xff)) << 8 |
                                     (unsigned)(DAT_10396f04[n].a0 * DAT_10396f04[n].a1) >> 8 & 0xff);
                                EMIT(0xb6000000, 0x3000);
                                EMIT(0xde000000, *(uint32_t_ *)&invW);
                                EMIT(0xdf000000, *(uint32_t_ *)&pt[2]);
                                {
                                    uint32_t_ *p_ = DAT_106e7710;
                                    DAT_106e7710 += 2;
                                    s  = (short)((ix + ex) >> 2);
                                    u0 = (s > 0) ? s : 0;
                                    s  = (short)((iy + ey) >> 2);
                                    u1 = (s > 0) ? s : 0;
                                    p_[0] = ((u0 & 0xfff) | 0xfffe3000u) << 0xc | (u1 & 0xfff);
                                    s  = (short)((ix - ex) >> 2);
                                    u0 = (s > 0) ? s : 0;
                                    s  = (short)((iy - ey) >> 2);
                                    u1 = (s > 0) ? s : 0;
                                    p_[1] = (u0 & 0xfff) << 0xc | (u1 & 0xfff);
                                }
                            }
                        }
                    }
                }
            }
        }
        n = DAT_10396f04[n].next;
    }
}

