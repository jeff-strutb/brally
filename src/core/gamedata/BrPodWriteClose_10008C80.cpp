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

void BrPodWriter::Close()
{
    unsigned int offDir;
    char         aHdr[16];

    offDir = (unsigned int)ftell(g_BrPodFile);
    m4.WriteChecked(g_BrPodFile, g_BrPodDir, g_BrPodCount * sizeof(BrPodWriteEntry));

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    *(unsigned int *)(aHdr + 4)  = 0x1F4;
    *(unsigned int *)(aHdr + 8)  = g_BrPodCount;
    *(unsigned int *)(aHdr + 12) = offDir;
    fseek(g_BrPodFile, 0, 0);
    m4.WriteChecked(g_BrPodFile, aHdr, 16);
    fclose(g_BrPodFile);
}
