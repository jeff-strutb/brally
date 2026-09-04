/* WHAT IT DOES: populate the session list -- asks the host for its
 * description if this machine is hosting, otherwise fills the list from what
 * the browser found. */
/* @implements 0x1003DD20 glide Fn3DD20
 * @cpp_kind method
 * @cpp_symbol ?Fn3DD20@@YAHXZ
 *
 * Free cdecl (no unused-this `push ecx`; extra `sub esp,8` so the
 * prologue is `mov eax, fs:[0]` first). One `new Phase` (maxState=1,
 * unwind operator delete). Ctor DECLARED, no dtor. Live 1 in esi.
 *
 * Obj hook: do not name a pointer for g_obj (`a1` eax). Direct class*
 * member access puts the object in ecx; int f08 occupies eax.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Phase;

typedef void (*PhaseEnterFn)(Phase *);

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    void *pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};

struct Item {
    int f00;
    int f04;
};

class Obj {
public:
    char pad[8];
    int f08;
};

typedef char chk_sz[sizeof(Phase) == 0xC8 ? 1 : -1];
typedef char chk_c[(unsigned)&((Phase *)0)->f0C == 0xC ? 1 : -1];
typedef char chk_68[(unsigned)&((Phase *)0)->f68 == 0x68 ? 1 : -1];

Phase *g_slot;
Phase *g_cur;
int g_flag;
int g_host;
void *g_pHost;
int g_mode;
int g_inited;
Obj *g_obj;

void ResetSlots(void);
void GetDesc(void *, Item **);
int ActivateD140(int);
int ActivateD220(int);
int ActivateD3C0(int);
int ActivateD620(int);
int ActivateD930(int);
int ActivateD7D0(int);
void EnterFn(Phase *);
void HostFirst(void);
void HostAgain(void);
void ObjHook(Obj *, int);

typedef void (__stdcall *Host_f7C)(void *self, void *item, int z);

int Fn3DD20(void)
{
    Phase *p;
    Item *item;
    void *host;
    int one;
    int h;

    one = 1;
    g_flag = one;
    ResetSlots();

    if (g_host != 0) {
        item = 0;
        host = g_pHost;
        if (host != 0)
            GetDesc(host, &item);
        if (item != 0) {
            item->f04 &= ~0x20;
            host = g_pHost;
            ((Host_f7C)(*(void ***)host)[0x1F])(host, item, 0);
        }
    }

    ActivateD140(0);
    ActivateD220(0);
    ActivateD3C0(0);
    if (g_host != 0) {
        ActivateD620(0);
        ActivateD930(0);
    } else {
        ActivateD7D0(0);
    }

    p = g_slot;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);
        g_cur->f0C = one;
        g_cur->f68 = one;
    } else {
        g_cur = p;
    }

    h = g_host;
    g_mode = 6;
    if (h != 0) {
        if (g_inited == 0) {
            HostFirst();
            g_inited = one;
            goto after_host;
        }
    }
    if (h != 0)
        HostAgain();
after_host:
    ;

    if (g_obj != 0 && g_obj->f08 != 0)
        ObjHook(g_obj, g_obj->f08);
    return one;
}
