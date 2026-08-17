/* br_ai.c -- opponent AI: racing line, lookahead, aim error.
 *
 * See br_ai.h for where the data lives, which original functions each piece
 * comes from, and what is deliberately not ported.
 *
 * Everything is decoded byte-wise. The path lives in the .TRK payload, which
 * is BIG-ENDIAN N64 data that br_track.c leaves untouched (its element
 * swapper 0x10018B60 is not ported), so a struct overlay would be wrong in
 * the one way that still compiles.
 */
#include "br_ai.h"

#include <string.h>

/* --- byte-wise primitives ------------------------------------------------ */

static uint32_t br_ai_u32be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint16_t br_ai_u16be(const unsigned char *p)
{
    return (uint16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

static float br_ai_f32be(const unsigned char *p)
{
    uint32_t u = br_ai_u32be(p);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static void br_ai_vec3be(BrVec3 *pOut, const unsigned char *p)
{
    pOut->x = br_ai_f32be(p + 0);
    pOut->y = br_ai_f32be(p + 4);
    pOut->z = br_ai_f32be(p + 8);
}

/* 0x100189E0's rule, as br_track.c already reproduces it for the header: zero
 * stays zero, anything below the base (SIGNED compare) becomes zero, and the
 * rest becomes an offset. The links inside the payload never pass through
 * br_track.c because its section swapper is not ported, so they are still raw
 * N64 addresses in the image and are relocated here on read. */
static uint32_t br_ai_reloc(uint32_t n64va)
{
    if (n64va == 0)
        return 0;
    if ((int32_t)n64va < (int32_t)BR_TRK_N64_BASE)
        return 0;
    return n64va - BR_TRK_N64_BASE;
}

static int br_ai_in_range(const BrTrack *pTrack, uint32_t off, uint32_t cb)
{
    if (pTrack == NULL || pTrack->pbImage == NULL)
        return 0;
    if (off == 0 || off >= pTrack->cbImage)
        return 0;
    return (pTrack->cbImage - off) >= cb;
}

/* --- nodes and points ---------------------------------------------------- */

int BrAiNodeAt(const BrTrack *pTrack, uint32_t off, BrAiNode *pOut)
{
    const unsigned char *p;
    uint32_t cbPoints;

    memset(pOut, 0, sizeof(*pOut));
    if (!br_ai_in_range(pTrack, off, BR_AI_NODE_POINTS))
        return 1;

    p = pTrack->pbImage + off;
    pOut->pTrack  = pTrack;
    pOut->off     = off;
    pOut->offNext = br_ai_reloc(br_ai_u32be(p + 0x00));
    pOut->offSib  = br_ai_reloc(br_ai_u32be(p + 0x04));
    pOut->count   = br_ai_u16be(p + 0x14);
    pOut->flags   = br_ai_u16be(p + 0x16);

    /* count + 1 records: the sentinel is what makes the original's read of
     * pts[i+1] at i == count-1 land on real data. Confirmed arithmetically --
     * consecutive nodes in a shipped track are exactly this far apart. */
    cbPoints = ((uint32_t)pOut->count + 1u) * BR_AI_POINT_STRIDE;
    if (!br_ai_in_range(pTrack, off, BR_AI_NODE_POINTS + cbPoints)) {
        memset(pOut, 0, sizeof(*pOut));
        return 1;
    }
    return 0;
}

int BrAiRoot(const BrTrack *pTrack, BrAiNode *pOut)
{
    return BrAiNodeAt(pTrack, BrTrackHdrU32(pTrack, BR_TRK_H_AIPATH), pOut);
}

int BrAiPoint_(const BrAiNode *pNode, uint32_t i, BrAiPoint *pOut)
{
    const unsigned char *p;

    if (pNode->pTrack == NULL || i > (uint32_t)pNode->count)
        return 1;
    p = pNode->pTrack->pbImage + pNode->off
      + BR_AI_NODE_POINTS + i * BR_AI_POINT_STRIDE;

    br_ai_vec3be(&pOut->left,   p + 0x00);
    br_ai_vec3be(&pOut->centre, p + 0x0C);
    br_ai_vec3be(&pOut->right,  p + 0x18);
    pOut->arc = br_ai_f32be(p + 0x24);
    return 0;
}

float BrAiLapLength(const BrTrack *pTrack)
{
    BrAiNode n;
    BrAiPoint pt;

    if (BrAiRoot(pTrack, &n) != 0)
        return 0.0f;
    if (BrAiPoint_(&n, 0, &pt) != 0)
        return 0.0f;
    return pt.arc;
}

/* --- the lookahead (0x1005D7D5) ------------------------------------------ */

float BrAiLookahead(float speed)
{
    float t = BR_AI_LOOKAHEAD_BASE + BR_AI_LOOKAHEAD_GAIN * speed;

    /* `fcom 80.0f` + `test ah,0x41` + `jne keep`: the value survives when the
     * compare reports less-or-equal OR unordered. Spelt `> ` so NaN survives
     * too; `!(t <= max)` would clamp it, which is the opposite. */
    if (t > BR_AI_LOOKAHEAD_MAX)
        t = BR_AI_LOOKAHEAD_MAX;
    return t;
}

/* --- advancing the waypoint cursor (0x1005D82E) -------------------------- */

int BrAiAdvanceTarget(BrAiNode *pNode, uint32_t *pIndex, float dist)
{
    uint32_t i = *pIndex;

    if (pNode->pTrack == NULL)
        return 1;
    if (i > (uint32_t)pNode->count)
        return 1;

    do {
        BrAiPoint a, b;

        /* pts[i] - pts[i+1]; i+1 may be `count`, which is the sentinel. */
        if (BrAiPoint_(pNode, i, &a) != 0 || BrAiPoint_(pNode, i + 1u, &b) != 0)
            return 1;
        dist -= a.arc - b.arc;

        i++;
        if (i == (uint32_t)pNode->count) {
            BrAiNode nx;
            unsigned guard;

            if (BrAiNodeAt(pNode->pTrack, pNode->offNext, &nx) != 0)
                return 1;           /* DEVIATION: the original faults here. */
            /* while (node->flags & SKIP) node = node->sibling; -- also
             * unguarded in the original. The guard bounds a ring of siblings
             * that are all flagged; a well-formed track never reaches it. */
            for (guard = 0; (nx.flags & BR_AI_NODE_SKIP) != 0; guard++) {
                if (guard > 0xFFFFu
                    || BrAiNodeAt(pNode->pTrack, nx.offSib, &nx) != 0)
                    return 1;
            }
            *pNode = nx;
            i = 0;
        }
        /* Continue while C0 is clear: not-less-than and ordered. C's `>=` is
         * false for NaN, which is the same exit the original takes. */
    } while (dist >= 0.0f);

    *pIndex = i;
    return 0;
}

/* --- the corridor (0x1005D0D7 / 0x1005D111) ------------------------------ */

void BrAiCorridor(BrVec3 *pNearLeft, BrVec3 *pNearRight,
                  const BrAiPoint *pPt, float t)
{
    /* Argument order preserved from the two call sites. BrVec3Lerp is
     * (a - b) * t + b, so these are genuinely different functions of t. */
    if (pNearRight != NULL)
        BrVec3Lerp(pNearRight, &pPt->left, &pPt->right, t);   /* 0x1005D0D7 */
    if (pNearLeft != NULL)
        BrVec3Lerp(pNearLeft, &pPt->right, &pPt->left, t);    /* 0x1005D111 */
}

/* --- aiming (0x10034420, 0x10034310) ------------------------------------- */

void BrAiAimDir(BrVec3 *pOut, const BrVec3 *pPos, const BrVec3 *pAim)
{
    BrVec3 d;
    float len;

    d.x = pAim->x - pPos->x;
    d.y = pAim->y - pPos->y;
    d.z = pAim->z - pPos->z;

    /* 0x10034420 sums the squares, takes the square root through the same
     * wrapper BrVec3Length uses, then tests it against 0.0f with `test ah,0x40`
     * -- C3 alone, so an unordered result also takes the degenerate path. */
    len = BrVec3Length(&d);
    if (!(len < 0.0f || len > 0.0f)) {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
        pOut->z = 1.0f;                         /* 0x100344BF */
        return;
    }
    /* The original divides 1.0f by the length once and multiplies. */
    len = 1.0f / len;
    pOut->x = d.x * len;
    pOut->y = d.y * len;
    pOut->z = d.z * len;
}

void BrAiAimError(const BrMat4 *pFrame, const BrVec3 *pDir,
                  float *pFwd, float *pLat)
{
    BrVec3 row;

    /* Rows are copied out rather than aliased: a BrMat4 row is float[4] and
     * a BrVec3 is three floats, so pointing one at the other is not a legal
     * access even where it happens to work. */
    if (pLat != NULL) {                         /* 0x1005D9C0, car+0x10 */
        row.x = pFrame->m[1][0];
        row.y = pFrame->m[1][1];
        row.z = pFrame->m[1][2];
        *pLat = BrVec3Dot(&row, pDir);
    }
    if (pFwd != NULL) {                         /* 0x1005D9D2, car+0x00 */
        row.x = pFrame->m[0][0];
        row.y = pFrame->m[0][1];
        row.z = pFrame->m[0][2];
        *pFwd = BrVec3Dot(&row, pDir);
    }
}

float BrAiSteerDirection(const BrMat4 *pFrame, const BrVec3 *pDir)
{
    float lat = 0.0f;

    BrAiAimError(pFrame, pDir, NULL, &lat);
    return -lat;                                /* 0x1005DF30 `fchs` */
}

/* =====================================================================
 * 0x1005D770's second half. See br_ai.h for the reading of each rule.
 * ===================================================================== */

/* --- 0x100344D0, and a deliberate second copy ---------------------------
 * BRGlide 0x100344D0 == BRD3D 0x1003AE50, which slice2_21.c already ports as
 * BrVec3NormaliseGuard. CONVENTIONS.md says reuse, and br_dl.c records why
 * that is not possible for this particular leaf: slice2_21.c pulls in BrSqrtF
 * from slice4_53.c, which pulls in most of the game, so linking it turns a
 * four-object test into an unlinkable one. br_dl.c carries the same copy for
 * the same reason and says so; this is the third instance of one original
 * function, stated here rather than discovered later. It is pure leaf
 * arithmetic with no state, so the aliased-storage hazard does not apply.
 *
 * The zero guard is `fcom 0.0f` + `test ah,0x40` (C3 alone) + `jne <zero>`, so
 * an UNORDERED length takes the same arm -- hence `!(len != 0.0f)`. The
 * result on that arm is (0,0,1), not a zero vector. */
static void br_ai_normalise(BrVec3 *pV)
{
    float len = BrVec3Length(pV);

    if (!(len != 0.0f)) {
        pV->x = 0.0f;
        pV->y = 0.0f;
        pV->z = 1.0f;
        return;
    }
    len = 1.0f / len;                           /* fdivr 1.0f, once */
    pV->x *= len;
    pV->y *= len;
    pV->z *= len;
}

/* --- 1. the path frame (0x1005DA05 .. 0x1005DA79) ------------------------ */

void BrAiPathFrameAt(BrAiPathFrame *pOut,
                     const BrAiPoint *pPt, const BrAiPoint *pNext)
{
    BrVec3 w;

    /* 0x1005DA11: 0x10034420(out, pos = pts[i].centre, aim = pts[i+1].centre)
     * -- the same normalise-the-difference routine BrAiAimDir wraps, so the
     * degenerate case is (0,0,1) rather than a NaN. */
    BrAiAimDir(&pOut->tangent, &pPt->centre, &pNext->centre);

    /* 0x1005DA3A: up = tangent x (left - right), then normalised at
     * 0x1005DA5A. The argument order is load-bearing and is the original's:
     * 0x10034560(out, a = +0x00, b = +0x18) is a - b, and 0x100342B0's A is
     * the SECOND pushed pointer. */
    BrVec3Sub(&w, &pPt->left, &pPt->right);              /* car+0xF30, raw   */
    BrVec3Cross(&pOut->up, &pOut->tangent, &w);          /* car+0xF3C        */
    br_ai_normalise(&pOut->up);

    /* 0x1005DA6B: lateral = up x tangent, over the top of the raw edge
     * vector, then normalised at 0x1005DA74. (up x tangent) x-expands to the
     * component of (left - right) perpendicular to the tangent, so `lateral`
     * points at the +0x00 edge -- which is what makes BrAiHalfWidth positive
     * on both shipped tracks. */
    BrVec3Cross(&pOut->lateral, &pOut->up, &pOut->tangent);
    br_ai_normalise(&pOut->lateral);
}

/* --- 2. the line offset (0x1005DA79 .. 0x1005DAC0) ----------------------- */

float BrAiLineOffset(const BrAiPathFrame *pFrm, const BrVec3 *pPos,
                     const BrVec3 *pVel, const BrVec3 *pCentre)
{
    BrVec3 d;

    BrVec3Sub(&d, pPos, pCentre);               /* 0x1005DA98 -> car+0xF4C */
    BrVec3MulAddTo(&d, pVel, BR_AI_LEAD);       /* 0x1005DAAD, 0x3ECCCCCD  */
    return BrVec3Dot(&pFrm->lateral, &d);       /* 0x1005DAB7              */
}

float BrAiHeading(const BrMat4 *pFrame, const BrAiPathFrame *pFrm)
{
    BrVec3 row;                                 /* row 0, copied not aliased */

    row.x = pFrame->m[0][0];
    row.y = pFrame->m[0][1];
    row.z = pFrame->m[0][2];
    return BrVec3Dot(&row, &pFrm->lateral);     /* 0x1005DACD */
}

/* --- 3. the corridor (0x1005DB0A .. 0x1005DB68) -------------------------- */

float BrAiHalfWidth(const BrAiPathFrame *pFrm, const BrAiPoint *pPt)
{
    BrVec3 h;

    BrVec3Sub(&h, &pPt->left, &pPt->centre);    /* 0x1005DB2C */
    return BrVec3Dot(&pFrm->lateral, &h);       /* 0x1005DB3D */
}

float BrAiCorridorLimit(float halfWidth)
{
    /* `fcom 5.0f` + `test ah,0x41` + `jne <*0.4>`: the subtract arm needs an
     * ordered greater-than, which `>` is. A NaN half-width multiplies. */
    if (halfWidth > 5.0f)                       /* 0x10077908 */
        return halfWidth - 3.0f;                /* 0x100778DC */
    return halfWidth * 0.4f;                    /* 0x1007790C */
}

/* --- 4. the correction law ---------------------------------------------- */

float BrAiSteerCorrection(float offset, float heading, float limit,
                          BrAiBias *pBias)
{
    float a = (offset < 0.0f) ? -offset : offset;   /* 0x1005DAF6/0x1005DB02 */
    float s;

    *pBias = BR_AI_BIAS_NONE;
    s = (a - limit) / a;                        /* 0x1005DBA7 then 0x1005DBB5 */

    /* `fld offset` + `fcomp 0.0f` + `test ah,0x41` + `jne <else>`: the first
     * arm needs an ordered greater-than, so zero AND NaN take the else. */
    if (offset > 0.0f) {
        if (heading > -0.05) {                  /* qword 0x10077910 */
            *pBias = BR_AI_BIAS_NEG;            /* 0x1005DBBB/0x1005DBC5 */
            return s * (-0.2f * heading) - -0.03f;      /* 0x10077918/0x1007791C */
        }
        /* `test ah,1` + `je <no action>`: the action needs C0, which an
         * unordered compare also sets -- so a NaN heading, which failed the
         * test above, lands HERE. Spelt negated for exactly that reason. */
        if (!(heading >= -0.15)) {              /* qword 0x10077920 */
            *pBias = BR_AI_BIAS_POS;            /* 0x1005E3CC/0x1005E3D6 */
            return s * (-0.3f * heading) - -0.1f;       /* 0x10077928/0x100778AC */
        }
        return 0.0f;                            /* the dead band */
    }
    if (!(heading >= 0.05)) {                   /* qword 0x10077930 */
        *pBias = BR_AI_BIAS_POS;                /* 0x1005E416/0x1005E420 */
        return s * (0.2f * heading) - -0.03f;   /* 0x100778E0/0x1007791C */
    }
    if (heading > 0.15) {                       /* qword 0x10077938 */
        *pBias = BR_AI_BIAS_NEG;                /* 0x1005E468/0x1005E472 */
        return s * (0.3f * heading) - -0.1f;    /* 0x10077940/0x100778AC */
    }
    return 0.0f;
}

/* --- 5. the in-corridor arbitration (0x1005E48D .. 0x1005E67D) ----------- */

int BrAiSign3(float v)
{
    /* 0x1005E4B6 `fcom 0.1f` + `test ah,0x41` + `jne <lower test>`: +1 needs
     * an ordered greater-than. 0x1005E4CF `fcomp -0.1f` + `test ah,1` + `je
     * <zero>`: -1 is taken on C0, which unordered also sets. So NaN -> -1. */
    if (v > 0.1f)                               /* 0x100778A4 */
        return 1;
    if (!(v >= -0.1f))                          /* 0x100778AC */
        return -1;
    return 0;
}

float BrAiSteerHold(float speed, int sVel, int sAux, int sFwd,
                    BrAiBias *pBias, int *pfCutThrottle)
{
    float mag = 0.0f;

    *pBias = BR_AI_BIAS_NONE;
    *pfCutThrottle = 0;

    /* 0x1005E49B `fcomp 10.0f` + `test ah,0x41` + `jne <do nothing>`: the
     * body needs an ordered greater-than. */
    if (!(speed > 10.0f))                       /* 0x10077898 */
        return 0.0f;

    if (sVel == 0 && sFwd == 0)                 /* 0x1005E55C */
        return 0.0f;

    if (sVel != 0) {
        if (sFwd + sVel == 0) {                 /* 0x1005E57A: opposite */
            if (sVel == 1 && sAux == 1) {       /* 0x1005E581/0x1005E585 */
                *pBias = BR_AI_BIAS_POS;
                return 0.1f;                    /* 0x3DCCCCCD */
            }
            if (sVel == -1 && sAux == -1)       /* 0x1005E5A2/0x1005E5A7 */
                *pBias = BR_AI_BIAS_NEG;
            /* 0x1005E5B7 sets the magnitude on BOTH paths through here,
             * including the one that left the bias alone. */
            return 0.1f;
        }
        if (sVel == sFwd) {                     /* 0x1005E5C8: same, non-zero */
            if (sVel == 1 && sAux == 1) {       /* 0x1005E5CC/0x1005E5D0 */
                *pBias = BR_AI_BIAS_NEG;
                return 0.4f;                    /* 0x3ECCCCCD */
            }
            if (sVel == -1 && sAux == -1)       /* 0x1005E5ED/0x1005E5F2 */
                *pBias = BR_AI_BIAS_POS;
            return 0.4f;                        /* 0x1005E602 */
        }
        /* sVel non-zero, neither equal nor opposite: sFwd must be 0, and the
         * original falls into the block below with it. */
    }

    if (sAux == 0) {                            /* 0x1005E60F */
        if (sFwd == 0)                          /* 0x1005E613 */
            return 0.0f;
        if (sVel == 0)                          /* 0x1005E626 */
            goto cut;
    }
    if (sAux == sFwd) {                         /* 0x1005E62A */
        if (sAux == 1) {                        /* 0x1005E62E */
            *pBias = BR_AI_BIAS_NEG;
            mag = 0.5f;                         /* 0x3F000000 */
            goto cut;
        }
        if (sAux == -1)                         /* 0x1005E648 */
            *pBias = BR_AI_BIAS_POS;
        mag = 0.5f;                             /* 0x1005E659, both paths */
        goto cut;
    }
    if (sAux == 0)                              /* 0x1005E663 */
        goto cut;
    if (sAux + sVel == 0)                       /* 0x1005E667 */
        return 0.0f;

cut:
    *pfCutThrottle = 1;                         /* 0x1005E671 */
    return mag;
}

/* --- 6. the aim error, scaled and clamped (0x1005DD52 .. 0x1005DDB4) ----- */

float BrAiSteerInput(float scale, float lat, float speed, int *pfCutThrottle)
{
    float p;

    *pfCutThrottle = 0;

    /* 0x1005DD5E `fcomp 10.0f` + `test ah,0x41` + `jne <plain>`. */
    if (!(speed > 10.0f))                       /* 0x10077898 */
        return lat;                             /* 0x1005DDB0 */

    p = scale * lat;                            /* 0x1005DD72 */
    if (p > 1.0f) {                             /* 0x100778F8, ordered only  */
        p = 1.0f;
        *pfCutThrottle = 1;                     /* 0x1005DDA8 */
    } else if (!(p >= -1.0f)) {                 /* 0x100778E4, C0 -> NaN too */
        p = -1.0f;
        *pfCutThrottle = 1;
    }
    return p;
}

/* --- 7. the response curve (0x1005DDB4 .. 0x1005DEC6) -------------------- */

float BrAiSteerResponse(float p, float mag, BrAiBias bias)
{
    float k, q, x;

    /* 0x1005DDB8 `fcomp 0.01f` + `test ah,1` + `je <keep>`: the floor is
     * applied on C0, so a NaN magnitude also becomes 0.01f. The floored value
     * is written back and is what the arms below multiply by. */
    if (!(mag >= 0.01f))                        /* 0x10077954 / 0x3C23D70A */
        mag = 0.01f;
    k = 0.2f / mag;                             /* 0x100778E0 over the slot  */

    /* 0x1005DDD9 `fcomp 0.0f` + `test ah,1` + `je <p >= 0>`: the negative arm
     * is taken on C0, so a NaN p goes left. */
    if (!(p >= 0.0f)) {
        q = p;
        if (bias == BR_AI_BIAS_POS) {           /* 0x1005DDF1 reads CBE8 first */
            /* `fcompp` against -k + `test ah,1` + `je <additive>`. */
            if (!(p >= -k))
                q = p - mag * p;                /* 0x1005DE0A..0x1005DE10 */
            else
                q = p - -0.2f;                  /* 0x1005DE14, 0x10077918 */
        } else if (bias == BR_AI_BIAS_NEG) {    /* 0x1005DE1C reads CF04 */
            if (!(p >= -k))
                q = p * (mag - -1.0f);          /* 0x1005DE39, 0x100778E4 */
            else
                q = p - 0.2f;                   /* 0x1005DE47, 0x100778E0 */
        }
        x = q - -1.0f;                          /* 0x1005DE4D */
        return x * x * x * x - 1.0f;            /* 0x1005DE53..0x1005DE5B */
    }

    q = p;
    if (bias == BR_AI_BIAS_POS) {               /* 0x1005DE6A */
        /* `fcom k` + `test ah,0x41` + `jne <additive>`: ordered greater. */
        if (p > k)
            q = p * (mag - -1.0f);              /* 0x1005DE79 */
        else
            q = p - -0.2f;                      /* 0x1005DE87 */
    } else if (bias == BR_AI_BIAS_NEG) {        /* 0x1005DE8F */
        if (p > k)
            q = p - mag * p;                    /* 0x1005DEA2..0x1005DEA8 */
        else
            q = p - 0.2f;                       /* 0x1005DEAC */
    }
    x = 1.0f - q;                               /* 0x1005DEB2 fsubr */
    return 1.0f - x * x * x * x;                /* 0x1005DEB8..0x1005DEC0 */
}

void BrAiSteerCompute(BrAiSteerOut *pOut, const BrAiSteerIn *pIn)
{
    float a = (pIn->offset < 0.0f) ? -pIn->offset : pIn->offset;
    float p;
    int   cut1 = 0, cut2 = 0;

    /* 0x1005DB6C `fcomp st(1)` + `test ah,0x41` + `jne <hold>`: C3 is set for
     * EQUAL, so |offset| == limit holds rather than corrects, and NaN holds. */
    if (a > pIn->limit)
        pOut->mag = BrAiSteerCorrection(pIn->offset, pIn->heading,
                                        pIn->limit, &pOut->bias);
    else
        pOut->mag = BrAiSteerHold(pIn->speed, pIn->sVel, pIn->sAux, pIn->sFwd,
                                  &pOut->bias, &cut1);

    p = BrAiSteerInput(pIn->scale, pIn->lat, pIn->speed, &cut2);
    pOut->curve = BrAiSteerResponse(p, pOut->mag, pOut->bias);
    pOut->value = -pOut->curve;                 /* 0x1005DF30 */
    pOut->fCutThrottle = (cut1 || cut2);

    /* Report the FLOORED magnitude: the original writes the floor back into
     * the slot at 0x1005DDC5, so nothing downstream ever sees the raw value. */
    if (!(pOut->mag >= 0.01f))
        pOut->mag = 0.01f;
}

/* --- 8. the throttle ladder (0x1005DBDD .. 0x1005DD52) ------------------- */

float BrAiThrottleTerm(const BrVec3 *pA, const BrVec3 *pB,
                       const BrVec3 *pBNext, const BrVec3 *pVel)
{
    BrVec3 ab, bb, n;
    float  u, v;

    BrVec3Sub(&ab, pA, pB);                     /* 0x1005DC14 -> the +0x6C slot */
    BrVec3Sub(&bb, pBNext, pB);                 /* 0x1005DC23 -> the +0x78 slot */
    BrVec3Cross(&n, &bb, &ab);                  /* 0x1005DC3A: A is bb        */
    br_ai_normalise(&n);                        /* 0x1005DC47                 */

    v = BrVec3Dot(pVel, &n);                    /* 0x1005DC5B */
    u = BrVec3Dot(pVel, &ab) * 0.03f;           /* 0x1005DC6D, 0x10077944 */

    /* Three arms, transcribed rather than folded. For ordered inputs this is
     * exactly max(|u|, v); it is left as branches because that is where the
     * NaN behaviour lives -- a NaN u fails both tests and comes out as -u. */
    if (u > v)                                  /* 0x1005DC7B, ordered only */
        return u;
    if (!(u >= -v))                             /* 0x1005DC98, C0 -> NaN too */
        return -u;
    return v;                                   /* 0x1005DCA9 */
}

BrAiThrottle BrAiThrottleLevel(float q, int iLevel)
{
    float i = (float)iLevel;                    /* 0x1005DCAB `fild` */

    /* 0x1005DCBD `fcompp` q vs 6*i + `test ah,0x41` + `je <brake>`: brake
     * needs an ordered greater-than. */
    if (q > 6.0f * i)                           /* 0x10077948 */
        return BR_AI_THR_BRAKE;
    /* 0x1005DCCE compares (4.5*i) against q with `test ah,1` + `jne`, i.e.
     * C0 on the SCALED side -- 4.5*i < q, or unordered. So a NaN q, which
     * failed the test above, lands here. */
    if (!(4.5f * i >= q))                       /* 0x1007794C */
        return BR_AI_THR_HARD;
    if (!(3.0f * i >= q))                       /* 0x100778DC */
        return BR_AI_THR_LIFT;
    return BR_AI_THR_CONTINUE;
}

float BrAiThrottleScale(BrAiThrottle act)
{
    switch (act) {
    case BR_AI_THR_BRAKE:                       /* 0x1005DD10 */
    case BR_AI_THR_HARD:                        /* 0x1005DD38 */
        return BR_AI_THROTTLE_SCALE_HARD;
    case BR_AI_THR_LIFT:                        /* 0x1005DD4A */
        return BR_AI_THROTTLE_SCALE_LIFT;
    default:
        return BR_AI_THROTTLE_SCALE_NONE;       /* 0x1005DBE2 */
    }
}

/* --- 9. the recovery timers (0x1005DED6 .. 0x1005E04D) ------------------- */

void BrAiRecoveryReset(BrAiRecovery *pSt)
{
    pSt->cHoldFwd = 0;                          /* 0x1005C83B */
    pSt->cHoldRev = 0;                          /* 0x1005C841 */
    pSt->cRevRun  = 0;                          /* 0x1005C847 */
    pSt->cFwdRun  = BR_AI_FWD_RUN_INIT;         /* 0x1005C84D, 0xFFFFFF4C */
}

BrAiDrive BrAiDriveStep(BrAiRecovery *pSt, float aimFwd, float velFwd,
                        float speed, float curve,
                        float *pSteer, unsigned *pfSet)
{
    BrAiDrive mode;

    /* 0x1005DEE7 `test ah,1` + `jne <behind>`: the ahead arm needs an ordered
     * >= 0, so a NaN aim goes to the behind arm. */
    if (aimFwd >= 0.0f) {
        if (pSt->cHoldRev != 0) {               /* 0x1005DEEC */
            pSt->cHoldRev--;
            mode = BR_AI_DRIVE_REVERSE;
        } else if (!(velFwd >= -1.0f)) {        /* 0x1005DF06, 0x100778E4 */
            mode = BR_AI_DRIVE_BRAKE;
        } else {
            mode = BR_AI_DRIVE_FORWARD;
        }
    } else {
        if (pSt->cHoldFwd != 0) {               /* 0x1005DF15 */
            pSt->cHoldFwd--;
            mode = BR_AI_DRIVE_FORWARD;
        } else if (velFwd > 1.0f) {             /* 0x1005DF71, 0x100778F8 */
            mode = BR_AI_DRIVE_BRAKE;
        } else {
            mode = BR_AI_DRIVE_REVERSE;
        }
    }

    if (mode == BR_AI_DRIVE_BRAKE) {            /* 0x1005DF82 */
        *pfSet |= BR_AI_CTL_40000;
        return mode;                            /* no steering write at all */
    }

    if (mode == BR_AI_DRIVE_FORWARD) {          /* 0x1005DF26 */
        *pSteer = -curve;                       /* 0x1005DF30 */
        pSt->cFwdRun++;                         /* 0x1005DF3B */
        if (pSt->cFwdRun > BR_AI_RUN_LATCH      /* 0x1005DF44 */
            && !(speed >= 1.0f)) {              /* 0x1005DF51, C0 -> NaN too */
            pSt->cHoldRev = BR_AI_HOLD_FRAMES;  /* 0x1005DF62 */
            pSt->cRevRun  = 0;                  /* 0x1005E041 */
            pSt->cFwdRun  = 0;                  /* 0x1005E047 */
        }
        return mode;
    }

    /* REVERSE, 0x1005DF93. The command is the SIGN of the curve, not its
     * negation -- so the car countersteers while backing out. */
    *pfSet |= BR_AI_CTL_10000;                  /* 0x1005DFA5 */
    *pSteer = !(curve >= 0.0f) ? -1.0f : 1.0f;  /* 0x1005DFBA / 0x1005DFC9 */
    *pfSet |= BR_AI_CTL_20000;                  /* 0x1005DFD8 */

    if (pSt->cRevRun > BR_AI_REV_FLIP) {        /* 0x1005DFE6, 0x96 */
        if (pSt->cRevRun > BR_AI_REV_RESET)     /* 0x1005DFED, 0x10E */
            pSt->cRevRun = BR_AI_RUN_LATCH;     /* 0x1005DFF4, back to 30 */
        else
            *pSteer = (float)(*pSteer * -1.0);  /* 0x1005E009, qword -1.0 */
    }
    pSt->cRevRun++;                             /* 0x1005E018 */
    if (pSt->cRevRun > BR_AI_RUN_LATCH          /* 0x1005E021 */
        && !(speed >= 1.0f)) {                  /* 0x1005E02A */
        pSt->cHoldFwd = BR_AI_HOLD_FRAMES;      /* 0x1005E037 */
        pSt->cRevRun  = 0;                      /* 0x1005E041 */
        pSt->cFwdRun  = 0;                      /* 0x1005E047 */
    }
    return mode;
}

/* --- 10. the overtaking pass (0x1005E054 .. 0x1005E1CD) ------------------ */

float BrAiOvertakeOffset(const BrAiRival *paRivals, int cRivals, int iSelf,
                         float progress, float offset, float lapLength)
{
    float best = BR_AI_OVERTAKE_RANGE;          /* 0x1005E05F, 0x42B40000 */
    float add  = 0.0f;                          /* 0x1005E069 */
    int   iBest = -1;                           /* 0x1005E05A */
    int   i;

    for (i = 0; i < cRivals; i++) {
        float d, diff;
        unsigned guard;

        if (!paRivals[i].fPresent)              /* 0x1005E07E, a null slot */
            continue;
        if (i == iSelf)                         /* 0x1005E086 */
            continue;

        d = paRivals[i].progress - progress;    /* 0x1005E099, both +0xFF4 */

        /* DEVIATION: both wrap loops are bounded here. The original's second
         * one tests C0, which an unordered compare sets, so a NaN progress
         * spins forever; a lapLength of zero hangs both. The guard cannot
         * fire on well-formed data -- one lap of wrap is one iteration. */
        for (guard = 0; d > lapLength; guard++) {           /* 0x1005E0B1 */
            if (guard > 0xFFFFu)
                break;
            d -= lapLength;
        }
        for (guard = 0; d < -lapLength; guard++) {          /* 0x1005E0D9 */
            if (guard > 0xFFFFu)
                break;
            d += lapLength;
        }

        if (!(d > 0.0f))                        /* 0x1005E0E8: behind, skip  */
            continue;
        if (!(d < best))                        /* 0x1005E0F9: further, skip */
            continue;

        diff = paRivals[i].offset - offset;     /* 0x1005E10E, rival's +0xF58 */
        best = d;                               /* 0x1005E114, before the arms */

        if (diff < 3.0f) {                      /* 0x100778DC */
            add = -10.0f * (1.0f - d * 0.011111111f);  /* 0x10077964/0x10077968 */
            if (!(add >= -BR_AI_OVERTAKE_CLAMP))       /* 0x1007796C */
                add = -BR_AI_OVERTAKE_CLAMP;
            iBest = i;
        } else if (diff > -3.0f) {              /* 0x100778F4 -- see br_ai.h:
                                                 * unreachable for ordered
                                                 * input, kept anyway */
            add = 10.0f * (1.0f - d * 0.011111111f);   /* 0x10077898 */
            if (add > BR_AI_OVERTAKE_CLAMP)            /* 0x10077908 */
                add = BR_AI_OVERTAKE_CLAMP;
            iBest = i;
        }
    }

    return (iBest < 0) ? 0.0f : add;            /* 0x1005E1A3 */
}

/* --- 11. the aim smoothing (0x1005D93D) --------------------------------- */

void BrAiAimSmooth(BrVec3 *pAim, const BrVec3 *pTarget)
{
    BrVec3Lerp(pAim, pAim, pTarget, BR_AI_LEAD);
}
