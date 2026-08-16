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
