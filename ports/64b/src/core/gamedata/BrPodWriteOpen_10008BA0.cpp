/* WHAT IT DOES: starts writing a POD archive -- the game's own bundle format
 * for its data files. It opens the file, leaves room at the front for a
 * header it can only fill in at the end, and clears the directory it will
 * build up as members are added. */
/* @implements 0x10008BA0 glide BrPodWriteOpen
 * @cpp_kind method
 * @cpp_symbol ?Open@BrPodWriter@@QAEHPBD@Z
 *
 * C++: a thiscall member (`ret 4`), opening through the stream subobject at
 * +4 (`add ecx,4` -- this is still live in ecx).  BYTE-EXACT 2026-09-28 on
 * the first compile.  The C twin (a __fastcall with a dummy edx) emitted the
 * g_BrPodFile store before fseek's argument pushes; as a member call the
 * store sinks between the pushes, where the original has it.  The 0 it
 * returns is the eax the memset's `rep stosd` leaves behind.  br_podwrite.c
 * keeps the port body.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_12.h"   /* br_globals: its objects */
#include <stdio.h>
#include <string.h>

extern "C" int BrPodWriteOpen(void *self, const char *pszPath)
{
    (void)self;
    g_BrPodFile = BrFileCreateChecked((char *)pszPath);
    fseek(g_BrPodFile, 0x10, 0);
    memset(g_BrPodDir, 0, sizeof g_BrPodDir);
    g_BrPodCount = 0;
    return 0;
}
