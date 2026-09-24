/* br_dlproject.c -- 0x10022070, the display-list vertex projection.
 *
 * Glide matching arm only.  The port's projection is the static
 * br_dl_project in br_dl.c, which works on the port's BrDl/BrDlVtx model;
 * this is the original's own shape: an output vertex, the transformed input,
 * and the colour in three floats.
 *
 * Built /O2 /Op (the sweep's O2p variant).  The quarter-pixel snap is inline
 * assembly -- `fld t; fistp [int]` -- which is why the function has an EBP
 * frame and why every intermediate goes through the one float temp (packed
 * by VC5 into the dead `pIn` parameter slot at [ebp+0xC]). */
#ifdef BR_MATCHING_BUILD

typedef struct BrDlProjOut {
    float x;            /* +0x00 */
    float y;            /* +0x04 */
    float z;            /* +0x08 */
    float r;            /* +0x0C */
    float g;            /* +0x10 */
    float b;            /* +0x14 */
    float f18;
    float f1c;
    float oow;          /* +0x20  1/w */
} BrDlProjOut;

typedef struct BrDlProjIn {
    float f00;
    float cx;           /* +0x04 */
    float cy;           /* +0x08 */
    float f0c;
    float f10;
    float f14;
    float cw;           /* +0x18 */
} BrDlProjIn;

extern float DAT_105ccd48;      /* viewport scale x */
extern float DAT_105cd9f8;      /* viewport translate x */
extern float DAT_105ccfdc;      /* viewport scale y */
extern float DAT_105cd9fc;      /* viewport translate y */
extern int   DAT_105ce310;      /* fistp scratch */

/* WHAT IT DOES: turns a transformed vertex into a screen position: divides
 * through by depth to get perspective, applies the viewport scale and
 * offset, stores the vertex's colour, and snaps the result to the nearest
 * quarter of a pixel -- the resolution the hardware rasteriser works at.
 * The snap rounds to nearest (the FPU's mode), not toward zero. */
/* @implements 0x10022070 glide br_dl_project */
void br_dl_project(BrDlProjOut *pOut, const BrDlProjIn *pIn,
                   float r, float g, float b)
{
    float w, t;

    w = 1.0f / pIn->cw;
    /* A dword copy: the y term then re-reads 1/w from the vertex, where a
     * float assignment lets VC5 reuse the temp. */
    *(int *)&pOut->oow = *(int *)&w;
    /* The (float) casts keep scale-then-1/w as the multiply order. */
    pOut->x = (float)(pIn->cx * DAT_105ccd48) * w + DAT_105cd9f8;
    pOut->y = (float)(pIn->cy * DAT_105ccfdc) * pOut->oow + DAT_105cd9fc;
    pOut->r = r;
    pOut->g = g;
    pOut->b = b;

    t = pOut->x * 4.0f;
    __asm fld t
    __asm fistp DAT_105ce310
    t = (float)DAT_105ce310;
    pOut->x = t * 0.25f;

    t = pOut->y * 4.0f;
    __asm fld t
    __asm fistp DAT_105ce310
    t = (float)DAT_105ce310;
    pOut->y = t * 0.25f;
}

#endif /* BR_MATCHING_BUILD */
