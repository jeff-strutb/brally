/* Glide match for the sprite blit dispatcher — 0x10001320
 *
 * src/core/menus/br_uispr.c carries a `BrUiSprClip()` tagged at this address,
 * but that is only the geometry HALF of the original: the port factored the
 * clip out into a predicate returning w/h through `int32_t *` out-parameters,
 * and left the two blit calls to its caller.  The original is one function --
 * clip, then compute both surface pointers, then dispatch to the keyed blit
 * (0x10001440) or the plain one (0x100013F0).  `tools/claimcheck.py` flags it
 * as "orig calls 2, port 0"; that flag was right.
 *
 * Signature, read off the frame (`sub esp,8` + four saves, args from
 * [esp+0x1c]): (pDst, x, y, pSrc, pRect, flags), void.
 *
 * Surface layout: 16-bit pixels at +0x00, width +0x04, height +0x08,
 * colour key (u16) at +0x0C.  Both strides reach the blits in BYTES
 * (`lea esi,[ecx+ecx]`, `add eax,eax`).
 *
 * Faithfulness points, all from the bytes:
 *  - w and h are rejected when NEGATIVE, not when zero (`sub ebx,ecx / js`),
 *    so a degenerate rect still reaches the blit.
 *  - both clips are UNSIGNED (`jb`) and ONE-SIDED: only the right and bottom
 *    edges move, nothing clamps a negative x or y.  Reproduced, not fixed.
 *  - there is no NULL check on any pointer.
 *  - the key reaches the keyed blit through the SHORT-PUSH idiom
 *    (`mov cx,[ecx+0xc]; push ecx`, the upper half left holding the source
 *    pointer), which is what a `unsigned short` parameter produces.
 *
 * PARKED T3a, 2026-09-03.  SIZE AND INSTRUCTION COUNT ARE EXACT (206 B / 85
 * insns) and the register-blind gap is 0; ONE divergence region, at +0x1F:
 *
 *   original   1f mov eax,[eax+0xc]   <- rect[3] reuses the DEAD rect pointer
 *              22 sub eax,ecx            register, so h starts life in eax
 *              28 js  return
 *              36 mov ebp,eax         <- h moved to ebp only after the test
 *              40 mov [esp+0x2c],ebp  <- and homed inside the FIRST clip
 *   ours       1f mov ebp,[eax+0xc]   <- h goes straight to ebp
 *              28 mov [esp+0x2c],ebp  <- and is homed BEFORE the js
 *
 * Same instructions, same slots ([esp+0x10] y0, [esp+0x14] x0, [esp+0x2c] h
 * in the dead pRect home): only which register receives rect[3] and where
 * the home store sinks to.  The 76 masked bytes are the naming rotation that
 * follows.
 *
 * DEAD PROBES -- all give the identical 206/85/76 output, do not re-run:
 *   h spelling: `h = pRect[3] - y0` after `y0 = pRect[1]`; `h = pRect[3] -
 *     pRect[1]` with y0 assigned after; split `h = pRect[3]; h -= y0;`
 *   declaration order of the locals (w,h,x0,y0 vs x0,y0,w,h; pitches and
 *     pointers before or after)
 *   `unsigned x, y` parameters with the clip casts dropped
 *   hoisting `dw = pDst->w` / `dh = pDst->h` into locals used by the clip,
 *     the pitch and the offset
 * What DID matter, and is the reason this is size-exact: the two surface
 * pointers and both pitches must be computed ONCE, before the flag test.
 * Spelling them inline in each call arm duplicates them (+33 B, reggap 18+8).
 */
#ifdef BR_MATCHING_BUILD

typedef struct SpSurf {
    unsigned short *p;              /* +0x00 */
    int             w;              /* +0x04 */
    int             h;              /* +0x08 */
    unsigned short  key;            /* +0x0C */
} SpSurf;

void FUN_100013f0(unsigned short *pDst, int dstPitch, int w, int h,
                  unsigned short *pSrc, int srcPitch);
void FUN_10001440(unsigned short *pDst, int dstPitch, int w, int h,
                  unsigned short *pSrc, int srcPitch, unsigned short key);

/* WHAT IT DOES: copy a rectangle of pixels from one off-screen picture to
 * another at a given position. The blitter behind the front end's sprites. */
/* @implements 0x10001320 glide BrUiSprBlit */
void BrUiSprBlit(SpSurf *pDst, int x, int y, SpSurf *pSrc,
                 const int *pRect, int flags)
{
    int x0, y0, w, h;
    int dstPitch, srcPitch;
    unsigned short *pd, *ps;

    x0 = pRect[0];                              /* 0x1000132A */
    w  = pRect[2] - x0;
    if (w < 0)
        return;
    h  = pRect[3] - pRect[1];
    y0 = pRect[1];
    h  = pRect[3];
    h -= y0;
    if (h < 0)
        return;

    if ((unsigned)(x + w) >= (unsigned)pDst->w) {        /* 0x1000135E */
        if ((unsigned)pDst->w < (unsigned)x)
            return;
        w = pDst->w - x;
    }
    if ((unsigned)(y + h) >= (unsigned)pDst->h) {        /* 0x10001377 */
        if ((unsigned)pDst->h < (unsigned)y)
            return;
        h = pDst->h - y;
    }

    pd = pDst->p + (y * pDst->w + x);           /* 0x10001385 */
    dstPitch = pDst->w * 2;
    ps = pSrc->p + (y0 * pSrc->w + x0);
    srcPitch = pSrc->w * 2;

    if ((flags & 1) != 0)                                /* 0x100013AA */
        FUN_10001440(pd, dstPitch, w, h, ps, srcPitch, pSrc->key);
    else
        FUN_100013f0(pd, dstPitch, w, h, ps, srcPitch);
}

#endif /* BR_MATCHING_BUILD */
