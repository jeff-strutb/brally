/* flac.c: a small lossless FLAC encoder for the CD's audio tracks.
 *
 * 16-bit stereo at 44.1 kHz in blocks of 4096: each block takes the
 * cheapest of left/right, left/side, side/right and mid/side, each channel
 * the cheapest of a constant, a fixed polynomial predictor of order 0..4, or
 * verbatim samples, with partitioned Rice-coded residuals. STREAMINFO carries
 * the MD5 of the PCM, as decoders verify. */
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"

#define BLOCK 4096
#define MAX_PORDER 8

typedef struct bits { uint8_t *p; size_t n, cap; uint64_t acc; int nacc; } bits;

static void put(bits *b, uint32_t v, int n)            /* n <= 32 */
{
    if (n == 0)
        return;
    b->acc = b->acc << n | (n == 32 ? v : v & ((1u << n) - 1));
    b->nacc += n;
    while (b->nacc >= 8) {
        b->nacc -= 8;
        b->p[b->n++] = (uint8_t)(b->acc >> b->nacc);
    }
}

static void put_signed(bits *b, int32_t v, int n) { put(b, (uint32_t)v, n); }

static void align(bits *b)
{
    if (b->nacc)
        put(b, 0, 8 - b->nacc);
}

static void put_unary(bits *b, uint32_t q)             /* q zeros, then a one */
{
    while (q >= 32) {
        put(b, 0, 32);
        q -= 32;
    }
    put(b, 1, (int)q + 1);
}

static uint8_t crc8(const uint8_t *p, size_t n)
{
    uint8_t c = 0;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = (uint8_t)(c & 0x80 ? c << 1 ^ 0x07 : c << 1);
    }
    return c;
}

static uint16_t crc16(const uint8_t *p, size_t n)
{
    uint16_t c = 0;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= (uint16_t)(p[i] << 8);
        for (k = 0; k < 8; k++)
            c = (uint16_t)(c & 0x8000 ? c << 1 ^ 0x8005 : c << 1);
    }
    return c;
}

/* ---- residuals ---------------------------------------------------------------------- */
static uint32_t zz(int32_t r) { return (uint32_t)(r << 1) ^ (uint32_t)(r >> 31); }

/* the best Rice parameter for n values summing (zigzagged) to sum, and its cost */
static int rice_param(const uint32_t *u, int n, uint64_t *cost)
{
    uint64_t sum = 0, best = ~(uint64_t)0;
    int i, k, bk = 0, k0;
    for (i = 0; i < n; i++)
        sum += u[i];
    k0 = 0;
    while (k0 < 14 && ((uint64_t)n << (k0 + 1)) < sum)
        k0++;
    for (k = k0 > 0 ? k0 - 1 : 0; k <= k0 + 1 && k <= 14; k++) {
        uint64_t c = (uint64_t)n * (uint64_t)(k + 1);
        for (i = 0; i < n; i++)
            c += u[i] >> k;
        if (c < best) {
            best = c;
            bk = k;
        }
    }
    *cost = best;
    return bk;
}

/* the cheapest partition order: its cost in bits (with headers) */
static uint64_t plan_residual(const uint32_t *u, int bs, int order, int *porder, int *params)
{
    uint64_t best = ~(uint64_t)0;
    int p, k;
    for (p = 0; p <= MAX_PORDER; p++) {
        int parts = 1 << p, len = bs >> p, pr[1 << MAX_PORDER];
        uint64_t total = 6;
        if (bs % parts || len <= order)
            break;
        for (k = 0; k < parts; k++) {
            uint64_t c;
            int start = k == 0 ? order : k * len, n = (k + 1) * len - start;
            pr[k] = rice_param(u + start, n, &c);
            total += 4 + c;
        }
        if (total < best) {
            best = total;
            *porder = p;
            memcpy(params, pr, sizeof(int) * (size_t)parts);
        }
    }
    return best;
}

static void write_residual(bits *b, const uint32_t *u, int bs, int order, int porder, const int *params)
{
    int parts = 1 << porder, len = bs >> porder, k, i;
    put(b, 0, 2);                                       /* Rice, 4-bit parameters */
    put(b, (uint32_t)porder, 4);
    for (k = 0; k < parts; k++) {
        int start = k == 0 ? order : k * len, end = (k + 1) * len, rp = params[k];
        put(b, (uint32_t)rp, 4);
        for (i = start; i < end; i++) {
            put_unary(b, u[i] >> rp);
            put(b, u[i], rp);
        }
    }
}

/* ---- one channel ---------------------------------------------------------------------- */
typedef struct sub {
    int kind;                 /* 0 constant, 1 verbatim, 2 fixed */
    int order, porder, params[1 << MAX_PORDER];
    uint64_t cost;
    uint32_t u[BLOCK];
} sub;

static void fixed_residual(const int32_t *x, int bs, int order, uint32_t *u)
{
    int i;
    for (i = order; i < bs; i++) {
        int32_t r;
        switch (order) {
        case 0: r = x[i]; break;
        case 1: r = x[i] - x[i - 1]; break;
        case 2: r = x[i] - 2 * x[i - 1] + x[i - 2]; break;
        case 3: r = x[i] - 3 * x[i - 1] + 3 * x[i - 2] - x[i - 3]; break;
        default: r = x[i] - 4 * x[i - 1] + 6 * x[i - 2] - 4 * x[i - 3] + x[i - 4]; break;
        }
        u[i] = zz(r);
    }
}

static void plan_sub(const int32_t *x, int bs, int bps, sub *s, sub *tmp)
{
    int i, o, same = 1;
    for (i = 1; i < bs && same; i++)
        same = x[i] == x[0];
    if (same) {
        s->kind = 0;
        s->cost = 8 + (uint64_t)bps;
        return;
    }
    s->kind = 1;
    s->cost = 8 + (uint64_t)bps * (uint64_t)bs;
    for (o = 0; o <= 4 && o < bs; o++) {
        uint64_t c;
        fixed_residual(x, bs, o, tmp->u);
        c = 8 + (uint64_t)bps * (uint64_t)o + plan_residual(tmp->u, bs, o, &tmp->porder, tmp->params);
        if (c < s->cost) {
            s->kind = 2;
            s->order = o;
            s->porder = tmp->porder;
            s->cost = c;
            memcpy(s->params, tmp->params, sizeof(int) << tmp->porder);
            memcpy(s->u, tmp->u, sizeof(uint32_t) * (size_t)bs);
        }
    }
}

static void write_sub(bits *b, const int32_t *x, int bs, int bps, const sub *s)
{
    int i;
    if (s->kind == 0) {
        put(b, 0x00, 8);
        put_signed(b, x[0], bps);
    } else if (s->kind == 1) {
        put(b, 0x02, 8);
        for (i = 0; i < bs; i++)
            put_signed(b, x[i], bps);
    } else {
        put(b, (uint32_t)(0x08 | s->order) << 1, 8);      /* 0, type 001xxx, no wasted bits */
        for (i = 0; i < s->order; i++)
            put_signed(b, x[i], bps);
        write_residual(b, s->u, bs, s->order, s->porder, s->params);
    }
}

/* ---- the file --------------------------------------------------------------------------- */
typedef struct enc {
    int32_t l[BLOCK], r[BLOCK], m[BLOCK], sd[BLOCK];
    sub sl, sr, sm, ss, tmp;
    uint8_t frame[BLOCK * 2 * 3 + 64];
} enc;

static void write_frame(enc *e, FILE *f, int bs, uint32_t index)
{
    bits b = { e->frame, 0, sizeof e->frame, 0, 0 };
    uint64_t c_ind, c_ls, c_rs, c_ms;
    int i, assign, hdr_end;
    uint8_t utf[7];
    int nu;
    for (i = 0; i < bs; i++) {
        e->m[i] = (e->l[i] + e->r[i]) >> 1;
        e->sd[i] = e->l[i] - e->r[i];
    }
    plan_sub(e->l, bs, 16, &e->sl, &e->tmp);
    plan_sub(e->r, bs, 16, &e->sr, &e->tmp);
    plan_sub(e->m, bs, 16, &e->sm, &e->tmp);
    plan_sub(e->sd, bs, 17, &e->ss, &e->tmp);
    c_ind = e->sl.cost + e->sr.cost;
    c_ls = e->sl.cost + e->ss.cost;
    c_rs = e->ss.cost + e->sr.cost;
    c_ms = e->sm.cost + e->ss.cost;
    assign = 1;
    if (c_ls < c_ind) { assign = 8; c_ind = c_ls; }
    if (c_rs < c_ind) { assign = 9; c_ind = c_rs; }
    if (c_ms < c_ind) { assign = 10; }

    /* header */
    put(&b, 0xFFF8, 16);                                 /* sync, fixed block size */
    put(&b, bs == BLOCK ? 12 : 7, 4);                     /* 4096, or 16 bits at the end */
    put(&b, 9, 4);                                        /* 44.1 kHz */
    put(&b, (uint32_t)assign, 4);
    put(&b, 4 << 1, 4);                                   /* 16 bits per sample, reserved 0 */
    if (index < 0x80) {                                   /* frame number, UTF-8 coded */
        utf[0] = (uint8_t)index;
        nu = 1;
    } else {
        uint32_t v = index;
        int k = index < 0x800 ? 2 : index < 0x10000 ? 3 : index < 0x200000 ? 4 : index < 0x4000000 ? 5 : 6;
        for (i = k - 1; i > 0; i--) {
            utf[i] = (uint8_t)(0x80 | (v & 0x3F));
            v >>= 6;
        }
        utf[0] = (uint8_t)((0xFF00 >> k) | v);
        nu = k;
    }
    for (i = 0; i < nu; i++)
        put(&b, utf[i], 8);
    if (bs != BLOCK)
        put(&b, (uint32_t)(bs - 1), 16);
    hdr_end = (int)b.n;
    put(&b, crc8(b.p, (size_t)hdr_end), 8);

    switch (assign) {
    case 1:  write_sub(&b, e->l, bs, 16, &e->sl); write_sub(&b, e->r, bs, 16, &e->sr); break;
    case 8:  write_sub(&b, e->l, bs, 16, &e->sl); write_sub(&b, e->sd, bs, 17, &e->ss); break;
    case 9:  write_sub(&b, e->sd, bs, 17, &e->ss); write_sub(&b, e->r, bs, 16, &e->sr); break;
    default: write_sub(&b, e->m, bs, 16, &e->sm); write_sub(&b, e->sd, bs, 17, &e->ss); break;
    }
    align(&b);
    {
        uint16_t c = crc16(b.p, b.n);
        put(&b, c, 16);
    }
    fwrite(b.p, 1, b.n, f);
}

int rb_flac_encode(FILE *in, uint64_t frames, const char *path, rb_progress cb, void *ctx, double lo, double hi,
                   volatile int *cancel, char *err, size_t errlen)
{
    enc *e = calloc(1, sizeof *e);
    uint8_t pcm[BLOCK * 4], hdr[42], md5[16];
    char part[2100];
    uint64_t done = 0;
    uint32_t index = 0;
    rb_md5 m;
    FILE *f;
    int i, ok = 1;
    if (!e) {
        rb_err(err, errlen, "out of memory");
        return 0;
    }
    snprintf(part, sizeof part, "%s.part", path);
    if (!(f = rb_fopen(part, "wb"))) {
        free(e);
        rb_err(err, errlen, "cannot write %s", path);
        return 0;
    }
    memset(hdr, 0, sizeof hdr);
    fwrite(hdr, 1, sizeof hdr, f);                         /* STREAMINFO, filled in at the end */
    rb_md5_init(&m);
    while (done < frames) {
        int bs = frames - done < BLOCK ? (int)(frames - done) : BLOCK;
        if (cancel && *cancel) {
            rb_err(err, errlen, "cancelled");
            ok = 0;
            break;
        }
        if (fread(pcm, 4, (size_t)bs, in) != (size_t)bs) {
            rb_err(err, errlen, "the disc image ends inside an audio track");
            ok = 0;
            break;
        }
        rb_md5_update(&m, pcm, (size_t)bs * 4);
        for (i = 0; i < bs; i++) {
            e->l[i] = (int16_t)(pcm[4 * i] | pcm[4 * i + 1] << 8);
            e->r[i] = (int16_t)(pcm[4 * i + 2] | pcm[4 * i + 3] << 8);
        }
        write_frame(e, f, bs, index++);
        done += (uint64_t)bs;
        if (cb && (index & 63) == 0)
            cb(ctx, lo + (hi - lo) * (double)done / (double)frames, NULL);
    }
    if (ok) {
        bits b = { hdr, 0, sizeof hdr, 0, 0 };
        rb_md5_final(&m, md5);
        put(&b, 0x664C6143, 32);                           /* "fLaC" */
        put(&b, 0x80, 8);                                  /* last metadata block, STREAMINFO */
        put(&b, 34, 24);
        put(&b, BLOCK, 16);                                /* min, max block size */
        put(&b, BLOCK, 16);
        put(&b, 0, 24);                                    /* frame sizes unknown */
        put(&b, 0, 24);
        put(&b, 44100, 20);
        put(&b, 1, 3);                                     /* 2 channels */
        put(&b, 15, 5);                                    /* 16 bits */
        put(&b, (uint32_t)(frames >> 32), 4);
        put(&b, (uint32_t)frames, 32);
        for (i = 0; i < 16; i++)
            put(&b, md5[i], 8);
        ok = rb_seek(f, 0) && fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr;
    }
    ok &= fclose(f) == 0;
    free(e);
    if (ok && !rb_rename(part, path)) {
        rb_err(err, errlen, "cannot write %s", path);
        ok = 0;
    }
    if (!ok)
        rb_rmtree(part);
    return ok;
}
