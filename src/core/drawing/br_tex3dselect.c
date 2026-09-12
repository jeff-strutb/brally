/* br_tex3dselect.c -- drawing: pick (and if needed, build) the pixels a
 * texture record should upload.
 *
 * 0x10027B60 is the front door the downloaders call (as FUN_10027b60, its
 * name across the tree): it decides between the record's raw texels, an
 * already-expanded mip, the shared expansion buffer -- running the big
 * 0x100250D0 expander over the N64-format source when the cache is stale --
 * and the resampler's output when the stored size disagrees.  Kept OUT of
 * br_tex3d_expand.c on purpose: that TU is the parked giant and its codegen
 * is placement-sensitive.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif

#ifdef BR_MATCHING_BUILD

extern unsigned short DAT_1186c988[];   /* the shared expansion buffer   */
extern unsigned short DAT_105e1828[];   /* the resampler's output        */
extern int            DAT_10697a60;     /* the record's palette handle   */

/* The REAL 22-argument shape of 0x100250D0 (br_tex3d.c carries the port's
 * 21-argument model, which is why this lives in its own TU). */
void BrTex3dExpand(unsigned short *, int, int, unsigned char *, int, int,
                   int, int, int, int, int, unsigned char,
                   int, unsigned char, unsigned char, unsigned char,
                   unsigned char, unsigned char, unsigned char,
                   unsigned char, unsigned char, int);
void BrTex3dModulate(int rec, unsigned short *pBuf);        /* 0x10027CD0 */
int  BrTex3dMipChainLoad(unsigned short *, unsigned short *, int); /* 0x10027E10 */

/* WHAT IT DOES: hands back the pixels a texture record should upload, and
 * records the record's palette handle on the side. A record flagged as raw
 * returns its own texels. One with a pre-expanded mip chain returns the
 * chain's first level (or the shared buffer when that level is live).
 * Otherwise the N64-format source is expanded into the shared buffer
 * (skipped when the record says the buffer is already current), modulated
 * in place when the record asks, and finally resampled into the second
 * buffer when the decoded size disagrees with the stored one. */
/* @implements 0x10027B60 glide FUN_10027b60 */
int FUN_10027b60(int rec)
{
    unsigned int flags;
    int          lvl;
    int          ret;

    flags = *(unsigned int *)(rec + 0x260);
    if (flags & 0x40) {
        DAT_10697a60 = *(int *)(rec + 0x29c);
        return *(int *)(rec + 0x48);
    }
    ret = (int)DAT_1186c988;
    if (*(int *)(rec + 0x268) != 0) {
        int live = *(int *)(rec + *(int *)(rec + 0x274) * 4 + 0x27c);
        if (live != 0) {
            DAT_10697a60 = *(int *)(rec + 0x28c);
            return live;
        }
        {
            int pal = *(int *)(rec + 0x28c);
            int r0  = *(int *)(rec + 0x27c);
            DAT_10697a60 = pal;
            return r0;
        }
    }
    if (!(flags & 0x10)) {
        lvl = *(int *)(rec + 0x58);
        BrTex3dExpand(DAT_1186c988,
                      *(int *)(rec + 0x29c),
                      *(int *)(rec + (lvl << 6) + 0x64),
                      *(unsigned char **)(rec + 0x48),
                      *(int *)(rec + 0x4c),
                      *(int *)(rec + (lvl << 6) + 0x60),
                      *(int *)(rec + 0x50),
                      *(int *)(rec + 0x54),
                      lvl,
                      *(int *)(rec + 0x5c),
                      rec + 0x60,
                      (unsigned char)flags,
                      *(int *)(rec + 0x264),
                      *(unsigned char *)(rec + 0x290),
                      *(unsigned char *)(rec + 0x291),
                      *(unsigned char *)(rec + 0x292),
                      *(unsigned char *)(rec + 0x293),
                      *(unsigned char *)(rec + 0x294),
                      *(unsigned char *)(rec + 0x295),
                      *(unsigned char *)(rec + 0x296),
                      *(unsigned char *)(rec + 0x297),
                      *(int *)(rec + 0x298));
    }
    DAT_10697a60 = *(int *)(rec + 0x29c);
    if ((*(unsigned int *)(rec + 0x260) & 2)
        && (*(unsigned int *)(rec + 0x260) & 0x80)) {
        *(int *)(rec + 0x18) = *(int *)(rec + 0x1c);
        BrTex3dModulate(rec, DAT_1186c988);
    }
    if (*(int *)(rec + 0x2a0) != *(int *)(rec + 8)
        || *(int *)(rec + 0x2a4) != *(int *)(rec + 0xc)) {
        int h = BrTex3dMipChainLoad(DAT_105e1828, DAT_1186c988, rec);
        *(int *)(rec + 0x3c) = h;
        DAT_10697a60 = h;
        ret = (int)DAT_105e1828;
    }
    return ret;
}

#endif /* BR_MATCHING_BUILD */
