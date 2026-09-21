/* WHAT IT DOES: deep-copies one 0x874-byte record into this one: four
 * 168-byte sub-blocks, then the selector at +0x2A0 -- and rather than copy
 * the +0x2A4 pointer, re-points it at THIS object's own sub-block 0/1/2/3
 * chosen by the selector (so the copy never aliases the source) -- then the
 * rest of the record: three dwords, a 0x104 block, a 0x400 block, four
 * dwords, one 16-byte block, fourteen dwords, a 0x20 block, a 0x40 block
 * and the final dword. */
/* @implements 0x10062E50 glide BrRec874Copy_10062E50
 * @cpp_kind method
 * @cpp_symbol ?Copy@Rec62E50@@QAEXPAV1@@Z
 *
 * Thiscall, one stack arg (`ret 4`), 524 B, no calls -- which is why VC5
 * keeps `this` in EAX.  The block sizes are load-bearing: 0xA8/0x104/0x400/
 * 0x20/0x40 struct assignments expand to `rep movsd`, and the 16-byte one
 * at +0x7C8 expands to four mov pairs through a lea'd base pair -- Ghidra
 * renders those four as plain scalars, but the elected-base addressing in
 * the original is the small-struct-copy expansion.  The selector is a
 * switch on a NAMED local (the dec/je chain runs on the loaded register);
 * the case pointers fall out of the dest-lea CSE from the four big copies.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct CpyA { char b[0xA8]; };     /* rep movsd 0x2a  */
struct CpyB { char b[0x104]; };    /* rep movsd 0x41  */
struct CpyC { char b[0x400]; };    /* rep movsd 0x100 */
struct CpyD { char b[0x10]; };     /* four mov pairs  */
struct CpyE { char b[0x20]; };     /* rep movsd 8     */
struct CpyF { char b[0x40]; };     /* rep movsd 0x10  */

class Rec62E50 {
public:
    CpyA  a0;                       /* +0x000 */
    CpyA  a1;                       /* +0x0A8 */
    CpyA  a2;                       /* +0x150 */
    CpyA  a3;                       /* +0x1F8 */
    int   sel;                      /* +0x2A0 */
    CpyA *pSel;                     /* +0x2A4 -> own a0..a3 */
    int   f2A8;                     /* +0x2A8 */
    int   f2AC;
    int   f2B0;
    CpyB  b2B4;                     /* +0x2B4 */
    CpyC  c3B8;                     /* +0x3B8 */
    int   f7B8;                     /* +0x7B8 */
    int   f7BC;
    int   f7C0;
    int   f7C4;
    CpyD  d7C8;                     /* +0x7C8 */
    int   f7D8;                     /* +0x7D8 */
    int   f7DC;
    int   f7E0;
    int   f7E4;
    int   f7E8;
    int   f7EC;
    int   f7F0;
    int   f7F4;
    int   f7F8;
    int   f7FC;
    int   f800;
    int   f804;
    int   f808;
    int   f80C;
    CpyE  e810;                     /* +0x810 */
    CpyF  g830;                     /* +0x830 */
    int   f870;                     /* +0x870 */

    void Copy(Rec62E50 *pSrc);      /* 0x10062E50 -- defined here */
};

typedef char chk_a1[(unsigned)&((Rec62E50 *)0)->a1   == 0x0A8 ? 1 : -1];
typedef char chk_a3[(unsigned)&((Rec62E50 *)0)->a3   == 0x1F8 ? 1 : -1];
typedef char chk_se[(unsigned)&((Rec62E50 *)0)->sel  == 0x2A0 ? 1 : -1];
typedef char chk_b4[(unsigned)&((Rec62E50 *)0)->b2B4 == 0x2B4 ? 1 : -1];
typedef char chk_c8[(unsigned)&((Rec62E50 *)0)->c3B8 == 0x3B8 ? 1 : -1];
typedef char chk_b8[(unsigned)&((Rec62E50 *)0)->f7B8 == 0x7B8 ? 1 : -1];
typedef char chk_d8[(unsigned)&((Rec62E50 *)0)->d7C8 == 0x7C8 ? 1 : -1];
typedef char chk_10[(unsigned)&((Rec62E50 *)0)->e810 == 0x810 ? 1 : -1];
typedef char chk_30[(unsigned)&((Rec62E50 *)0)->g830 == 0x830 ? 1 : -1];
typedef char chk_70[(unsigned)&((Rec62E50 *)0)->f870 == 0x870 ? 1 : -1];

void Rec62E50::Copy(Rec62E50 *pSrc)
{
    int s;

    a0 = pSrc->a0;
    a1 = pSrc->a1;
    a2 = pSrc->a2;
    a3 = pSrc->a3;
    s = pSrc->sel;
    sel = s;
    switch (s) {
    case 1:
        pSel = &a1;
        break;
    case 2:
        pSel = &a2;
        break;
    case 3:
        pSel = &a3;
        break;
    default:
        pSel = &a0;
        break;
    }
    f2A8 = pSrc->f2A8;
    f2AC = pSrc->f2AC;
    f2B0 = pSrc->f2B0;
    b2B4 = pSrc->b2B4;
    c3B8 = pSrc->c3B8;
    f7B8 = pSrc->f7B8;
    f7BC = pSrc->f7BC;
    f7C0 = pSrc->f7C0;
    f7C4 = pSrc->f7C4;
    d7C8 = pSrc->d7C8;
    f7D8 = pSrc->f7D8;
    f7DC = pSrc->f7DC;
    f7E0 = pSrc->f7E0;
    f7E4 = pSrc->f7E4;
    f7E8 = pSrc->f7E8;
    f7EC = pSrc->f7EC;
    f7F0 = pSrc->f7F0;
    f7F4 = pSrc->f7F4;
    f7F8 = pSrc->f7F8;
    f7FC = pSrc->f7FC;
    f800 = pSrc->f800;
    f804 = pSrc->f804;
    f808 = pSrc->f808;
    f80C = pSrc->f80C;
    e810 = pSrc->e810;
    g830 = pSrc->g830;
    f870 = pSrc->f870;
}
