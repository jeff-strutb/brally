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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

/* One directory record: slice2_12.h's BrPodWriteEntry, 76 bytes. */
struct BrPodWriteEntry {
    unsigned int  offData;          /* +0x00 */
    unsigned int  cbData;           /* +0x04 */
    unsigned char b08, b09, b0A, b0B;
    char          szName[64];       /* +0x0C */
};

/* The file-stream helper object at writer+4 (the same +4 subobject
 * 0x100087D0's CleanupName calls into). */
class BrPodStream {
public:
    FILE *Open(const char *pszPath);                               /* 0x10008DC0 */
    void  WriteChecked(FILE *pFile, const void *pv, unsigned cb);  /* 0x10008E90 */
};

class BrPodWriter {
public:
    char        pad[4];
    BrPodStream m4;                 /* +0x04 */

    int  Open(const char *pszPath);
    void Close();
};

/* The three writer globals, defined in br_podwrite.c. */
extern "C" {
extern FILE            *g_BrPodFile;        /* 0x10272E8C */
extern unsigned int     g_BrPodCount;       /* 0x10272E88 */
extern BrPodWriteEntry  g_BrPodDir[4096];   /* 0x10226E88, 0x13000 dwords */
}

int BrPodWriter::Open(const char *pszPath)
{
    g_BrPodFile = m4.Open(pszPath);
    fseek(g_BrPodFile, 0x10, 0);
    memset(g_BrPodDir, 0, sizeof g_BrPodDir);
    g_BrPodCount = 0;
    return 0;
}
