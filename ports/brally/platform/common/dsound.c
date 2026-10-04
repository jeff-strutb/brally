/* dsound.c: DirectSound -- the game's sound effects, mixed for the host.
 *
 * Every sample is a static secondary buffer (br_sndbuf.c), filled once
 * through Lock/Unlock and then driven with SetVolume, SetPan, SetFrequency,
 * Play (one-shot or looping), Stop, SetCurrentPosition and GetStatus. Unlock
 * takes the samples as floats and the host's audio callback sums the playing
 * buffers with DirectSound's laws, as the wasm lane does
 * (ports/macos/wasm/host/host_dx.c):
 *
 *   volume  hundredths of a dB, 0 = as recorded, never amplified;
 *           -10000 (DSBVOLUME_MIN) is silence
 *   pan     hundredths of a dB taken off the far side
 *   freq    the rate the samples are played at, 0 = the buffer's own
 *
 * resampled to the output rate by 4-point Hermite interpolation; a volume or
 * pan change ramps over one block so continuous changes do not click.
 *
 * While the host's mixer runs it is the play cursor: a one-shot stops when
 * its last sample has been played. Without one (no audio device, headless)
 * the cursor is the clock, so the game's state moves the same and nothing
 * is heard. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"

#define OUT_RATE 44100

typedef struct dsbuf {
    const struct IDirectSoundBufferVtbl *lpVtbl;   /* first: what the game holds */
    ULONG    refs;
    DWORD    flags, size, rate, align;
    int      ch, bits, pcmfmt;
    uint8_t *mem;                  /* what Lock hands out */
    float   *pcm;                  /* Unlock's float copy, ch interleaved */
    DWORD    frames;
    int      playing, looping, live;
    DWORD    pos;                  /* the cursor when stopped, bytes */
    DWORD    t0;                   /* timeGetTime at Play / SetCurrentPosition */
    LONG     vol, pan;
    DWORD    freq;
    double   fpos;                 /* the mixer's cursor, frames */
    float    gl, gr;               /* gains at the end of the last block */
} dsbuf;

static host_mutex *s_lock;
static dsbuf *s_live[512];
static int s_nlive;
static int s_mixing;               /* the host mixer runs: it moves the cursors */

static void lock(void)
{
    if (!s_lock)
        s_lock = host_mutex_new();
    host_mutex_lock(s_lock);
}
static void unlock(void) { host_mutex_unlock(s_lock); }

/* ---- the mix --------------------------------------------------------------------- */
static float herm(float s0, float s1, float s2, float s3, float x)
{
    float c1 = 0.5f * (s2 - s0);
    float c2 = s0 - 2.5f * s1 + 2.0f * s2 - 0.5f * s3;
    float c3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);
    return ((c3 * x + c2) * x + c1) * x + s1;
}

static float at(const dsbuf *b, int64_t i, int k)
{
    if (i < 0 || i >= (int64_t)b->frames) {
        if (!b->looping || !b->frames)
            return 0;
        i %= (int64_t)b->frames;
        if (i < 0)
            i += b->frames;
    }
    return b->pcm[i * b->ch + k];
}

static float cb_gain(LONG cb) { return cb <= -10000 ? 0.0f : cb >= 0 ? 1.0f : powf(10.0f, (float)cb / 2000.0f); }

static void unlive(dsbuf *b)
{
    int i;
    if (!b->live)
        return;
    for (i = 0; i < s_nlive; i++)
        if (s_live[i] == b) {
            s_live[i] = s_live[--s_nlive];
            break;
        }
    b->live = 0;
}

int plat_dsound_live(void) { return s_nlive; }

/* the playing buffers, added into lr (audio.c's mixer) */
void plat_dsound_mix(float *lr, int n)
{
    int v, i;
    lock();
    for (v = 0; v < s_nlive; v++) {
        dsbuf *b = s_live[v];
        float g = cb_gain(b->vol);
        float gl = g * (b->pan > 0 ? cb_gain(-b->pan) : 1.0f);
        float gr = g * (b->pan < 0 ? cb_gain(b->pan) : 1.0f);
        float dl = (gl - b->gl) / (float)n, dr = (gr - b->gr) / (float)n, al = b->gl, ar = b->gr;
        double step = (double)(b->freq ? b->freq : b->rate) / OUT_RATE, p = b->fpos;
        if (!b->playing || !b->pcm || !b->frames) {
            b->live = 0;
            s_live[v--] = s_live[--s_nlive];
            continue;
        }
        for (i = 0; i < n; i++) {
            int64_t k = (int64_t)p;
            float x = (float)(p - (double)k);
            al += dl;
            ar += dr;
            if (b->ch == 1) {
                float s = herm(at(b, k - 1, 0), at(b, k, 0), at(b, k + 1, 0), at(b, k + 2, 0), x);
                lr[2 * i] += s * al;
                lr[2 * i + 1] += s * ar;
            } else {
                lr[2 * i] += herm(at(b, k - 1, 0), at(b, k, 0), at(b, k + 1, 0), at(b, k + 2, 0), x) * al;
                lr[2 * i + 1] += herm(at(b, k - 1, 1), at(b, k, 1), at(b, k + 1, 1), at(b, k + 2, 1), x) * ar;
            }
            p += step;
            if (p >= b->frames) {
                if (b->looping)
                    p = fmod(p, (double)b->frames);
                else {
                    b->playing = 0;
                    b->pos = 0;
                    p = 0;
                    break;
                }
            }
        }
        b->fpos = p;
        b->gl = gl;
        b->gr = gr;
        if (!b->playing) {
            b->live = 0;
            s_live[v--] = s_live[--s_nlive];
        }
    }
    unlock();
}

/* ---- buffers ------------------------------------------------------------------------ */
/* the play cursor in bytes; called locked */
static DWORD cursor(dsbuf *b)
{
    uint64_t p;
    if (s_mixing && !(b->flags & DSBCAPS_PRIMARYBUFFER)) {
        p = (uint64_t)b->fpos * b->align;
        return (DWORD)(p < b->size ? p : 0);
    }
    if (!b->playing)
        return b->pos;
    p = b->pos + (uint64_t)(timeGetTime() - b->t0) * b->rate * b->align / 1000;
    p -= p % b->align;
    if (p >= b->size) {
        if (b->looping)
            p %= b->size;
        else {
            b->playing = 0;
            b->pos = 0;
            return 0;
        }
    }
    return (DWORD)p;
}

static void set_format(dsbuf *b, const WAVEFORMATEX *w)
{
    b->pcmfmt = w->wFormatTag == WAVE_FORMAT_PCM;
    b->ch = w->nChannels ? w->nChannels : 1;
    b->bits = w->wBitsPerSample;
    b->rate = w->nSamplesPerSec ? w->nSamplesPerSec : 22050;
    b->align = w->nBlockAlign ? w->nBlockAlign : (DWORD)(b->ch * b->bits / 8);
    if (!b->align)
        b->align = 1;
}

static HRESULT STDMETHODCALLTYPE b_qi(IDirectSoundBuffer *o, REFIID iid, void **out)
{
    (void)iid;
    ((dsbuf *)o)->refs++;
    *out = o;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE b_addref(IDirectSoundBuffer *o) { return ++((dsbuf *)o)->refs; }
static ULONG STDMETHODCALLTYPE b_release(IDirectSoundBuffer *o)
{
    dsbuf *b = (dsbuf *)o;
    if (b->refs && --b->refs)
        return b->refs;
    lock();
    unlive(b);
    b->playing = 0;
    unlock();
    free(b->pcm);
    free(b->mem);
    free(b);
    return 0;
}
static HRESULT STDMETHODCALLTYPE b_caps(IDirectSoundBuffer *o, LPDSBCAPS c)
{
    dsbuf *b = (dsbuf *)o;
    c->dwFlags = b->flags;
    c->dwBufferBytes = b->size;
    c->dwUnlockTransferRate = 0;
    c->dwPlayCpuOverhead = 0;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_pos(IDirectSoundBuffer *o, LPDWORD play, LPDWORD write)
{
    DWORD p;
    lock();
    p = cursor((dsbuf *)o);
    unlock();
    if (play)
        *play = p;
    if (write)
        *write = p;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_getfmt(IDirectSoundBuffer *o, LPWAVEFORMATEX w, DWORD n, LPDWORD got)
{
    dsbuf *b = (dsbuf *)o;
    WAVEFORMATEX f;
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = (WORD)b->ch;
    f.nSamplesPerSec = b->rate;
    f.nBlockAlign = (WORD)b->align;
    f.nAvgBytesPerSec = b->rate * b->align;
    f.wBitsPerSample = (WORD)b->bits;
    if (w)
        memcpy(w, &f, n < sizeof f ? n : sizeof f);
    if (got)
        *got = sizeof f;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_getvol(IDirectSoundBuffer *o, LPLONG v) { if (v) *v = ((dsbuf *)o)->vol; return S_OK; }
static HRESULT STDMETHODCALLTYPE b_getpan(IDirectSoundBuffer *o, LPLONG v) { if (v) *v = ((dsbuf *)o)->pan; return S_OK; }
static HRESULT STDMETHODCALLTYPE b_getfreq(IDirectSoundBuffer *o, LPDWORD v)
{
    dsbuf *b = (dsbuf *)o;
    if (v)
        *v = b->freq ? b->freq : b->rate;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_status(IDirectSoundBuffer *o, LPDWORD s)
{
    dsbuf *b = (dsbuf *)o;
    lock();
    cursor(b);
    if (s)
        *s = (b->playing ? DSBSTATUS_PLAYING : 0) | (b->playing && b->looping ? DSBSTATUS_LOOPING : 0);
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_init(IDirectSoundBuffer *o, LPDIRECTSOUND d, LPCDSBUFFERDESC desc)
{
    (void)o; (void)d; (void)desc;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_lock(IDirectSoundBuffer *o, DWORD off, DWORD n, LPVOID *p1, LPDWORD n1,
                                        LPVOID *p2, LPDWORD n2, DWORD fl)
{
    dsbuf *b = (dsbuf *)o;
    DWORD a;
    if (fl & DSBLOCK_FROMWRITECURSOR) {
        lock();
        off = cursor(b);
        unlock();
    }
    if (fl & DSBLOCK_ENTIREBUFFER) {
        off = 0;
        n = b->size;
    }
    off %= b->size ? b->size : 1;
    if (n > b->size)
        n = b->size;
    a = n < b->size - off ? n : b->size - off;
    *p1 = b->mem + off;
    if (n1)
        *n1 = a;
    if (p2)
        *p2 = n > a ? b->mem : NULL;
    if (n2)
        *n2 = n - a;
    return S_OK;
}
/* the buffer's samples, as floats, become what the mixer plays: PCM, 8-bit
 * unsigned or 16-bit signed */
static HRESULT STDMETHODCALLTYPE b_unlock(IDirectSoundBuffer *o, LPVOID a, DWORD na, LPVOID c, DWORD nc)
{
    dsbuf *b = (dsbuf *)o;
    DWORD frames, i, n;
    int k;
    float *pcm, *old;
    (void)a; (void)na; (void)c; (void)nc;
    if ((b->flags & DSBCAPS_PRIMARYBUFFER) || !b->pcmfmt || (b->bits != 8 && b->bits != 16))
        return S_OK;
    frames = b->size / b->align;
    n = frames * (DWORD)b->ch;
    pcm = (float *)malloc((n ? n : 1) * sizeof *pcm);
    if (!pcm)
        return E_OUTOFMEMORY;
    for (i = 0; i < frames; i++)
        for (k = 0; k < b->ch; k++) {
            const uint8_t *s = b->mem + i * b->align + (DWORD)k * (DWORD)(b->bits / 8);
            pcm[i * (DWORD)b->ch + (DWORD)k] = b->bits == 8 ? ((int)s[0] - 128) / 128.0f
                                                            : (int16_t)(s[0] | s[1] << 8) / 32768.0f;
        }
    lock();
    old = b->pcm;
    b->pcm = pcm;
    b->frames = frames;
    if (b->fpos >= frames)
        b->fpos = 0;
    unlock();
    free(old);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_play(IDirectSoundBuffer *o, DWORD r1, DWORD r2, DWORD fl)
{
    dsbuf *b = (dsbuf *)o;
    (void)r1; (void)r2;
    lock();
    if (!b->playing) {
        b->playing = 1;
        b->t0 = timeGetTime();
    }
    b->looping = (fl & DSBPLAY_LOOPING) != 0;
    if (b->pcm && !b->live && s_nlive < (int)(sizeof s_live / sizeof s_live[0])) {
        s_live[s_nlive++] = b;
        b->live = 1;
    }
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_setpos(IDirectSoundBuffer *o, DWORD p)
{
    dsbuf *b = (dsbuf *)o;
    lock();
    b->pos = p % (b->size ? b->size : 1);
    b->pos -= b->pos % b->align;
    b->t0 = timeGetTime();
    b->fpos = b->pos / b->align;
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_setfmt(IDirectSoundBuffer *o, LPCWAVEFORMATEX w)
{
    if (w)
        set_format((dsbuf *)o, w);
    return S_OK;
}
static LONG clampl(LONG v, LONG lo, LONG hi) { return v < lo ? lo : v > hi ? hi : v; }
static HRESULT STDMETHODCALLTYPE b_setvol(IDirectSoundBuffer *o, LONG v)
{
    lock();
    ((dsbuf *)o)->vol = clampl(v, -10000, 0);
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_setpan(IDirectSoundBuffer *o, LONG v)
{
    lock();
    ((dsbuf *)o)->pan = clampl(v, -10000, 10000);
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_setfreq(IDirectSoundBuffer *o, DWORD v)
{
    lock();
    ((dsbuf *)o)->freq = v ? (DWORD)clampl((LONG)v, 100, 200000) : 0;
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_stop(IDirectSoundBuffer *o)
{
    dsbuf *b = (dsbuf *)o;
    lock();
    b->pos = cursor(b);
    b->playing = 0;
    unlive(b);
    unlock();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE b_restore(IDirectSoundBuffer *o) { (void)o; return S_OK; }

static const struct IDirectSoundBufferVtbl s_bvt = {
    b_qi, b_addref, b_release, b_caps, b_pos, b_getfmt, b_getvol, b_getpan, b_getfreq, b_status,
    b_init, b_lock, b_play, b_setpos, b_setfmt, b_setvol, b_setpan, b_setfreq, b_stop, b_unlock, b_restore,
};

/* ---- the device ------------------------------------------------------------------------ */
typedef struct dsdev {
    const struct IDirectSoundVtbl *lpVtbl;
    ULONG refs;
} dsdev;

static HRESULT STDMETHODCALLTYPE d_qi(IDirectSound *o, REFIID iid, void **out)
{
    (void)iid;
    ((dsdev *)o)->refs++;
    *out = o;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE d_addref(IDirectSound *o) { return ++((dsdev *)o)->refs; }
static ULONG STDMETHODCALLTYPE d_release(IDirectSound *o)
{
    dsdev *d = (dsdev *)o;
    return d->refs ? --d->refs : 0;      /* the device stays: the mixer keeps running */
}
static HRESULT STDMETHODCALLTYPE d_create(IDirectSound *o, LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER *out, LPUNKNOWN unk)
{
    dsbuf *b;
    (void)o; (void)unk;
    *out = NULL;
    if (!desc)
        return E_INVALIDARG;
    b = (dsbuf *)calloc(1, sizeof *b);
    if (!b)
        return E_OUTOFMEMORY;
    b->lpVtbl = &s_bvt;
    b->refs = 1;
    b->flags = desc->dwFlags;
    b->size = desc->dwBufferBytes;
    if (b->flags & DSBCAPS_PRIMARYBUFFER) {
        static const WAVEFORMATEX f = { WAVE_FORMAT_PCM, 2, 22050, 88200, 4, 16, 0 };
        set_format(b, &f);
        b->size = 0x4000;
    } else {
        if (!desc->lpwfxFormat || !b->size) {
            free(b);
            return E_INVALIDARG;
        }
        set_format(b, desc->lpwfxFormat);
    }
    b->mem = (uint8_t *)calloc(1, b->size);
    if (!b->mem) {
        free(b);
        return E_OUTOFMEMORY;
    }
    *out = (LPDIRECTSOUNDBUFFER)b;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE d_caps(IDirectSound *o, LPDSCAPS c)
{
    (void)o;
    if (c && c->dwSize > 8)
        memset(&c->dwFlags, 0, c->dwSize - 4);
    if (c)
        c->dwFlags = 0x0F0F;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE d_dup(IDirectSound *o, LPDIRECTSOUNDBUFFER src, LPDIRECTSOUNDBUFFER *out)
{
    (void)o; (void)src;
    *out = NULL;
    return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE d_coop(IDirectSound *o, HWND h, DWORD l) { (void)o; (void)h; (void)l; return S_OK; }
static HRESULT STDMETHODCALLTYPE d_compact(IDirectSound *o) { (void)o; return S_OK; }
static HRESULT STDMETHODCALLTYPE d_getspk(IDirectSound *o, LPDWORD c) { (void)o; if (c) *c = 4; return S_OK; }
static HRESULT STDMETHODCALLTYPE d_setspk(IDirectSound *o, DWORD c) { (void)o; (void)c; return S_OK; }
static HRESULT STDMETHODCALLTYPE d_init(IDirectSound *o, const GUID *g) { (void)o; (void)g; return S_OK; }

static const struct IDirectSoundVtbl s_dvt = {
    d_qi, d_addref, d_release, d_create, d_caps, d_dup, d_coop, d_compact, d_getspk, d_setspk, d_init,
};

/* CoCreateInstance(CLSID_DirectSound): the device, and the output started
 * on first use (audio.c; BR_SFX=0 keeps it off: the clock is the cursor) */
HRESULT plat_dsound_create(REFIID iid, LPVOID *out)
{
    static dsdev dev = { &s_dvt, 0 };
    static int opened;
    (void)iid;
    if (!opened) {
        opened = 1;
        if (plat_audio_start()) {
            lock();
            s_mixing = 1;
            unlock();
        }
    }
    dev.refs++;
    *out = &dev;
    return S_OK;
}
