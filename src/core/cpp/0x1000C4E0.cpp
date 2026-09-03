/* @implements 0x1000C4E0 glide BrRippleApply_1000C4E0
 * @cpp_kind method
 * @cpp_symbol ?Apply@Rip0C4E0@@QAEHPBMH@Z
 *
 * Thiscall, two stack args (`ret 8`), 1246 B. Pushes a ripple through
 * every vertex of the display lists hanging off +0x29C4: classify the
 * direction vector's bearing into one of eight octants, take that
 * octant's screen-space clamp box and per-axis step, then walk ten
 * command lists at each of ten table strides, and for every G_VTX block
 * displace the vertices that land inside the box.
 *
 * The bearing is `atan2 * (180/pi)` rounded with `_ftol`, so the octant
 * bounds below are DEGREES; the clamp bounds are signed 16-bit screen
 * coordinates held in int slots and compared as `(short)`.
 *
 * The phase float is computed once and stays on the x87 stack across
 * every `_ftol` call in the loop (`fadd st(1)`), which is why it is
 * written as the expression `(float)phase` inside the loop rather than a
 * named float local -- a local would get a stack slot and `fadd [slot]`.
 *
 * Shapes already recovered, and worth keeping if this is picked up again:
 * the scaled magnitude is the PARAMETER, updated in place (`shl edi,2`,
 * `neg edi`) rather than a separate local; the bearing gate is
 * `pDir[0] <= 1.25f`, not `>`; the inner loop's two early-outs are
 * `continue`, not `break`, and index 9 is skipped by one of them; and the
 * opcode byte is a named local, because writing `(w0 >> 24) == K` twice
 * makes VC5 emit two `and`+`cmp` pairs instead of one shift and two
 * compares.
 *
 * PARKED at -30 bytes, register-blind gap 12 extra / 21 missing on 1246
 * bytes. Three causes, none of them source-reachable so far:
 *  - the three step chains shift on the 16-bit register (`mov ax,di /
 *    sar ax,1`) and then store the whole `eax` into an int slot whose
 *    upper half is stale. Every spelling tried -- `(short)mag >> 1`, a
 *    `short` local shifted in place, a shared `short` temp across the
 *    arms -- gives `movsx` plus a 32-bit shift instead (782 / 796 / 782).
 *  - VC5 tail-merges the two arms of the FIRST of the three displacement
 *    blocks (the other two stay duplicated, as all three are in the
 *    original). Same emitter-level cross-jumping as the entry in
 *    docs/VC5-IDIOMS.md; the arms already carry the whole expression.
 *  - the variable homes are swapped: the original keeps the vertical step
 *    in `bp` and spills the magnitude to the argument slot, and reloads
 *    `this` from its own slot every inner iteration; ours does the
 *    reverse and hoists `this` into a register.
 * Flags are not the lever: /O2 /Oy- is 1052 diffs, /O2 /Op 794 with a
 * WORSE register-blind gap (25/28) despite matching the size to +2, and
 * /Ox and /O2 /Ob0 are identical to /O2.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Rip0C4E0 {
public:
    char           pad0000[0x29AC];
    unsigned char  b29ac;
    unsigned char  b29ad;
    unsigned char  b29ae;
    char           pad29af[0x29C4 - 0x29AF];
    int          **pp29c4;          /* +0x29C4 */
    short          w29c8[8];        /* +0x29C8 */
    short          w29d8;           /* +0x29D8 -- read as a byte, written as a word */
    char           pad29da[0x2A70 - 0x29DA];
    int            a2a70[8];        /* +0x2A70 */
    int            a2a90[8];        /* +0x2A90 */

    int Apply(const float *pDir, int mag);
};

typedef char chk_ac[(unsigned)&((Rip0C4E0 *)0)->b29ac == 0x29AC ? 1 : -1];
typedef char chk_c4[(unsigned)&((Rip0C4E0 *)0)->pp29c4 == 0x29C4 ? 1 : -1];
typedef char chk_c8[(unsigned)&((Rip0C4E0 *)0)->w29c8 == 0x29C8 ? 1 : -1];
typedef char chk_d8[(unsigned)&((Rip0C4E0 *)0)->w29d8 == 0x29D8 ? 1 : -1];
typedef char chk_70[(unsigned)&((Rip0C4E0 *)0)->a2a70 == 0x2A70 ? 1 : -1];
typedef char chk_90[(unsigned)&((Rip0C4E0 *)0)->a2a90 == 0x2A90 ? 1 : -1];

extern "C" {
short g_brABE44;                        /* 0x100ABE44 -- the per-octant cap */
float BrAtan2_10034E30(float y, float x);
void  BrFn1005A4E0(int a, int b, int c);
void  BrFn1006E0A0(int oct, int *pA, int *pB);
}

int Rip0C4E0::Apply(const float *pDir, int mag)
{
    int   deg;
    int   xlo, xhi, ylo, yhi;
    int   oct;
    int   sx;
    short sy;
    int   phase;
    int   t;
    int   iOuter, iInner;
    int  *pCmd;
    float *pVtx;
    int   n;

    deg = (int)(BrAtan2_10034E30(pDir[0], pDir[1]) * 57.2957763671875f);

    if (deg >= 0x14 && deg < 0x154) {
        if (deg < 0x32) {
            xlo = 0x80;   xhi = 0x3FFF;  yhi = 0x3FFF;  ylo = 0x40;   oct = 1;
        } else if (deg < 0x82) {
            xlo = -16383; xhi = 0x3FFF;  yhi = 0x3FFF;  ylo = 0x40;   oct = 2;
        } else if (deg < 0xA0) {
            xhi = -128;   xlo = -16383;  yhi = 0x3FFF;  ylo = 0x40;   oct = 3;
        } else if (deg < 0xC8) {
            xhi = -255;   xlo = -16383;  yhi = 0x3FFF;  ylo = -16383; oct = 4;
        } else if (deg < 0xE6) {
            xhi = -128;   xlo = -16383;  yhi = -64;     ylo = -16383; oct = 5;
        } else if (deg < 0x136) {
            xhi = 0x3FFF; yhi = -64;     xlo = -16383;  ylo = -16383; oct = 6;
        } else {
            xhi = 0x3FFF; xlo = 0x80;    yhi = -64;     ylo = -16383; oct = 7;
        }
    } else {
        xlo = 0xFF;   xhi = 0x3FFF;  yhi = 0x3FFF;  ylo = -16383; oct = 0;
    }

    mag = mag * 4;

    BrFn1005A4E0(b29ac, b29ad, b29ae);
    BrFn1006E0A0(oct, a2a70, a2a90);

    if (w29c8[oct] >= g_brABE44)
        return 1;

    w29c8[oct] = (short)(mag + w29c8[oct]);

    w29d8 = (short)(((((unsigned char)w29d8 - 3) & 7)) - 4);
    phase = (short)w29d8;
    if (phase < 0)
        phase = phase + 1;

    if (pDir[1] > 1.0f) {
        sx = mag;
    } else if (pDir[1] > 0.0f) {
        sx = (short)mag >> 1;
    } else if (pDir[1] < -1.0f) {
        sx = -mag;
    } else {
        sx = -((short)mag >> 1);
    }

    if (pDir[2] > 1.0f) {
        sy = (short)((short)mag >> 2);
    } else if (pDir[2] > 0.0f) {
        sy = (short)((short)mag >> 3);
    } else if (pDir[2] < -1.0f) {
        sy = (short)(-((short)mag >> 2));
    } else {
        sy = (short)(-((short)mag >> 3));
    }

    if (pDir[0] <= 1.25f) {
        if (pDir[0] > 0.0f) {
            mag = (short)mag >> 1;
        } else if (pDir[0] < -1.25f) {
            mag = -mag;
            sy = (short)(sy * 2);
        } else {
            mag = -mag;
        }
    }

    for (iOuter = 0x2006; iOuter < 0x2024; iOuter += 0xA) {
        for (iInner = 0; iInner < 0xA; iInner++) {
            if (iInner == 9)
                continue;
            pCmd = pp29c4[iInner + iOuter];
            if (pCmd == 0)
                continue;

            for (;;) {
                unsigned int w0 = (unsigned int)pCmd[0];
                unsigned int op = w0 >> 24;

                if (op == 4) {
                    pVtx = (float *)pCmd[1];
                    pCmd += 2;
                    n = (int)((w0 >> 10) & 0x3F);
                    if (n != 0) {
                        do {
                            int ix = (int)(pVtx[0] + (float)phase);

                            if ((short)ix > (short)xlo
                                && (short)ix < (short)xhi) {
                                int iy = (int)(pVtx[1] + (float)phase);

                                if ((short)iy > (short)ylo
                                    && (short)iy < (short)yhi) {
                                    int iz = (int)(pVtx[2] + (float)phase);

                                    if ((short)iz > -48 && (short)iz < 224) {
                                        if ((iy & 0x80) != 0)
                                            t = (4 - (iy & 0xF)) * (short)mag >> 5;
                                        else
                                            t = ((iy & 0xF) - 12) * (short)mag >> 5;
                                        pVtx[0] = (float)t + pVtx[0];

                                        if ((ix & 0x80) != 0)
                                            t = (4 - (ix & 0xF)) * (short)sx >> 5;
                                        else
                                            t = ((ix & 0xF) - 12) * (short)sx >> 5;
                                        pVtx[1] = (float)t + pVtx[1];

                                        ix = ix + iy;
                                        if ((ix & 0x80) != 0)
                                            t = (8 - (ix & 0xF)) * (short)sy >> 6;
                                        else
                                            t = ((ix & 0xF) - 8) * (short)sy >> 6;
                                        pVtx[2] = (float)t + pVtx[2];
                                    }
                                }
                            }
                            pVtx += 8;
                        } while (--n != 0);
                    }
                } else if (op == 0xB8) {
                    break;
                } else {
                    pCmd += 2;
                }
            }
        }
    }

    return 1;
}
