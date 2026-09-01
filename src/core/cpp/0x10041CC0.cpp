/* @implements 0x10041CC0 glide BrPhaseDtor_10048870
 * @cpp_kind dtor
 * @cpp_symbol ??1Ph48870@@UAE@XZ
 *
 * Virtual destructor, no EH frame (members are plain pointers): vptr
 * reset, then two guarded slot-0 `f00(1)` releases on the Phase members
 * at +0xC0/+0xC4, each zeroed after. The +0xC0 member read is hoisted
 * above the vptr store by the scheduler.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Phase {
public:
    virtual void *f00(int);
};

class Ph48870 {
public:
    virtual ~Ph48870();
    char pad[0xBC];
    Phase *fC0;
    Phase *fC4;
};

typedef char chk_c0[(unsigned)&((Ph48870 *)0)->fC0 == 0xC0 ? 1 : -1];
typedef char chk_c4[(unsigned)&((Ph48870 *)0)->fC4 == 0xC4 ? 1 : -1];

Ph48870::~Ph48870()
{
    Phase *p;
    Phase *q;

    p = fC0;
    if (p != 0)
        p->f00(1);
    fC0 = 0;
    q = fC4;
    if (q != 0)
        q->f00(1);
    fC4 = 0;
}
