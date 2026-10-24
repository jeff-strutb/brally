/* lift.c: the game's initialised data, from the cartridge's (tgr_romdata).
 *
 * What the N64's boot code did: the ROM's .data/.rodata (ROM 0x70AB0..
 * 0xAD400) lands at 0x8026FAB0 and .bss after it is zero.  Here it lands in
 * the arena; then each symbol's multi-byte units are turned to native order
 * (tools/globals.py's runs) and the pointer variables, which are native
 * objects, are given native pointers for the original addresses the ROM
 * holds in them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tgr_addr.h"
#include "tgr_syms.h"
#include "plat.h"

#define DATA_VA   0x8026FAB0u
#define DATA_ROM  0x70AB0u
#define DATA_END  0xAD400u
#define TEXT_LO   0x80200000u

static void swap_unit(uint8_t *p, uint32_t size)
{
    uint8_t t;
    switch (size) {
    case 2: t = p[0]; p[0] = p[1]; p[1] = t; break;
    case 4: t = p[0]; p[0] = p[3]; p[3] = t; t = p[1]; p[1] = p[2]; p[2] = t; break;
    case 8: {
        int i;
        for (i = 0; i < 4; i++) { t = p[i]; p[i] = p[7 - i]; p[7 - i] = t; }
        break;
    }
    }
}

/* bytes of .data already turned native: each byte is swapped once */
static uint8_t s_done[DATA_END - DATA_ROM];

/* the object of the image an original address falls in */
static const TgrObj *obj_at(uint32_t a)
{
    int lo = 0, hi = tgr_nobjs - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (a < tgr_objs[m].addr)
            hi = m - 1;
        else if (a - tgr_objs[m].addr >= tgr_objs[m].size)
            lo = m + 1;
        else
            return &tgr_objs[m];
    }
    return NULL;
}

static void lift_fields(const TgrMap *map, uint32_t nmap, uint8_t *nat, const uint8_t *rom);

#define BSS_END 0x80382BB0u

/* what a pointer to a record laid out differently natively points at: the
 * record in its native layout (an object declared so, or a copy made here,
 * once, of the ROM's bytes or zeros for .bss), never the N64's bytes */
static void *pointee(uint32_t a, int t)
{
    const TgrType *ty = &tgr_types[t];
    const TgrObj *o = tgr_dyn_at(a);
    uint8_t *p;
    if (o && o->addr == a)
        return o->nat;
    o = obj_at(a);
    if ((o && o->map && a - o->addr < tgr_map_end(o)) || a < DATA_VA || a >= BSS_END)
        return tgr_ptr32(a);        /* declared in its native layout, or fixed RAM */
    p = calloc(1, ty->natsize);
    tgr_obj_add(a, ty->n64size, p, ty->natsize, ty->map, ty->nmap);
    if (a < DATA_VA + (DATA_END - DATA_ROM))
        lift_fields(ty->map, ty->nmap, p, tgr_romdata + (DATA_ROM - TGR_ROMDATA_BASE) + (a - DATA_VA));
    return p;
}

/* an object laid out differently natively: each field from the ROM's bytes,
 * a pointer as the native pointer for the original address it holds */
static void lift_fields(const TgrMap *map, uint32_t nmap, uint8_t *nat, const uint8_t *rom)
{
    uint32_t i, k, b;
    for (i = 0; i < nmap; i++) {
        const TgrMap *m = &map[i];
        for (k = 0; k < m->count; k++) {
            const uint8_t *src = rom + m->n64off + k * m->n64stride;
            uint8_t *dst = nat + m->natoff + k * m->natstride;
            if (m->size == 0) {
                uint32_t a = (uint32_t)src[0] << 24 | (uint32_t)src[1] << 16 | (uint32_t)src[2] << 8 | src[3];
                void *p;
                if (a >= TEXT_LO && a < DATA_VA)
                    p = tgr_fn(a);
                else if (a && m->ptype >= 0)
                    p = pointee(a, m->ptype);
                else if (a & 0x80000000u)
                    p = tgr_ptr32(a);
                else
                    p = (void *)(uintptr_t)a;   /* a number the source types as a pointer */
                memcpy(dst, &p, sizeof p);
            } else {
                uint32_t n = m->size & 0xFFFF, w = m->size >> 16 ? m->size >> 16 : n;
                for (b = 0; b < n; b++)             /* (little-endian: the value in the low bytes) */
                    dst[b] = src[n - 1 - b];
                for (; b < w; b++)
                    dst[b] = 0;
            }
        }
    }
}

static void swap_runs(uint32_t addr, uint32_t run0, uint32_t nruns)
{
    const TgrObj *o = obj_at(addr);
    uint8_t *base;
    if (o && o->map)
        return;                                     /* lifted field by field already */
    base = tgr_ptr32(addr);
    uint32_t j, k, b;
    for (j = 0; j < nruns; j++) {
        const TgrRun *r = &tgr_runs[run0 + j];
        for (k = 0; k < r->count; k++) {
            uint32_t a = addr + r->off + k * r->stride;
            int free = a >= DATA_VA && a + r->size <= DATA_VA + (DATA_END - DATA_ROM);
            for (b = 0; free && b < r->size; b++)
                free = !s_done[a - DATA_VA + b];
            if (!free)
                continue;
            swap_unit(base + r->off + k * r->stride, r->size);
            memset(s_done + (a - DATA_VA), 1, r->size);
        }
    }
}

void tgr_lift(void)
{
    int i;
    uint32_t k;
    const uint8_t *data = tgr_romdata + (DATA_ROM - TGR_ROMDATA_BASE);   /* ROM DATA_ROM on */
    for (i = 0; i < tgr_nobjs; i++) {               /* each .data object from the ROM */
        const TgrObj *o = &tgr_objs[i];
        if (o->addr >= DATA_VA && o->addr < DATA_VA + (DATA_END - DATA_ROM) && !o->map) {
            uint32_t n = o->size, room = DATA_VA + (DATA_END - DATA_ROM) - o->addr;
            memcpy(o->nat, data + (o->addr - DATA_VA), n < room ? n : room);
        }
    }
    for (i = 0; i < tgr_nobjs; i++)                 /* those laid out differently, field by field */
        if (tgr_objs[i].map && tgr_objs[i].addr >= DATA_VA && tgr_objs[i].addr < DATA_VA + (DATA_END - DATA_ROM))
        {
            const TgrObj *o = &tgr_objs[i];
            uint32_t end0 = tgr_map_end(o), end1 = o->natsize - (o->size - end0);
            uint32_t room = DATA_VA + (DATA_END - DATA_ROM) - o->addr, n = o->size < room ? o->size : room;
            lift_fields(o->map, o->nmap, (uint8_t *)o->nat, data + (o->addr - DATA_VA));
            if (n > end0)                           /* the bytes after the fields, as they are */
                memcpy(o->nat + end1, data + (o->addr - DATA_VA) + end0, n - end0);
        }
    memset(s_done, 0, sizeof s_done);
    for (i = 0; i < tgr_nsyms; i++)
        swap_runs(tgr_syms[i].addr, tgr_syms[i].run0, tgr_syms[i].nruns);
    for (i = 0; i < tgr_nnatives; i++) {
        const TgrNat *n = &tgr_natives[i];
        if (!n->lifted)
            continue;
        for (k = 0; k < n->count; k++) {
            const uint8_t *b = data + (n->addr - DATA_VA) + 4 * k;
            uint32_t a = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
            if (a >= TEXT_LO && a < DATA_VA) {
                n->nat[k] = tgr_fn(a);
            } else if (a && n->ptype >= 0) {
                n->nat[k] = pointee(a, n->ptype);
            } else if (a & 0x80000000u) {
                n->nat[k] = tgr_ptr32(a);
                if (n->nruns)                       /* what it points at, if in .data */
                    swap_runs(a, n->run0, n->nruns);
            } else {
                n->nat[k] = (void *)(uintptr_t)a;     /* a number the source types as a pointer */
            }
        }
    }
}
