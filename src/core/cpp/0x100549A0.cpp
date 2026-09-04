/* WHAT IT DOES: poll one save slot: refresh it, and return without doing
 * anything more if the slot is empty. */
/* @implements 0x100549A0 glide BrSlotPoll_100549A0
 * @cpp_kind method
 * @cpp_symbol ?Poll@Slots549A0@@QAEHH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 132 B. Same object as
 * 0x10054E20: the 0x438-stride slot array based at `this + 0x2C`, so
 * the record pointer is `this + idx*0x438` with 0x2C folded into every
 * displacement. Tick the record's own object (its +0x04 vcall), and if
 * the record's +0x420 "busy" word is set, ask it for a result with the
 * +0x14 vcall; a positive result means keep waiting. Zero or negative
 * clears the busy word and the shared global, then hands the owner's
 * +0x14 callback either the slot index (result 0) or -1 (negative).
 * Returns 0 only when the record was not busy.
 *
 * The zero is materialised once (`xor edx,edx`) and serves both stores
 * and all three comparisons, which is why the null test on the callback
 * reads `cmp eax,edx` and not `test eax,eax`.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Rec549A0 {
public:
    virtual void s0();
    virtual void s1();          /* +0x04 -- tick */
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual char s5();          /* +0x14 -- result */
};

typedef int (*BrFn549A0)(void *pObj, int idx);

class Slots549A0 {
public:
    char       pad000[0x14];
    BrFn549A0  pfn;             /* +0x14 */

    int Poll(int idx);
};

typedef char chk_pfn549A0[(unsigned)&((Slots549A0 *)0)->pfn == 0x14 ? 1 : -1];

extern "C" {
int g_brAA28D8;                 /* 0x10AC5C30 */
}

#define BR_SLOT 0x438

int Slots549A0::Poll(int idx)
{
    char *p = (char *)this + idx * BR_SLOT;
    Rec549A0 *pRec = (Rec549A0 *)(p + 0x2C);
    int c;

    pRec->s1();

    if (*(int *)(p + 0x44C) == 0)
        return 0;

    c = pRec->s5();

    if (c <= 0) {
        g_brAA28D8 = 0;
        *(int *)(p + 0x44C) = 0;

        if (pfn != 0) {
            if (c >= 0)
                pfn(this, idx);
            else
                pfn(this, -1);
        }
    }

    return 1;
}
