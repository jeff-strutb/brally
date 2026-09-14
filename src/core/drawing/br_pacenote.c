/* br_pacenote.c -- drawing: the route-marker (pace-note) icon emitter.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Walks the recorded-route linked list br_rec.c's BrRecHdrLatch_10010F80
 * latches into DAT_10396F04 (0x20-byte records, `next` at +0x1C), projects
 * each marker with BrMat4TransformPoint4, and for the ones that land inside
 * the near/far and screen-space bounds emits a small textured-rect display
 * list through the b_dl.c-style EMIT convention (br_objdl.c's DAT_106E7710
 * cursor).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

typedef unsigned int   uint32_t_;
typedef unsigned short uint16_t_;

extern int     DAT_106ec798;      /* viewport rect index                */
extern int     DAT_10396f04;      /* pace-note list, 0x20-byte records  */
extern float   DAT_10396eb8;      /* the view/proj matrix, 16 floats    */
extern uint32_t *DAT_106e7710;    /* display-list write cursor          */

extern float _DAT_1007725c, _DAT_10077260;   /* w near/far gate         */
extern float _DAT_10077264, _DAT_10077268;   /* re-scaled y range       */
extern float _DAT_1007726c, _DAT_10077270;   /* x half-width scale/bias */

int ftol(void);                                            /* real cdecl */
void BrMat4TransformPoint4(float pOut[4], const float *pV, const float *pM); /* 0x10034920 */

#define EMIT(W0, W1) \
    { uint32_t_ *p_ = DAT_106e7710; DAT_106e7710 += 2; \
      p_[0] = (uint32_t_)(W0); p_[1] = (uint32_t_)(W1); }

/* WHAT IT DOES: walks one viewport's route-marker list.  Each marker is
 * projected by the view matrix; markers outside a w-depth gate, or whose
 * rescaled screen y falls outside the visible band, or whose rescaled x
 * lands outside the marker's own half-width, are skipped.  A marker that
 * survives all three gates is emitted as a small textured billboard: a
 * pipe-sync, the marker's colour/alpha word, the combine mode, the screen
 * x/y (from the truncated projected position offset by the viewport's own
 * origin), and a scissor sub-rectangle built from the same x/y plus the
 * caller's two size arguments. */
/* T2, not yet byte-exact: 761/846 B (85 B short), REGNORM 30+50 -- real
 * structural gap, not just register noise: missing word-width tests on the
 * two ushort record reads (the "next" link and one byte-product read read
 * as 16-bit, not promoted), and the colour/alpha word's two-byte product
 * likely needs its own local rather than being folded into one expression.
 * Not chased further this session -- residue, not a guess dressed as fact. */
/* @implements 0x10011300 glide BrPaceNoteEmit_10011300 */
void BrPaceNoteEmit_10011300(int pViewport, uint32_t_ n, int r, uint32_t_ g, uint32_t_ b)
{
    int   vx, vy, vw, vh;
    int   base;
    int   rec;
    float pt[4];        /* [0]=x [1]=y [2]=z [3]=w, BrMat4TransformPoint4's order */
    float scaleY;

    base = pViewport + DAT_106ec798 * 0x58;
    vy   = *(int *)(base + 8);
    vx   = *(int *)(base);
    vh   = *(int *)(base + 0xc);
    vw   = *(int *)(base + 4);

    while (n != 0) {
        rec = DAT_10396f04 + (int)n * 0x20;

        BrMat4TransformPoint4(pt, (const float *)rec, &DAT_10396eb8);

        if (_DAT_1007725c < pt[3] || pt[3] < _DAT_10077260) {
            float invW = _DAT_10077264 / pt[3];

            scaleY = invW * pt[2];
            if (scaleY < _DAT_10077264 && _DAT_10077268 < scaleY) {
                float sx = invW * pt[0];
                float half = *(float *)(rec + 0x18) * invW * _DAT_1007726c - _DAT_10077270;
                float sz = invW * pt[1];

                if (-half < sx && sx < half && -half < sz && sz < half) {
                    int px = ftol();
                    int py = ftol();

                    if (px > 0 && py > 0) {
                        int   ix, iy;
                        short s0, s1, s2, s3;
                        unsigned u0, u1, u2, u3;

                        ix = ftol();
                        ix = ix + (vy + vx * 2) * 2;
                        iy = ftol();
                        iy = iy + (vh + vw * 2) * 2;

                        EMIT(0xe7000000, 0);
                        EMIT(0xfa00ffff,
                             (((r << 8) | (g & 0xff)) << 8 | (b & 0xff)) << 8 |
                             (unsigned)(*(unsigned char *)(rec + 0x1e) *
                                        *(unsigned char *)(rec + 0x1f) >> 8));
                        EMIT(0xb6000000, 0x3000);
                        EMIT(0xde000000, *(uint32_t_ *)&invW);
                        EMIT(0xdf000000, *(uint32_t_ *)&scaleY);

                        s0 = (short)((ix + px) >> 2);
                        u0 = (s0 < 1) ? 0u : (unsigned)s0;
                        s1 = (short)((iy + py) >> 2);
                        u1 = (s1 < 1) ? 0u : (unsigned)s1;
                        s2 = (short)((ix - px) >> 2);
                        u2 = (s2 < 1) ? 0u : (unsigned)s2;
                        s3 = (short)((iy - py) >> 2);
                        u3 = (s3 < 1) ? 0u : (unsigned)s3;

                        {
                            uint32_t_ *p_ = DAT_106e7710;
                            DAT_106e7710 += 2;
                            p_[0] = ((u0 & 0xfffu) | 0xfffe3000u) << 0xc | (u1 & 0xfffu);
                            p_[1] = (u2 & 0xfffu) << 0xc | (u3 & 0xfffu);
                        }
                    }
                }
            }
        }

        n = *(uint16_t_ *)(rec + 0x1c);
    }
}

#endif /* BR_MATCHING_BUILD */
