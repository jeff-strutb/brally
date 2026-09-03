/* 0x10006350 -- the Glide body of BrNetDropMatching.
 *
 * slice1_02.c keeps the PORT body (a BrNetState * and bounded writes); this
 * is the original's, and the two cannot share a signature: the original
 * takes only the key and reaches every slot through the globals.
 *
 * The loop walks a POINTER over the name field at slot+0x570 with the slot
 * stride, bounded by the end address, and carries the index alongside it for
 * the three accessor calls -- which is what indexing the global array by `i`
 * and using `slots[i].name` once per iteration produces. The two Win32
 * handles are hoisted into registers because both are called inside the
 * loop, and the free-list push has NO bound: that guard is the port's.
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

typedef struct BrNetSlot978 {
    void *hMutex;                    /* +0x000 = 0x1021CE58 */
    char  pad004[0x28];
    int   f02C;                      /* +0x02C */
    char  pad030[0x570 - 0x30];
    char  name[0x978 - 0x570];       /* +0x570 = 0x1021D3C8 */
} BrNetSlot978;

typedef char br_assert_slot978[(sizeof(BrNetSlot978) == 0x978) ? 1 : -1];

extern BrNetSlot978 slots[16];       /* 0x1021CE58 */
extern void        *g_h1022AF30;     /* 0x10226A60 */
extern int          g_a10221288[];   /* 0x1021CDB8 */
extern int          g_i10221318;     /* 0x1021CE48 */

int  BrNetSlotGetF004(int slot);
int  BrNetSlotGetF02C(int slot);
void BrNetSlotSetF02C(int slot, int value);
void BrNetAnnounce(const char *psz);

/* @implements 0x10006350 glide BrNetDropMatching */
void BrNetDropMatching(int key)
{
    char szMsg[0x400];
    int  i;

    for (i = 0; i < 16; ++i) {
        if (BrNetSlotGetF004(i) != key)
            continue;
        /* `test al,0x3f` -- any of the low six flag bits. */
        if ((BrNetSlotGetF02C(i) & 0x3F) == 0)
            continue;

        WaitForSingleObject(g_h1022AF30, 0xffffffff);
        g_i10221318 = g_i10221318 + 1;
        g_a10221288[g_i10221318] = i;
        ReleaseMutex(g_h1022AF30);

        BrNetSlotSetF02C(i, 0);

        sprintf(szMsg, "%%15%s left the game.", slots[i].name);
        BrNetAnnounce(szMsg);
    }
}

#endif /* BR_MATCHING_BUILD */
