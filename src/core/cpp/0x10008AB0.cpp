/* WHAT IT DOES: opens the POD archive named on the object, reads its 16-byte
 * header, aborts through the fatal printf if the three-byte magic is not
 * "POD", sizes the directory at 76 bytes per entry (allocated with operator
 * new), seeks to the directory offset the header gave and reads the
 * directory in.  The stream and directory pointers and the directory size
 * are left on the object for the lookups that follow. */
/* @implements 0x10008AB0 glide BrPodOpen
 * @cpp_kind method
 * @cpp_symbol ?Open@BrPodFile@@QAEXXZ
 *
 * 148 B, thiscall, no stack args.  The C twin in src/core/ghidra_batch.c is
 * instruction-exact except for ONE construct: the header read passes the
 * constant 0x10 to a callee whose receiver is in ecx (`push 0x10 / push ebx /
 * push eax / mov ecx,edi / call`).  From C that callee has to be __fastcall
 * with every argument wrapped in a struct (VC5-IDIOMS "thiscall with 3+
 * arguments"), and a struct built from a constant is `mov eax,0x10 / push
 * eax`, never `push 0x10` -- tools/corpus.py finds no solved C site pushing an
 * immediate to a this-in-ecx callee.  The receiver is the 4-byte checked-file
 * sub-object at +0x04 (its two methods are 0x10008E10 / 0x10008E60, matched
 * in the C lane as __stdcall functions that ignore ecx), so this is a member
 * call and belongs here.
 *
 * Do not dllimport operator new: the original calls the 0x10074572 thunk
 * (E8), not the IAT (FF 15) -- same as 0x10056260.cpp.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

void *__cdecl operator new(unsigned int);

extern "C" void BrLogFatalPrintf(const char *pszFmt, ...);   /* 0x10008EC0 */
extern "C" char DAT_1007b5bc[];                              /* "POD" */
extern "C" char s__s_is_not_a_valid_POD_file_1007b5a0[];

/* The checked-file helper object: open-or-die and read-or-die. */
class BrFileIo {
public:
    char  pad[4];
    FILE *Open(char *pszPath);                      /* 0x10008E10 */
    void  Read(FILE *pFile, void *pv, int cb);      /* 0x10008E60 */
};

class BrPodFile {
public:
    int      f00;
    BrFileIo io;              /* +0x04 */
    char     magic[8];        /* +0x08  first 8 of the 16-byte header */
    int      nEntries;        /* +0x10 */
    long     offDir;          /* +0x14 */
    void    *pDir;            /* +0x18  76 bytes per entry */
    FILE    *pFile;           /* +0x1c */
    char     szName[0x400];   /* +0x20 */
    unsigned cbDir;           /* +0x420 */

    void Open();
};

typedef char chk_io[(unsigned)&((BrPodFile *)0)->io == 0x04 ? 1 : -1];
typedef char chk_n[(unsigned)&((BrPodFile *)0)->nEntries == 0x10 ? 1 : -1];
typedef char chk_cb[(unsigned)&((BrPodFile *)0)->cbDir == 0x420 ? 1 : -1];

void BrPodFile::Open()
{
    pFile = io.Open(szName);
    io.Read(pFile, magic, 0x10);
    if (strncmp(magic, DAT_1007b5bc, 3) != 0) {
        BrLogFatalPrintf(s__s_is_not_a_valid_POD_file_1007b5a0, szName);
    }
    cbDir = nEntries * 0x4c;
    pDir  = operator new(cbDir);
    fseek(pFile, offDir, 0);
    io.Read(pFile, pDir, cbDir);
}
