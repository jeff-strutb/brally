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
#include <stdio.h>
#include <string.h>
#include "br_podarc.h"

/* The archive object gains one non-virtual method here. */
static void BrPodArcOpen(BrPodArc *pod)
{
    pod->pFile = BrFileOpenChecked(pod->szName);
    BrFileReadChecked(pod->pFile, pod->magic, 0x10);    /* magic, count, offset */
    if (strncmp(pod->magic, DAT_1007b5bc, 3) != 0) {
        BrLogFatalPrintf(s__s_is_not_a_valid_POD_file_1007b5a0, pod->szName);
    }
    pod->cbDir    = pod->cEntries * 0x4c;
    pod->aEntries = (BrPodArcEntry *)BrOperatorNew(pod->cbDir);
    fseek(pod->pFile, pod->offDir, 0);
    BrFileReadChecked(pod->pFile, pod->aEntries, pod->cbDir);
}

extern "C" void BrPodOpen(void *self)
{
    BrPodArcOpen((BrPodArc *)self);
}
