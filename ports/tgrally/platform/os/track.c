/* track.c: the loaded track's native records (tgr/track.h).
 *
 * The cartridge's track file is unpacked to 0x80025C00, big-endian, its
 * pointers original addresses (it is built for that address).  After each
 * unpack, BrTrackLoad has tgr_track_convert make the records the game reads:
 * the header, its objects, textures, gates and specials, the path segments
 * linked by pointer, the camera points and the collision tables.  A table's
 * length is not stored: it runs to the next thing the header points at (the
 * file lays them end to end), and a table read past its end reads the next
 * one's bytes, converted the same way.  Pointers into what stays in the file
 * (textures, display lists, names) become pointers into the file.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tgr_core.h"
#include "tgr/track.h"

#define FILE_AT     0x80025C00u     /* where the file is unpacked */
#define FILE_MAX    0x171400u       /* the most BrTrackLoad allows */

BrTrackHdr tgr_track;

static void **s_mem;                /* this track's records, freed at the next load */
static int s_nmem, s_capmem;
static uint32_t s_ends[64];         /* everything the header points at, sorted: the tables' ends */
static int s_nends;

static void *alloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (s_nmem == s_capmem) {
        s_capmem = s_capmem ? s_capmem * 2 : 256;
        s_mem = realloc(s_mem, s_capmem * sizeof *s_mem);
    }
    s_mem[s_nmem++] = p;
    return p;
}

static uint32_t r32(uint32_t a) { return tgr_rd32(tgr_ptr32(a)); }
static uint16_t r16(uint32_t a) { return tgr_rd16(tgr_ptr32(a)); }
static float rf(uint32_t a) { return tgr_rdf(tgr_ptr32(a)); }
static uint32_t hdr(uint32_t off) { return r32(FILE_AT + off); }

static int infile(uint32_t a) { return a >= FILE_AT && a < FILE_AT + FILE_MAX; }

/* the bytes from a to the next thing the header points at */
static uint32_t extent(uint32_t a)
{
    int i;
    for (i = 0; i < s_nends; i++)
        if (s_ends[i] > a)
            return s_ends[i] - a;
    return FILE_AT + FILE_MAX - a;
}

static void end_add(uint32_t a)
{
    int i, j;
    if (!infile(a))
        return;
    for (i = 0; i < s_nends && s_ends[i] < a; i++)
        ;
    if (i < s_nends && s_ends[i] == a)
        return;
    if (s_nends == (int)(sizeof s_ends / sizeof s_ends[0]))
        return;
    for (j = s_nends++; j > i; j--)
        s_ends[j] = s_ends[j - 1];
    s_ends[i] = a;
}

static unsigned short *u16s(uint32_t a)
{
    uint32_t n, i;
    unsigned short *t;
    if (!a)
        return 0;
    n = extent(a) / 2;
    t = alloc(n * 2);
    for (i = 0; i < n; i++)
        t[i] = r16(a + i * 2);
    return t;
}

static void vec3(BrVec3 *v, uint32_t a)
{
    v->x = rf(a);
    v->y = rf(a + 4);
    v->z = rf(a + 8);
}

static BrVec3 *vec3s(uint32_t a, uint32_t n)
{
    uint32_t i;
    BrVec3 *t;
    if (!a)
        return 0;
    t = alloc(n * sizeof *t);
    for (i = 0; i < n; i++)
        vec3(&t[i], a + i * 12);
    return t;
}

/* ---- path segments: converted once each, linked by pointer ------------------ */
/* The walks along a segment read the point after its last (BrPathGates reads
 * pt[i + 1] up to i = count - 1): on the N64 the bytes after the segment,
 * whatever follows it in the file.  Each segment keeps that many more points,
 * converted from those bytes. */
#define PT_PAST 9

static struct { uint32_t a; BrPathSeg *s; } *s_segs;
static int s_nsegs, s_capsegs;

static BrPathSeg *seg(uint32_t a)
{
    BrPathSeg *s;
    int i, k, n;
    if (!a)
        return 0;
    for (i = 0; i < s_nsegs; i++)
        if (s_segs[i].a == a)
            return s_segs[i].s;
    n = r16(a + 0x14);
    s = alloc(sizeof *s + (n + PT_PAST - 1) * sizeof s->pt[0]);
    if (s_nsegs == s_capsegs) {
        s_capsegs = s_capsegs ? s_capsegs * 2 : 64;
        s_segs = realloc(s_segs, s_capsegs * sizeof *s_segs);
    }
    s_segs[s_nsegs].a = a;
    s_segs[s_nsegs++].s = s;
    s->x0 = *(uint8_t *)tgr_ptr32(a + 0x10);
    s->y0 = *(uint8_t *)tgr_ptr32(a + 0x11);
    s->x1 = *(uint8_t *)tgr_ptr32(a + 0x12);
    s->y1 = *(uint8_t *)tgr_ptr32(a + 0x13);
    s->count = (unsigned short)n;
    s->flags = r16(a + 0x16);
    for (k = -1; k < n + PT_PAST; k++) {    /* and the bytes either side, read as points */
        uint32_t p = a + 0x40 + k * 0x28;
        vec3(&s->pt[k].left, p);
        vec3(&s->pt[k].pos, p + 0x0C);
        vec3(&s->pt[k].right, p + 0x18);
        s->pt[k].dist = rf(p + 0x24);
    }
    s->next = seg(r32(a));          /* after the record is listed: the links make loops */
    s->alt = seg(r32(a + 4));
    return s;
}

/* ---- textures --------------------------------------------------------------- */
static BrTexAnim *anim(uint32_t a)
{
    BrTexAnim *t;
    int n, k;
    n = r16(a + 2);
    t = alloc(sizeof *t + (n > 1 ? n - 1 : 0) * sizeof t->key[0]);
    t->x0 = r16(a);
    t->n = (unsigned short)n;
    t->x4 = r32(a + 4);
    t->t0 = r32(a + 8);
    for (k = 0; k < n; k++) {
        t->key[k].tex = (int)r32(a + 0x0C + k * 12);
        t->key[k].pal = (int)r32(a + 0x10 + k * 12);
        t->key[k].time = r32(a + 0x14 + k * 12);
    }
    return t;
}

void tgr_track_convert(void)
{
    BrTrackHdr *t = &tgr_track;
    uint32_t a, n, i, size = t->size;
    int k;

    for (k = 0; k < s_nmem; k++)
        free(s_mem[k]);
    s_nmem = 0;
    s_nsegs = 0;
    s_nends = 0;
    memset(t, 0, sizeof *t);
    t->size = size;

    for (i = 0x0C; i <= 0x94; i += 4)       /* the header's pointers, and the sky's list */
        end_add(hdr(i));

    t->hdrSize = hdr(0x04);
    t->x28 = rf(FILE_AT + 0x28);
    t->x2c = rf(FILE_AT + 0x2C);
    t->fogLo = rf(FILE_AT + 0x38);
    t->fogHi = rf(FILE_AT + 0x3C);
    vec3(&t->start, FILE_AT + 0x40);
    t->heading = rf(FILE_AT + 0x4C);
    t->x50 = hdr(0x50);
    memcpy(t->fogColour, tgr_ptr32(FILE_AT + 0x80), 3);

    /* collision */
    if ((a = hdr(0x0C)) != 0) {
        n = extent(a) / 8;
        t->tris = alloc(n * sizeof *t->tris);
        for (i = 0; i < n; i++) {
            t->tris[i].v[0] = r16(a + i * 8);
            t->tris[i].v[1] = r16(a + i * 8 + 2);
            t->tris[i].v[2] = r16(a + i * 8 + 4);
            t->tris[i].surface = r16(a + i * 8 + 6);
        }
    }
    if ((a = hdr(0x14)) != 0)
        t->verts = vec3s(a, extent(a) / 12);
    t->gridEntries = u16s(hdr(0x20));
    t->gridCells = u16s(hdr(0x24));
    t->queue2 = u16s(hdr(0x68));
    t->x6c = u16s(hdr(0x6C));
    t->triggers = u16s(hdr(0x8C));
    t->triTrigger = u16s(hdr(0x90));
    if ((a = hdr(0x94)) != 0)
        t->surf = tgr_ptr32(a);             /* bytes: read where they are */

    /* objects */
    t->nObjs = (int)hdr(0x64);
    if ((a = hdr(0x60)) != 0 && t->nObjs > 0) {
        t->objs = alloc(t->nObjs * sizeof *t->objs);
        for (k = 0; k < t->nObjs; k++) {
            BrTrackObj *o = &t->objs[k];
            uint32_t p = a + k * 0x54;
            int r, c;
            for (r = 0; r < 4; r++)
                for (c = 0; c < 4; c++)
                    o->m[r][c] = rf(p + (r * 4 + c) * 4);
            o->scale = rf(p + 0x40);
            o->dl = r32(p + 0x44);
            o->hide = r16(p + 0x48);
            o->x4a = r16(p + 0x4A);
            o->flags = r16(p + 0x4C);
            o->tris = r16(p + 0x4E);
            o->vtxs = r16(p + 0x50);
            o->x52 = r16(p + 0x52);
        }
    }
    if ((a = hdr(0x5C)) != 0 && t->nObjs > 0) {
        t->names = alloc(t->nObjs * sizeof *t->names);
        for (k = 0; k < t->nObjs; k++)
            t->names[k] = r32(a + k * 4) ? tgr_ptr32(r32(a + k * 4)) : 0;
    }

    /* textures */
    t->nTex = (int)hdr(0x18);
    if ((a = hdr(0x1C)) != 0 && t->nTex > 0) {
        t->tex = alloc(t->nTex * sizeof *t->tex);
        for (k = 0; k < t->nTex; k++) {
            BrTexSlot *s = &t->tex[k];
            uint32_t p = a + k * 0x24, d;
            d = r32(p);
            s->dst = d ? tgr_ptr32(d) : 0;
            d = r32(p + 4);
            s->palDst = d ? tgr_ptr32(d) : 0;
            s->bits = r32(p + 0x20);
            if (BR_TEXSLOT_ANIMATED(s) && r32(p + 8))
                s->anim = anim(r32(p + 8));
            else
                s->pal = r32(p + 8);
        }
    }

    /* the path */
    t->path = seg(hdr(0x70));
    t->nSegs = (int)hdr(0x7C);
    if ((a = hdr(0x78)) != 0 && t->nSegs > 0) {
        t->segs = alloc(t->nSegs * sizeof *t->segs);
        for (k = 0; k < t->nSegs; k++)
            t->segs[k] = seg(r32(a + k * 4));
    }

    /* the camera points */
    t->nCams = (int)hdr(0x88);
    if ((a = hdr(0x84)) != 0 && t->nCams > 0)
        t->cams = vec3s(a, t->nCams);

    /* gates and specials, in the header */
    t->nGates = (int)hdr(0x160);
    for (k = 0; k < 10; k++) {
        uint32_t p = FILE_AT + 0x98 + k * 0x14;
        t->gate[k].a[0] = rf(p);
        t->gate[k].a[1] = rf(p + 4);
        t->gate[k].b[0] = rf(p + 8);
        t->gate[k].b[1] = rf(p + 12);
        t->gate[k].award = rf(p + 16);
    }
    t->nSpecials = (int)hdr(0x224);
    for (k = 0; k < 16; k++) {
        uint32_t p = FILE_AT + 0x164 + k * 12;
        if (k < t->nSpecials && (*(uint8_t *)tgr_ptr32(p + 8) == 4 || *(uint8_t *)tgr_ptr32(p + 8) == 5) && r32(p))
            t->specials[k].path = vec3s(r32(p), extent(r32(p)) / 12);
        else
            t->specials[k].obj = (int)r32(p);
        t->specials[k].arg = (int)r32(p + 4);
        t->specials[k].kind = *(uint8_t *)tgr_ptr32(p + 8);
    }
}
