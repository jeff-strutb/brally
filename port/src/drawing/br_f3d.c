/* br_f3d.c -- N64 F3D display-list walker (portable C99). See br_f3d.h. */
#include "br_f3d.h"
#include <string.h>

/* Opcodes that legitimately appear in these lists. Anything else means we
 * have walked off the end into other data, so the walk must stop -- otherwise
 * a single list "consumes" kilobytes of unrelated bytes and the statistics
 * become meaningless. */
static int known_op(unsigned op)
{
    switch (op) {
    case BR_G_VTX: case BR_G_TRI1: case BR_G_TRI2: case BR_G_ENDDL:
    case BR_G_DL:
    case 0x03:                        /* G_MOVEMEM */
    case 0xB6: case 0xB7:             /* clear/set geometry mode */
    case 0xB9: case 0xBA:             /* setothermode L/H */
    case 0xBB:                        /* texture */
    case 0xBC:                        /* moveword */
    case 0xBD:                        /* popmtx */
    case 0xE6: case 0xE7:             /* load/pipe sync */
    case 0xE8: case 0xE9: case 0xEA:  /* tile/full sync, setkeygb */
    case 0xF0:                        /* loadtlut */
    case 0xF2: case 0xF3: case 0xF5:  /* settilesize, loadblock, settile */
    case 0xF8: case 0xFB: case 0xFC:  /* fog, env, combine */
    case 0xFD:                        /* settimg */
        return 1;
    default:
        return 0;
    }
}

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

/* One triangle: three pre-doubled indices. Returns 1 if all are valid. */
static int tri(unsigned a, unsigned b, unsigned c, unsigned cCache,
               BrF3dStats *st)
{
    if ((a | b | c) & 1) { st->cBadIndices++; return 0; }   /* must be even */
    a >>= 1; b >>= 1; c >>= 1;
    if (a >= cCache || b >= cCache || c >= cCache) { st->cBadIndices++; return 0; }
    st->cTriangles++;
    return 1;
}

int BrF3dWalk(const void *pvList, size_t cbMax, BrF3dStats *pStats)
{
    const unsigned char *p = (const unsigned char *)pvList;
    size_t off = 0;

    while (off + 8 <= cbMax) {
        uint32_t w0 = be32(p + off), w1 = be32(p + off + 4);
        unsigned op = w0 >> 24;
        pStats->cCommands++;

        switch (op) {
        case BR_G_VTX: {
            /* F3DEX packing, corrected -- see the note in br_f3d.h.
             *
             * bits[23:16] are the DESTINATION INDEX, not bits[7:1]. The real
             * handler (Glide 0x10021A20) reads it as `mov cl, byte ptr
             * [ebp-2]`, i.e. byte 2 of w0, then indexes a 104-byte-stride
             * vertex array with it. bits[7:1] hold (16n - 1) and are not a
             * bound on anything.
             *
             * The old reading rejected every G_VTX with n >= 9, because
             * (16n - 1) >> 1 exceeds 64 from n = 9 upward -- and the walker
             * returned on rejection, abandoning the whole list. bb.rca
             * reported 76 triangles where the file holds 1824, and ce.rca 471
             * where it holds 1079. The suite stayed green because its
             * assertions only checked ratios and "> 0", which a 4% sample
             * satisfies as happily as the whole model. */
            unsigned n    = (w0 >> 10) & 0x3F;        /* bits[15:10] = n      */
            unsigned dest = (w0 >> 16) & 0xFF;        /* bits[23:16] = index  */
            if (n == 0 || dest + n > BR_F3D_VTX_SLOTS) {
                pStats->cBadIndices++;
                return 1;
            }
            (void)w1;                                  /* segment address */
            (void)dest;
            pStats->cVtxLoads++;
            pStats->cVerticesLoaded += n;
            break;
        }
        case BR_G_TRI1:
            tri((w1 >> 16) & 0xFF, (w1 >> 8) & 0xFF, w1 & 0xFF,
                BR_F3D_VTX_CACHE, pStats);
            break;
        case BR_G_TRI2:
            tri((w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF,
                BR_F3D_VTX_CACHE, pStats);
            tri((w1 >> 16) & 0xFF, (w1 >> 8) & 0xFF, w1 & 0xFF,
                BR_F3D_VTX_CACHE, pStats);
            break;
        case BR_G_ENDDL:
            return 0;
        default:
            if (!known_op(op)) {
                pStats->cUnknownOps++;
                return 1;             /* end of list: stop, do not consume */
            }
            break;                    /* recognised but not interpreted yet */
        }
        off += 8;
    }
    return 0;
}

int BrF3dScanFile(const void *pvData, size_t cbData, BrF3dStats *pStats)
{
    const unsigned char *p = (const unsigned char *)pvData;
    size_t off;

    memset(pStats, 0, sizeof(*pStats));
    /* Find runs that begin with a G_VTX whose w1 looks like an address, then
     * walk from there. Segmented addresses use a small segment number in the
     * top byte; the retail data also uses KSEG0 (0x80). */
    for (off = 0; off + 16 <= cbData; off += 8) {
        uint32_t w0 = be32(p + off), w1 = be32(p + off + 4);
        unsigned seg = w1 >> 24;
        if ((w0 >> 24) != BR_G_VTX)
            continue;
        if (!(seg == 0x80 || seg <= 0x0F))
            continue;
        {
            BrF3dStats s;
            memset(&s, 0, sizeof(s));
            BrF3dWalk(p + off, cbData - off > 4096 ? 4096 : cbData - off, &s);
            if (s.cTriangles == 0)
                continue;
            pStats->cCommands       += s.cCommands;
            pStats->cVtxLoads       += s.cVtxLoads;
            pStats->cVerticesLoaded += s.cVerticesLoaded;
            pStats->cTriangles      += s.cTriangles;
            pStats->cUnknownOps     += s.cUnknownOps;
            pStats->cBadIndices     += s.cBadIndices;
            /* skip past what we just consumed */
            off += (s.cCommands ? (s.cCommands - 1) * 8 : 0);
        }
    }
    return 0;
}
