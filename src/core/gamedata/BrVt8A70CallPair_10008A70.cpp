/* WHAT IT DOES: look a value up and pass the result straight to the handler
 * for it -- a two-step lookup written as one call. */
/* @implements 0x10008A70 glide BrVt8A70CallPair
 * @cpp_kind method
 * @cpp_symbol ?CallPair@Vt8A70@@QAEXH@Z
 *
 * 25 B thiscall, one stack arg. Nested vcall pair: slot 3 transforms the
 * argument, slot 9 consumes the result. C++ pushes the inner result
 * before reloading ecx -- the C fastcall twin cannot order it that way.
 */
class Vt8A70 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual int  s3(int);          /* +0x0C */
    virtual void s4(); virtual void s5(); virtual void s6();
    virtual void s7(); virtual void s8();
    virtual void s9(int);          /* +0x24 */
    void CallPair(int a);
};

void Vt8A70::CallPair(int a)
{
    s9(s3(a));
}
