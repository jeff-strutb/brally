/* @implements 0x10054E20 glide BrSaveSlotDelete_10054E20
 * @cpp_kind method
 * @cpp_symbol ?Delete@Slots54E20@@QAEHH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 510 B. Remove one entry from the
 * 0x438-byte slot array: clear the named slot, shift every later slot
 * down one (a whole-struct assignment, which VC5 emits as a bare
 * `rep movsd` of 0x10E dwords), re-key each moved slot's live object
 * through the +0x28 vcall, clear what is now the unused tail slot, and
 * drop the count. Returns 1.
 *
 * The two clear blocks are NOT one inlined helper -- the first one also
 * zeroes the +0x434 field and the second does not, so the original had
 * the block written out twice with one line missing from the copy.
 *
 * The slot array is based at `this` itself (record = this + idx*0x438,
 * name at +0x35), and the clear block runs off the END of its own record
 * into the next one -- +0x43C..+0x460 are all past the 0x438 stride. That
 * is the original's, not a transcription error, which is why this is
 * written as raw offsets off a `char *` rather than through a struct.
 * One field, the short at +0x438 exactly, the original addresses from
 * `this` with a recomputed (idx + 1) scale instead of as a displacement
 * off the record base; spelling it that way reproduces the second lea
 * chain.
 *
 * Store ORDER is source order here -- the stores go through a char*, so
 * VC5 will not reorder them, and the emitted sequence reads back as the
 * statement list directly: name, +0x34, +0x448, the (idx+1) short,
 * +0x436, the +0x450 group, then +0x43C/+0x440/+0x444/+0x44C/+0x460.
 * Writing them in offset order costs ~5 diffs and nothing else.
 *
 * PARKED at 480 diffs / +34 bytes, and it is ONE allocator decision:
 *     orig    `push ecx`   -- one frame dword (the strcpy temp), and the
 *                             loop counter lives in ebx, so the loop's
 *                             zero tests are `test ecx,ecx`
 *     recomp  `sub esp,8`  -- two frame dwords, because ebx is pinned
 *                             holding a materialised 0 across the whole
 *                             function, so the counter spills and the
 *                             loop's tests become `cmp ecx,ebx`
 * The original materialises 0 in ebx for the FIRST clear block, lets it
 * die, reuses ebx as the counter through the loop, then re-zeroes it with
 * a fresh `xor ebx,ebx` before the second clear block. Ours keeps the
 * single zero live throughout. Everything before the loop is otherwise
 * byte-identical modulo the 4-byte frame shift.
 * DO NOT RE-PROBE -- unchanged by: a separate `last` local for the tail
 * block, scoping the record pointers inside their blocks, rewriting the
 * loop with explicit induction pointers (worse: sub esp,0xC), and flags
 * /O2 /Op, /Ox, /O2 /Ot, /O2 /Gy, /O2 /Ob0 (all 480).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

struct BrEnt54E20 {
    int a;      /* +0x00 */
    int b;      /* +0x04 */
};

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
    virtual void s10(int b, int a, int slot);   /* +0x28 -- re-key */

    char           pad004[0x1A60C - 4];
    BrEnt54E20     ents[100];       /* +0x1A60C */
    unsigned short wCount;          /* +0x1A92C */

    int Delete(int idx);
};

typedef char chk_ents[(unsigned)&((Slots54E20 *)0)->ents == 0x1A60C ? 1 : -1];
typedef char chk_cnt[(unsigned)&((Slots54E20 *)0)->wCount == 0x1A92C ? 1 : -1];

extern "C" {
char g_szBr396F08[];        /* 0x10396F08 */
}

#define BR_SLOT 0x438

int Slots54E20::Delete(int idx)
{
    int i;

    if (idx >= 0) {
        char *p = (char *)this + idx * BR_SLOT;

        strcpy(p + 0x35, g_szBr396F08);
        p[0x34] = 0;
        *(short *)(p + 0x448) = 0;
        *(short *)((char *)this + (idx + 1) * BR_SLOT) = 0;
        *(short *)(p + 0x436) = 0;
        *(int *)(p + 0x450) = 0;
        *(int *)(p + 0x454) = 0;
        *(int *)(p + 0x458) = 0;
        *(int *)(p + 0x45C) = 0;
        *(int *)(p + 0x43C) = 0;
        *(int *)(p + 0x440) = 0;
        *(int *)(p + 0x444) = 0;
        *(int *)(p + 0x44C) = 0;
        *(int *)(p + 0x460) = 0;
        ents[idx].a = 0;
        ents[idx].b = 0;
    }

    i = idx + 1;
    if (i != wCount) {
        while (i <= wCount - 1) {
            memcpy((char *)this + i * BR_SLOT - 0x40C,
                   (char *)this + i * BR_SLOT + 0x2C, BR_SLOT);
            if (ents[i].b != 0 && ents[i].a > 0)
                s10(ents[i].b, ents[i].a, i - 1);
            i++;
        }
    }

    if (wCount - 1 > 0) {
        char *p;

        i = wCount - 1;
        p = (char *)this + i * BR_SLOT;
        strcpy(p + 0x35, g_szBr396F08);
        p[0x34] = 0;
        *(short *)(p + 0x448) = 0;
        *(short *)((char *)this + (i + 1) * BR_SLOT) = 0;
        *(short *)(p + 0x436) = 0;
        *(int *)(p + 0x450) = 0;
        *(int *)(p + 0x454) = 0;
        *(int *)(p + 0x458) = 0;
        *(int *)(p + 0x45C) = 0;
        *(int *)(p + 0x43C) = 0;
        *(int *)(p + 0x440) = 0;
        *(int *)(p + 0x444) = 0;
        *(int *)(p + 0x44C) = 0;
        ents[i].a = 0;
        ents[i].b = 0;
    }

    wCount--;
    return 1;
}
