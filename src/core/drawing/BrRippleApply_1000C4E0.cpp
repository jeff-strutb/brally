/* WHAT IT DOES: pushes a ripple through the car's display lists.  It turns
 * the given direction into a compass bearing, picks one of eight octants and
 * that octant's screen-space box, bumps the octant's counter (and stops if it
 * is already at the cap), then walks thirty of the car's command lists and
 * nudges every vertex inside the box by a small amount that depends on where
 * the vertex sits and on the ripple strength, per axis. */
/* @implements 0x1000C4E0 glide BrRippleApply_1000C4E0
 * @cpp_kind method
 * @cpp_symbol ?Apply@Rip0C4E0@@QAEXPBMF@Z
 *
 * Thiscall, two stack args (`ret 8`), 1246 B, no return value (the original
 * never sets eax).  Hand-transcribed from the bytes.  The one caller
 * (0x10062454) passes `this + 0x350` as the direction and builds the
 * strength with `movzx ax, al` -- a SHORT parameter.
 *
 * Shapes the bytes fix:
 *  - the strength is the short parameter, scaled in place (`mag <<= 2`) and
 *    kept in edi; three short steps come off it (sx in a slot, sy in bp, and
 *    sz, which reuses the parameter's own stack slot once mag is dead).
 *    Short locals held in registers spill as whole dwords, so the stale
 *    upper halves in the original are not a bug.
 *  - the thirty lists are `0x2006 + k*10 + i` for k < 3, i < 10 (i == 9
 *    skipped): the step-10 counter is VC5's reduction of `k*10`, and the
 *    second-order `(base + i) * 4` is left alone.  Written as a step-10 loop
 *    variable instead, VC5 reduces the whole index to a byte offset.
 *  - the command walk tests `op != 4` first; the vertex pointer is fetched
 *    between two single `pCmd++` steps; the vertex count is `while (n--)`.
 *  - byte-exact under /O2 /Gi (the lane's second variant); under plain /O2
 *    two commutative adds come out with their operands swapped.
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

    void Apply(const float *pDir, short mag);
};

typedef char chk_ac[(unsigned)&((Rip0C4E0 *)0)->b29ac == 0x29AC ? 1 : -1];
typedef char chk_c4[(unsigned)&((Rip0C4E0 *)0)->pp29c4 == 0x29C4 ? 1 : -1];
typedef char chk_c8[(unsigned)&((Rip0C4E0 *)0)->w29c8 == 0x29C8 ? 1 : -1];
typedef char chk_d8[(unsigned)&((Rip0C4E0 *)0)->w29d8 == 0x29D8 ? 1 : -1];
typedef char chk_70[(unsigned)&((Rip0C4E0 *)0)->a2a70 == 0x2A70 ? 1 : -1];
typedef char chk_90[(unsigned)&((Rip0C4E0 *)0)->a2a90 == 0x2A90 ? 1 : -1];

extern "C" {
short g_ABE44;                        /* 0x100ABE44 -- the per-octant cap */
float BrAtan2_10034E30(float y, float x);
void  BrFn1005A4E0(int a, int b, int c);
void  BrFn1006E0A0(int oct, int *pA, int *pB);
}

void Rip0C4E0::Apply(const float *pDir, short mag)
{
    int   deg;
    int   xlo, xhi, ylo, yhi;
    int   oct;
    short sx, sy;
    short sz;
    int   phase;
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

    mag <<= 2;

    BrFn1005A4E0(b29ac, b29ad, b29ae);
    BrFn1006E0A0(oct, a2a70, a2a90);

    if (w29c8[oct] >= g_ABE44)
        return;

    w29c8[oct] = (short)(mag + w29c8[oct]);

    w29d8 = (short)(((((unsigned char)w29d8 - 3) & 7)) - 4);
    phase = w29d8;
    if (phase < 0)
        phase = phase + 1;

    if (pDir[1] > 1.0f)
        sx = mag;
    else if (pDir[1] > 0.0f)
        sx = (short)(mag >> 1);
    else if (pDir[1] < -1.0f)
        sx = (short)-mag;
    else
        sx = (short)-(mag >> 1);

    if (pDir[2] > 1.0f)
        sy = (short)(mag >> 2);
    else if (pDir[2] > 0.0f)
        sy = (short)(mag >> 3);
    else if (pDir[2] < -1.0f)
        sy = (short)-(mag >> 2);
    else
        sy = (short)-(mag >> 3);

    if (pDir[0] > 1.25f) {
        sz = mag;
    } else if (pDir[0] > 0.0f) {
        sz = (short)(mag >> 1);
    } else if (pDir[0] < -1.25f) {
        sz = (short)-mag;
        sy <<= 1;
    } else {
        sz = (short)-mag;
    }

    for (iOuter = 0; iOuter < 3; iOuter++) {
        for (iInner = 0; iInner < 0xA; iInner++) {
            if (iInner == 9)
                continue;
            pCmd = pp29c4[0x2006 + iOuter * 10 + iInner];
            if (pCmd == 0)
                continue;

            for (;;) {
                unsigned int w0 = (unsigned int)pCmd[0];
                unsigned int op = w0 >> 24;

                if (op != 4) {
                    if (op == 0xB8)
                        break;
                    pCmd += 2;
                } else {
                    pCmd++;
                    pVtx = (float *)*pCmd;
                    n = (int)((w0 >> 10) & 0x3F);
                    pCmd++;
                    while (n--) {
                        int ix = (int)(pVtx[0] + (float)phase);

                        if ((short)ix > (short)xlo && (short)ix < (short)xhi) {
                            int iy = (int)(pVtx[1] + (float)phase);

                            if ((short)iy > (short)ylo && (short)iy < (short)yhi) {
                                int iz = (int)(pVtx[2] + (float)phase);

                                if ((short)iz > -48 && (short)iz < 224) {
                                    if ((iy & 0x80) != 0)
                                        pVtx[0] = (float)((4 - (iy & 0xF)) * sz >> 5) + pVtx[0];
                                    else
                                        pVtx[0] = (float)(((iy & 0xF) - 12) * sz >> 5) + pVtx[0];

                                    if ((ix & 0x80) != 0)
                                        pVtx[1] = (float)((4 - (ix & 0xF)) * sx >> 5) + pVtx[1];
                                    else
                                        pVtx[1] = (float)(((ix & 0xF) - 12) * sx >> 5) + pVtx[1];

                                    ix = ix + iy;
                                    if ((ix & 0x80) != 0)
                                        pVtx[2] = (float)((8 - (ix & 0xF)) * sy >> 6) + pVtx[2];
                                    else
                                        pVtx[2] = (float)(((ix & 0xF) - 8) * sy >> 6) + pVtx[2];
                                }
                            }
                        }
                        pVtx += 8;
                    }
                }
            }
        }
    }
}
