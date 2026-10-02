/* WHAT IT DOES: finishes the archive: writes the directory of members at the
 * end, rewinds to the front to fill in the header with the magic word,
 * member count and directory position, and closes the file. */
/* @implements 0x10008C80 glide BrPodWriteClose
 * @cpp_kind method
 * @cpp_symbol ?Close@BrPodWriter@@QAEXXZ
 *
 * C++: a thiscall member; `this+4` (the stream subobject) is kept in esi for
 * both WriteChecked calls.  BYTE-EXACT 2026-09-28 on the first compile.  The
 * C twin (__fastcall) loaded ecx before the second call's pushes and so held
 * g_BrPodFile in edx; as a member call the `mov ecx,esi` is the last thing
 * before the call, as the original has it.  aHdr[3] is never written (the
 * original's quirk).  br_podwrite.c keeps the port body.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_12.h"   /* br_globals: its objects */
#include <stdio.h>
#include <string.h>

/* The writer object's only state is the three globals; its +4 helper's
 * methods ignore `this`. */
extern "C" void BrPodWriteClose(void *self)
{
    unsigned int offDir;
    char         aHdr[16];

    (void)self;
    offDir = (unsigned int)ftell(g_BrPodFile);
    BrFileWriteChecked(g_BrPodFile, g_BrPodDir, g_BrPodCount * sizeof(BrPodWriteEntry));

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    *(unsigned int *)(aHdr + 4)  = 0x1F4;
    *(unsigned int *)(aHdr + 8)  = g_BrPodCount;
    *(unsigned int *)(aHdr + 12) = offDir;
    fseek(g_BrPodFile, 0, 0);
    BrFileWriteChecked(g_BrPodFile, aHdr, 16);
    fclose(g_BrPodFile);
}
