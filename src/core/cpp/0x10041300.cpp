/* @implements 0x10041300 glide BrItemInit_10041300
 * @cpp_kind method
 * @cpp_symbol ?Init@Obj41300@@QAEXPADHDPAH@Z
 *
 * Thiscall, four stack args (`ret 0x10`), 247 B. Initialise the embedded
 * +0x2B5C item: copy the caller's label in, fold the flag bits into the
 * item's flag word, stamp its kind byte, clear the four measurement
 * fields, seed its geometry from the owner's +0x3C/+0x40 pair and the
 * caller's rect, then let the item lay itself out through one of two
 * vcalls (kind 3 takes the +0x08 slot, everything else +0x04) and, when
 * bit 0 of the flags is set, through the +0x28 measure vcall whose float
 * result is thrown away. Finally the owner caches the laid-out numbers.
 *
 * THE ITEM IS THE 0x438 RECORD from 0x10054E20's slot array, and this is
 * where its head is pinned down: the dword at +0x00 is a VTABLE pointer
 * (the three vcalls all go through the one cached load), +0x04 is the
 * flag word, +0x08 the kind byte, +0x09 the label. Everything from
 * +0x40A on matches the offsets that function clears, and the fields sum
 * to exactly 0x438. Keep the two files' layouts in step.
 *
 * The owner's +0x3C/+0x40 pair reaches the item as integer movs, so it is
 * copied with the dword-pun spelling; the SAME +0x40 is then read as a
 * float for the `(int)` conversion, which is the usual VC5 `call __ftol`.
 * That conversion's result is kept in a local -- the original adds the
 * item's +0x40C short to the value still in eax rather than reloading
 * +0x54.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Rec438 {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 */
    virtual void  s2();         /* +0x08 */
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();        /* +0x28 -- measure, result discarded */

    int   f004;                 /* +0x004 flag word */
    char  b008;                 /* +0x008 kind */
    char  szName[0x401];        /* +0x009 */
    short w40A;                 /* +0x40A */
    short w40C;                 /* +0x40C */
    short pad40E;
    int   f410;                 /* +0x410 */
    int   f414;
    int   f418;
    short w41C;                 /* +0x41C */
    short pad41E;
    int   f420;                 /* +0x420 */
    int   a424[4];              /* +0x424 */
    int   f434;                 /* +0x434 */
};                              /* 0x438 */

typedef char chk_rec[sizeof(Rec438) == 0x438 ? 1 : -1];
typedef char chk_name[(unsigned)&((Rec438 *)0)->szName == 9 ? 1 : -1];
typedef char chk_w40C[(unsigned)&((Rec438 *)0)->w40C == 0x40C ? 1 : -1];

class Obj41300 {
public:
    char   pad000[0x3C];
    int    f03C;                /* +0x03C */
    int    f040;                /* +0x040 */
    char   pad044[4];
    short  w048;                /* +0x048 */
    short  w04A;                /* +0x04A */
    char   pad04C[4];
    int    f050;                /* +0x050 */
    int    f054;                /* +0x054 */
    int    f058;                /* +0x058 */
    int    f05C;                /* +0x05C */
    char   pad060[0x2B5C - 0x60];
    Rec438 m2B5C;               /* +0x2B5C */

    void Init(char *pName, int flags, char kind, int *pRect);
};

typedef char chk_item[(unsigned)&((Obj41300 *)0)->m2B5C == 0x2B5C ? 1 : -1];
typedef char chk_f03C[(unsigned)&((Obj41300 *)0)->f03C == 0x3C ? 1 : -1];
typedef char chk_f05C[(unsigned)&((Obj41300 *)0)->f05C == 0x5C ? 1 : -1];

void Obj41300::Init(char *pName, int flags, char kind, int *pRect)
{
    int v;

    strcpy(m2B5C.szName, pName);

    m2B5C.f004 |= flags;
    m2B5C.b008 = kind;
    m2B5C.w41C = 0;
    m2B5C.w40C = 0;
    m2B5C.w40A = 0;
    m2B5C.a424[0] = pRect[0];
    m2B5C.a424[2] = pRect[2];
    m2B5C.f410 = f03C;
    m2B5C.f414 = f040;
    m2B5C.f418 = 0;
    m2B5C.f420 = 0;

    if (kind == 3)
        m2B5C.s2();
    else
        m2B5C.s1();

    if (flags & 1)
        m2B5C.s10();

    v = (int)*(float *)&f040;
    f054 = v;
    f050 = pRect[0];
    f058 = pRect[2];
    f05C = v + m2B5C.w40C;
    w048 = m2B5C.w40A;
    w04A = m2B5C.w40C;
}
