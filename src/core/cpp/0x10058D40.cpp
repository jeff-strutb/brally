/* @implements 0x10058D40 glide Ctl58D40
 * @cpp_kind method
 * @cpp_symbol ?Rebuild@Ctl58D40@@QAEXXZ
 *
 * Tear down the existing Node list, then new Node (0x14) per table row
 * whose w*h*6 fits in (g_mem << 20). maxState=1: one live new at a time.
 * Ctor and dtor DECLARED (unwind is operator delete of the in-flight new,
 * not the list teardown).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Node {
public:
    int w;
    int h;
    int f8;
    int fC;
    Node *next;
    Node();
    ~Node();
    void Append(Node *);
};

typedef char chk_sz[sizeof(Node) == 0x14 ? 1 : -1];
typedef char chk_n[(unsigned)&((Node *)0)->next == 0x10 ? 1 : -1];

struct Dim {
    int w;
    int h;
};

Node *g_head;
int g_count;
int g_mem;

Dim g_tab[7] = {
    {0x200, 0x180},
    {0x280, 0x190},
    {0x280, 0x1E0},
    {0x320, 0x258},
    {0x3C0, 0x2D0},
    {0x358, 0x1E0},
    {0x400, 0x300},
};

class Ctl58D40 {
public:
    void Rebuild();
};

void Ctl58D40::Rebuild()
{
    Node *p;
    Node *n;
    int *d;

    p = g_head;
    if (p != 0) {
        delete p;
        g_head = 0;
        g_count = 0;
    }
    /* esi walks the .h of each pair ([esi]=h, [esi-4]=w) up to &g_tab[7].h */
    for (d = &g_tab[0].h; (int)d < (int)&g_tab[7].h; d += 2) {
        if (d[-1] * d[0] * 6 > (g_mem << 20))
            continue;
        n = new Node;
        n->w = d[-1];
        n->h = d[0];
        n->f8 = 0x10;
        n->fC = 0;
        if (g_head == 0)
            g_head = n;
        else
            g_head->Append(n);
        g_count++;
    }
}
