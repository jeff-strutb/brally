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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_podarc.h"   /* BrPodArc */
#include "br_pod.h"

#include "br_path.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define POD_HEADER_SIZE 16
#define POD_ENTRY_SIZE  76

/* (port-only rd_u32le removed) */


/* Original: CleanupName @ 0x100085F0 (__stdcall, ret 8).
 * Uppercases into a fixed 64-byte field and NUL-pads the remainder. */
/* (port-only BrPodCleanupName removed) */


/* BrPodOpen: the placed body is BrPodOpen_10008AB0.cpp */

/* (port-only BrPodClose removed) */


/* Original: GetNumForName @ 0x10008750. */
/* (port-only BrPodGetNumForName removed) */


/* Original: GetPodLength @ 0x10008780.
 * DEVIATION: returns 0 out of range; the original reported and indexed anyway. */
/* (port-only BrPodGetLength removed) */


/* Original: ReadPod @ 0x100087B0 -- fseek to offData, then read cbData. */
/* (port-only BrPodRead removed) */


/* Original: LoadPod @ 0x10008810 -- GetPodLength, malloc, ReadPod. */
/* (port-only BrPodLoad removed) */


#include "br_match.h"

/* WHAT IT DOES: writes a file name onto the POD object, in the buffer that
 * sits 32 bytes from the start. A missing name is ignored and the object is
 * left as it was. There is no length cap. */
/* @implements 0x10008B40 d3d BrPodSetName */
/* @implements 0x10008D20 glide BrPodSetName */
typedef struct { const char *psz; } BrPodSetNameArg;
void BR_THISCALL1 BrPodSetName(void *pThis, BrPodSetNameArg a)
{
    if (a.psz != NULL)
        strcpy(((BrPodArc *)pThis)->szName, a.psz);
}

/* -- Ghidra-matched functions --------------------------- */
/* WHAT IT DOES: identity function: returns its argument unchanged (fastcall). */
/* @implements 0x10008D50 glide BrPodIdentity */
/* @n64 0x80268560 located */

unsigned char * __fastcall BrPodIdentity(unsigned char * param_1)

{
  return param_1;
}

/* WHAT IT DOES: no-op stub. */
/* @implements 0x10008D60 glide BrPodNop */

int BrPodNop(void)

{
  return;
}

