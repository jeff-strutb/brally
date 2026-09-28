/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003DC20 glide Ctl3DC20
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3DC20@@QAEHXZ
 *
 * Family-1 installer plus BrOptOpen2954 tail. First-time path returns 1
 * after HostGo so g_host stays live in eax across the g_inited test
 * (mov ecx,[g_inited], not a1). The inited path is a second `if (g_host)
 * HostGo()` — that is the orig `test eax; je` before the shared call.
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

typedef char chk_sz[sizeof(Phase) == 0xC8 ? 1 : -1];

extern "C" Phase *DAT_10ac5cac;   /* the original global; was a per-file stand-in definition */
#define g_slot DAT_10ac5cac
Phase *g_cur;
extern "C" int DAT_100a9360;   /* the original global; was a per-file stand-in definition */
#define g_mode DAT_100a9360
int g_host;
int g_inited;
extern "C" int DAT_10ac5bd4;   /* the original global; was a per-file stand-in definition */
#define g_kind DAT_10ac5bd4

/* EnterFn was a stand-in; the original calls FUN_10051600 (?FUN_10051600@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
int FUN_10051600(GameUi *);
#define EnterFn ((void (*)(Phase *))FUN_10051600)
void HostStart(void);
void HostInit(void);
void HostGo(void);

class Ctl3DC20 {
public:
    int Activate();
};

int Ctl3DC20::Activate()
{
    Phase *p;

    p = g_slot;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);
        g_cur->f0C = 1;
        g_cur->f68 = 1;
    } else {
        g_cur = p;
    }
    g_mode = 6;
    if (g_host != 0) {
        if (g_inited == 0) {
            if (g_kind == 2 || g_kind == 3)
                HostStart();
            HostInit();
            g_inited = 1;
            HostGo();
            return 1;
        }
    }
    if (g_host != 0)
        HostGo();
    return 1;
}
