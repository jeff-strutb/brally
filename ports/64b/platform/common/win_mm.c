/* win_mm.c: winmm (mmio, MCI) and msacm for the game.
 *
 * mmio reads RIFF files (the sound banks' WAVs). A file is read into memory
 * whole when opened; that memory is the I/O buffer mmioGetInfo hands out,
 * so the game's buffered reads walk it directly and mmioAdvance only ever
 * finds the end of the file.
 *
 * MCI is CD audio. Not yet: every command reports no device, which the game
 * treats as a machine without a CD drive for music. */
#include <stdlib.h>
#include <string.h>

#include "plat.h"

typedef struct pmmio {
    int      magic;
    uint8_t *data;
    LONG     size, pos;
} pmmio;
#define MMIO_MAGIC 0x4D4D494F

static pmmio *M(HMMIO h)
{
    pmmio *m = (pmmio *)h;
    return (m && m->magic == MMIO_MAGIC) ? m : NULL;
}

HMMIO WINAPI mmioOpenA(LPSTR name, LPMMIOINFO info, DWORD flags)
{
    char host[1024];
    FILE *f;
    pmmio *m;
    long n;
    if (info)
        info->wErrorRet = 0;
    if ((flags & (MMIO_WRITE | MMIO_READWRITE)) || !name || !plat_path(name, host, sizeof host)) {
        if (info)
            info->wErrorRet = 257;      /* MMIOERR_FILENOTFOUND */
        return NULL;
    }
    f = fopen(host, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    m = (pmmio *)calloc(1, sizeof *m);
    m->magic = MMIO_MAGIC;
    m->size = (LONG)n;
    m->data = (uint8_t *)malloc(n > 0 ? (size_t)n : 1);
    if (fread(m->data, 1, (size_t)n, f) != (size_t)n)
        m->size = 0;
    fclose(f);
    return (HMMIO)m;
}

MMRESULT WINAPI mmioClose(HMMIO h, UINT flags)
{
    pmmio *m = M(h);
    (void)flags;
    if (!m)
        return 5;
    m->magic = 0;
    free(m->data);
    free(m);
    return MMSYSERR_NOERROR;
}

LONG WINAPI mmioRead(HMMIO h, HPSTR out, LONG n)
{
    pmmio *m = M(h);
    if (!m || n < 0)
        return -1;
    if (n > m->size - m->pos)
        n = m->size - m->pos;
    memcpy(out, m->data + m->pos, (size_t)n);
    m->pos += n;
    return n;
}

LONG WINAPI mmioSeek(HMMIO h, LONG off, int origin)
{
    pmmio *m = M(h);
    LONG p;
    if (!m)
        return -1;
    p = origin == SEEK_SET ? off : origin == SEEK_CUR ? m->pos + off : m->size + off;
    if (p < 0 || p > m->size)
        return -1;
    m->pos = p;
    return p;
}

MMRESULT WINAPI mmioGetInfo(HMMIO h, LPMMIOINFO info, UINT flags)
{
    pmmio *m = M(h);
    (void)flags;
    if (!m)
        return 5;
    memset(info, 0, sizeof *info);
    info->hmmio = h;
    info->cchBuffer = m->size;
    info->pchBuffer = (HPSTR)m->data;
    info->pchNext = (HPSTR)m->data + m->pos;
    info->pchEndRead = (HPSTR)m->data + m->size;
    info->pchEndWrite = info->pchEndRead;
    info->lBufOffset = 0;
    info->lDiskOffset = m->size;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI mmioSetInfo(HMMIO h, LPCMMIOINFO info, UINT flags)
{
    pmmio *m = M(h);
    (void)flags;
    if (!m)
        return 5;
    m->pos = (LONG)((uint8_t *)info->pchNext - m->data);
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI mmioAdvance(HMMIO h, LPMMIOINFO info, UINT flags)
{
    pmmio *m = M(h);
    (void)flags;
    if (!m)
        return 5;
    /* the whole file is the buffer: there is nothing further to read */
    m->pos = (LONG)((uint8_t *)info->pchNext - m->data);
    info->pchEndRead = (HPSTR)m->data + m->size;
    return MMSYSERR_NOERROR;
}

static DWORD rd32(const uint8_t *p) { return (DWORD)p[0] | (DWORD)p[1] << 8 | (DWORD)p[2] << 16 | (DWORD)p[3] << 24; }

MMRESULT WINAPI mmioDescend(HMMIO h, LPMMCKINFO ck, const MMCKINFO *parent, UINT flags)
{
    pmmio *m = M(h);
    LONG end;
    if (!m)
        return 5;
    end = parent ? (LONG)(parent->dwDataOffset + parent->cksize) : m->size;
    for (;;) {
        DWORD id, size, type = 0;
        if (m->pos + 8 > end)
            return MMIOERR_CHUNKNOTFOUND;
        id = rd32(m->data + m->pos);
        size = rd32(m->data + m->pos + 4);
        if ((id == FOURCC_RIFF || id == FOURCC_LIST) && m->pos + 12 <= m->size)
            type = rd32(m->data + m->pos + 8);
        if ((flags & MMIO_FINDRIFF) ? (id == FOURCC_RIFF && (!ck->fccType || type == ck->fccType)) :
            (flags & MMIO_FINDLIST) ? (id == FOURCC_LIST && (!ck->fccType || type == ck->fccType)) :
            (flags & MMIO_FINDCHUNK) ? id == ck->ckid : 1) {
            ck->ckid = id;
            ck->cksize = size;
            ck->fccType = type;
            ck->dwDataOffset = (DWORD)(m->pos + 8);
            ck->dwFlags = 0;
            m->pos += (id == FOURCC_RIFF || id == FOURCC_LIST) ? 12 : 8;
            return MMSYSERR_NOERROR;
        }
        m->pos += 8 + (LONG)((size + 1) & ~1u);
    }
}

MMRESULT WINAPI mmioAscend(HMMIO h, LPMMCKINFO ck, UINT flags)
{
    pmmio *m = M(h);
    (void)flags;
    if (!m)
        return 5;
    m->pos = (LONG)(ck->dwDataOffset + ((ck->cksize + 1) & ~1u));
    if (m->pos > m->size)
        m->pos = m->size;
    return MMSYSERR_NOERROR;
}

/* ---- MCI: CD audio ------------------------------------------------------------- */
#define MCIERR_DEVICE_NOT_INSTALLED 0x0144
MCIERROR WINAPI mciSendCommandA(MCIDEVICEID id, UINT msg, DWORD_PTR flags, DWORD_PTR parms)
{
    (void)id; (void)flags; (void)parms;
    PLOG("mciSendCommandA(%u): no CD audio device\n", msg);
    return MCIERR_DEVICE_NOT_INSTALLED;
}

/* ---- msacm ------------------------------------------------------------------------- */
MMRESULT WINAPI acmMetrics(HACMOBJ h, UINT metric, LPVOID out)
{
    (void)h; (void)metric; (void)out;
    return 8;      /* MMSYSERR_NOTSUPPORTED */
}
