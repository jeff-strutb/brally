/* WHAT IT DOES: assigns a control to an action in one of the four control
 * profiles: writes the new key code (mixed with its modifier byte) into the
 * action's slot, then refills the two neighbouring slots from the profile's
 * default table, clearing each refilled slot again if the same key is
 * already bound to another action in that profile. */
/* @implements 0x10062B80 glide BrCtrlCfgAssign
 * @cpp_kind method
 * @cpp_symbol ?Assign@CtrlCfg62B80@@QAEXHHHH@Z
 *
 * Thiscall, FOUR stack args (`ret 0x10`), 169 B -- unreachable from the C
 * lane, whose __fastcall spelling would take the profile in edx.  The
 * profile argument's own stack slot is SPENT as the outer loop counter
 * (`mov [esp+0x14], 2` ... `dec eax; mov [esp+0x14], eax`); the fresh
 * counter local lands there only with `int mod` (a byte-typed mod frees
 * its own slot first and the counter takes that one instead).  The
 * profile select is a `dec/je` chain with the default arm FIRST, i.e. a
 * switch on the profile number; the default table is kept as a running
 * DELTA (`sub edi,ebp`, then `[edi+esi]`) -- `def -= (int)base` in place;
 * the binding scan keeps both a counter and a walker only as
 * `for (q = base; i < 0x1c; i++, q += 3)` with `i = 0` set before the
 * slot store.
 *
 * T2 2026-09-13 (fresh, 22 cpp probes): 169/176 B, 60/59 insns, every
 * instruction shape present.  RESIDUE: one register plan.  The original
 * homes base in ebp / the delta in edi / the middle counter in ebx and
 * loads mod into dl and key into ecx (`xor dl,cl; and edx,0xff;
 * xor edx,ecx`); ours homes base in edi / delta in ebx / counter in ebp
 * and loads mod into cl and key into eax, widening the byte through a
 * zeroed dx (`xor dx,dx; xor cl,al; mov dl,cl`).  Dead: int/ushort/uchar
 * for key and mod in every combination, the mix as one expression, as
 * `mod ^= key`, through an int or uchar temp, mix-before-p and after,
 * `key` copied to a local first, counter as `profile` or a fresh local.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

extern "C" {
unsigned short DAT_100b38a0[];      /* default bindings, profile 0 */
unsigned short DAT_100b3948[];      /* profile 1 */
unsigned short DAT_100b39f0[];      /* profile 2 */
unsigned short DAT_100b3a98[];      /* profile 3 */
}

class CtrlCfg62B80 {
public:
    unsigned short tab[4][0x1c * 3];    /* +0x000, +0x0A8, +0x150, +0x1F8 */

    void Assign(int profile, int action, int key, int mod);
};

typedef char chk_tab62B80[sizeof(((CtrlCfg62B80 *)0)->tab[0]) == 0xA8 ? 1 : -1];

void CtrlCfg62B80::Assign(int profile, int action, int key, int mod)
{
    unsigned short *base;
    char           *def;
    unsigned short *p;
    unsigned short *q;
    unsigned short  v;
    int             i;
    int             k;
    int             n;

    switch (profile) {
    case 1:  base = tab[1]; def = (char *)DAT_100b3948; break;
    case 2:  base = tab[2]; def = (char *)DAT_100b39f0; break;
    case 3:  base = tab[3]; def = (char *)DAT_100b3a98; break;
    default: base = tab[0]; def = (char *)DAT_100b38a0; break;
    }

    n = 2;
    p = base + action * 3;
    def -= (int)base;
    *p = (unsigned short)((unsigned char)(mod ^ key) ^ key);
    do {
        p = p + 1;
        k = 0x1c;
        do {
            v = *(unsigned short *)(def + (int)p);
            i = 0;
            *p = v;
            for (q = base; i < 0x1c; i++, q += 3) {
                if (*q == v) {
                    *p = 0;
                    break;
                }
            }
            k--;
        } while (k != 0);
        n--;
    } while (n != 0);
}
