#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

#include <string.h>

#pragma intrinsic(strlen)

class Phase;

typedef void (*PhaseEnterFn)(Phase *);
typedef int (*PhaseHookFn)(void *);

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    PhaseHookFn pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};

struct Ctl1E164 {
    char _[0x1E164];
    unsigned short f1E164;
};

typedef char chk_sz[sizeof(Phase) == 0xC8 ? 1 : -1];

Phase *g_slot;
Phase *g_cur;
int g_inGame;
int g_hostFlag;
int g_mode;
int g_joinFlag;
char g_name[1];
void *g_AC5D30;
Ctl1E164 *g_AC5D2C;
Phase *g_hookObj;

void EnterFn(Phase *);
int HookFn(void *);
int JoinFn(void);
void SetupFn(void);

class Ctl3D7D0 {
public:
    int Activate();
};

int Ctl3D7D0::Activate()
{
    Phase *p;
    int r;

    g_hostFlag = 0;
    g_joinFlag = 0;
    if (g_inGame != 0)
        goto activate;
    if (g_mode != 2 && g_mode != 3) {
        if (JoinFn() == 0)
            goto done;
        goto activate;
    }
    g_joinFlag = 1;
    if (g_mode == 2 && strlen(g_name) < 7)
        goto done;
    if (g_AC5D30 == 0)
        goto setup;
    if (g_AC5D2C->f1E164 <= 0)
        goto setup;
    if (JoinFn() == 0)
        goto done;
    goto activate;
setup:
    SetupFn();
    goto done;

activate:
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
    g_hookObj->pfnHook = HookFn;
done:
    r = 1;
    return r;
}
