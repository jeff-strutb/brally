/* WHAT IT DOES: delete a save slot -- blanks its name and clears its fields
 * so the slot reads as empty. */
/* @implements 0x10054E20 glide BrSaveSlotDelete_10054E20
 * @cpp_kind method
 * @cpp_symbol ?Delete@Slots54E20@@QAEHH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 510 B.  Remove one entry from the
 * slot array: clear the named slot, shift every later slot down one (a
 * whole-record assignment, VC5's bare `rep movsd` of 0x10E dwords), re-key
 * each moved slot's live object through the +0x28 vcall, clear what is now
 * the unused tail slot, and drop the count.  Returns 1.
 *
 * Re-transcribed 2026-09-27 with a real record type.  The slots are
 * 0x438-byte records starting at this+0x2C (the fields end exactly at the
 * stride: +0x434 is the last dword), so the old "clear runs off the end of
 * its own record" and the "(idx + 1) scale" short were both just record
 * fields seen from the wrong base (+0x40C + 0x2C = 0x438).  The 16-byte
 * a424 group and the ents pair are cleared with memset: the original's
 * fresh `xor eax,eax` / `xor edx,edx` through a pointer is VC5's inline
 * memset, and with those out of the ebx zero web the zero no longer lives
 * across the loop -- that was the whole of the old 480-diff residue.
 * The two clear blocks differ by one field (the tail copy leaves f434).
 *
 * Byte-exact 2026-09-27, as an O2 /Gi TU (verified through the serial
 * /Gi chain, every O2-Gi row unchanged).  Under plain /O2 it stops at
 * register-blind 1+1 (`inc eax` for the `lea ecx,[eax+1]` at the loop
 * header); /Gi gets that.  The last two source facts: the header test is
 * `idx + 1 != wCount` (operand order of the cmp), and the tail block reuses
 * the PARAMETER -- `idx = wCount - 1` -- which is why the original keeps
 * that index in idx's argument slot.
 */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "slice3_39.h"   /* BrTextBox, the canonical record */

struct BrEnt54E20 {
    int a;
    int b;
};

struct BrSlot54E20 {
    char  pad000[8];
    char  bUsed;            /* +0x008 */
    char  szName[0x401];    /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;
    int   f414;
    int   f418;
    short w41C;
    short pad41E;
    int   f420;
    int   a424[4];
    int   f434;
};
typedef char chk_slot[sizeof(BrSlot54E20) == 0x438 ? 1 : -1];

class Slots54E20 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6();
    virtual void s7();
    virtual void s8();
    virtual void s9();
    virtual void s10(int b, int a, int slot);

    char           pad004[0x2C - 4];
    BrSlot54E20    recs[100];       /* +0x2C */
    BrEnt54E20     ents[100];       /* +0x1A60C */
    unsigned short wCount;          /* +0x1A92C */

    int Delete(int idx);
};



extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}
int Slots54E20::Delete(int idx)
{
    int i;

    if (idx >= 0) {
        strcpy((*(char (*)[1025])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->sz[0]), g_aBr39B720);
        (*(char *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f08) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f41C) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->height) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->width) = 0;
        memset((*(int (*)[4])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->left), 0, sizeof((*(int (*)[4])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->left)));
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->x) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->y) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f418) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f420) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f434) = 0;
        memset(&(*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[idx]), 0, sizeof((*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[idx])));
    }

    if (idx + 1 != (*(unsigned short *)&((BrTextList *)(this))->count)) {
        for (i = idx + 1; i <= (*(unsigned short *)&((BrTextList *)(this))->count) - 1; i++) {
            (*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[i - 1]) = (*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[i]);
            if ((*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[i]).b != 0 && (*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[i]).a > 0)
                s10((*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[i]).b, (*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[i]).a, i - 1);
        }
    }

    idx = (*(unsigned short *)&((BrTextList *)(this))->count) - 1;
    if (idx > 0) {
        strcpy((*(char (*)[1025])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->sz[0]), g_aBr39B720);
        (*(char *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f08) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f41C) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->height) = 0;
        (*(short *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->width) = 0;
        memset((*(int (*)[4])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->left), 0, sizeof((*(int (*)[4])&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->left)));
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->x) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->y) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f418) = 0;
        (*(int *)&((BrTextBox *)&((*(BrSlot54E20 *)&((BrTextList *)(this))->aItems[idx])))->f420) = 0;
        memset(&(*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[idx]), 0, sizeof((*(BrEnt54E20 *)&((BrTextList *)(this))->aBlobs[idx])));
    }

    (*(unsigned short *)&((BrTextList *)(this))->count)--;
    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrSaveSlotDelete_10054E20(void *self, int idx)
{
    return ((class Slots54E20 *)self)->Delete(idx);
}
/* end of C entry points */
