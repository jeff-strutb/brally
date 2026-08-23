/* br_pod.c -- POD archive reader, decompiled from BRD3D.dll (portable C99).
 *
 * Original functions: 0x100085F0 CleanupName, 0x10008750 GetNumForName,
 * 0x10008780 GetPodLength, 0x100087B0 ReadPod, 0x10008810 LoadPod.
 *
 * Deliberate deviations from the original, all for portability or safety:
 *   - integers are decoded byte-wise instead of by struct overlay, so the
 *     reader is endian- and alignment-agnostic;
 *   - CleanupName validates length *before* copying (the original copied
 *     first, so its "Memory Corrupted!" diagnostic fired after the overrun);
 *   - the bounds checks return instead of reporting and then indexing anyway.
 * Each of these is noted at the call site.
 */
#include "br_pod.h"

#include "br_path.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define POD_HEADER_SIZE 16
#define POD_ENTRY_SIZE  76

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Original: CleanupName @ 0x100085F0 (__stdcall, ret 8).
 * Uppercases into a fixed 64-byte field and NUL-pads the remainder. */
void BrPodCleanupName(const char *pszSrc, char *pszDst)
{
    char   szBase[BR_POD_NAME_LEN * 4];
    size_t i;

    memset(pszDst, 0, BR_POD_NAME_LEN);
    if (pszSrc == NULL)
        return;

    /* THE BASENAME STEP. This was missing.
     *
     * 0x100085F0 calls 0x10008B90 FIRST -- `call 0x10008b90` at 0x10008600,
     * before it measures anything -- so what it uppercases and length-checks is
     * the BASENAME, not the caller's string. Omitting that made this function
     * agree with the original only for inputs that contain no backslash, which
     * is every name in the retail archive and none of the interesting cases.
     * Caught by a pass that was reading 0x10008B90 for other reasons.
     *
     * The splitter is slice6_78's BrPodWriterMakeName (that same 0x10008B90),
     * reused rather than reimplemented -- one original address, one body. Its
     * first parameter is the original's dead `this`; the body never reads it.
     *
     * That routine carries a quirk of the original which now applies here too:
     * it never examines the LAST character, so a trailing backslash is not a
     * separator and "dir\\" cleans to "dir\\", not "". strrchr would differ. */
    if (strlen(pszSrc) >= sizeof szBase) {
        /* DEVIATION: the splitter writes an unbounded copy. Refuse rather than
         * overrun; the original would corrupt the destination here. */
        return;
    }
    BrPodWriterMakeName(NULL, pszSrc, szBase);

    /* DEVIATION: bounded up front. The original ran the copy first and only
     * then compared the length against 64, so its check could not prevent the
     * corruption it reported. */
    for (i = 0; i < BR_POD_NAME_LEN && szBase[i] != '\0'; i++)
        pszDst[i] = (char)toupper((unsigned char)szBase[i]);
}

int BrPodOpen(BrPod *pPod, const char *pszPath)
{
    unsigned char hdr[POD_HEADER_SIZE];
    unsigned char *pDir = NULL;
    uint32_t cEntries, offDir, i;

    memset(pPod, 0, sizeof(*pPod));

    pPod->pFile = fopen(pszPath, "rb");
    if (pPod->pFile == NULL)
        return 1;

    if (fread(hdr, 1, sizeof(hdr), pPod->pFile) != sizeof(hdr))
        goto fail;
    if (memcmp(hdr, "POD", 3) != 0 || hdr[3] != '\0')
        goto fail;

    cEntries = rd_u32le(hdr + 8);
    offDir   = rd_u32le(hdr + 12);
    if (cEntries == 0 || cEntries > 0x100000)
        goto fail;

    pDir = (unsigned char *)malloc((size_t)cEntries * POD_ENTRY_SIZE);
    if (pDir == NULL)
        goto fail;
    if (fseek(pPod->pFile, (long)offDir, SEEK_SET) != 0)
        goto fail;
    if (fread(pDir, POD_ENTRY_SIZE, cEntries, pPod->pFile) != cEntries)
        goto fail;

    pPod->aEntries = (BrPodEntry *)calloc(cEntries, sizeof(BrPodEntry));
    if (pPod->aEntries == NULL)
        goto fail;

    for (i = 0; i < cEntries; i++) {
        const unsigned char *e = pDir + (size_t)i * POD_ENTRY_SIZE;
        pPod->aEntries[i].offData = rd_u32le(e + 0);
        pPod->aEntries[i].cbData  = rd_u32le(e + 4);
        pPod->aEntries[i].unknown = rd_u32le(e + 8);
        memcpy(pPod->aEntries[i].szName, e + 12, BR_POD_NAME_LEN);
        pPod->aEntries[i].szName[BR_POD_NAME_LEN] = '\0';
    }
    pPod->cEntries = cEntries;
    free(pDir);
    return 0;

fail:
    free(pDir);
    BrPodClose(pPod);
    return 1;
}

void BrPodClose(BrPod *pPod)
{
    if (pPod->pFile != NULL)
        fclose(pPod->pFile);
    free(pPod->aEntries);
    memset(pPod, 0, sizeof(*pPod));
}

/* Original: GetNumForName @ 0x10008750. */
int BrPodGetNumForName(const BrPod *pPod, const char *pszName)
{
    char szWanted[BR_POD_NAME_LEN + 1];
    uint32_t i;

    memset(szWanted, 0, sizeof(szWanted));
    BrPodCleanupName(pszName, szWanted);

    for (i = 0; i < pPod->cEntries; i++) {
        if (strcmp(pPod->aEntries[i].szName, szWanted) == 0)
            return (int)i;
    }
    return -1;
}

/* Original: GetPodLength @ 0x10008780.
 * DEVIATION: returns 0 out of range; the original reported and indexed anyway. */
uint32_t BrPodGetLength(const BrPod *pPod, int iEntry)
{
    if (iEntry < 0 || (uint32_t)iEntry >= pPod->cEntries)
        return 0;
    return pPod->aEntries[iEntry].cbData;
}

/* Original: ReadPod @ 0x100087B0 -- fseek to offData, then read cbData. */
int BrPodRead(BrPod *pPod, int iEntry, void *pvBuffer)
{
    const BrPodEntry *pent;

    if (iEntry < 0 || (uint32_t)iEntry >= pPod->cEntries)
        return 1;

    pent = &pPod->aEntries[iEntry];
    if (fseek(pPod->pFile, (long)pent->offData, SEEK_SET) != 0)
        return 1;
    if (fread(pvBuffer, 1, pent->cbData, pPod->pFile) != pent->cbData)
        return 1;
    return 0;
}

/* Original: LoadPod @ 0x10008810 -- GetPodLength, malloc, ReadPod. */
void *BrPodLoad(BrPod *pPod, int iEntry, uint32_t *pcbOut)
{
    uint32_t cb;
    void *pv;

    cb = BrPodGetLength(pPod, iEntry);
    if (cb == 0)
        return NULL;

    pv = malloc(cb);
    if (pv == NULL)
        return NULL;

    if (BrPodRead(pPod, iEntry, pv) != 0) {
        free(pv);
        return NULL;
    }
    if (pcbOut != NULL)
        *pcbOut = cb;
    return pv;
}

#include "br_match.h"

/* WHAT IT DOES: writes a file name onto the POD object, in the buffer that
 * sits 32 bytes from the start. A missing name is ignored and the object is
 * left as it was. There is no length cap. */
/* @implements 0x10008B40 d3d BrPodSetName */
#ifdef BR_MATCHING_BUILD
typedef struct { const char *psz; } BrPodSetNameArg;
void BR_THISCALL1 BrPodSetName(void *pThis, BrPodSetNameArg a)
{
    if (a.psz != NULL)
        strcpy((char *)pThis + 0x20, a.psz);
}
#else
void BrPodSetName(void *pThis, const char *pszName)
{
    if (pszName != NULL)
        strcpy((char *)pThis + 0x20, pszName);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: identity function — returns its argument unchanged (fastcall). */
/* @implements 0x10008D50 glide BrPodIdentity */

int __fastcall BrPodIdentity(int param_1)

{
  return param_1;
}

/* WHAT IT DOES: no-op stub. */
/* @implements 0x10008D60 glide BrPodNop */

int BrPodNop(void)

{
  return;
}

#endif /* BR_MATCHING_BUILD */
