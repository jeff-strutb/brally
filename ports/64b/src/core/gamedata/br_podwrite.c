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

/* 0x10008A00 */
/* WHAT IT DOES: adds one member file to the archive being written: notes
 * where in the file the data will sit, writes the data, and records the name
 * (uppercased) and size in the directory. An over-long name is complained
 * about and then used anyway, and the directory is capped here, which the
 * original did not do. */
/* @implements 0x10008BE0 glide BrPodWriteAdd */
void __fastcall BrPodWriteAdd(void *pThis, const char *pszName,
                              const void *pvData, uint32_t cbData,
                              unsigned char b08, unsigned char b09)
{
    BrPodWriteEntry *pEnt;
    void            *pStream;

    pEnt = &g_BrPodDir[(*(uint32_t *)&g_BrPodCount)];
    pStream = (char *)pThis + 4;
    (*(uint32_t *)&g_BrPodCount)++;
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

    BrFileWriteChecked(g_BrPodFile, pvData, cbData);
}

