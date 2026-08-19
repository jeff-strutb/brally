/* test_br_ai.c -- behaviour of the opponent-AI waypoint layer.
 *
 * The assertions here are BEHAVIOUR and FORMAT IDENTITIES, never volume or
 * plausibility. Each could fail if the decode or the walk were wrong and none
 * can fail merely because a particular track is long, short or oddly shaped.
 *
 * Half the suite runs on a path built here, so the exact cursor the walk
 * lands on is checked against numbers this file chose. The other half runs on
 * a shipped .TRK and checks identities of the encoding -- the centre point is
 * the midpoint of the two edges, the arc-length field falls strictly along
 * every node, the ring closes, and the last sentinel is exactly zero.
 *
 * Without the shipped tracks in testdata/tracks the second half SKIPs, per
 * br_testdata.h.
 */
#include "br_ai.h"
#include "br_testdata.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static int g_checks;   /* every CHECK, so the runner can see work happened */

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                        \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
            g_fail++;                                                          \
        }                                                                      \
    } while (0)

/* ===================================================================== */
/* A path built here, so the expected cursor is arithmetic, not a guess.  */
/* ===================================================================== */

/* Three nodes. A's `next` is C, C carries the SKIP flag, and C's sibling is
 * B -- so a hop out of A must land on B, which is what 0x1005D84F does.
 *
 *   A @0x1000  count 3   centres y = 0,10,20   arc 100,90,80  sentinel 70
 *   B @0x2000  count 2   centres y = 30,40     arc  70,60     sentinel 50
 *   C @0x3000  count 1   SKIP, sibling = B
 *
 * Every segment is therefore exactly 10 long.
 */
#define A_OFF 0x1000u
#define B_OFF 0x2000u
#define C_OFF 0x3000u
#define IMG_CB 0x4000u

static void put_u32be(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static void put_u16be(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;
}

static void put_f32be(unsigned char *p, float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    put_u32be(p, u);
}

static void put_vec3be(unsigned char *p, float x, float y, float z)
{
    put_f32be(p + 0, x); put_f32be(p + 4, y); put_f32be(p + 8, z);
}

/* left/right straddle the centre by 5 in x, so a corridor inset is easy to
 * read off and the centre is the exact midpoint. */
static void put_point(unsigned char *img, uint32_t nodeOff, uint32_t i,
                      float y, float arc)
{
    unsigned char *p = img + nodeOff + BR_AI_NODE_POINTS + i * BR_AI_POINT_STRIDE;
    put_vec3be(p + 0x00, -5.0f, y, 0.0f);
    put_vec3be(p + 0x0C,  0.0f, y, 0.0f);
    put_vec3be(p + 0x18,  5.0f, y, 0.0f);
    put_f32be (p + 0x24, arc);
}

static void put_node(unsigned char *img, uint32_t off, uint32_t nextOff,
                     uint32_t sibOff, uint16_t count, uint16_t flags)
{
    unsigned char *p = img + off;
    put_u32be(p + 0x00, nextOff ? nextOff + BR_TRK_N64_BASE : 0u);
    put_u32be(p + 0x04, sibOff  ? sibOff  + BR_TRK_N64_BASE : 0u);
    put_u16be(p + 0x14, count);
    put_u16be(p + 0x16, flags);
}

static void build_track(BrTrack *pTrack)
{
    unsigned char *img = (unsigned char *)calloc(1, IMG_CB);
    uint32_t i;

    memset(pTrack, 0, sizeof(*pTrack));
    pTrack->pbImage = img;
    pTrack->cbImage = IMG_CB;

    /* abHdr holds host-order FILE OFFSETS once BrTrackOpen has relocated it,
     * which is what BrTrackHdrU32 reads. Write one directly. */
    pTrack->abHdr[BR_TRK_H_AIPATH + 0] = (unsigned char)(A_OFF & 0xFF);
    pTrack->abHdr[BR_TRK_H_AIPATH + 1] = (unsigned char)((A_OFF >> 8) & 0xFF);
    pTrack->abHdr[BR_TRK_H_AIPATH + 2] = (unsigned char)((A_OFF >> 16) & 0xFF);
    pTrack->abHdr[BR_TRK_H_AIPATH + 3] = (unsigned char)((A_OFF >> 24) & 0xFF);

    put_node(img, A_OFF, C_OFF, 0,     3, 0);
    put_node(img, B_OFF, A_OFF, 0,     2, 0);
    put_node(img, C_OFF, 0,     B_OFF, 1, BR_AI_NODE_SKIP);

    for (i = 0; i <= 3; i++)
        put_point(img, A_OFF, i, 10.0f * (float)i, 100.0f - 10.0f * (float)i);
    for (i = 0; i <= 2; i++)
        put_point(img, B_OFF, i, 30.0f + 10.0f * (float)i,
                  70.0f - 10.0f * (float)i);
    put_point(img, C_OFF, 0, 0.0f, 0.0f);
    put_point(img, C_OFF, 1, 0.0f, 0.0f);
}

static void test_decode(void)
{
    BrTrack t;
    BrAiNode n;
    BrAiPoint pt;

    build_track(&t);

    CHECK(BrAiRoot(&t, &n) == 0, "root did not decode");
    CHECK(n.off == A_OFF, "root off %u", (unsigned)n.off);
    CHECK(n.count == 3, "root count %u", (unsigned)n.count);
    CHECK(n.offNext == C_OFF, "root next %u", (unsigned)n.offNext);
    CHECK(n.offSib == 0, "root sib %u", (unsigned)n.offSib);

    /* The sentinel record at index == count is addressable; count+1 is not. */
    CHECK(BrAiPoint_(&n, 3, &pt) == 0, "sentinel not readable");
    CHECK(pt.arc == 70.0f, "sentinel arc %f", (double)pt.arc);
    CHECK(BrAiPoint_(&n, 4, &pt) != 0, "read past the sentinel succeeded");

    CHECK(BrAiPoint_(&n, 1, &pt) == 0, "point 1 not readable");
    CHECK(pt.centre.y == 10.0f && pt.left.x == -5.0f && pt.right.x == 5.0f,
          "point 1 decoded as (%f,%f,%f)", (double)pt.left.x,
          (double)pt.centre.y, (double)pt.right.x);
    CHECK(pt.centre.x == (pt.left.x + pt.right.x) * 0.5f,
          "centre is not the midpoint");

    CHECK(BrAiLapLength(&t) == 100.0f, "lap length %f",
          (double)BrAiLapLength(&t));

    BrTrackClose(&t);
}

/* The behaviour the brief asks for: a longer lookahead picks a target further
 * along the path, and the walk always moves. */
static void test_advance(void)
{
    BrTrack t;
    BrAiNode n;
    uint32_t i;
    float prevArc;
    float d;

    build_track(&t);

    /* dist 0 still advances exactly one point -- the original's loop body
     * runs before its test. */
    CHECK(BrAiRoot(&t, &n) == 0, "root");
    i = 0;
    CHECK(BrAiAdvanceTarget(&n, &i, 0.0f) == 0, "advance(0) failed");
    CHECK(n.off == A_OFF && i == 1, "advance(0) -> node %u index %u",
          (unsigned)n.off, (unsigned)i);

    /* Each segment is 10 long, so 15 crosses two and 25 crosses three -- and
     * the third crossing exhausts node A, which hops through the SKIP-flagged
     * node C onto node B at index 0. */
    CHECK(BrAiRoot(&t, &n) == 0, "root");
    i = 0;
    CHECK(BrAiAdvanceTarget(&n, &i, 15.0f) == 0, "advance(15) failed");
    CHECK(n.off == A_OFF && i == 2, "advance(15) -> node %u index %u",
          (unsigned)n.off, (unsigned)i);

    CHECK(BrAiRoot(&t, &n) == 0, "root");
    i = 0;
    CHECK(BrAiAdvanceTarget(&n, &i, 25.0f) == 0, "advance(25) failed");
    CHECK(n.off == B_OFF && i == 0,
          "advance(25) should hop past the SKIP node onto B, got node %u "
          "index %u", (unsigned)n.off, (unsigned)i);

    /* The ring closes: from B, enough distance comes back to A. */
    CHECK(BrAiNodeAt(&t, B_OFF, &n) == 0, "node B");
    i = 0;
    CHECK(BrAiAdvanceTarget(&n, &i, 15.0f) == 0, "advance from B failed");
    CHECK(n.off == A_OFF && i == 0, "B + 15 -> node %u index %u",
          (unsigned)n.off, (unsigned)i);

    /* Monotone: a bigger lookahead never picks a target with MORE arc left.
     * Arc is the encoding's own "distance still to run", so this is the
     * "target advances" property stated without reference to indices.
     *
     * The sweep stops short of 40: this path is a RING of total forward arc
     * 50, so a larger distance wraps and the arc legitimately jumps back to
     * 100. Wrapping is tested separately above; mixing it in here would make
     * the monotonicity claim false for a correct implementation. */
    prevArc = 1e30f;
    for (d = 0.0f; d <= 39.0f; d += 3.0f) {
        BrAiPoint pt;
        CHECK(BrAiRoot(&t, &n) == 0, "root");
        i = 0;
        CHECK(BrAiAdvanceTarget(&n, &i, d) == 0, "advance(%f)", (double)d);
        CHECK(BrAiPoint_(&n, i, &pt) == 0, "target point at d=%f", (double)d);
        CHECK(pt.arc <= prevArc, "arc rose from %f to %f at d=%f",
              (double)prevArc, (double)pt.arc, (double)d);
        /* The walk stops as soon as it has consumed the distance, so the
         * consumed arc is at least d and at most d plus one segment -- the
         * step before last had not yet reached d. Both bounds are inclusive:
         * d=0 consumes exactly one whole segment. */
        CHECK(100.0f - pt.arc >= d, "consumed %f for d=%f",
              (double)(100.0f - pt.arc), (double)d);
        CHECK(100.0f - pt.arc <= d + 10.0f, "overshot: consumed %f for d=%f",
              (double)(100.0f - pt.arc), (double)d);
        prevArc = pt.arc;
    }

    BrTrackClose(&t);
}

/* ===================================================================== */
/* The lookahead law and the corridor                                    */
/* ===================================================================== */

static void test_lookahead(void)
{
    CHECK(BrAiLookahead(0.0f) == 20.0f, "lookahead(0) = %f",
          (double)BrAiLookahead(0.0f));
    CHECK(BrAiLookahead(10.0f) == 50.0f, "lookahead(10) = %f",
          (double)BrAiLookahead(10.0f));
    CHECK(BrAiLookahead(20.0f) == 80.0f, "lookahead(20) = %f",
          (double)BrAiLookahead(20.0f));
    CHECK(BrAiLookahead(1000.0f) == 80.0f, "lookahead clamp = %f",
          (double)BrAiLookahead(1000.0f));
    /* The clamp is `> max`, not `!(<= max)`, so an unordered speed keeps its
     * NaN instead of being turned into 80. */
    CHECK(BrAiLookahead((float)NAN) != BrAiLookahead((float)NAN),
          "NaN speed was clamped to a number");
}

static void test_corridor(void)
{
    BrAiPoint pt;
    BrVec3 nl, nr;

    memset(&pt, 0, sizeof pt);
    pt.left.x  = -5.0f;
    pt.right.x =  5.0f;
    pt.centre.x = 0.0f;

    BrAiCorridor(&nl, &nr, &pt, BR_AI_CORRIDOR_INSET);
    /* nearRight = (left - right)*0.2 + right = 3; nearLeft =
     * (right - left)*0.2 + left = -3. Each sits between its own edge and the
     * centre, which is what an inset corridor means. */
    CHECK(nr.x == 3.0f, "nearRight.x = %f", (double)nr.x);
    CHECK(nl.x == -3.0f, "nearLeft.x = %f", (double)nl.x);
    CHECK(nl.x < pt.centre.x && pt.centre.x < nr.x,
          "the corridor does not straddle the centre");
    CHECK(pt.left.x < nl.x && nr.x < pt.right.x,
          "the corridor is not inside the edges");

    /* BrVec3Lerp is (a-b)*t+b, so t == 0 yields the SECOND argument. That is
     * the whole reason the two calls differ. */
    BrAiCorridor(&nl, &nr, &pt, 0.0f);
    CHECK(nr.x == pt.right.x, "t=0 nearRight = %f", (double)nr.x);
    CHECK(nl.x == pt.left.x, "t=0 nearLeft = %f", (double)nl.x);
}

/* ===================================================================== */
/* Aiming: a waypoint ahead and to the left steers left                  */
/* ===================================================================== */

/* Rows are (forward, right, up, position), per the project's frame note.
 * Forward is +y here and right is +x, so "left" is -x. */
static void frame_identity(BrMat4 *m)
{
    memset(m, 0, sizeof *m);
    m->m[0][1] = 1.0f;   /* forward = +y */
    m->m[1][0] = 1.0f;   /* right   = +x */
    m->m[2][2] = 1.0f;   /* up      = +z */
    m->m[3][3] = 1.0f;
}

static void test_aim(void)
{
    BrMat4 f;
    BrVec3 pos, aim, dir;
    float fwd, lat, sLeft, sRight, sAhead;

    frame_identity(&f);
    pos.x = 0.0f; pos.y = 0.0f; pos.z = 0.0f;

    /* Ahead and to the LEFT. */
    aim.x = -3.0f; aim.y = 10.0f; aim.z = 0.0f;
    BrAiAimDir(&dir, &pos, &aim);
    CHECK(fabs((double)(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z) - 1.0)
              < 1e-5, "aim direction is not unit length");
    BrAiAimError(&f, &dir, &fwd, &lat);
    CHECK(fwd > 0.0f, "a target ahead gave forward %f", (double)fwd);
    CHECK(lat < 0.0f, "a target to the left gave lateral %f", (double)lat);
    sLeft = BrAiSteerDirection(&f, &dir);
    CHECK(sLeft > 0.0f, "a waypoint ahead and to the left steered %f "
                        "(positive means left)", (double)sLeft);

    /* Ahead and to the RIGHT: the mirror image, exactly. */
    aim.x = 3.0f;
    BrAiAimDir(&dir, &pos, &aim);
    BrAiAimError(&f, &dir, &fwd, &lat);
    CHECK(fwd > 0.0f, "a target ahead gave forward %f", (double)fwd);
    CHECK(lat > 0.0f, "a target to the right gave lateral %f", (double)lat);
    sRight = BrAiSteerDirection(&f, &dir);
    CHECK(sRight < 0.0f, "a waypoint ahead and to the right steered %f",
          (double)sRight);
    CHECK(sRight == -sLeft, "mirroring the target did not negate the steer "
          "(%f vs %f)", (double)sRight, (double)sLeft);

    /* Dead ahead: no steer at all. */
    aim.x = 0.0f;
    BrAiAimDir(&dir, &pos, &aim);
    sAhead = BrAiSteerDirection(&f, &dir);
    CHECK(sAhead == 0.0f, "a waypoint dead ahead steered %f", (double)sAhead);

    /* Further off-line steers harder in the same direction. */
    aim.x = -9.0f;
    BrAiAimDir(&dir, &pos, &aim);
    CHECK(BrAiSteerDirection(&f, &dir) > sLeft,
          "a target further left did not steer harder");

    /* Behind and to the left still steers left, and reports it. */
    aim.x = -3.0f; aim.y = -10.0f;
    BrAiAimDir(&dir, &pos, &aim);
    BrAiAimError(&f, &dir, &fwd, &lat);
    CHECK(fwd < 0.0f, "a target behind gave forward %f", (double)fwd);
    CHECK(BrAiSteerDirection(&f, &dir) > 0.0f, "behind-left did not steer left");

    /* Degenerate: aim == position yields (0,0,1) -- 0x100344BF, not a NaN. */
    aim = pos;
    BrAiAimDir(&dir, &pos, &aim);
    CHECK(dir.x == 0.0f && dir.y == 0.0f && dir.z == 1.0f,
          "degenerate aim gave (%f,%f,%f)", (double)dir.x, (double)dir.y,
          (double)dir.z);
}

/* ===================================================================== */
/* 0x1005D770's second half: the frame, the corridor, the steering        */
/* magnitude, the throttle ladder, the recovery timers, the overtake.     */
/* ===================================================================== */

/* The car frame as the IMAGE builds it, not as the README labels it.
 * 0x1006F249 stores (0,0,1) into row 2 and 0x1006F255 crosses row2 x row0
 * into row 1, so with forward = +y and up = +z, row 1 is -x. Every assertion
 * below is stated in terms of +/-row1, never "left" or "right", because that
 * is the only thing the image pins down.
 *
 * frame_identity() above deliberately keeps its own (row1 = +x) frame: the
 * tests written against it assert a relation to row 1 and stay true either
 * way, and changing it would be changing a passing test to suit new code. */
static void frame_car(BrMat4 *m)
{
    memset(m, 0, sizeof *m);
    m->m[0][1] =  1.0f;   /* row 0, forward = +y            */
    m->m[1][0] = -1.0f;   /* row 1 = row2 x row0 = z x y    */
    m->m[2][2] =  1.0f;   /* row 2, up = +z                 */
    m->m[3][3] =  1.0f;
}

/* A straight path along +y, `left` (+0x00) at -x and `right` (+0x18) at +x,
 * so the frame comes out tangent (0,1,0), up (0,0,1), lateral (-1,0,0) --
 * i.e. lateral is exactly row 1 of frame_car. */
static void straight_points(BrAiPoint *pPt, BrAiPoint *pNext, float halfW)
{
    memset(pPt, 0, sizeof *pPt);
    memset(pNext, 0, sizeof *pNext);
    pPt->left.x   = -halfW;  pPt->left.y   = 0.0f;
    pPt->centre.x =  0.0f;   pPt->centre.y = 0.0f;
    pPt->right.x  =  halfW;  pPt->right.y  = 0.0f;
    pNext->left.x   = -halfW; pNext->left.y   = 10.0f;
    pNext->centre.x =  0.0f;  pNext->centre.y = 10.0f;
    pNext->right.x  =  halfW; pNext->right.y  = 10.0f;
}

static int near_(float a, float b, double tol)
{
    return fabs((double)(a - b)) < tol;
}

static void test_path_frame(void)
{
    BrAiPoint pt, nx;
    BrAiPathFrame f, g;
    float hw;

    straight_points(&pt, &nx, 6.0f);
    BrAiPathFrameAt(&f, &pt, &nx);

    /* Unit length and mutually perpendicular -- identities of the
     * construction (two crosses and two normalises), not expectations. */
    CHECK(near_(BrVec3Length(&f.tangent), 1.0f, 1e-5), "tangent |%f|",
          (double)BrVec3Length(&f.tangent));
    CHECK(near_(BrVec3Length(&f.lateral), 1.0f, 1e-5), "lateral |%f|",
          (double)BrVec3Length(&f.lateral));
    CHECK(near_(BrVec3Length(&f.up), 1.0f, 1e-5), "up |%f|",
          (double)BrVec3Length(&f.up));
    CHECK(near_(BrVec3Dot(&f.tangent, &f.lateral), 0.0f, 1e-5),
          "tangent.lateral = %f", (double)BrVec3Dot(&f.tangent, &f.lateral));
    CHECK(near_(BrVec3Dot(&f.tangent, &f.up), 0.0f, 1e-5), "tangent.up");
    CHECK(near_(BrVec3Dot(&f.lateral, &f.up), 0.0f, 1e-5), "lateral.up");

    CHECK(near_(f.tangent.y, 1.0f, 1e-5), "tangent is not along the segment");
    CHECK(f.up.z > 0.0f, "up came out %f, not +z", (double)f.up.z);
    CHECK(near_(f.lateral.x, -1.0f, 1e-5), "lateral = (%f,%f,%f)",
          (double)f.lateral.x, (double)f.lateral.y, (double)f.lateral.z);

    /* The load-bearing orientation claim, checked rather than asserted in
     * prose: `lateral` points at the +0x00 edge, so the half width is
     * POSITIVE and equals the geometric half width. */
    hw = BrAiHalfWidth(&f, &pt);
    CHECK(near_(hw, 6.0f, 1e-4), "half width %f", (double)hw);
    CHECK(BrVec3Dot(&f.lateral, &f.lateral) > 0.0f, "degenerate lateral");

    /* Swapping the two edges is the one thing that can flip the frame, and
     * it flips BOTH the up and the lateral together -- so the half width
     * stays positive. That is a property of the two crosses, and it is why
     * the sign of `left` versus `right` cannot silently invert the law. */
    {
        BrVec3 t = pt.left;
        pt.left = pt.right; pt.right = t;
        t = nx.left; nx.left = nx.right; nx.right = t;
        BrAiPathFrameAt(&g, &pt, &nx);
        CHECK(near_(g.up.z, -f.up.z, 1e-5), "swapping the edges did not flip up");
        CHECK(near_(g.lateral.x, -f.lateral.x, 1e-5),
              "swapping the edges did not flip lateral");
        CHECK(BrAiHalfWidth(&g, &pt) > 0.0f,
              "the half width went negative under an edge swap: %f",
              (double)BrAiHalfWidth(&g, &pt));
    }
}

static void test_line_offset(void)
{
    BrAiPoint pt, nx;
    BrAiPathFrame f;
    BrVec3 pos, vel;
    float o0, o1;

    straight_points(&pt, &nx, 6.0f);
    BrAiPathFrameAt(&f, &pt, &nx);

    memset(&pos, 0, sizeof pos);
    memset(&vel, 0, sizeof vel);

    /* Dead on the line, stationary. */
    CHECK(BrAiLineOffset(&f, &pos, &vel, &pt.centre) == 0.0f,
          "a car on the line reported an offset");

    /* Displaced toward the +0x00 edge (which lateral points at): positive. */
    pos.x = -2.0f;
    o0 = BrAiLineOffset(&f, &pos, &vel, &pt.centre);
    CHECK(near_(o0, 2.0f, 1e-5), "offset toward `left` = %f", (double)o0);
    pos.x = 2.0f;
    CHECK(near_(BrAiLineOffset(&f, &pos, &vel, &pt.centre), -2.0f, 1e-5),
          "the offset is not odd about the centre");

    /* The 0.4f LEAD: the same position with a velocity heading further out
     * reports a larger offset, by exactly 0.4 * the lateral velocity. */
    pos.x = -2.0f;
    vel.x = -5.0f;
    o1 = BrAiLineOffset(&f, &pos, &vel, &pt.centre);
    CHECK(near_(o1 - o0, BR_AI_LEAD * 5.0f, 1e-4),
          "the velocity lead contributed %f, not %f", (double)(o1 - o0),
          (double)(BR_AI_LEAD * 5.0f));
    CHECK(o1 > o0, "the lead did not push the offset outward");

    /* Heading: the car's row 0 dotted with the path lateral. Nose straight
     * down the path -> zero; nose swung toward +lateral -> positive. */
    {
        BrMat4 m;
        frame_car(&m);
        CHECK(near_(BrAiHeading(&m, &f), 0.0f, 1e-5),
              "a car aligned with the path reported heading %f",
              (double)BrAiHeading(&m, &f));
        m.m[0][0] = -0.3f; m.m[0][1] = 0.95f;      /* nose toward -x = +lateral */
        CHECK(BrAiHeading(&m, &f) > 0.0f,
              "a nose swung toward +lateral gave heading %f",
              (double)BrAiHeading(&m, &f));
    }
}

static void test_corridor_limit(void)
{
    float a, b;

    /* The two arms and the knee. 5*0.4 == 5-3 == 2, so the law is CONTINUOUS
     * at its threshold -- an identity of the two constants, and the thing an
     * inverted comparison would break. */
    CHECK(BrAiCorridorLimit(4.0f) == 1.6f, "limit(4) = %f",
          (double)BrAiCorridorLimit(4.0f));
    CHECK(BrAiCorridorLimit(5.0f) == 2.0f, "limit(5) = %f",
          (double)BrAiCorridorLimit(5.0f));
    CHECK(BrAiCorridorLimit(6.0f) == 3.0f, "limit(6) = %f",
          (double)BrAiCorridorLimit(6.0f));
    a = BrAiCorridorLimit(5.0f);
    b = BrAiCorridorLimit(5.0001f);
    CHECK(near_(a, b, 1e-3), "the corridor law jumps at its knee: %f -> %f",
          (double)a, (double)b);

    /* Monotone, positive, and always strictly inside the track. */
    {
        float w, prev = -1.0f;
        for (w = 0.5f; w < 40.0f; w += 0.5f) {
            float l = BrAiCorridorLimit(w);
            CHECK(l > 0.0f, "limit(%f) = %f", (double)w, (double)l);
            CHECK(l < w, "limit(%f) = %f is not inside the track", (double)w,
                  (double)l);
            CHECK(l > prev, "limit fell from %f to %f at w=%f", (double)prev,
                  (double)l, (double)w);
            prev = l;
        }
    }
}

static void test_correction(void)
{
    BrAiBias bias;
    float m, mm;
    float lim = 2.0f;

    /* Off toward +lateral, nose straight: the bias points the other way. */
    m = BrAiSteerCorrection(4.0f, 0.0f, lim, &bias);
    CHECK(bias == BR_AI_BIAS_NEG, "offset>0 gave bias %d", (int)bias);
    CHECK(near_(m, 0.03f, 1e-6), "offset>0 heading 0 gave %f", (double)m);

    /* Off toward -lateral: the exact mirror, same magnitude. */
    mm = BrAiSteerCorrection(-4.0f, 0.0f, lim, &bias);
    CHECK(bias == BR_AI_BIAS_POS, "offset<0 gave bias %d", (int)bias);
    CHECK(near_(mm, m, 1e-6), "the law is not mirror-symmetric: %f vs %f",
          (double)mm, (double)m);

    /* The dead band, both sides. It is NOT symmetric about zero heading:
     * -0.15..-0.05 on the +offset side, +0.05..+0.15 on the other. */
    CHECK(BrAiSteerCorrection(4.0f, -0.10f, lim, &bias) == 0.0f,
          "the +offset dead band produced a correction");
    CHECK(bias == BR_AI_BIAS_NONE, "the dead band set a bias");
    CHECK(BrAiSteerCorrection(-4.0f, 0.10f, lim, &bias) == 0.0f,
          "the -offset dead band produced a correction");
    CHECK(bias == BR_AI_BIAS_NONE, "the dead band set a bias");
    /* ...and the same heading on the OTHER side is outside the band. */
    CHECK(BrAiSteerCorrection(4.0f, 0.10f, lim, &bias) != 0.0f,
          "the dead band is symmetric about zero heading, and should not be");

    /* The outer arm flips the bias: a car already swinging back hard gets
     * damped, not pushed further. -0.15 is the threshold. */
    (void)BrAiSteerCorrection(4.0f, -0.16f, lim, &bias);
    CHECK(bias == BR_AI_BIAS_POS, "the outer arm did not flip the bias");
    (void)BrAiSteerCorrection(-4.0f, 0.16f, lim, &bias);
    CHECK(bias == BR_AI_BIAS_NEG, "the outer arm did not flip the bias");

    /* The (|offset| - limit)/|offset| term. It rises from 0 at the corridor
     * edge toward 1 far out, so the magnitude slides from the bare constant
     * toward constant + heading-term. It is NOT monotone in one direction --
     * which way it goes depends on the SIGN of the heading term -- so the
     * property to assert is that the heading contribution grows with
     * distance, in whichever direction it points.
     *
     * (An earlier draft of this test asserted "the correction rises with
     * distance" and failed for a correct implementation, because with a
     * positive heading on the +offset side the -0.2 factor makes the heading
     * term negative. That is the shape of mistake CONVENTIONS.md warns
     * about: an expectation, not a property.) */
    {
        float h;
        for (h = -0.9f; h <= 0.9f; h += 0.3f) {
            float o, prev = -1.0f;
            float base;
            /* Skip the dead band's edge, and headings so near zero that
             * the heading term itself is nothing to measure. */
            if (h > -0.16f && h < 0.06f)
                continue;
            base = BrAiSteerCorrection(lim + 1e-4f, h, lim, &bias);
            if (bias == BR_AI_BIAS_NONE)
                continue;
            /* At the corridor edge s ~ 0, so only the +0.03 / +0.1 constant
             * survives, whatever the heading is. */
            CHECK(near_(base, 0.03f, 1e-3) || near_(base, 0.1f, 1e-3),
                  "at the corridor edge heading %f gave %f, which is neither "
                  "constant", (double)h, (double)base);
            for (o = lim + 0.01f; o < 40.0f; o += 0.5f) {
                float v = BrAiSteerCorrection(o, h, lim, &bias);
                float d = (float)fabs((double)(v - base));
                CHECK(d >= prev - 1e-6f,
                      "the heading contribution shrank from %f to %f at "
                      "offset %f, heading %f", (double)prev, (double)d,
                      (double)o, (double)h);
                prev = d;
            }
            CHECK(prev > 1e-4f, "heading %f contributed nothing at any "
                  "distance", (double)h);
        }
    }

    /* A NaN heading takes the OUTER arm on both sides: the two inner tests
     * are `test ah,0x41` + `jne` (which unordered fails) and the two outer
     * ones are `test ah,1` + `je` (which unordered passes). */
    (void)BrAiSteerCorrection(4.0f, (float)NAN, lim, &bias);
    CHECK(bias == BR_AI_BIAS_POS, "a NaN heading did not take the outer arm");
    (void)BrAiSteerCorrection(-4.0f, (float)NAN, lim, &bias);
    CHECK(bias == BR_AI_BIAS_POS, "a NaN heading did not take the inner arm "
          "on the -offset side");

    /* offset == 0 is on the `else` side: `fcomp 0.0` + `test ah,0x41` sets C3
     * for equal, so zero takes the same arm as negative. */
    (void)BrAiSteerCorrection(0.0f, 0.0f, lim, &bias);
    CHECK(bias == BR_AI_BIAS_POS, "offset == 0 did not take the -offset arm");
}

static void test_response(void)
{
    float p;
    float prev = -1e30f;

    /* Fixed points and odd symmetry with no bias. */
    CHECK(BrAiSteerResponse(0.0f, 0.5f, BR_AI_BIAS_NONE) == 0.0f,
          "response(0) = %f", (double)BrAiSteerResponse(0.0f, 0.5f,
                                                        BR_AI_BIAS_NONE));
    CHECK(near_(BrAiSteerResponse(1.0f, 0.5f, BR_AI_BIAS_NONE), 1.0f, 1e-6),
          "response(1) = %f",
          (double)BrAiSteerResponse(1.0f, 0.5f, BR_AI_BIAS_NONE));
    CHECK(near_(BrAiSteerResponse(-1.0f, 0.5f, BR_AI_BIAS_NONE), -1.0f, 1e-6),
          "response(-1) = %f",
          (double)BrAiSteerResponse(-1.0f, 0.5f, BR_AI_BIAS_NONE));

    for (p = -1.0f; p <= 1.0f; p += 0.05f) {
        float a = BrAiSteerResponse(p, 0.5f, BR_AI_BIAS_NONE);
        float b = BrAiSteerResponse(-p, 0.5f, BR_AI_BIAS_NONE);
        CHECK(near_(a, -b, 1e-5), "response is not odd at p=%f (%f vs %f)",
              (double)p, (double)a, (double)b);
        CHECK(a > prev, "response fell from %f to %f at p=%f", (double)prev,
              (double)a, (double)p);
        CHECK(a >= -1.0001f && a <= 1.0001f, "response(%f) = %f left [-1,1]",
              (double)p, (double)a);
        prev = a;
    }

    /* Expansive near zero -- a quartic on (1 - p), not a gain. */
    CHECK(BrAiSteerResponse(0.1f, 0.5f, BR_AI_BIAS_NONE) > 0.3f,
          "response(0.1) = %f, which is not the quartic",
          (double)BrAiSteerResponse(0.1f, 0.5f, BR_AI_BIAS_NONE));

    /* The uniformity the enum depends on: POS raises the output and NEG
     * lowers it, on BOTH sides of zero, even though the four arms use
     * different arithmetic. This is what would break if any one of the four
     * had its operands the wrong way round.
     *
     * IT ONLY HOLDS WHILE THE BIASED p STAYS INSIDE [-1,1], and that is a
     * property of the original, not a limitation of the test. Both quartics
     * are evaluated on a shifted argument -- (q+1)^4 - 1 and 1 - (1-q)^4 --
     * so each is monotone only on its own side of its fold. Push q past -1
     * (or 1) and the curve turns around: with mag 0.5, p = -0.9 gives
     * q = -1.35 under NEG, and (q+1)^4 - 1 = -0.985, which is HIGHER than the
     * unbiased -0.9999, not lower. The original does this; the range where
     * the bias means what its name says is |q| <= 1, which for mag 0.5 is
     * |p| <= 0.5. */
    for (p = -0.5f; p <= 0.5f; p += 0.1f) {
        float n = BrAiSteerResponse(p, 0.5f, BR_AI_BIAS_NONE);
        float u = BrAiSteerResponse(p, 0.5f, BR_AI_BIAS_POS);
        float d = BrAiSteerResponse(p, 0.5f, BR_AI_BIAS_NEG);
        CHECK(u > n, "BIAS_POS did not raise the response at p=%f (%f vs %f)",
              (double)p, (double)u, (double)n);
        CHECK(d < n, "BIAS_NEG did not lower the response at p=%f (%f vs %f)",
              (double)p, (double)d, (double)n);
    }
    /* The fold itself, pinned so that "fixing" the quartic into a monotone
     * curve would be caught. */
    CHECK(BrAiSteerResponse(-0.9f, 0.5f, BR_AI_BIAS_NEG)
              > BrAiSteerResponse(-0.9f, 0.5f, BR_AI_BIAS_NONE),
          "the quartic no longer folds past q = -1");
    CHECK(near_(BrAiSteerResponse(-0.9f, 0.5f, BR_AI_BIAS_NEG), -0.985f, 1e-3),
          "the fold value moved: %f",
          (double)BrAiSteerResponse(-0.9f, 0.5f, BR_AI_BIAS_NEG));

    /* THE DISCONTINUITY AT p == 0, which is real and is preserved. The branch
     * is `fcomp 0.0` + `test ah,1` + `je`, so p == 0 takes the p >= 0 arm --
     * and the +-0.2 nudge then lands on the far side of that arm's fold. The
     * SIGN is continuous across zero (both are "steer toward -lateral" for
     * NEG) but the magnitude jumps from 0.59 to 1.07. */
    {
        float above = BrAiSteerResponse(0.0f, 0.03f, BR_AI_BIAS_NEG);
        float below = BrAiSteerResponse(-1e-6f, 0.03f, BR_AI_BIAS_NEG);
        CHECK(above < 0.0f && below < 0.0f,
              "the bias direction is not continuous across p == 0 (%f, %f)",
              (double)above, (double)below);
        CHECK(near_(above, -1.0736f, 1e-3), "p == 0 under NEG gave %f",
              (double)above);
        CHECK(near_(below, -0.5904f, 1e-3), "p == 0- under NEG gave %f",
              (double)below);
        CHECK(fabs((double)(above - below)) > 0.4,
              "the p == 0 magnitude discontinuity has gone away");
    }

    /* The k = 0.2/mag gate. A weak request (mag floored at 0.01) gives
     * k = 20, which no |p| <= 1 can exceed, so it must take the ADDITIVE
     * arm: response(p, weak, POS) == response(p + 0.2, weak, NONE). A strong
     * request (mag 1 -> k = 0.2) takes the multiplicative arm at p = 0.5. */
    CHECK(near_(BrAiSteerResponse(0.5f, 0.0f, BR_AI_BIAS_POS),
                BrAiSteerResponse(0.7f, 0.0f, BR_AI_BIAS_NONE), 1e-5),
          "a weak request did not take the additive arm");
    CHECK(near_(BrAiSteerResponse(0.5f, 1.0f, BR_AI_BIAS_POS),
                BrAiSteerResponse(1.0f, 1.0f, BR_AI_BIAS_NONE), 1e-5),
          "a strong request did not take the multiplicative arm (p*(mag+1))");
    /* The floor itself: any magnitude below 0.01 behaves as 0.01. */
    CHECK(BrAiSteerResponse(0.5f, 0.0f, BR_AI_BIAS_POS)
          == BrAiSteerResponse(0.5f, 0.01f, BR_AI_BIAS_POS),
          "the 0.01f magnitude floor is not applied");
    CHECK(BrAiSteerResponse(0.5f, (float)NAN, BR_AI_BIAS_POS)
          == BrAiSteerResponse(0.5f, 0.01f, BR_AI_BIAS_POS),
          "a NaN magnitude did not take the floor");
}

static void test_steer_input(void)
{
    int cut;
    float v;

    /* Below 10 the scale and the clamp are both bypassed entirely -- a
     * standing car can report an aim error outside [-1,1]. */
    v = BrAiSteerInput(2.0f, 3.0f, 5.0f, &cut);
    CHECK(v == 3.0f, "slow: %f", (double)v);
    CHECK(cut == 0, "slow: the throttle bit was cut");

    /* Above it, scaled then clamped, and the clamp cuts the 0x10000 bit. */
    v = BrAiSteerInput(1.0f, 0.5f, 20.0f, &cut);
    CHECK(v == 0.5f, "fast unclamped: %f", (double)v);
    CHECK(cut == 0, "fast unclamped: the throttle bit was cut");

    v = BrAiSteerInput(2.0f, 0.9f, 20.0f, &cut);
    CHECK(v == 1.0f, "fast clamped high: %f", (double)v);
    CHECK(cut == 1, "the upper clamp did not cut the throttle bit");

    v = BrAiSteerInput(2.0f, -0.9f, 20.0f, &cut);
    CHECK(v == -1.0f, "fast clamped low: %f", (double)v);
    CHECK(cut == 1, "the lower clamp did not cut the throttle bit");

    /* THE TRAP. The upper arm is `fcom` + `test ah,0x41` + `jne` (ordered
     * greater only) and the lower is `fcom` + `test ah,1` + `je <keep>` (C0,
     * which unordered also sets). A NaN aim error therefore comes out as
     * FULL LOCK ONE WAY -- not NaN, and not the other way. */
    v = BrAiSteerInput(1.0f, (float)NAN, 20.0f, &cut);
    CHECK(v == -1.0f, "a NaN aim error gave %f, not -1", (double)v);
    CHECK(cut == 1, "the NaN clamp did not cut the throttle bit");
    /* ...and below the speed gate the same NaN passes straight through. */
    v = BrAiSteerInput(1.0f, (float)NAN, 5.0f, &cut);
    CHECK(v != v, "a slow NaN aim error was clamped to %f", (double)v);
}

/* Fill a BrAiSteerIn for a car sitting on the racing line with the aim error
 * `lat` and nothing else going on. */
static void steer_in(BrAiSteerIn *pIn, float lat, float offset)
{
    memset(pIn, 0, sizeof *pIn);
    pIn->lat    = lat;
    pIn->offset = offset;
    pIn->limit  = 2.0f;
    pIn->scale  = 1.0f;
    pIn->speed  = 0.0f;
}

static void test_steer_compute(void)
{
    BrAiSteerIn in;
    BrAiSteerOut a, b, c;

    /* On the line, aimed straight down it: no steering at all. */
    steer_in(&in, 0.0f, 0.0f);
    BrAiSteerCompute(&a, &in);
    CHECK(a.value == 0.0f, "a car on the racing line steered %f",
          (double)a.value);
    CHECK(a.bias == BR_AI_BIAS_NONE, "a car on the line requested a bias");
    CHECK(a.mag == 0.01f, "the reported magnitude %f is not the floor",
          (double)a.mag);

    /* A waypoint on the +row1 side steers that way, and harder the further
     * off it is. row1 == the path's `lateral`, so this is the "ahead and to
     * one side" case with the side named by the image rather than by a
     * compass. The command's sign is the NEGATION of the aim error. */
    steer_in(&in, 0.3f, 0.0f);
    BrAiSteerCompute(&a, &in);
    steer_in(&in, 0.6f, 0.0f);
    BrAiSteerCompute(&b, &in);
    CHECK(a.value < 0.0f, "a +row1 target steered %f", (double)a.value);
    CHECK(b.value < a.value, "a target further toward +row1 did not steer "
          "harder (%f vs %f)", (double)b.value, (double)a.value);
    steer_in(&in, -0.3f, 0.0f);
    BrAiSteerCompute(&c, &in);
    CHECK(c.value > 0.0f, "a -row1 target steered %f", (double)c.value);
    CHECK(near_(c.value, -a.value, 1e-5),
          "mirroring the target did not negate the command (%f vs %f)",
          (double)c.value, (double)a.value);

    /* THE CLOSED LOOP. Same aim error, three lateral positions. A car pushed
     * out past the corridor toward +lateral must steer LESS toward +lateral
     * than the same car on the line, and one pushed the other way must steer
     * more. This is the assertion that fails if any of the four bias arms in
     * the response curve, or either bias assignment in the correction law,
     * has its sign inverted. */
    steer_in(&in, 0.0f, 0.0f);
    BrAiSteerCompute(&a, &in);                  /* on the line     */
    steer_in(&in, 0.0f, 6.0f);                  /* out past +limit */
    BrAiSteerCompute(&b, &in);
    steer_in(&in, 0.0f, -6.0f);                 /* out past -limit */
    BrAiSteerCompute(&c, &in);
    CHECK(b.bias == BR_AI_BIAS_NEG, "a car out toward +lateral asked for %d",
          (int)b.bias);
    CHECK(c.bias == BR_AI_BIAS_POS, "a car out toward -lateral asked for %d",
          (int)c.bias);
    CHECK(b.value > a.value, "a car displaced toward +lateral did not steer "
          "back (%f vs %f on the line)", (double)b.value, (double)a.value);
    CHECK(c.value < a.value, "a car displaced toward -lateral did not steer "
          "back (%f vs %f on the line)", (double)c.value, (double)a.value);

    /* The mirror holds for any non-zero aim error, and NOT at exactly zero:
     * p == 0 takes the p >= 0 arm on both sides (see test_response), so the
     * two magnitudes there are 1.07 and 0.59 rather than a matched pair.
     * Stated as two assertions so the asymmetry is pinned rather than
     * papered over. */
    steer_in(&in, 0.1f, 6.0f);
    BrAiSteerCompute(&b, &in);
    steer_in(&in, -0.1f, -6.0f);
    BrAiSteerCompute(&c, &in);
    CHECK(near_(b.value, -c.value, 1e-5),
          "the two corrections are not mirror images (%f vs %f)",
          (double)b.value, (double)c.value);
    steer_in(&in, 0.0f, 6.0f);
    BrAiSteerCompute(&b, &in);
    steer_in(&in, 0.0f, -6.0f);
    BrAiSteerCompute(&c, &in);
    CHECK(b.value > 0.0f && c.value < 0.0f,
          "at zero aim error the corrections point the wrong way (%f, %f)",
          (double)b.value, (double)c.value);
    CHECK(!near_(b.value, -c.value, 1e-3),
          "the p == 0 asymmetry has gone away (%f vs %f)", (double)b.value,
          (double)c.value);

    /* ...and the correction survives an aim error pulling the other way:
     * a car out toward +lateral still steers back harder than one on the
     * line looking at the same waypoint. */
    steer_in(&in, 0.3f, 0.0f);
    BrAiSteerCompute(&a, &in);
    steer_in(&in, 0.3f, 6.0f);
    BrAiSteerCompute(&b, &in);
    CHECK(b.value > a.value, "the lateral correction was swamped by the aim "
          "error (%f vs %f)", (double)b.value, (double)a.value);

    /* The corridor boundary. |offset| == limit HOLDS rather than corrects,
     * because the original's compare tests C3 as well as C0 and C3 is set on
     * equal. One ulp further out and it corrects. */
    steer_in(&in, 0.0f, 2.0f);
    BrAiSteerCompute(&a, &in);
    CHECK(a.bias == BR_AI_BIAS_NONE,
          "|offset| exactly at the limit corrected instead of holding");
    steer_in(&in, 0.0f, 2.0001f);
    BrAiSteerCompute(&b, &in);
    CHECK(b.bias == BR_AI_BIAS_NEG, "just past the limit did not correct");

    /* A NaN offset holds too -- the same unordered arm. */
    steer_in(&in, 0.0f, (float)NAN);
    BrAiSteerCompute(&a, &in);
    CHECK(a.bias == BR_AI_BIAS_NONE, "a NaN offset took the correction arm");
}

static void test_steer_hold(void)
{
    BrAiBias bias;
    int cut;
    float m;

    /* Sign3's band and its NaN side. */
    CHECK(BrAiSign3(0.2f) == 1, "sign3(0.2)");
    CHECK(BrAiSign3(0.1f) == 0, "sign3 at the +edge is not inclusive");
    CHECK(BrAiSign3(0.0f) == 0, "sign3(0)");
    CHECK(BrAiSign3(-0.1f) == 0, "sign3 at the -edge is not inclusive");
    CHECK(BrAiSign3(-0.2f) == -1, "sign3(-0.2)");
    CHECK(BrAiSign3((float)NAN) == -1,
          "sign3(NaN) = %d; the first test is `jne` on 0x41 and the second "
          "`je` on 0x01, so unordered lands on -1", BrAiSign3((float)NAN));

    /* Below 10 the whole block is skipped. */
    m = BrAiSteerHold(9.0f, 1, 1, -1, &bias, &cut);
    CHECK(m == 0.0f && bias == BR_AI_BIAS_NONE && cut == 0,
          "the hold block ran below the speed gate");

    /* All-zero signs: nothing. */
    m = BrAiSteerHold(20.0f, 0, 0, 0, &bias, &cut);
    CHECK(m == 0.0f && bias == BR_AI_BIAS_NONE && cut == 0, "0/0/0 acted");

    /* sVel and sFwd opposite (the two axes have opposite senses, so this is
     * drifting and aiming the SAME way): a light 0.1 nudge. */
    m = BrAiSteerHold(20.0f, 1, 1, -1, &bias, &cut);
    CHECK(m == 0.1f, "opposite/aux+1 gave %f", (double)m);
    CHECK(bias == BR_AI_BIAS_POS, "opposite/aux+1 gave bias %d", (int)bias);
    m = BrAiSteerHold(20.0f, -1, -1, 1, &bias, &cut);
    CHECK(m == 0.1f, "opposite/aux-1 gave %f", (double)m);
    CHECK(bias == BR_AI_BIAS_NEG, "opposite/aux-1 gave bias %d", (int)bias);
    /* ...and with the aux sign not matching, the magnitude still lands but
     * the bias does not. 0x1005E5B7 stores it on both paths. */
    m = BrAiSteerHold(20.0f, 1, -1, -1, &bias, &cut);
    CHECK(m == 0.1f, "opposite/aux mismatch dropped the magnitude: %f",
          (double)m);
    CHECK(bias == BR_AI_BIAS_NONE, "opposite/aux mismatch set a bias");

    /* sVel == sFwd (drifting and aiming apart): a firmer 0.4, and the bias
     * is the OPPOSITE of the light case for the same aux sign. */
    m = BrAiSteerHold(20.0f, 1, 1, 1, &bias, &cut);
    CHECK(m == 0.4f, "same/aux+1 gave %f", (double)m);
    CHECK(bias == BR_AI_BIAS_NEG, "same/aux+1 gave bias %d", (int)bias);
    m = BrAiSteerHold(20.0f, -1, -1, -1, &bias, &cut);
    CHECK(m == 0.4f, "same/aux-1 gave %f", (double)m);
    CHECK(bias == BR_AI_BIAS_POS, "same/aux-1 gave bias %d", (int)bias);
    CHECK(cut == 0, "the sVel arms cut the throttle bit; only the tail does");

    /* The tail: sVel zero, so the aux/fwd pair decides, and every arm of it
     * clears the 0x10000 control bit. */
    m = BrAiSteerHold(20.0f, 0, 1, 1, &bias, &cut);
    CHECK(m == 0.5f, "aux==fwd==+1 gave %f", (double)m);
    CHECK(bias == BR_AI_BIAS_NEG, "aux==fwd==+1 gave bias %d", (int)bias);
    CHECK(cut == 1, "the tail did not cut the throttle bit");
    m = BrAiSteerHold(20.0f, 0, -1, -1, &bias, &cut);
    CHECK(m == 0.5f && bias == BR_AI_BIAS_POS && cut == 1,
          "aux==fwd==-1 gave %f / %d / %d", (double)m, (int)bias, cut);
    /* aux zero, vel zero, fwd live: the 0x1005E626 arm -- cut the throttle
     * bit and nothing else. */
    m = BrAiSteerHold(20.0f, 0, 0, 1, &bias, &cut);
    CHECK(m == 0.0f && bias == BR_AI_BIAS_NONE && cut == 1,
          "sFwd alone gave %f / %d / %d", (double)m, (int)bias, cut);
    /* ...but sVel alone, with both the others zero, returns at 0x1005E613
     * WITHOUT cutting. The two look interchangeable and are not. */
    m = BrAiSteerHold(20.0f, 1, 0, 0, &bias, &cut);
    CHECK(m == 0.0f && bias == BR_AI_BIAS_NONE && cut == 0,
          "sVel alone gave %f / %d / %d", (double)m, (int)bias, cut);
}

static void test_throttle(void)
{
    BrVec3 a, b, bn, v;
    float q;
    int i;

    /* A plane through the origin: A along +x, the strip edge along +y, so
     * the normal is -z. */
    a.x = 1.0f; a.y = 0.0f; a.z = 0.0f;
    b.x = 0.0f; b.y = 0.0f; b.z = 0.0f;
    bn.x = 0.0f; bn.y = 1.0f; bn.z = 0.0f;

    /* Closing on the plane along its normal: q is the closing speed. */
    v.x = 0.0f; v.y = 0.0f; v.z = -1.0f;
    q = BrAiThrottleTerm(&a, &b, &bn, &v);
    CHECK(near_(q, 1.0f, 1e-5), "closing on the wall gave q = %f", (double)q);

    /* Moving away from it: the normal term goes negative, and the 0.03
     * along-plane term (zero here) wins, so q is 0 rather than negative. */
    v.z = 1.0f;
    q = BrAiThrottleTerm(&a, &b, &bn, &v);
    CHECK(near_(q, 0.0f, 1e-5), "receding from the wall gave q = %f",
          (double)q);

    /* Along the plane: the 0.03 term, and it is an ABSOLUTE value -- running
     * the velocity backwards gives the same q. That is the whole point of the
     * three-way branch and is what a folded `max(u, v)` would lose. */
    v.x = 100.0f; v.y = 0.0f; v.z = 0.0f;
    q = BrAiThrottleTerm(&a, &b, &bn, &v);
    CHECK(near_(q, 3.0f, 1e-4), "along the plane gave q = %f", (double)q);
    v.x = -100.0f;
    CHECK(near_(BrAiThrottleTerm(&a, &b, &bn, &v), 3.0f, 1e-4),
          "the along-plane term is not absolute: %f",
          (double)BrAiThrottleTerm(&a, &b, &bn, &v));

    /* The ladder, and its boundaries. The thresholds are `3*i < q` etc, so
     * landing EXACTLY on one keeps the gentler action. */
    for (i = 1; i <= 4; i++) {
        float f = (float)i;
        CHECK(BrAiThrottleLevel(3.0f * f, i) == BR_AI_THR_CONTINUE,
              "q == 3*%d did not continue", i);
        CHECK(BrAiThrottleLevel(3.0f * f + 0.01f, i) == BR_AI_THR_LIFT,
              "just past 3*%d did not lift", i);
        CHECK(BrAiThrottleLevel(4.5f * f, i) == BR_AI_THR_LIFT,
              "q == 4.5*%d did not stay on lift", i);
        CHECK(BrAiThrottleLevel(4.5f * f + 0.01f, i) == BR_AI_THR_HARD,
              "just past 4.5*%d was not hard", i);
        CHECK(BrAiThrottleLevel(6.0f * f, i) == BR_AI_THR_HARD,
              "q == 6*%d did not stay hard", i);
        CHECK(BrAiThrottleLevel(6.0f * f + 0.01f, i) == BR_AI_THR_BRAKE,
              "just past 6*%d did not brake", i);
        /* Deeper levels tolerate more: the same q that brakes at level 1
         * is merely a lift at level 2. */
        if (i == 1)
            CHECK(BrAiThrottleLevel(6.1f, 2) == BR_AI_THR_LIFT,
                  "the thresholds do not scale with the level");
    }
    /* A NaN q fails the brake test (`jne` on 0x41) and passes the next
     * (`jne` on 0x01), so it lands on HARD. */
    CHECK(BrAiThrottleLevel((float)NAN, 1) == BR_AI_THR_HARD,
          "a NaN closing speed did not land on HARD");

    CHECK(BrAiThrottleScale(BR_AI_THR_CONTINUE) == 1.0f, "scale continue");
    CHECK(BrAiThrottleScale(BR_AI_THR_LIFT) == 1.3f, "scale lift");
    CHECK(BrAiThrottleScale(BR_AI_THR_HARD) == 2.0f, "scale hard");
    CHECK(BrAiThrottleScale(BR_AI_THR_BRAKE) == 2.0f, "scale brake");
}

static void test_recovery(void)
{
    BrAiRecovery st;
    float steer;
    unsigned set;
    int i;

    /* The respawn's four stores, one of which is not zero. */
    BrAiRecoveryReset(&st);
    CHECK(st.cHoldFwd == 0 && st.cHoldRev == 0 && st.cRevRun == 0,
          "the reset left a counter non-zero");
    CHECK(st.cFwdRun == BR_AI_FWD_RUN_INIT,
          "the reset left cFwdRun at %d, not -180", (int)st.cFwdRun);

    /* Plain forward drive: the command is the NEGATED curve. */
    memset(&st, 0, sizeof st);
    steer = 99.0f; set = 0;
    CHECK(BrAiDriveStep(&st, 1.0f, 5.0f, 30.0f, 0.5f, &steer, &set)
              == BR_AI_DRIVE_FORWARD, "aim ahead did not drive forward");
    CHECK(steer == -0.5f, "forward wrote %f, not the negated curve",
          (double)steer);
    CHECK(set == 0, "forward set a control bit");

    /* Brake: the aim is ahead but the car is travelling backwards fast.
     * It sets 0x40000 and writes NO steering -- the previous frame's command
     * stands, which is the original's behaviour and easy to lose. */
    memset(&st, 0, sizeof st);
    steer = 99.0f; set = 0;
    CHECK(BrAiDriveStep(&st, 1.0f, -2.0f, 30.0f, 0.5f, &steer, &set)
              == BR_AI_DRIVE_BRAKE, "backwards-at-speed did not brake");
    CHECK(steer == 99.0f, "brake overwrote the steering with %f",
          (double)steer);
    CHECK(set == BR_AI_CTL_40000, "brake set %08x", set);
    /* -1.0 exactly is not yet braking: the test is `velFwd < -1`. */
    steer = 99.0f; set = 0;
    CHECK(BrAiDriveStep(&st, 1.0f, -1.0f, 30.0f, 0.5f, &steer, &set)
              == BR_AI_DRIVE_FORWARD, "velFwd == -1 braked");

    /* Reverse: the command is the SIGN of the curve, not its negation, so
     * the car countersteers on its way out. */
    memset(&st, 0, sizeof st);
    steer = 0.0f; set = 0;
    CHECK(BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set)
              == BR_AI_DRIVE_REVERSE, "aim behind did not reverse");
    CHECK(steer == 1.0f, "reverse with a positive curve wrote %f",
          (double)steer);
    memset(&st, 0, sizeof st);
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, -0.5f, &steer, &set);
    CHECK(steer == -1.0f, "reverse with a negative curve wrote %f",
          (double)steer);
    CHECK((set & BR_AI_CTL_10000) != 0 && (set & BR_AI_CTL_20000) != 0,
          "reverse set %08x, not both 0x10000 and 0x20000", set);

    /* THE LATCH. Stuck at zero speed pointing forward: the run counter has to
     * pass 30 -- so exactly 31 calls -- before the opposite hold is loaded
     * with 60 and both run counters are cleared. */
    memset(&st, 0, sizeof st);
    for (i = 0; i < BR_AI_RUN_LATCH; i++) {
        steer = 0.0f; set = 0;
        CHECK(BrAiDriveStep(&st, 1.0f, 0.0f, 0.0f, 0.0f, &steer, &set)
                  == BR_AI_DRIVE_FORWARD, "frame %d was not forward", i);
        CHECK(st.cHoldRev == 0, "the latch fired early, on frame %d", i);
        CHECK(st.cFwdRun == i + 1, "cFwdRun is %d on frame %d",
              (int)st.cFwdRun, i);
    }
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, 1.0f, 0.0f, 0.0f, 0.0f, &steer, &set);
    CHECK(st.cHoldRev == BR_AI_HOLD_FRAMES, "the latch loaded %d, not 60",
          (int)st.cHoldRev);
    CHECK(st.cFwdRun == 0 && st.cRevRun == 0,
          "the latch did not clear both run counters");

    /* ...and it RELEASES: 60 frames of forced reverse even though the aim is
     * still ahead, then straight back to forward. */
    for (i = 0; i < BR_AI_HOLD_FRAMES; i++) {
        steer = 0.0f; set = 0;
        CHECK(BrAiDriveStep(&st, 1.0f, 0.0f, 30.0f, 0.0f, &steer, &set)
                  == BR_AI_DRIVE_REVERSE, "the hold released early at %d", i);
        CHECK(st.cHoldRev == BR_AI_HOLD_FRAMES - 1 - i,
              "cHoldRev is %d after %d frames", (int)st.cHoldRev, i + 1);
    }
    steer = 0.0f; set = 0;
    CHECK(BrAiDriveStep(&st, 1.0f, 0.0f, 30.0f, 0.0f, &steer, &set)
              == BR_AI_DRIVE_FORWARD, "the hold did not release");

    /* Speed defeats the latch entirely: a car that is actually moving never
     * accumulates a hold, however long it drives. */
    memset(&st, 0, sizeof st);
    for (i = 0; i < 200; i++) {
        steer = 0.0f; set = 0;
        (void)BrAiDriveStep(&st, 1.0f, 5.0f, 30.0f, 0.0f, &steer, &set);
    }
    CHECK(st.cHoldRev == 0, "a moving car latched a hold");
    CHECK(st.cFwdRun == 200, "cFwdRun = %d after 200 moving frames",
          (int)st.cFwdRun);

    /* The respawn value delays the first latch by 180 frames on top of the
     * 30: 211 calls, not 31. */
    BrAiRecoveryReset(&st);
    for (i = 0; i < 210; i++) {
        steer = 0.0f; set = 0;
        (void)BrAiDriveStep(&st, 1.0f, 0.0f, 0.0f, 0.0f, &steer, &set);
    }
    CHECK(st.cHoldRev == 0, "the respawn offset did not delay the latch");
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, 1.0f, 0.0f, 0.0f, 0.0f, &steer, &set);
    CHECK(st.cHoldRev == BR_AI_HOLD_FRAMES,
          "the latch did not fire on frame 211");

    /* The reverse-run flip window, 150 < cRevRun <= 270. Held above walking
     * pace so the latch cannot interfere. */
    memset(&st, 0, sizeof st);
    st.cRevRun = 100;
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set);
    CHECK(steer == 1.0f, "below the flip window the command was %f",
          (double)steer);
    st.cRevRun = 200;
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set);
    CHECK(steer == -1.0f, "inside the flip window the command was %f",
          (double)steer);
    CHECK(st.cRevRun == 201, "cRevRun is %d inside the window",
          (int)st.cRevRun);
    /* Past 270 it stops flipping and rewinds the counter to 30 instead. */
    st.cRevRun = 300;
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set);
    CHECK(steer == 1.0f, "past the flip window the command was %f",
          (double)steer);
    CHECK(st.cRevRun == BR_AI_RUN_LATCH + 1,
          "past 270 cRevRun became %d, not 30 then incremented",
          (int)st.cRevRun);
    /* 150 and 270 themselves are OUTSIDE their arms -- both tests are `jle`. */
    st.cRevRun = BR_AI_REV_FLIP;
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set);
    CHECK(steer == 1.0f, "cRevRun == 150 flipped, and 0x1005DFE6 is `jle`");
    st.cRevRun = BR_AI_REV_RESET;
    steer = 0.0f; set = 0;
    (void)BrAiDriveStep(&st, -1.0f, 0.0f, 30.0f, 0.5f, &steer, &set);
    CHECK(steer == -1.0f && st.cRevRun == BR_AI_REV_RESET + 1,
          "cRevRun == 270 rewound, and 0x1005DFED is `jle`");
}

static void test_overtake(void)
{
    BrAiRival a[4];
    float lap = 1000.0f;
    float v;

    memset(a, 0, sizeof a);

    /* Nobody there. */
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "an empty field produced an offset");

    /* One rival right on the bumper, level with us: the offset SATURATES at
     * its clamp -- 10 * (1 - 1/90) is 9.9, and the clamp is 5. */
    a[1].fPresent = 1;
    a[1].progress = 1.0f;
    a[1].offset   = 0.0f;
    v = BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap);
    CHECK(v == -BR_AI_OVERTAKE_CLAMP, "a rival on the bumper gave %f, not the "
          "clamp", (double)v);

    /* Exactly at the clamp: d = 45 gives 10 * 0.5 == 5. Both the clamped and
     * the unclamped side of the boundary. */
    a[1].progress = 45.0f;
    v = BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap);
    CHECK(near_(v, -BR_AI_OVERTAKE_CLAMP, 1e-4),
          "d == 45 gave %f, not exactly the clamp", (double)v);
    a[1].progress = 81.0f;
    v = BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap);
    CHECK(near_(v, -1.0f, 1e-4), "d == 81 gave %f, not -1", (double)v);
    CHECK(v > -BR_AI_OVERTAKE_CLAMP, "a distant rival was still clamped");

    /* The offset falls off linearly to nothing at the 90 range. */
    a[1].progress = 89.9f;
    CHECK(fabs((double)BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap)) < 0.02,
          "a rival at the edge of range still pulled %f",
          (double)BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap));
    a[1].progress = 90.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "a rival at exactly 90 was considered");
    a[1].progress = 200.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "a rival out of range was considered");

    /* A rival BEHIND is ignored -- d must be strictly positive. */
    a[1].progress = -10.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "a rival behind was considered");
    a[1].progress = 0.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "a rival exactly level was considered");

    /* The side the offset pushes toward flips with the rival's own line
     * offset: a rival 5 out on one side sends us the other way. */
    a[1].progress = 10.0f;
    a[1].offset   = 5.0f;
    v = BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap);
    CHECK(v > 0.0f, "a rival at +5 pushed us to %f", (double)v);
    a[1].offset = -5.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) < 0.0f,
          "a rival at -5 did not push the other way");
    /* The 3.0 threshold sits between them, and it is the DIFFERENCE that
     * matters, not the rival's absolute position: the same rival at +5 sends
     * the car one way or the other depending on where the car itself is. */
    a[1].offset = 5.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 3.0f, lap) < 0.0f,
          "diff == 2 took the wrong arm");
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 1.0f, lap) > 0.0f,
          "diff == 4 took the wrong arm");
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 2.5f, lap) < 0.0f,
          "diff == 2.5 took the wrong arm");
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 2.0f, lap) > 0.0f,
          "diff == 3 exactly did not take the upper arm");

    /* The NEAREST rival ahead wins, not the first or the last found. */
    a[1].progress = 50.0f; a[1].offset =  5.0f;   /* far,  would push + */
    a[2].fPresent = 1;
    a[2].progress = 10.0f; a[2].offset =  0.0f;   /* near, pushes -     */
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) < 0.0f,
          "the far rival won over the near one");
    a[1].progress =  5.0f;                        /* now the nearer one */
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) > 0.0f,
          "swapping which rival is nearer did not change the answer");

    /* Self is skipped, and so is an empty slot. */
    memset(a, 0, sizeof a);
    a[0].fPresent = 1;
    a[0].progress = 10.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap) == 0.0f,
          "the car considered itself");
    CHECK(BrAiOvertakeOffset(a, 4, 1, 0.0f, 0.0f, lap) != 0.0f,
          "slot 0 was skipped for the wrong car");

    /* THE WRAP, and what it actually is. car+0xFF4 is a CUMULATIVE distance
     * with the lap count already folded in (br_ai.h's BrAiLapLength note:
     * 0x100600A9 adds a lap to it on the rollover), so two cars either side
     * of the start line already have close values and need no wrapping. The
     * two loops fold by a WHOLE lap, into [-lap, +lap], which is what makes
     * LAPPED TRAFFIC visible: a rival a full lap plus ten ahead is treated
     * as ten ahead. It is NOT a fold into half a lap -- 0x1005E09F reads the
     * root node's pts[0].arc, the full lap length, and neither loop halves
     * it. */
    memset(a, 0, sizeof a);
    a[1].fPresent = 1;
    a[1].progress = lap + 60.0f;      /* far enough out not to be clamped */
    v = BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap);
    CHECK(v != 0.0f, "the wrap did not make a lapped rival visible");
    CHECK(near_(v, -10.0f * (1.0f - 60.0f / 90.0f), 1e-4),
          "the wrapped distance came out wrong: %f", (double)v);
    /* Several laps fold too -- the loops repeat, they do not subtract once. */
    a[1].progress = 3.0f * lap + 60.0f;
    CHECK(near_(BrAiOvertakeOffset(a, 4, 0, 0.0f, 0.0f, lap), v, 1e-4),
          "three laps ahead did not fold to the same place as one");
    /* ...and the other way round, the same pair is BEHIND: -lap-10 folds to
     * -10, which fails the "strictly ahead" test. */
    a[1].progress = 0.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, lap + 10.0f, 0.0f, lap) == 0.0f,
          "the wrap turned a lapped rival behind into one ahead");
    /* And it does NOT rescue a car just the other side of the start line
     * when the progress really is near zero: a rival at 5 with the car at
     * lap-5 is 990 BEHIND and stays behind, because the fold is by a whole
     * lap and -990 is already inside [-lap, lap]. That is the original's
     * behaviour and it is only harmless because +0xFF4 is cumulative. */
    a[1].progress = 5.0f;
    CHECK(BrAiOvertakeOffset(a, 4, 0, lap - 5.0f, 0.0f, lap) == 0.0f,
          "the fold is narrower than a whole lap");
}

/* ===================================================================== */
/* A shipped track                                                       */
/* ===================================================================== */

static void test_real_track(const char *pszPath)
{
    BrTrack t;
    BrAiNode n, root;
    BrAiPoint pt, nx;
    uint32_t i, nodes, points;
    float lap;
    int closed = 0;

    if (BrTrackOpen(&t, pszPath) != 0) {
        printf("FAIL %s: BrTrackOpen failed\n", pszPath);
        g_fail++;
        return;
    }

    if (BrAiRoot(&t, &root) != 0) {
        printf("FAIL %s: header +0x70 is not a decodable path node\n", pszPath);
        g_fail++;
        BrTrackClose(&t);
        return;
    }

    lap = BrAiLapLength(&t);
    CHECK(lap > 0.0f, "%s: lap length %f", pszPath, (double)lap);

    n = root;
    nodes = 0;
    points = 0;
    for (;;) {
        CHECK(BrAiPoint_(&n, 0, &pt) == 0, "%s: node %u point 0", pszPath,
              (unsigned)n.off);
        for (i = 0; i < (uint32_t)n.count; i++) {
            float mid;
            CHECK(BrAiPoint_(&n, i, &pt) == 0, "%s: point %u", pszPath,
                  (unsigned)i);
            CHECK(BrAiPoint_(&n, i + 1u, &nx) == 0,
                  "%s: sentinel read at %u", pszPath, (unsigned)i);

            /* The encoding's own identity: the centre is the midpoint of the
             * two edges. Rounding is float32 on coordinates of order 10^3, so
             * 1e-3 is three orders of magnitude of headroom, not a threshold
             * chosen to make the data fit. */
            mid = (pt.left.x + pt.right.x) * 0.5f;
            CHECK(fabs((double)(mid - pt.centre.x)) < 1e-3,
                  "%s: centre.x %f vs midpoint %f", pszPath,
                  (double)pt.centre.x, (double)mid);
            mid = (pt.left.y + pt.right.y) * 0.5f;
            CHECK(fabs((double)(mid - pt.centre.y)) < 1e-3,
                  "%s: centre.y %f vs midpoint %f", pszPath,
                  (double)pt.centre.y, (double)mid);
            mid = (pt.left.z + pt.right.z) * 0.5f;
            CHECK(fabs((double)(mid - pt.centre.z)) < 1e-3,
                  "%s: centre.z %f vs midpoint %f", pszPath,
                  (double)pt.centre.z, (double)mid);

            /* Arc length is "distance still to run": strictly decreasing
             * inside a node, which is what makes a segment length positive
             * and the walk terminate. */
            CHECK(pt.arc > nx.arc, "%s: arc did not fall at node %u point %u "
                  "(%f -> %f)", pszPath, (unsigned)n.off, (unsigned)i,
                  (double)pt.arc, (double)nx.arc);

            /* Nothing in the ring has more distance left to run than the
             * whole lap. Deliberately NOT "the arc never rises across a node
             * join": the join value is stored twice, once as node k's
             * sentinel and once as node k+1's point 0, and the two copies
             * differ in the last float32 bit at three joins in race.trk --
             * the code is right and that assertion would be wrong. desert.trk
             * additionally has a genuine jump at one join, so the ring is not
             * a single unbranched chain of distances either. */
            CHECK(pt.arc <= lap, "%s: point arc %f exceeds the lap %f",
                  pszPath, (double)pt.arc, (double)lap);

            /* The path frame on real geometry. THE THIRD CHECK IS THE ONE
             * THAT MATTERS: br_ai.h claims `up` comes out +Z on every point
             * of both shipped tracks, and the whole orientation argument --
             * that the AI's lateral axis points at the +0x00 edge, so the
             * half width is positive and the correction law is stabilising
             * rather than divergent -- rests on it. It is measured here
             * rather than asserted in prose. */
            {
                BrAiPathFrame frm;
                float hw, lim;

                BrAiPathFrameAt(&frm, &pt, &nx);
                CHECK(fabs((double)(BrVec3Length(&frm.lateral) - 1.0f)) < 1e-4,
                      "%s: lateral is not unit at node %u point %u", pszPath,
                      (unsigned)n.off, (unsigned)i);
                CHECK(fabs((double)BrVec3Dot(&frm.lateral, &frm.tangent))
                          < 1e-3,
                      "%s: lateral is not perpendicular to the tangent at "
                      "node %u point %u", pszPath, (unsigned)n.off,
                      (unsigned)i);
                CHECK(frm.up.z > 0.0f,
                      "%s: up came out %f at node %u point %u", pszPath,
                      (double)frm.up.z, (unsigned)n.off, (unsigned)i);

                hw = BrAiHalfWidth(&frm, &pt);
                CHECK(hw > 0.0f, "%s: half width %f at node %u point %u",
                      pszPath, (double)hw, (unsigned)n.off, (unsigned)i);
                lim = BrAiCorridorLimit(hw);
                CHECK(lim > 0.0f && lim < hw,
                      "%s: corridor limit %f for half width %f", pszPath,
                      (double)lim, (double)hw);

                /* A car parked on the centre line, stationary, is inside the
                 * corridor and steers nothing -- on the real track, not on a
                 * synthetic one. */
                {
                    BrVec3 vel;
                    BrAiSteerIn in;
                    BrAiSteerOut out;

                    vel.x = vel.y = vel.z = 0.0f;
                    memset(&in, 0, sizeof in);
                    in.offset = BrAiLineOffset(&frm, &pt.centre, &vel,
                                               &pt.centre);
                    in.limit  = lim;
                    in.scale  = 1.0f;
                    BrAiSteerCompute(&out, &in);
                    CHECK(out.value == 0.0f,
                          "%s: a car on the racing line steered %f at node %u "
                          "point %u", pszPath, (double)out.value,
                          (unsigned)n.off, (unsigned)i);
                }
            }
            points++;
        }

        if (n.offNext == root.off) {
            /* The ring closes, and the last sentinel is exactly zero -- the
             * lap is fully accounted for. */
            CHECK(BrAiPoint_(&n, (uint32_t)n.count, &pt) == 0,
                  "%s: last sentinel", pszPath);
            CHECK(pt.arc == 0.0f, "%s: last sentinel arc %f", pszPath,
                  (double)pt.arc);
            closed = 1;
            break;
        }
        if (BrAiNodeAt(&t, n.offNext, &n) != 0)
            break;
        if (++nodes > 4096u)
            break;
    }
    CHECK(closed, "%s: the path ring did not close on the root", pszPath);
    CHECK(points > 0, "%s: no path points", pszPath);

    /* Behaviour on real data: the walk consumes at least the distance asked
     * for, always moves, and never goes backwards as the distance grows. */
    {
        float d;
        float prevArc = lap + 1.0f;
        for (d = 0.0f; d <= BR_AI_LOOKAHEAD_MAX; d += 7.5f) {
            uint32_t idx = 0;
            n = root;
            CHECK(BrAiAdvanceTarget(&n, &idx, d) == 0,
                  "%s: advance(%f) failed", pszPath, (double)d);
            CHECK(!(n.off == root.off && idx == 0),
                  "%s: advance(%f) did not move", pszPath, (double)d);
            CHECK(BrAiPoint_(&n, idx, &pt) == 0, "%s: target point", pszPath);
            CHECK(lap - pt.arc >= d, "%s: advance(%f) consumed only %f",
                  pszPath, (double)d, (double)(lap - pt.arc));
            CHECK(pt.arc <= prevArc,
                  "%s: a longer lookahead picked an earlier target", pszPath);
            prevArc = pt.arc;
        }
    }

    /* The gate table cannot claim more gates than the header has room for:
     * +0x98 + 10*0x14 is exactly +0x160, where the count itself lives. */
    CHECK(BrTrackHdrU32(&t, BR_TRK_H_CGATES) <= BR_AI_GATE_MAX,
          "%s: gate count %u exceeds the %u the header can hold", pszPath,
          (unsigned)BrTrackHdrU32(&t, BR_TRK_H_CGATES),
          (unsigned)BR_AI_GATE_MAX);

    printf("  %s: %u path points, lap %.1f\n", pszPath, (unsigned)points,
           (double)lap);
    BrTrackClose(&t);
}

int main(void)
{
    test_decode();
    test_advance();
    test_lookahead();
    test_corridor();
    test_aim();

    test_path_frame();
    test_line_offset();
    test_corridor_limit();
    test_correction();
    test_response();
    test_steer_input();
    test_steer_compute();
    test_steer_hold();
    test_throttle();
    test_recovery();
    test_overtake();

    /* Report the half that needs no disc data BEFORE the skip gate, or a
     * missing asset would swallow a real failure and exit 0. */
    if (g_fail != 0) {
        printf("test_br_ai: %d failure(s) before the track tests\n", g_fail);
        return 1;
    }

    BR_REQUIRE_TESTDATA("testdata/tracks/race.trk", "test_br_ai");
    test_real_track("testdata/tracks/race.trk");
    test_real_track("testdata/tracks/desert.trk");

    if (g_fail != 0) {
        printf("test_br_ai: %d failure(s)\n", g_fail);
        return 1;
    }
    /* Report a COUNT, not just "OK". tools/regress.sh treats a bare OK as
     * unparseable on purpose: a suite that prints OK without running anything
     * is indistinguishable from one that passed, and this suite SKIPs its
     * track-backed half when the disc assets are absent. The count makes the
     * difference visible. */
    printf("test_br_ai: %d checks, 0 failures\n", g_checks);
    return 0;
}
