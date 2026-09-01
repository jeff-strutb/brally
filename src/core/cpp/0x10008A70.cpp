/* @implements 0x10008A70 glide BrVt8A70CallPair
 * @cpp_kind method
 * @cpp_symbol ?BrVt8A70CallPair@Vt8A70@@QAEHH@Z
 *
 * Nested self-vcall pair: return v9(v3(a)); vtbl cached in edi across
 * both calls (C++ member-call order).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Vt8A70 {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual int v3(int);
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual int v9(int);
    int BrVt8A70CallPair(int a);
};

int Vt8A70::BrVt8A70CallPair(int a)
{
    return v9(v3(a));
}
