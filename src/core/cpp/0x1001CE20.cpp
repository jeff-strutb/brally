/* @implements 0x1001CE20 glide BrAppStateSetMode
 * @cpp_kind method
 * @cpp_symbol ?BrAppStateSetMode@@YAHXZ
 *
 * Dense-class row, strong on the C++ screen (both vcall arms: EAX and
 * EDX scratch). Entry forks on (current-phase, active) pair; the
 * demo-globals arm sets the state batch; otherwise dispatch v4()/v3()
 * on the phase by its f0C flag, then the timed phase-leave (f68=0,
 * v6(0)) once the 90000-tick window expires.
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
    int f04;
    int f08;
    int f0C;
    int pad[0x16];
    int f68;
};

typedef char chk_c[(unsigned)&((Phase *)0)->f0C == 0xC ? 1 : -1];
typedef char chk_f68[(unsigned)&((Phase *)0)->f68 == 0x68 ? 1 : -1];

extern "C" {
Phase *g_cur;
int g_active;
int g_scrW2, g_scrH2, g_scrW3, g_scrW4, g_scrH3, g_scrH4;
int g_vidMode;
int g_demoFlag;
int g_time;
int g_mode;
int g_b3014;
int g_226e7c, g_226e80;
int g_b71530;
int *g_b71534;
extern int g_b71290Obj[];
int g_7b320, g_7b324, g_7b328, g_7b32c;
int g_bc0;
int g_5bc760;
void Fn6C460(void);
void Fn6C290(int);
void Fn56260(void);
unsigned int Fn6E280(void);
}

int BrAppStateSetMode(void)
{
    int w, h;

    if (g_cur == 0 && g_active == 0) {
        w = 0x280;
        h = 0x1e0;
        g_scrW2 = w;
        g_scrH2 = h;
        g_scrW3 = w;
        g_scrW4 = w;
        g_scrH3 = h;
        g_scrH4 = h;
        Fn6C460();
        g_vidMode = 3;
        return 1;
    }
    if (g_cur == 0 && g_active != 0) {
        Fn6C290(0);
        Fn56260();
        g_time = Fn6E280();
    }
    if (g_demoFlag != 0) {
        g_mode = 1;
        g_b3014 = 2;
        g_226e7c = 5;
        g_226e80 = 0;
        g_b71530 = 0;
        g_b71534 = g_b71290Obj;
        g_7b320 = 1;
        g_7b328 = 1;
        g_7b32c = 2;
        g_7b324 = 1;
        g_active = 0;
        g_vidMode = 3;
        return 1;
    }
    if (g_cur->f0C == 0)
        g_cur->v4();
    else
        g_cur->v3();
    if (g_cur != 0 && g_active != 0) {
        if (g_bc0 != 0) {
            if (g_time + 0x15f90 < Fn6E280()) {
                g_cur->f68 = 0;
                g_cur->v6(0);
                g_mode = 4;
                g_5bc760 = 0;
                return 1;
            }
        } else {
            g_time = Fn6E280();
        }
    }
    return 1;
}
