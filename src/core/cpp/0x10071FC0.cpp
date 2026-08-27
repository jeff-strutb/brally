/* @implements 0x10071FC0 glide Ctl71FC0
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl71FC0@@QAEHXZ
 *
 * Refcount-gated DirectInput installer. new T of 0x54 (ctor DECLARED, no
 * dtor). DirectInputCreateA is NOT dllimport (orig E8 to a JMP thunk);
 * MessageBoxA IS dllimport (orig FF 15). No NULL-return after new.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class DiDev {
public:
    char _[0x54];
    DiDev();
    void Init(void *);
};

typedef char chk_sz[sizeof(DiDev) == 0x54 ? 1 : -1];

int g_refcount;
void *g_hinst;
void *g_pDI;
void *g_hwnd;
DiDev *g_obj;

char *GetStr(int);
int __stdcall DirectInputCreateA(void *, unsigned, void **, void *);
int __declspec(dllimport) __stdcall MessageBoxA(void *, const char *, const char *, unsigned);

class Ctl71FC0 {
public:
    int Activate();
};

int Ctl71FC0::Activate()
{
    DiDev *p;

    if (++g_refcount != 1)
        return 1;
    if (DirectInputCreateA(g_hinst, 0x500, &g_pDI, 0) < 0) {
        MessageBoxA(g_hwnd, GetStr(0x127), GetStr(0x126), 0x10);
        return 0;
    }
    p = new DiDev;
    g_obj = p;
    p->Init(g_hwnd);
    return 1;
}
