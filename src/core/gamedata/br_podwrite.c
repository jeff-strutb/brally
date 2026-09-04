/* br_podwrite.c -- gamedata: writing a POD archive.
 *
 * The other half of br_pod.c: opening a POD for writing, appending members
 * and sealing it with its directory and header. Filed out of slice2_12.c
 * section 8, whole -- the three writer globals and the directory are shared
 * by all three entry points.
 *
 * See slice2_12.h for the recovered layouts.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrPodWriteOpen  BrPodWriteOpen_cdecl_hdr
#define BrPodWriteAdd   BrPodWriteAdd_cdecl_hdr
#define BrPodWriteClose BrPodWriteClose_cdecl_hdr
#define BrPodWriterMakeName BrPodWriterMakeName_cdecl_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_12.h"
#ifdef BR_MATCHING_BUILD
#undef BrPodWriteOpen
#undef BrPodWriteAdd
#undef BrPodWriteClose
#undef BrPodWriterMakeName

/* POD writer helpers: thiscall on the stream at this+4.  Struct-typed
 * stack args so they do not claim edx (BR_THISCALL stack-arg idiom). */
typedef struct { const char *p; } BrPodStr;
typedef struct { char *p; }       BrPodDst;
extern FILE *__fastcall BrPodStreamOpen(void *pStream, int _edx,
                                        const char *pszPath);
extern void __fastcall BrPodWriterMakeName(void *pStream, BrPodStr src,
                                           BrPodDst dst);
extern void __fastcall BrFileWriteCheckedT(void *pStream, int _edx,
                                           FILE *pFile, const void *pv,
                                           unsigned cb);
extern void BrLogFatalPrintf(const char *pFmt, ...);
__declspec(dllimport) char *_strupr(char *);

FILE             *g_BrPodFile;
uint32_t          g_BrPodCount;
BrPodWriteEntry   g_BrPodDir[BR_POD_WRITER_MAX];
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Little-endian, byte-wise: the original's stores are plain x86 `mov`s. */
static void BrPutU32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* =====================================================================
 * 8. POD archive writer
 * ===================================================================== */

#define BR_POD_DIR_STRIDE 76

/* 0x100089C0 */
/* WHAT IT DOES: starts writing a POD archive -- the game's own bundle format
 * for its data files. It opens the file, leaves room at the front for a
 * header it can only fill in at the end, and clears the directory it will
 * build up as members are added. */
#ifdef BR_MATCHING_BUILD
/* RESIDUE 6 masked bytes, RAW 0+0 / REGNORM 0+0 -- every instruction the
 * original has, one of them in a different PLACE.  The original sinks
 * `mov [g_BrPodFile],eax` past the three fseek argument pushes, into the
 * gap between the last push and the `call`; we emit it before the pushes.
 * DEAD 2026-09-03, do not re-run: assigning the global AFTER the fseek
 * (VC5 then parks the handle in esi across the call -- 33 diffs, an extra
 * push/pop pair) and dropping the local so the global is both the
 * assignment target and the fseek argument (identical 6).  Store
 * placement inside a call sequence is not source-reachable here. */
/* @implements 0x10008BA0 glide BrPodWriteOpen */
int __fastcall BrPodWriteOpen(void *pThis, int _edx, const char *pszPath)
{
    FILE *pFile = BrPodStreamOpen((char *)pThis + 4, _edx, pszPath);

    g_BrPodFile = pFile;
    fseek(pFile, 0x10, 0);
    memset(g_BrPodDir, 0, sizeof g_BrPodDir);
    g_BrPodCount = 0;
    return 0;
}
#else
/* WHAT IT DOES: the port spelling of the same open -- create the archive
 * file, skip past the space its header will occupy, and start with an empty
 * directory. Unlike the original it reports a failed open instead of
 * carrying on with a null file. */
/* @implements 0x10008BA0 glide BrPodWriteOpen */
int BrPodWriteOpen(BrPodWriter *pW, const char *pszPath)
{
    /* DEVIATION: the original opens through the stream object at +4
     * (0x10008BE0) and seeks with 0x1007C910. */
    pW->pFile = fopen(pszPath, "wb");
    if (pW->pFile == NULL)
        return 1;

    if (fseek(pW->pFile, 0x10, SEEK_SET) != 0) {
        fclose(pW->pFile);
        pW->pFile = NULL;
        return 1;
    }

    memset(pW->aEntries, 0, sizeof pW->aEntries);       /* 0x13000 dwords */
    pW->cEntries = 0;
    return 0;
}
#endif

/* 0x10008A00 */
/* WHAT IT DOES: adds one member file to the archive being written: notes
 * where in the file the data will sit, writes the data, and records the name
 * (uppercased) and size in the directory. An over-long name is complained
 * about and then used anyway, and the directory is capped here, which the
 * original did not do. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x10008BE0 glide BrPodWriteAdd */
void __fastcall BrPodWriteAdd(void *pThis, int _edx, const char *pszName,
                              const void *pvData, uint32_t cbData,
                              unsigned char b08, unsigned char b09)
{
    BrPodWriteEntry *pEnt;
    void            *pStream;

    pEnt = &g_BrPodDir[g_BrPodCount];
    pStream = (char *)pThis + 4;
    g_BrPodCount++;
    pEnt->offData = 0;

    {
        BrPodStr src;
        BrPodDst dst;
        src.p = pszName;
        dst.p = pEnt->szName;
        BrPodWriterMakeName(pStream, src, dst);
    }

    if (strlen(pEnt->szName) > 0x40)
        BrLogFatalPrintf("Add: Name is too long to be a pod name.");

    _strupr(pEnt->szName);

    {
        uint32_t off = (uint32_t)ftell(g_BrPodFile);
        pEnt->offData = off;
        pEnt->b08     = b08;
        pEnt->cbData  = cbData;
        pEnt->b09     = b09;
    }

    BrFileWriteCheckedT(pStream, (int)pvData, g_BrPodFile, pvData, cbData);
}
#else
/* WHAT IT DOES: the port spelling of the same append -- record where the
 * member's data lands, write it, and note the uppercased name and size in
 * the directory. Bounded where the original ran off the end of the name
 * field and capped where the original never checked the directory. */
/* @implements 0x10008BE0 glide BrPodWriteAdd */
void BrPodWriteAdd(BrPodWriter *pW, const char *pszName,
                   const void *pvData, uint32_t cbData,
                   uint8_t b08, uint8_t b09)
{
    BrPodWriteEntry *pEnt;
    const void      *pNul;
    size_t           cbName;
    size_t           i;
    long             off;

    if (pW->cEntries >= BR_POD_WRITER_MAX)
        return;                         /* DEVIATION: the original never checks */

    pEnt = &pW->aEntries[pW->cEntries];
    pW->cEntries += 1;                  /* the count moves BEFORE the write */
    pEnt->offData = 0;

    BrPodWriterMakeName(pW, pszName, pEnt->szName);

    /* DEVIATION: the original runs a `repne scasb` off the end of the 64-byte
     * field when the name fills it completely. Bounded here. */
    pNul   = memchr(pEnt->szName, '\0', sizeof pEnt->szName);
    cbName = (pNul != NULL)
             ? (size_t)((const char *)pNul - pEnt->szName)
             : sizeof pEnt->szName;

    if (cbName > 0x40)
        fprintf(stderr, "Add: Name is too long to be a pod name.\n");
    /* ...and then adds it anyway, exactly as the original does. */

    for (i = 0; i < cbName; ++i) {      /* 0x1007F240 == _strupr */
        char c = pEnt->szName[i];
        if (c >= 'a' && c <= 'z')
            pEnt->szName[i] = (char)(c - 'a' + 'A');
    }

    off = ftell(pW->pFile);                     /* 0x1007C9F0 */
    pEnt->offData = (uint32_t)off;
    pEnt->cbData  = cbData;
    pEnt->b08     = b08;
    pEnt->b09     = b09;

    if (cbData != 0)
        fwrite(pvData, 1, (size_t)cbData, pW->pFile);   /* 0x10008C90 */
}
#endif

/* 0x10008AA0 */
/* WHAT IT DOES: finishes the archive: writes the directory of members at the
 * end, rewinds to the front to fill in the header with the magic word,
 * member count and directory position, and closes the file. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x10008C80 glide BrPodWriteClose */
void __fastcall BrPodWriteClose(void *pThis)
{
    uint32_t offDir;
    uint32_t cbDir;
    char     aHdr[16];

    /* `mov esi, ecx` must survive ftell; this+4 is added AFTER the call. */
    offDir = (uint32_t)ftell(g_BrPodFile);
    pThis  = (char *)pThis + 4;
    cbDir  = g_BrPodCount * (uint32_t)sizeof(BrPodWriteEntry);
    BrFileWriteCheckedT(pThis, (int)cbDir, g_BrPodFile, g_BrPodDir, cbDir);

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    *(uint32_t *)(aHdr + 4)  = BR_POD_WRITER_MAGIC_EXTRA;
    *(uint32_t *)(aHdr + 8)  = g_BrPodCount;
    *(uint32_t *)(aHdr + 12) = offDir;
    fseek(g_BrPodFile, 0, 0);
    BrFileWriteCheckedT(pThis, (int)g_BrPodFile, g_BrPodFile, aHdr, 16);
    fclose(g_BrPodFile);
}
#else
/* WHAT IT DOES: the port spelling of the same seal -- write the directory
 * out record by record, rewind and fill in the header with the magic word,
 * member count and directory position, then close the file. Serialised field
 * by field rather than blitted, so the file is the same on any host. */
/* @implements 0x10008C80 glide BrPodWriteClose */
void BrPodWriteClose(BrPodWriter *pW)
{
    uint8_t  aRec[BR_POD_DIR_STRIDE];
    uint8_t  aHdr[16];
    uint32_t i;
    long     offDir;

    offDir = ftell(pW->pFile);          /* recorded BEFORE the directory */

    /* DEVIATION: the original writes the in-memory directory array straight
     * out with one call, which is only the on-disk layout because the host is
     * little-endian. Serialised field by field here. */
    for (i = 0; i < pW->cEntries; ++i) {
        const BrPodWriteEntry *pEnt = &pW->aEntries[i];

        memset(aRec, 0, sizeof aRec);
        BrPutU32(aRec + 0x00, pEnt->offData);
        BrPutU32(aRec + 0x04, pEnt->cbData);
        aRec[0x08] = pEnt->b08;
        aRec[0x09] = pEnt->b09;
        aRec[0x0A] = pEnt->b0A;
        aRec[0x0B] = pEnt->b0B;
        memcpy(aRec + 0x0C, pEnt->szName, sizeof pEnt->szName);
        fwrite(aRec, 1, sizeof aRec, pW->pFile);
    }

    fseek(pW->pFile, 0, SEEK_SET);

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    aHdr[3] = 0;                        /* DEVIATION: never assigned originally */
    BrPutU32(aHdr + 4, BR_POD_WRITER_MAGIC_EXTRA);
    BrPutU32(aHdr + 8, pW->cEntries);
    BrPutU32(aHdr + 12, (uint32_t)offDir);
    fwrite(aHdr, 1, sizeof aHdr, pW->pFile);

    fclose(pW->pFile);                  /* 0x1007CD50 */
    pW->pFile = NULL;
}
#endif
