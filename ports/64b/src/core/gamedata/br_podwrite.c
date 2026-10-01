/* br_podwrite.c -- gamedata: writing a POD archive.
 *
 * The other half of br_pod.c: opening a POD for writing, appending members
 * and sealing it with its directory and header. Filed out of slice2_12.c
 * section 8, whole -- the three writer globals and the directory are shared
 * by all three entry points.
 *
 * See slice2_12.h for the recovered layouts.
 */
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrPodWriteOpen  BrPodWriteOpen_cdecl_hdr
#define BrPodWriteAdd   BrPodWriteAdd_cdecl_hdr
#define BrPodWriteClose BrPodWriteClose_cdecl_hdr
#define BrPodWriterMakeName BrPodWriterMakeName_cdecl_hdr
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_12.h"
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
/* BrPodWriterMakeName: prototype in br_funcs.h */
/* BrFileWriteCheckedT: prototype in br_funcs.h */
/* BrLogFatalPrintf: prototype in br_funcs.h */
/* 64-bit core: _strupr is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

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
/* Not tagged: the Glide match is the C++ member in
 * src/core/gamedata/BrPodWriteOpen_10008BA0.cpp.  This __fastcall copy stays
 * compiled so BrPodWriteAdd below keeps the TU state it matched with. */
int __fastcall BrPodWriteOpen(void *pThis, int _edx, const char *pszPath)
{
    FILE *pFile = BrPodStreamOpen((char *)pThis + 4, _edx, pszPath);

    g_BrPodFile = pFile;
    fseek(pFile, 0x10, 0);
    memset(g_BrPodDir, 0, sizeof g_BrPodDir);
    g_BrPodCount = 0;
    return 0;
}

/* 0x10008A00 */
/* WHAT IT DOES: adds one member file to the archive being written: notes
 * where in the file the data will sit, writes the data, and records the name
 * (uppercased) and size in the directory. An over-long name is complained
 * about and then used anyway, and the directory is capped here, which the
 * original did not do. */
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

    BrFileWriteCheckedT(pStream, (int)pvData, g_BrPodFile);
}

/* 0x10008AA0 */
/* WHAT IT DOES: finishes the archive: writes the directory of members at the
 * end, rewinds to the front to fill in the header with the magic word,
 * member count and directory position, and closes the file. */
/* Not tagged: the Glide match is the C++ member in
 * src/core/gamedata/BrPodWriteClose_10008C80.cpp. */
void __fastcall BrPodWriteClose(void *pThis)
{
    uint32_t offDir;
    uint32_t cbDir;
    char     aHdr[16];

    /* `mov esi, ecx` must survive ftell; this+4 is added AFTER the call. */
    offDir = (uint32_t)ftell(g_BrPodFile);
    pThis  = (char *)pThis + 4;
    cbDir  = g_BrPodCount * (uint32_t)sizeof(BrPodWriteEntry);
    BrFileWriteCheckedT(pThis, (int)cbDir, g_BrPodFile);

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    *(uint32_t *)(aHdr + 4)  = BR_POD_WRITER_MAGIC_EXTRA;
    *(uint32_t *)(aHdr + 8)  = g_BrPodCount;
    *(uint32_t *)(aHdr + 12) = offDir;
    fseek(g_BrPodFile, 0, 0);
    BrFileWriteCheckedT(pThis, (int)g_BrPodFile, g_BrPodFile);
    fclose(g_BrPodFile);
}
