/* @implements 0x1003F610 glide Ctl3F610
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3F610@@QAEHXZ
 *
 * ResetBuf, MusicFn(3, 0x200020), slot load, g_track=3 (both paths,
 * interleaved after the test), then shared-return activate.
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

Phase *g_slot;
Phase *g_cur;
char g_buf;
int g_track;

void EnterFn(Phase *);
void ResetBuf(void *);
void MusicFn(int, unsigned);

class Ctl3F610 {
public:
    int Activate();
};

int Ctl3F610::Activate()
{
    Phase *p;

    ResetBuf(&g_buf);
    MusicFn(3, 0x200020);
    p = g_slot;
    g_track = 3;
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
    return 1;
}
