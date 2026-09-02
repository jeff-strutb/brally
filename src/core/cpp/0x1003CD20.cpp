/* @implements 0x1003CD20 glide BrOpt37D0
 * @cpp_kind method
 * @cpp_symbol BrOpt37D0
 *
 * Free cdecl (GameObj*): when both gates are set, clears the pSub phase's
 * +0x68 flag (pSub at +0x2AE8, RE-READ for the call receiver exactly as
 * the original does), fires its +0x18 vcall with a pushed 0 (EDX
 * pattern, arg pushed before the receiver loads), then hands 0 to the
 * shutdown sequence 0x100325B0.  Always returns 1.  No EH (no new).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BrPhaseCD {
public:
    virtual int s0();
    virtual int s1();
    virtual int s2();
    virtual int s3();
    virtual int s4();
    virtual int s5();
    virtual int f18(void *);   /* +0x18 */

    char pad04[0x68 - 4];
    int  f68;
};

struct BrGameObjCD {
    char        pad00[0x2AE8];
    BrPhaseCD  *pSub;          /* +0x2AE8 */
};

extern "C" {
extern int DAT_10ac5bec;
extern int DAT_10ac4090;

int BrExt_10038F30(int a);     /* 0x100325B0 */

int BrOpt37D0(BrGameObjCD *pObj)
{
    if (DAT_10ac5bec != 0 && DAT_10ac4090 != 0) {
        pObj->pSub->f68 = 0;
        pObj->pSub->f18(0);
        BrExt_10038F30(0);
    }
    return 1;
}
}
