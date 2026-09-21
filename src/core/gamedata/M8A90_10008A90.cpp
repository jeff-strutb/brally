/* WHAT IT DOES: the same lookup-then-handle pattern over two arguments. */
/* @implements 0x10008A90 glide M8A90
 * @cpp_kind method
 * @cpp_symbol ?M8A90@Vt8A90@@QAEHHH@Z
 *
 * Twin of 0x10008A70: return v7(v3(a, b)); vtbl cached in edi.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Vt8A90 {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual int v3(int, int);
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual int v7(int);
    int M8A90(int a, int b);
};

int Vt8A90::M8A90(int a, int b)
{
    return v7(v3(a, b));
}
