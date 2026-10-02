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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)


/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the shared expansion buffer   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the resampler's output        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* the record's palette handle   */

/* The REAL 22-argument shape of 0x100250D0 (br_tex3d.c carries the port's
 * 21-argument model, which is why this lives in its own TU). */
/* BrTex3dExpand: prototype in br_funcs.h */
/* BrTex3dModulate: prototype in br_funcs.h */
/* BrTex3dMipChainLoad: prototype in br_funcs.h */

/* WHAT IT DOES: hands back the pixels a texture record should upload, and
 * records the record's palette handle on the side. A record flagged as raw
 * returns its own texels. One with a pre-expanded mip chain returns the
 * chain's first level (or the shared buffer when that level is live).
 * Otherwise the N64-format source is expanded into the shared buffer
 * (skipped when the record says the buffer is already current), modulated
 * in place when the record asks, and finally resampled into the second
 * buffer when the decoded size disagrees with the stored one. */
/* @implements 0x10027B60 glide FUN_10027b60 */
uint16_t *FUN_10027b60(BrTexReq272 * rec)
{
    unsigned int flags;
    int          lvl;
    uint16_t    *ret;

    flags = *(unsigned int *)&rec->f260;
    if (flags & 0x40) {
        DAT_10697a60 = rec->cb29c;
        return (uint16_t *)rec->p1;
    }
    ret = (uint16_t *)DAT_1186c988;
    if (rec->f268 != 0) {
        uint16_t *live = rec->apMip[rec->f274];
        if (live != 0) {
            DAT_10697a60 = rec->f028C;
            return live;
        }
        {
            int pal = rec->f028C;
            uint16_t *r0 = rec->apMip[0];
            DAT_10697a60 = pal;
            return r0;
        }
    }
    if (!(flags & 0x10)) {
        lvl = rec->iLevel;
        BrTex3dExpand(DAT_1186c988,
                      rec->cb29c,
                      rec->lv[lvl][1],
                      rec->p1,
                      rec->p2,
                      rec->lv[lvl][0],
                      rec->p8,
                      rec->p9,
                      lvl,
                      rec->f5c,
                      &rec->lv[0][0],
                      (unsigned char)flags,
                      rec->f264,
                      *(unsigned char *)&rec->b290,
                      *(unsigned char *)&rec->b291,
                      *(unsigned char *)&rec->b292,
                      *(unsigned char *)&rec->b293,
                      *(unsigned char *)&rec->b294,
                      *(unsigned char *)&rec->b295,
                      *(unsigned char *)&rec->b296,
                      *(unsigned char *)&rec->b297,
                      rec->f298);
    }
    DAT_10697a60 = rec->cb29c;
    if ((*(unsigned int *)&rec->f260 & 2)
        && (*(unsigned int *)&rec->f260 & 0x80)) {
        rec->aspect0 = rec->aspect1;
        BrTex3dMipModulate(rec, DAT_1186c988);
    }
    if (rec->w2a0 != rec->w
        || rec->h2a4 != rec->h) {
        int h = BrTex3dMipChainLoad(DAT_105e1828, DAT_1186c988, rec);
        rec->cbTotal = h;
        DAT_10697a60 = h;
        ret = DAT_105e1828;
    }
    return ret;
}

