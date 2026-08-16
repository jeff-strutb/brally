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

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
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
    printf("test_br_ai: OK\n");
    return 0;
}
