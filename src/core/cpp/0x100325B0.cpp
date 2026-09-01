/* @implements 0x100325B0 glide BrExt8F30
 * @cpp_kind method
 * @cpp_symbol ?BrExt8F30@@YAXH@Z
 *
 * App shutdown chain (D3D twin 0x10038F30). One slot-6 vcall on the
 * current phase (guarded by two globals), then a fixed teardown call
 * sequence with three guarded function-pointer globals, one static-
 * object thiscall (`mov ecx,imm; call M`), two imports at the tail —
 * the last one takes this function's only argument.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Phase {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6(int);
    int pad[0x19];
    int f68;
};

typedef char chk_f68[(unsigned)&((Phase *)0)->f68 == 0x68 ? 1 : -1];

class Obj0810 {
public:
    void Close8B50();
};

extern "C" {
Phase *g_cur;
int g_active;
void (*g_fpVid)(void);
void (*g_fpNet)(void);
void (*g_fpSnd)(void);
int g_replayOn;
int g_podOpen;
void Fn19A10(void);
void Fn13F00(void);
void Fn72840(void);
void Fn71EB0(void);
void Fn720A0(void);
void Fn6C6A0(void);
void Fn5F50(int);
void Fn35660(void);
void Fn355F0(void);
void Fn3030(void);
void Fn8D60(void);
void Fn5A6A0(void);
void Fn17F10(void);
void Fn6D2A0(void);
}

extern Obj0810 g_obj0810;

_CRTIMP void __cdecl ImpTail0(void);
_CRTIMP void __cdecl ImpTail1(int);

void BrExt8F30(int a1)
{
    if (g_cur != 0 && g_active != 0) {
        g_cur->f68 = 0;
        g_cur->v6(0);
    }
    Fn19A10();
    Fn13F00();
    if (g_fpVid != 0)
        g_fpVid();
    Fn72840();
    Fn71EB0();
    Fn720A0();
    Fn6C6A0();
    if (g_replayOn != 0)
        Fn5F50(1);
    Fn35660();
    Fn355F0();
    if (g_podOpen != 0)
        Fn3030();
    Fn8D60();
    if (g_fpNet != 0)
        g_fpNet();
    if (g_fpSnd != 0)
        g_fpSnd();
    Fn5A6A0();
    g_obj0810.Close8B50();
    Fn17F10();
    Fn6D2A0();
    ImpTail0();
    ImpTail1(a1);
}
