/* br_pod.h -- POD archive reader (portable).
 *
 * Decompiled from BRD3D.dll and rewritten for portability. The original is
 * C++ thiscall with virtual dispatch; this is plain C99 with an explicit
 * vtable-free interface, since nothing in the game overrides these.
 *
 * On-disk format, confirmed against BossRally.pod from the retail disc:
 *
 *   header (16 bytes, little-endian)
 *     +0x00  char  magic[4]    "POD\0"
 *     +0x04  u32   unknown     (0x1F4 in the retail archive; purpose unknown)
 *     NOTE: the header's 4th magic byte is never assigned by the writer either.
 *     Do not validate it as a fixed 0.
 *     +0x08  u32   cEntries
 *     +0x0C  u32   offDirectory
 *
 *   directory entry (76 bytes each, at offDirectory)
 *     +0x00  u32   offData
 *     +0x04  u32   cbData
 *     +0x08  u8    flagA       written by Add() from its own argument
 *     +0x09  u8    flagB       written by Add() from a second argument
 *     +0x0A  u8[2] NEVER WRITTEN -- stack garbage in a freshly built archive
 *                              (retail files happen to carry 0)
 *     +0x0C  char  szName[64]  NUL-padded, uppercased by the original writer
 *
 * The 76-byte stride was first derived from index arithmetic in ReadPod
 * (lea eax,[esi+esi*8] / lea ecx,[esi+eax*2] => idx*19 dwords) and then
 * confirmed against the retail file.
 *
 * All integers are read byte-wise, so this is endian-agnostic by construction
 * -- which matters because the N64 build of the same game is big-endian.
 */
#ifndef BR_POD_H
#define BR_POD_H

#include <stdio.h>
#include <stdint.h>

#define BR_POD_NAME_LEN 64

typedef struct BrPodEntry {
    uint32_t offData;
    uint32_t cbData;
    uint32_t unknown;
    char     szName[BR_POD_NAME_LEN + 1];   /* +1 so it is always terminated */
} BrPodEntry;

typedef struct BrPod {
    FILE       *pFile;
    uint32_t    cEntries;
    BrPodEntry *aEntries;
} BrPod;

/* Returns 0 on success, non-zero on failure. */
int   BrPodOpen(BrPod *pPod, const char *pszPath);
void  BrPodClose(BrPod *pPod);

/* Original: CleanupName @ 0x100085F0 -- uppercase and NUL-pad to 64 bytes.
 * The original copied before validating length; this checks first. */
void  BrPodCleanupName(const char *pszSrc, char *pszDst);

/* Original: GetNumForName @ 0x10008750. Returns -1 and reports if absent. */
int      BrPodGetNumForName(const BrPod *pPod, const char *pszName);

/* Original: GetPodLength @ 0x10008780. Returns 0 for an out-of-range index.
 * The original reported the violation and then indexed anyway. */
uint32_t BrPodGetLength(const BrPod *pPod, int iEntry);

/* Original: ReadPod @ 0x100087B0. Returns 0 on success. */
int   BrPodRead(BrPod *pPod, int iEntry, void *pvBuffer);

/* Original: LoadPod @ 0x10008810 -- allocate then fill. Caller frees. */
void *BrPodLoad(BrPod *pPod, int iEntry, uint32_t *pcbOut);

#endif /* BR_POD_H */
