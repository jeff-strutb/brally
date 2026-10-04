/* win_rsrc.c: string resources out of a Windows DLL on disc.
 *
 * The game loads its text from a resource-only DLL on the CD (LoadLibraryA
 * then LoadStringA for each id). This reads that DLL's RT_STRING table:
 * blocks of sixteen counted UTF-16 strings, block (id / 16) + 1. */
#include <stdlib.h>
#include <string.h>

#include "plat.h"

typedef struct ppe {
    uint8_t *d;
    size_t   n;
    uint32_t rsrc_rva, rsrc_off;
} ppe;

static uint32_t u32(const ppe *p, size_t o) { return o + 4 <= p->n ? (uint32_t)p->d[o] | (uint32_t)p->d[o + 1] << 8 | (uint32_t)p->d[o + 2] << 16 | (uint32_t)p->d[o + 3] << 24 : 0; }
static uint16_t u16(const ppe *p, size_t o) { return o + 2 <= p->n ? (uint16_t)(p->d[o] | p->d[o + 1] << 8) : 0; }

void *plat_pe_open(const char *host)
{
    FILE *f = fopen(host, "rb");
    ppe *p;
    uint32_t e, nsec, opt, optsz, i, so;
    long n;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = (ppe *)calloc(1, sizeof *p);
    p->n = (size_t)n;
    p->d = (uint8_t *)malloc(p->n ? p->n : 1);
    if (fread(p->d, 1, p->n, f) != p->n || p->n < 0x40 || p->d[0] != 'M' || p->d[1] != 'Z') {
        fclose(f);
        plat_pe_close(p);
        return NULL;
    }
    fclose(f);
    e = u32(p, 0x3C);
    nsec = u16(p, e + 6);
    optsz = u16(p, e + 20);
    opt = e + 24;
    p->rsrc_rva = u32(p, opt + 96 + 8 * 2);          /* data directory 2: resources */
    so = opt + optsz;
    for (i = 0; i < nsec; i++, so += 40) {
        uint32_t va = u32(p, so + 12), vsz = u32(p, so + 8), raw = u32(p, so + 20);
        if (p->rsrc_rva >= va && p->rsrc_rva < va + (vsz ? vsz : u32(p, so + 16)))
            p->rsrc_off = raw + (p->rsrc_rva - va);
    }
    if (!p->rsrc_off) {
        plat_pe_close(p);
        return NULL;
    }
    return p;
}

void plat_pe_close(void *pv)
{
    ppe *p = (ppe *)pv;
    if (p) {
        free(p->d);
        free(p);
    }
}

/* the entry for `id` in the resource directory at `dir`; the first when id < 0 */
static uint32_t find(const ppe *p, uint32_t dir, int id)
{
    uint32_t nnamed = u16(p, p->rsrc_off + dir + 12), nid = u16(p, p->rsrc_off + dir + 14), i;
    uint32_t ent = p->rsrc_off + dir + 16;
    for (i = 0; i < nnamed + nid; i++, ent += 8) {
        uint32_t name = u32(p, ent), off = u32(p, ent + 4);
        if (id < 0 || (!(name & 0x80000000u) && name == (uint32_t)id))
            return off;
    }
    return 0xFFFFFFFFu;
}

int plat_pe_string(void *pv, unsigned id, char *out, int n)
{
    const ppe *p = (const ppe *)pv;
    uint32_t o, data_rva, size, pos;
    unsigned k;
    if (!p || n <= 0)
        return 0;
    o = find(p, 0, 6);                                  /* RT_STRING */
    if (o == 0xFFFFFFFFu || !(o & 0x80000000u))
        return 0;
    o = find(p, o & 0x7FFFFFFFu, (int)(id / 16 + 1));   /* the block */
    if (o == 0xFFFFFFFFu || !(o & 0x80000000u))
        return 0;
    o = find(p, o & 0x7FFFFFFFu, -1);                   /* the first language */
    if (o == 0xFFFFFFFFu || (o & 0x80000000u))
        return 0;
    data_rva = u32(p, p->rsrc_off + o);
    size = u32(p, p->rsrc_off + o + 4);
    pos = p->rsrc_off + (data_rva - p->rsrc_rva);
    for (k = 0; k < id % 16; k++) {
        pos += 2 + 2u * u16(p, pos);
        if (pos >= p->rsrc_off + (data_rva - p->rsrc_rva) + size)
            return 0;
    }
    {
        int len = u16(p, pos), i;
        if (len >= n)
            len = n - 1;
        for (i = 0; i < len; i++) {
            uint16_t c = u16(p, pos + 2 + 2u * (unsigned)i);
            out[i] = (char)(c < 256 ? c : '?');
        }
        out[len] = 0;
        return len;
    }
}
