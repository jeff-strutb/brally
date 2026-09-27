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
 * NOT MATCHING: 508/510 B, register-blind 1+1.  At the loop header the
 * original forms i = idx + 1 with `lea ecx,[eax+1]` (idx kept in eax)
 * where VC5 here does `inc eax`.  Inert: every spelling of the two header
 * tests (i/next locals, !=/</<= forms, casts, pre-increment), record
 * pointer vs index in either clear block, a function ahead in the TU,
 * extern pads and five headers.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

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
typedef char chk_ents[(unsigned)&((Slots54E20 *)0)->ents == 0x1A60C ? 1 : -1];
typedef char chk_cnt[(unsigned)&((Slots54E20 *)0)->wCount == 0x1A92C ? 1 : -1];

extern "C" {
char g_szBr396F08[];
}
int Slots54E20::Delete(int idx)
{
    int i;

    if (idx >= 0) {
        strcpy(recs[idx].szName, g_szBr396F08);
        recs[idx].bUsed = 0;
        recs[idx].w41C = 0;
        recs[idx].w40C = 0;
        recs[idx].w40A = 0;
        memset(recs[idx].a424, 0, sizeof(recs[idx].a424));
        recs[idx].f410 = 0;
        recs[idx].f414 = 0;
        recs[idx].f418 = 0;
        recs[idx].f420 = 0;
        recs[idx].f434 = 0;
        memset(&ents[idx], 0, sizeof(ents[idx]));
    }

    if (wCount != idx + 1) {
        for (i = idx + 1; i <= wCount - 1; i++) {
            recs[i - 1] = recs[i];
            if (ents[i].b != 0 && ents[i].a > 0)
                s10(ents[i].b, ents[i].a, i - 1);
        }
    }

    i = wCount - 1;
    if (i > 0) {
        strcpy(recs[i].szName, g_szBr396F08);
        recs[i].bUsed = 0;
        recs[i].w41C = 0;
        recs[i].w40C = 0;
        recs[i].w40A = 0;
        memset(recs[i].a424, 0, sizeof(recs[i].a424));
        recs[i].f410 = 0;
        recs[i].f414 = 0;
        recs[i].f418 = 0;
        recs[i].f420 = 0;
        memset(&ents[i], 0, sizeof(ents[i]));
    }

    wCount--;
    return 1;
}
