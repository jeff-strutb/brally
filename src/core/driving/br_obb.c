/* br_obb.c -- driving: the oriented-box overlap test the car-versus-car
 * collision uses.
 *
 * RESPONSIBILITY: driving/ -- the car simulation.
 *
 * One function, 0x10068900: the separating-axis test between two boxes,
 * fifteen axes (three faces of each box and the nine edge cross products),
 * with the second box's rotation and offset expressed in the first box's
 * frame.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077a78;                          /* 0.0f */

/* WHAT IT DOES: answers whether two oriented boxes overlap.  m is the 3x3
 * rotation taking the second box into the first's frame, t the offset
 * between their centres in that frame, a and b the half-extents.  Every one
 * of the fifteen separating axes is projected: the offset's projection must
 * not exceed the sum of both boxes' projected half-extents.  All fifteen
 * tests are evaluated (no early out) and ANDed; the result is 1 for
 * overlap, 0 for a separating axis. */
/* T2 residue (2026-09-13): 1671/1653 B, 641/642 insns, regnorm 98+99, 27
 * regions -- one class repeated fifteen times: in every projected-extent
 * sum the original loads the local |m| slot and multiplies by the parameter
 * element (`fld [esp+S]; fmul [edi+4]`), this build the reverse; the
 * original also keeps |m6..m8| on the x87 stack and dups them (`fld st(4)`)
 * where this build reloads.  Dead: pointer copies of the parameters, an
 * array for the |m| values, all-ternary / all-if-else abs forms (they turn
 * the |m1..m8| copies into integer moves), product operand order (mul
 * operands canonicalise).  Matched: `ok = 1; ok &= ...` (the `and eax,1`
 * on the first test and the `and eax,ecx` at the return), the |m0| arm
 * polarity, the |t| and cross-term abs forms, the fifteen test order.
 * @t4-pass 0x10068900 w3 2026-09-13 probes 6 bytes 18 insns -1 regions 27 rows 98+99 census no
 * T3 verdict (2026-09-15): the recorded variant is WRONG.  report.csv picked
 * O2p (raw-byte-min, recomp 2000 B, 148+210 = 358 reg-blind rows).  The right
 * variant is plain O2 (frameless like the original, 1680 B, 215 rows, insn gap
 * 8) -- see the variant-selection note in resume-state 2026-09-15b.  Under O2
 * the residue is x87 scheduling (like carcol/chasestep); A2 215 vs 16.1 is a
 * distance wall, not missing code.  The A4 86 B uncompared is a resync
 * artifact.  Parks as T2; not a transcription target.
 */
/* @t4-pass 0x10068900 1 2026-09-20 probes 12 bytes 1671 insns 641 regions 27 rows 215 census yes  (SAT-sum product operand flip mem*local->local*mem: canonicalised, no move; x87 schedule) */
/* @t4-pass 0x10068900 2 2026-09-20 probes 10 bytes 1671 insns 641 regions 27 rows 215 census no   (baseline reconfirm; matches the w3 dead list -- ptr copies, |m| array, abs forms, dup vs reload) */
/* @t3 0x10068900 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1671/1653 insns 641/642 rows 108+107 regions 27 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is x87 scheduling only: in each of the fifteen projected-extent sums
 * the original multiplies the parameter element straight from memory and keeps
 * |m6..m8| on the x87 stack (fld st(4)) where this build loads-then-multiplies
 * and reloads (per the dossier above; sibling of carcol/chasestep).  A5 oracle
 * EQUIVALENT is the completeness proof (rule 12).  Do not reopen before the
 * end-grind. */
/* @implements 0x10068900 glide BrObbOverlap */
int BrObbOverlap(const float *m, const float *t, const float *a, const float *b)
{
    float am0, am1, am2, am3, am4, am5, am6, am7, am8;
    float at0, at1, at2;
    float c;
    int   ok;

    if (m[0] < _DAT_10077a78)
        am0 = -m[0];
    else
        am0 = m[0];
    am1 = m[1];
    if (m[1] < _DAT_10077a78)
        am1 = -am1;
    am2 = m[2];
    if (m[2] < _DAT_10077a78)
        am2 = -am2;
    am3 = m[3];
    if (m[3] < _DAT_10077a78)
        am3 = -am3;
    am4 = m[4];
    if (m[4] < _DAT_10077a78)
        am4 = -am4;
    am5 = m[5];
    if (m[5] < _DAT_10077a78)
        am5 = -am5;
    am6 = m[6];
    if (m[6] < _DAT_10077a78)
        am6 = -am6;
    am7 = m[7];
    if (m[7] < _DAT_10077a78)
        am7 = -am7;
    am8 = m[8];
    if (m[8] < _DAT_10077a78)
        am8 = -am8;

    at0 = t[0];
    if (t[0] < _DAT_10077a78)
        at0 = -at0;
    ok = 1;
    ok &= at0 <= b[0] * am0 + am2 * b[2] + am1 * b[1] + a[0];

    c = t[1] * m[3] + m[0] * t[0] + m[6] * t[2];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= a[0] * am0 + am6 * a[2] + am3 * a[1] + b[0];

    at1 = t[1];
    if (t[1] < _DAT_10077a78)
        at1 = -at1;
    ok &= at1 <= b[0] * am3 + am5 * b[2] + am4 * b[1] + a[1];

    at2 = t[2];
    if (t[2] < _DAT_10077a78)
        at2 = -at2;
    ok &= at2 <= b[0] * am6 + am8 * b[2] + am7 * b[1] + a[2];

    c = t[1] * m[4] + m[7] * t[2] + t[0] * m[1];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= a[0] * am1 + am7 * a[2] + am4 * a[1] + b[1];

    c = t[1] * m[5] + m[8] * t[2] + t[0] * m[2];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= a[0] * am2 + am8 * a[2] + am5 * a[1] + b[2];

    c = t[2] * m[3] - m[6] * t[1];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= am1 * b[2] + am3 * a[2] + am6 * a[1] + am2 * b[1];

    c = t[2] * m[4] - m[7] * t[1];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am2 + am0 * b[2] + am4 * a[2] + am7 * a[1];

    c = t[2] * m[5] - m[8] * t[1];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am1 + am5 * a[2] + am8 * a[1] + am0 * b[1];

    c = t[0] * m[6] - m[0] * t[2];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= a[0] * am6 + am4 * b[2] + am0 * a[2] + am5 * b[1];

    c = t[0] * m[7] - t[2] * m[1];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am5 + a[0] * am7 + am3 * b[2] + am1 * a[2];

    c = t[0] * m[8] - t[2] * m[2];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am4 + a[0] * am8 + am2 * a[2] + am3 * b[1];

    c = m[0] * t[1] - t[0] * m[3];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= a[0] * am3 + am7 * b[2] + am0 * a[1] + am8 * b[1];

    c = t[1] * m[1] - t[0] * m[4];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am8 + a[0] * am4 + am6 * b[2] + am1 * a[1];

    c = t[1] * m[2] - t[0] * m[5];
    if (c < _DAT_10077a78)
        c = -c;
    ok &= c <= b[0] * am7 + a[0] * am5 + am2 * a[1] + am6 * b[1];

    return ok;
}

#endif /* BR_MATCHING_BUILD */
