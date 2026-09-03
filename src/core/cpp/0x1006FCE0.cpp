/* @implements 0x1006FCE0 glide BrCarSlotSetup_1006FCE0
 * @cpp_kind method
 * @cpp_symbol ?SlotSetup@Car6FCE0@@QAEXHH@Z
 *
 * Thiscall, two stack args (`ret 8`), 104 B. Car slot setup: push the
 * three tint bytes at +0x29AC..AE through the tint-scale helper, load the
 * slot, reset the +0x2A70 record, zero the eight dwords at +0x2A90, then
 * hand the slot to the +0x29C4 binder (0x1006FCB0, the one br_carphys.c
 * relies on to have run).
 *
 * The tint bytes are `unsigned char` members widened to int arguments,
 * which VC5 spells `xor r,r; mov r8,[m]` rather than movzx -- all three
 * zeroing instructions hoist above the loads.
 *
 * PARKED at 12 diffs, all from ONE permutation inside the inline memset
 * expansion. Same three instructions, different order:
 *     orig    lea edi,[esi+0x2a90] / mov ecx,8 / xor eax,eax
 *     recomp  mov ecx,8 / xor eax,eax / lea edi,[esi+0x2a90]
 * Everything before and after (including the deferred `add esp,4` sitting
 * between the setup and the `rep stosd`) is byte-identical. The order is
 * fixed inside the expansion, not by the call site.
 * DO NOT RE-PROBE -- unchanged by: dest spelling (array name, &a[0],
 * hoisted `char *p`, hoisted `int *pz` declared at the top of the
 * function), dest type (int[8], unsigned char[32], nested struct +
 * sizeof), and flags /O2, /Ox, /O2 /Oi, /O2 /Ot, /O2 /Op, /O2 /Og /Oi
 * /Ot /Oy /Ob1 (all 12), /O2 /Os (78), /Od (85), /O2 /Oy- (85).
 *
 * This is the second emitter-level residue found in one session that no
 * source form reaches -- see the SIB entry in docs/VC5-IDIOMS.md. Both
 * point at the open compiler patch-level lead rather than at the source.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Car6FCE0 {
public:
    char          pad[0x29AC];
    unsigned char b29AC;        /* +0x29AC */
    unsigned char b29AD;        /* +0x29AD */
    unsigned char b29AE;        /* +0x29AE */
    char          pad29AF[0x2A70 - 0x29AF];
    int           f2A70;        /* +0x2A70 */
    char          pad2A74[0x2A90 - 0x2A74];
    int           a2A90[8];     /* +0x2A90 */

    void Bind(int slot);        /* 0x1006FCB0 */
    void SlotSetup(int slot, int a);
};

typedef char chk_b29AC[(unsigned)&((Car6FCE0 *)0)->b29AC == 0x29AC ? 1 : -1];
typedef char chk_f2A70[(unsigned)&((Car6FCE0 *)0)->f2A70 == 0x2A70 ? 1 : -1];
typedef char chk_a2A90[(unsigned)&((Car6FCE0 *)0)->a2A90 == 0x2A90 ? 1 : -1];

extern "C" {
void BrImgTintSetScale(int r, int g, int b);    /* 0x1005A4E0 */
void BrCarSlotLoad(int slot, int a, int b);     /* 0x1002EBD1 */
void BrSub10074E20(void *pRec);                 /* 0x1006E080 */
}

void Car6FCE0::SlotSetup(int slot, int a)
{
    BrImgTintSetScale(b29AC, b29AD, b29AE);
    BrCarSlotLoad(slot, a, 0);
    BrSub10074E20(&f2A70);
    memset(a2A90, 0, sizeof(a2A90));
    Bind(slot);
}
