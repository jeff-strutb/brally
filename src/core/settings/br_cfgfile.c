/* br_cfgfile.c -- reads "BossRally.cfg".  See br_cfgfile.h for the format,
 * the ESP trace, the identification of `this`, and the two preserved bugs.
 *
 * RESPONSIBILITY: settings.  One function of the original lives here, Glide
 * 0x10063060 / D3D 0x10069FF0, plus the byte layout it demands.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_cfgfile.h"

#include <stdio.h>
#include <string.h>

/* The header's arithmetic, checked at compile time rather than trusted.
 * C99 has no _Static_assert and this tree stays on one standard, so it is
 * the negative-array-size trick test_layout.c established. */
typedef char br_cfgfile_assert_size
    [(BR_CTRLCFG_FILE_SIZE == 0x878) ? 1 : -1];
typedef char br_cfgfile_assert_profile
    [(sizeof(BrCtrlProfile) == 0xA8) ? 1 : -1];

/* ======================================================================
 * Little-endian codecs.
 *
 * The original does none of this: it freads raw bytes over the live object
 * and the dwords land correctly because the host is 32-bit LE x86.  That is
 * not available here -- BrCtrlCfg carries a host pointer, so its tail is not
 * at the original's offsets -- so every integer crosses the file boundary
 * byte-wise, per CONVENTIONS.md.  On any little-endian host the result is
 * identical to the original's; on a big-endian one it is the same FILE,
 * which is what portability means for a format.
 * ====================================================================== */

static uint32_t BrCfgLoad32(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void BrCfgStore32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static uint16_t BrCfgLoad16(const unsigned char *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static void BrCfgStore16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

/* ======================================================================
 * One field, one fread.
 *
 * Every read in 0x10063060 is `fread(p, size, 1, fp)` with the result
 * compared against 1, so a field either arrives whole or the load fails on
 * it.  Keeping one call per field keeps that boundary exactly where the
 * original puts it: `fread(p, 1, size, fp)` would report a partial field as
 * a partial success and move the failure to the next check.
 *
 * The scratch buffer is sized for the largest field, +0x3B8's 0x400 bytes.
 * ====================================================================== */

#define BR_CFG_MAX_FIELD  0x400

static int BrCfgReadRaw(FILE *pFile, unsigned char *pBuf, size_t cb)
{
    return fread(pBuf, cb, 1, pFile) == 1;
}

static int BrCfgReadU32(FILE *pFile, uint32_t *pOut)
{
    unsigned char ab[4];

    if (!BrCfgReadRaw(pFile, ab, sizeof ab))
        return 0;
    *pOut = BrCfgLoad32(ab);
    return 1;
}

static int BrCfgReadI32(FILE *pFile, int32_t *pOut)
{
    uint32_t v;

    if (!BrCfgReadU32(pFile, &v))
        return 0;
    /* The original stores the four bytes and the field is read back as a
     * signed dword by its consumers; the round trip through uint32_t is the
     * portable spelling of the same bit pattern. */
    *pOut = (int32_t)v;
    return 1;
}

/* n dwords in ONE fread of n*4 bytes -- the original's `fread(p, 0x104, 1,
 * fp)` and friends, not n separate reads. */
static int BrCfgReadU32Array(FILE *pFile, uint32_t *pOut, size_t n)
{
    unsigned char ab[BR_CFG_MAX_FIELD];
    size_t        i;

    if (!BrCfgReadRaw(pFile, ab, n * 4u))
        return 0;
    for (i = 0; i < n; ++i)
        pOut[i] = BrCfgLoad32(ab + i * 4u);
    return 1;
}

/* One 0xA8-byte profile: BR_CTRL_ACTIONS * 3 little-endian 16-bit entries,
 * in the order they sit in memory (action-major, slot-minor). */
static int BrCfgReadProfile(FILE *pFile, BrCtrlProfile *pOut)
{
    unsigned char ab[sizeof(BrCtrlProfile)];
    int           a, s;

    if (!BrCfgReadRaw(pFile, ab, sizeof ab))
        return 0;
    for (a = 0; a < BR_CTRL_ACTIONS; ++a) {
        for (s = 0; s < 3; ++s)
            pOut->e[a][s] = BrCfgLoad16(ab + (size_t)(a * 3 + s) * 2u);
    }
    return 1;
}

/* ======================================================================
 * 0x10063060 -- load the settings file over an existing config object.
 * ====================================================================== */

/* WHAT IT DOES: loads the player's control settings from disk on top of the
 * settings already in memory. It checks the file's magic word and version
 * first, and works through a temporary copy so a partly-read file cannot
 * leave the live settings half-updated. */
/* @implements 0x10063060 glide BrCtrlCfgReadFile */
int32_t BrCtrlCfgReadFile(BrCtrlCfg *pThis, const char *pszPath)
{
    /* The stack temporary at B+0x18.  `BrCtrlCfg`, not 0x874 bytes: the
     * original's literal under-allocates it here by the width of a pointer
     * minus four. */
    BrCtrlCfg     tmp;
    unsigned char aMagic[BR_CTRLCFG_MAGIC_SIZE];   /* B+0x14 */
    uint32_t      version;                         /* B+0x10 */
    FILE         *pFile;

    /* DEVIATION: the original has no null guard and would fault.  Nothing
     * downstream depends on the fault, and a test needs to be able to ask. */
    if (pThis == NULL || pszPath == NULL)
        return 0;

    pFile = fopen(pszPath, BR_CTRLCFG_MODE_READ);
    if (pFile == NULL)
        return 0;               /* 0x10063098 -- straight to the epilogue */

    /* 0x100630A2 and 0x100630B7.  Both `lea ecx` displacements resolve to
     * the same B+0x18: a `push ebx` sits between them.  So this is one
     * temporary, constructed and then assigned from *pThis -- and
     * BrCtrlCfgCopy rebuilds tmp.pActive to point inside tmp, which is why
     * the copy is not a memcpy. */
    BrCtrlCfgCtor(&tmp);
    BrCtrlCfgCopy(&tmp, pThis);

    /* --- header ------------------------------------------------------- */

    if (!BrCfgReadRaw(pFile, aMagic, sizeof aMagic))
        goto fail;
    /* The original is `strncmp(buf, "RCfg", strlen("RCfg"))`.  memcmp is
     * identical here and needs no terminator: "RCfg" contains no NUL in
     * [0,4), so strncmp can only stop early at a mismatch, which is the same
     * place memcmp stops. */
    if (memcmp(aMagic, BR_CTRLCFG_MAGIC, sizeof aMagic) != 0)
        goto fail;

    if (!BrCfgReadU32(pFile, &version))
        goto fail;
    /* 0x1006311D: `cmp dword ptr [esp+0x10], 2` / `jne`.  Equality, so the
     * dword's signedness never arises. */
    if (version != BR_CTRLCFG_VERSION)
        goto fail;

    /* --- the body, in the original's order ---------------------------- */

    if (!BrCfgReadI32(pFile, &pThis->f2A8)) goto fail;          /* 0x2A8 */
    if (!BrCfgReadI32(pFile, &pThis->f2AC)) goto fail;          /* 0x2AC */
    if (!BrCfgReadI32(pFile, &pThis->f2B0)) goto fail;          /* 0x2B0 */

    if (!BrCfgReadU32Array(pFile, pThis->f2B4,
                           sizeof pThis->f2B4 / sizeof pThis->f2B4[0]))
        goto fail;                                              /* 0x2B4 */
    if (!BrCfgReadU32Array(pFile, pThis->f3B8,
                           sizeof pThis->f3B8 / sizeof pThis->f3B8[0]))
        goto fail;                                              /* 0x3B8 */

    if (!BrCfgReadI32(pFile, &pThis->f7B8)) goto fail;          /* 0x7B8 */
    if (!BrCfgReadI32(pFile, &pThis->f7BC)) goto fail;          /* 0x7BC */
    if (!BrCfgReadI32(pFile, &pThis->f7C0)) goto fail;          /* 0x7C0 */
    if (!BrCfgReadI32(pFile, &pThis->f7C4)) goto fail;          /* 0x7C4 */

    if (!BrCfgReadU32Array(pFile, pThis->f7C8,
                           sizeof pThis->f7C8 / sizeof pThis->f7C8[0]))
        goto fail;                                              /* 0x7C8 */

    if (!BrCfgReadI32(pFile, &pThis->f7D8)) goto fail;          /* 0x7D8 */
    if (!BrCfgReadI32(pFile, &pThis->f7DC)) goto fail;          /* 0x7DC */
    if (!BrCfgReadI32(pFile, &pThis->f7E0)) goto fail;          /* 0x7E0 */
    if (!BrCfgReadI32(pFile, &pThis->f7E4)) goto fail;          /* 0x7E4 */
    if (!BrCfgReadI32(pFile, &pThis->f7E8)) goto fail;          /* 0x7E8 */
    if (!BrCfgReadI32(pFile, &pThis->f7EC)) goto fail;          /* 0x7EC */
    if (!BrCfgReadI32(pFile, &pThis->f7F0)) goto fail;          /* 0x7F0 */
    if (!BrCfgReadI32(pFile, &pThis->f7F4)) goto fail;          /* 0x7F4 */
    if (!BrCfgReadI32(pFile, &pThis->f7F8)) goto fail;          /* 0x7F8 */
    if (!BrCfgReadI32(pFile, &pThis->f7FC)) goto fail;          /* 0x7FC */
    if (!BrCfgReadI32(pFile, &pThis->f800)) goto fail;          /* 0x800 */
    if (!BrCfgReadI32(pFile, &pThis->f804)) goto fail;          /* 0x804 */
    if (!BrCfgReadI32(pFile, &pThis->f808)) goto fail;          /* 0x808 */
    if (!BrCfgReadI32(pFile, &pThis->f80C)) goto fail;          /* 0x80C */

    if (!BrCfgReadU32Array(pFile, pThis->f810,
                           sizeof pThis->f810 / sizeof pThis->f810[0]))
        goto fail;                                              /* 0x810 */
    if (!BrCfgReadU32Array(pFile, pThis->f830,
                           sizeof pThis->f830 / sizeof pThis->f830[0]))
        goto fail;                                              /* 0x830 */

    if (!BrCfgReadI32(pFile, &pThis->f870)) goto fail;          /* 0x870 */

    /* +0x2A0 goes here, immediately before the four profile blocks that
     * cover [0, 0x2A0).  The low part of the object is read LAST. */
    if (!BrCfgReadI32(pFile, &pThis->active)) goto fail;        /* 0x2A0 */

    if (!BrCfgReadProfile(pFile, &pThis->profile[0])) goto fail;  /* 0x000 */
    if (!BrCfgReadProfile(pFile, &pThis->profile[1])) goto fail;  /* 0x0A8 */
    if (!BrCfgReadProfile(pFile, &pThis->profile[2])) goto fail;  /* 0x150 */
    if (!BrCfgReadProfile(pFile, &pThis->profile[3])) goto fail;  /* 0x1F8 */

    /* 0x10063472.  pActive is deliberately NOT rebuilt from the `active`
     * just loaded -- see BUG 2 in the header.  The original does not even
     * have an instruction here; the arm is fclose, destroy tmp, return 1. */
    fclose(pFile);
    return 1;

fail:
    /* 0x1006343D..0x1006345A, in the original's order and with the original's
     * result.  The assignment restores the settings the load was about to
     * damage and BrCtrlCfgInit -- which writes every field of the object --
     * then throws that restore away.  Both calls are transcribed because the
     * pair IS the behaviour: a bad config file resets the player's settings.
     * See BUG 1 in the header.  Do not "simplify" this to the Init alone;
     * the point of the file is to record that the original tried. */
    BrCtrlCfgCopy(pThis, &tmp);
    fclose(pFile);
    BrCtrlCfgInit(pThis);
    return 0;
}

/* ======================================================================
 * The layout, as bytes.  Not a decompilation -- see the header.
 * ====================================================================== */

static void BrCfgPutU32Array(unsigned char **ppOut, const uint32_t *pIn,
                             size_t n)
{
    size_t i;

    for (i = 0; i < n; ++i)
        BrCfgStore32(*ppOut + i * 4u, pIn[i]);
    *ppOut += n * 4u;
}

static void BrCfgPutI32(unsigned char **ppOut, int32_t v)
{
    BrCfgStore32(*ppOut, (uint32_t)v);
    *ppOut += 4;
}

static void BrCfgPutProfile(unsigned char **ppOut, const BrCtrlProfile *pIn)
{
    int a, s;

    for (a = 0; a < BR_CTRL_ACTIONS; ++a) {
        for (s = 0; s < 3; ++s)
            BrCfgStore16(*ppOut + (size_t)(a * 3 + s) * 2u, pIn->e[a][s]);
    }
    *ppOut += sizeof(BrCtrlProfile);
}

int BrCtrlCfgFileEncode(unsigned char *pOut, size_t cbOut,
                        const BrCtrlCfg *pIn)
{
    unsigned char *p = pOut;

    if (pOut == NULL || pIn == NULL || cbOut < (size_t)BR_CTRLCFG_FILE_SIZE)
        return -1;

    memcpy(p, BR_CTRLCFG_MAGIC, BR_CTRLCFG_MAGIC_SIZE);
    p += BR_CTRLCFG_MAGIC_SIZE;
    BrCfgStore32(p, BR_CTRLCFG_VERSION);
    p += 4;

    BrCfgPutI32(&p, pIn->f2A8);
    BrCfgPutI32(&p, pIn->f2AC);
    BrCfgPutI32(&p, pIn->f2B0);
    BrCfgPutU32Array(&p, pIn->f2B4, sizeof pIn->f2B4 / sizeof pIn->f2B4[0]);
    BrCfgPutU32Array(&p, pIn->f3B8, sizeof pIn->f3B8 / sizeof pIn->f3B8[0]);
    BrCfgPutI32(&p, pIn->f7B8);
    BrCfgPutI32(&p, pIn->f7BC);
    BrCfgPutI32(&p, pIn->f7C0);
    BrCfgPutI32(&p, pIn->f7C4);
    BrCfgPutU32Array(&p, pIn->f7C8, sizeof pIn->f7C8 / sizeof pIn->f7C8[0]);
    BrCfgPutI32(&p, pIn->f7D8);
    BrCfgPutI32(&p, pIn->f7DC);
    BrCfgPutI32(&p, pIn->f7E0);
    BrCfgPutI32(&p, pIn->f7E4);
    BrCfgPutI32(&p, pIn->f7E8);
    BrCfgPutI32(&p, pIn->f7EC);
    BrCfgPutI32(&p, pIn->f7F0);
    BrCfgPutI32(&p, pIn->f7F4);
    BrCfgPutI32(&p, pIn->f7F8);
    BrCfgPutI32(&p, pIn->f7FC);
    BrCfgPutI32(&p, pIn->f800);
    BrCfgPutI32(&p, pIn->f804);
    BrCfgPutI32(&p, pIn->f808);
    BrCfgPutI32(&p, pIn->f80C);
    BrCfgPutU32Array(&p, pIn->f810, sizeof pIn->f810 / sizeof pIn->f810[0]);
    BrCfgPutU32Array(&p, pIn->f830, sizeof pIn->f830 / sizeof pIn->f830[0]);
    BrCfgPutI32(&p, pIn->f870);
    BrCfgPutI32(&p, pIn->active);
    BrCfgPutProfile(&p, &pIn->profile[0]);
    BrCfgPutProfile(&p, &pIn->profile[1]);
    BrCfgPutProfile(&p, &pIn->profile[2]);
    BrCfgPutProfile(&p, &pIn->profile[3]);

    return (int)(p - pOut);
}
