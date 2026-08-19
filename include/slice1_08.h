/* slice1_08.h -- Boss Rally (BRD3D.dll) slice-1 a later pass.
 *
 * Address range 0x1006C9A0 .. 0x10073320.
 *
 * Two unrelated things live in this range:
 *
 *   1) 0x1006C9A0  a plane-evaluation leaf used by the ground/collision probe.
 *
 *   2) 0x100722D0 .. 0x10073060  the sound module. It is a thin wrapper over
 *      DirectSound. The identification is not a guess: the vtable slots used
 *      line up exactly with IDirectSound / IDirectSoundBuffer, and the
 *      magic numbers confirm it --
 *          -10000            DSBVOLUME_MIN            (0x100724D0)
 *          dwSize = 0x14     sizeof(DSBUFFERDESC)     (0x100722D0)
 *          dwSize = 0x14     sizeof(DSBCAPS)          (0x100722D0)
 *          status & 1        DSBSTATUS_PLAYING        (0x10072A00)
 *          caps  & 4         DSBCAPS_LOCHARDWARE      (0x100722D0)
 *          0x000100E2        STATIC|CTRLDEFAULT|GETCURRENTPOSITION2
 *
 *      Because DirectSound is not portable, the COM objects are modelled here
 *      as explicit vtable structs whose member OFFSETS match the original
 *      interfaces. Only the slots the original actually calls are typed; the
 *      rest are `void *` placeholders that exist purely to keep the offsets
 *      right. This is a DEVIATION in representation only -- the call sequence,
 *      the argument values and the error handling are unchanged.
 */
#ifndef SLICE1_08_H
#define SLICE1_08_H

#include <stdint.h>
#include "br_vec.h"

/* ------------------------------------------------------------------ */
/* 0x1006C9A0 -- plane evaluation                                      */
/* ------------------------------------------------------------------ */

/* Returns  dot(*pN, *pP) + d  -- the signed distance of pP from the plane
 * (n, d) when n is unit length.
 *
 * NOTE the argument order: the plane NORMAL and the plane CONSTANT are two
 * separate arguments even though every caller has them adjacent in one
 * 16-byte record (see BrCollPlane below); the caller at 0x1006F17D loads
 * `d` out of the record itself and passes it by value. Do not "tidy" this
 * into a single plane pointer -- the record pointer is arg1 and `d` is arg2.
 *
 * The original accumulates as ((Py*Ny + Pz*Nz) + Nx*Px) + d, which is what
 * is written below. */
float BrPlaneEval(const BrVec3 *pN, float d, const BrVec3 *pP);

/* ------------------------------------------------------------------ */
/* Collision-plane record -- layout only, no code (see report)         */
/* ------------------------------------------------------------------ */

/* 32-byte record built by 0x1006F720 and consumed by 0x1006F0C0.
 * Recorded here because the layout is fully established even though those
 * two functions are NOT ported (their callees 0x10002DE0 / 0x10002EF0 /
 * 0x1006C740 / 0x100747C0 fall outside this work packet).
 *
 * The grid holds 4 cells of 150 records each (4800 bytes/cell) at
 * 0x11750338, with the per-cell record count as a u16 at 0x117554A0. */
typedef struct BrCollPlane {
    float     nx, ny, nz;   /* +0x00 unit normal, = normalise((v1-v0) x (v2-v0)) */
    float     d;            /* +0x0C  = -dot(n, *pV0) */
    BrVec3   *pV0;          /* +0x10 */
    BrVec3   *pV1;          /* +0x14 */
    BrVec3   *pV2;          /* +0x18 */
    uint16_t  tri;          /* +0x1C triangle index */
    uint8_t   flags;        /* +0x1E per-triangle surface byte, masked with 7 */
    uint8_t   pad1F;        /* +0x1F */
} BrCollPlane;

/* ------------------------------------------------------------------ */
/* DirectSound interface models                                        */
/* ------------------------------------------------------------------ */

typedef struct BrDSBuffer BrDSBuffer;
typedef struct BrDSound   BrDSound;

/* sizeof(DSBUFFERDESC) == 0x14; the original hardcodes that in dwSize. */
typedef struct BrDSBufferDesc {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwBufferBytes;
    uint32_t dwReserved;
    void    *lpwfxFormat;
} BrDSBufferDesc;

/* sizeof(DSBCAPS) == 0x14. Only dwFlags is read back. */
typedef struct BrDSBCaps {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwBufferBytes;
    uint32_t dwUnlockTransferRate;
    uint32_t dwPlayCpuOverhead;
} BrDSBCaps;

/* Buffer creation flags the original passes (0x100722D0). */
#define BR_DSBCAPS_LOCHARDWARE   0x00000004u
#define BR_DSBSTATUS_PLAYING     0x00000001u
#define BR_DSBVOLUME_MIN         (-10000)
#define BR_SND_DESC_FLAGS        0x000100E2u   /* pVoice->f28 == 0 */
#define BR_SND_DESC_FLAGS_ALT    0x000140E2u   /* pVoice->f28 != 0 */

typedef struct BrDSBufferVtbl {
    void   *QueryInterface;                                     /* +0x00 */
    void   *AddRef;                                             /* +0x04 */
    int32_t (*Release)(BrDSBuffer *);                           /* +0x08 */
    int32_t (*GetCaps)(BrDSBuffer *, BrDSBCaps *);              /* +0x0C */
    void   *GetCurrentPosition;                                 /* +0x10 */
    void   *GetFormat;                                          /* +0x14 */
    void   *GetVolume;                                          /* +0x18 */
    void   *GetPan;                                             /* +0x1C */
    void   *GetFrequency;                                       /* +0x20 */
    int32_t (*GetStatus)(BrDSBuffer *, uint32_t *);             /* +0x24 */
    void   *Initialize;                                         /* +0x28 */
    int32_t (*Lock)(BrDSBuffer *, uint32_t, uint32_t,
                    void **, uint32_t *, void **, uint32_t *,
                    uint32_t);                                  /* +0x2C */
    int32_t (*Play)(BrDSBuffer *, uint32_t, uint32_t, uint32_t);/* +0x30 */
    int32_t (*SetCurrentPosition)(BrDSBuffer *, uint32_t);      /* +0x34 */
    void   *SetFormat;                                          /* +0x38 */
    int32_t (*SetVolume)(BrDSBuffer *, int32_t);                /* +0x3C */
    int32_t (*SetPan)(BrDSBuffer *, int32_t);                   /* +0x40 */
    int32_t (*SetFrequency)(BrDSBuffer *, uint32_t);            /* +0x44 */
    int32_t (*Stop)(BrDSBuffer *);                              /* +0x48 */
    int32_t (*Unlock)(BrDSBuffer *, void *, uint32_t,
                      void *, uint32_t);                        /* +0x4C */
    void   *Restore;                                            /* +0x50 */
} BrDSBufferVtbl;

struct BrDSBuffer { const BrDSBufferVtbl *pVtbl; };

typedef struct BrDSoundVtbl {
    void   *QueryInterface;                                     /* +0x00 */
    void   *AddRef;                                             /* +0x04 */
    void   *Release;                                            /* +0x08 */
    int32_t (*CreateSoundBuffer)(BrDSound *, const BrDSBufferDesc *,
                                 BrDSBuffer **, void *);        /* +0x0C */
} BrDSoundVtbl;

struct BrDSound { const BrDSoundVtbl *pVtbl; };

/* ------------------------------------------------------------------ */
/* The voice object                                                    */
/* ------------------------------------------------------------------ */

/* One playable sound. Offsets in the original are noted per field; the C
 * struct does NOT reproduce them (there are large unmapped gaps at
 * +0x2C..+0x9B and +0x A0..+0x1A7 whose contents this packet never touches),
 * so this is a DEVIATION in layout only.
 *
 * f10/f14 are in the engine's own units where 400 is the neutral point:
 *   pan    = (f10 - 400) * 10   -> f10 in [0,800] gives DSBPAN  [-4000,4000]
 *   volume = (f14*master/255 - 400) * 10
 * Neither is clamped by the engine; DirectSound does the clamping. */
typedef struct BrSndVoice BrSndVoice;
struct BrSndVoice {
    void       *pData;        /* +0x00  sample bytes (GlobalAlloc'd) */
    uint32_t    nDataBytes;   /* +0x04  also used as dwBufferBytes */
    void       *pFormat;      /* +0x08  WAVEFORMATEX* (GlobalAlloc'd) */
    uint32_t    f0C;          /* +0x0C  playback rate -> SetFrequency */
    int32_t     f10;          /* +0x10  pan, 400 = centre */
    int32_t     f14;          /* +0x14  volume, 400 = unity */
    int32_t     f18;          /* +0x18  non-zero -> DSBPLAY_LOOPING */
    int32_t     f1C;          /* +0x1C  1 while started */
    int32_t     f24;          /* +0x24  set from DSBCAPS_LOCHARDWARE */
    int32_t     f28;          /* +0x28  selects the alternate desc flags */
    BrDSBuffer *pBuf;         /* +0x9C */
    BrSndVoice *pNext;        /* +0x1A8 singly-linked chain */
};

/* ------------------------------------------------------------------ */
/* Module globals                                                      */
/* ------------------------------------------------------------------ */

/* 0x100BBAE0 -- a BYTE master volume, 0..255. 0 means "hard mute": the
 * original then passes DSBVOLUME_MIN and skips the scaling entirely. */
extern uint8_t BrSndMasterVolume;

/* The three "is sound usable" gates. Every entry point tests all three and
 * returns 1 (the SUCCESS code for those entry points -- see below) when any
 * is zero, i.e. a silent no-op. */
/* 0x100B5DE8 (Glide 0x100B55F0) -- ESTABLISHED: this is `PlaySFX=` out of
 * BossRally.ini. 0x100083CD does `g = atoi(value)` after matching the key,
 * so it is a plain enable/disable and zero silences the whole subsystem. */
extern int32_t   BrSndG0B5DE8;
extern BrDSound *BrSndPDS;        /* 0x118290F8 -- the IDirectSound */
/* 0x118290FC (Glide 0x1184C45C) -- ESTABLISHED: the init guard. 0x10073560
 * (Glide 0x1006C4D0) does `if (++g != 1) return 1` before creating the
 * device, so it is a one-shot counter and non-zero means "initialised". */
extern void     *BrSndG18290FC;

/* 0x100B5DF0 -- the voice table, indexed  slot + group*18.
 *
 * CORRECTED: this said 24 groups, and it is 26. The table runs
 * 0x100B5DF0..0x100B6540 (Glide 0x100B55F8..0x100B5D48), which is 1872 bytes
 * == 26 * 18 * 4, and 0x100B6540 is where the parallel bank table starts --
 * so both ends are pinned, in both builds, by tables that abut exactly.
 * Groups 24 and 25 are the two extra per-car engine layers ("<cc>h.wav" and
 * "<cc>r.wav"); 0x10073080 writes their bank entries and 0x10072E60 plays
 * group 25 by name. At 24 the bounds check in BrSndPlayEx rejected every
 * index from group 24 upward, so both engine layers were silently dropped.
 * See br_sfx.h, which owns the table's contents.
 *
 * A row is really `{ void *aSlot[16]; double baseRate; }` -- 16 dwords then
 * an 8-byte rate, which is where the 18-dword stride comes from and why only
 * slots 0..14 are ever used. */
#define BR_SND_SLOTS_PER_GROUP 18
#define BR_SND_GROUPS          26
extern BrSndVoice *BrSndVoices[BR_SND_GROUPS * BR_SND_SLOTS_PER_GROUP];

/* 0x100B6540 / 0x100B6C00 / 0x100B6C48 -- three parallel 15-entry tables of
 * indices into the sample-name table at 0x100B84F4. 0x100730A0 turns each
 * into a filename: "sfx/" + name + ".wav" / "h.wav" / "r.wav" respectively. */
#define BR_SND_BANK_SLOTS 15
typedef struct BrSndBank {
    int32_t aName0[BR_SND_BANK_SLOTS];   /* 0x100B6540 */
    int32_t aName1[BR_SND_BANK_SLOTS];   /* 0x100B6C00 */
    int32_t aName2[BR_SND_BANK_SLOTS];   /* 0x100B6C48 */
} BrSndBank;

/* DEVIATION: the original releases pData / pFormat / the voice itself with
 * GlobalHandle + GlobalUnlock + GlobalFree (KERNEL32). Portably that is a
 * single free, routed through this hook so tests can observe it. */
typedef void (*BrSndFreeFn)(void *p);
extern BrSndFreeFn BrSndFreeHook;

/* ------------------------------------------------------------------ */
/* Functions                                                           */
/* ------------------------------------------------------------------ */

/* 0x100722D0  create the DirectSound buffer for pVoice and upload pData.
 * Returns the DirectSound HRESULT: 0 == success. On any failure the buffer
 * is released and pVoice->pBuf set to NULL.
 *
 * GOTCHA: on the "Unlock failed" path the original overwrites the saved
 * failure code with the HRESULT of the SECOND Unlock, so a create that
 * failed can still return 0. Reproduced. */
int32_t BrSndVoiceCreate(BrSndVoice *pVoice);

/* 0x10072450  append pNode to the end of the chain rooted at pHead.
 * Clears pNode->pNext and pNode->f1C first. Always returns 0.
 * Argument order is (head, node) -- head first. */
int32_t BrSndVoiceAppend(BrSndVoice *pHead, BrSndVoice *pNode);

/* 0x10072490  SetPan((f10 - 400) * 10). Returns the HRESULT. */
int32_t BrSndVoiceApplyPan(BrSndVoice *pVoice);

/* 0x100724B0  SetFrequency(f0C). Returns the HRESULT. */
int32_t BrSndVoiceApplyFreq(BrSndVoice *pVoice);

/* 0x100724D0  SetVolume. If BrSndMasterVolume is 0 the volume is forced to
 * DSBVOLUME_MIN; otherwise it is (f14*master/255 - 400)*10 where the divide
 * is UNSIGNED (the original uses the 0x80808081 >> 7 reciprocal trick).
 * Returns the HRESULT. */
int32_t BrSndVoiceApplyVolume(BrSndVoice *pVoice);

/* 0x10072520  Release the buffer and NULL it. Always returns 0. */
int32_t BrSndVoiceRelease(BrSndVoice *pVoice);

/* 0x10072550  Stop, but only if f1C is set; clears f1C on success.
 * Returns the HRESULT (0 when f1C was already clear). */
int32_t BrSndVoiceStop(BrSndVoice *pVoice);

/* 0x10072820  derive pan and volume from a packed pair and apply both.
 *
 *   hi = min(packed >> 16, 32), lo = min(packed & 0xFFFF, 32)
 *   f14 = max(hi,lo) * 400 / 32
 *   f10 = 400 + (lo - hi) * 400 / max(hi,lo)     [skipped when max is 0]
 *
 * so f10 sweeps 0..800 (hard left .. hard right) and f14 0..400.
 *
 * GOTCHA: the return convention here is INVERTED relative to every other
 * function in this module -- 1 means success (and is also what you get when
 * sound is disabled), 0 means failure, including the pVoice == NULL case. */
int32_t BrSndVoiceSetLevels(BrSndVoice *pVoice, uint32_t packed);

/* 0x10072A00  start playback. If the buffer already reports
 * DSBSTATUS_PLAYING it merely rewinds to offset 0 and returns -- note it
 * does NOT set f1C on that path. Otherwise Play(0,0,f18?1:0) and, on
 * success, f1C = 1. Returns the HRESULT: 0 == success. */
int32_t BrSndVoiceStart(BrSndVoice *pVoice);

/* 0x100729E0  f18 = loop, then BrSndVoiceStart. Returns the HRESULT. */
int32_t BrSndVoiceSetLoopAndStart(BrSndVoice *pVoice, int32_t loop);

/* 0x10072A90  look up BrSndVoices[slot + group*18], set its levels from
 * `packed`, then start it with `loop`.
 * Returns 1 on success / when sound is disabled, 0 on failure. */
int32_t BrSndPlayEx(int32_t group, int32_t slot, uint32_t packed, int32_t loop);

/* 0x10072A70  BrSndPlayEx(group, 1, packed, loop) -- slot is hardcoded to 1. */
int32_t BrSndPlayGroup(int32_t group, uint32_t packed, int32_t loop);

/* 0x10072AF0  BrSndPlayGroup(group, packed, 0). */
int32_t BrSndPlaySimple(int32_t group, uint32_t packed);

/* 0x10072BF0  Stop every voice in the chain hanging off pHead.
 * pHead itself is NOT stopped. Always returns 0. */
int32_t BrSndVoiceStopChain(BrSndVoice *pHead);

/* 0x10072C20  detach and destroy the whole chain hanging off pHead:
 * release the buffer, free pFormat, free pData, free the node.
 * pHead itself is NOT freed; its pNext is cleared first. Always returns 0. */
int32_t BrSndVoiceFreeChain(BrSndVoice *pHead);

/* 0x10073060  zero all three 15-entry name-index tables. */
void BrSndBankReset(BrSndBank *pBank);

#endif /* SLICE1_08_H */
