/* @implements 0x10039870 glide BrCfgFindConflicts
 * @cpp_kind free
 * @cpp_symbol ?BrCfgFindConflicts@@YAHH@Z
 *
 * cdecl, ONE arg, `ret`, 277 B. Walk every pair of the 21 control bindings
 * and mark both halves of any pair whose two answers agree, returning
 * whether anything clashed. The per-binding flag is zeroed at the top of
 * the outer pass, which is what makes a flag set by an earlier pass on a
 * higher index disappear -- that is the original's behaviour, not a bug in
 * the transcription.
 *
 * The port body in slice2_23.c takes `(BrUiGlobals *pG, int kind, void *)`;
 * the original is cdecl with ONE argument, the bindings and flags are
 * direct globals, and the two lookups are thiscall members of a global
 * object at 0x10B71290 (`mov ecx, offset g_obj`). Same split as the
 * 0x10038650 sibling.
 *
 * Two details the original spells out and the port comments:
 *  - the outer counter is bumped BEFORE its bound test, so the inner loop
 *    is skipped only on the final pass -- keep the `i + 1 >= 21` form.
 *  - the 0x0C/0x0D/0x0E skip applies only while the OUTER index is below
 *    12 (`cmp ebx, 0x100AAB34`), not the inner one.
 *
 * VC5 strength-reduces both loops to pointer walks and reuses the incoming
 * argument's home slot for the byte answer once the argument is dead.
 *
 * DO NOT cache the binding key in a local: the original re-reads
 * `g_brBindAAAD4[n].key` for EACH of the two lookups (and again for the
 * skip test). Caching it costs an extra spill slot -- `sub esp,0x18` where
 * the original has `sub esp,0x14` -- and rotates the loop registers.
 * Re-reading is worth all 201 diffs of the first draft.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct BrBind39870 {
    unsigned int key;           /* +0x00 */
    int          f04;           /* +0x04 */
};

class Cfg39870 {
public:
    int  GetA(int kind, unsigned int key);      /* 0x10062C30 */
    char GetB(int kind, unsigned int key);      /* 0x10062CA0 */
};

extern "C" {
Cfg39870    g_brCfgB71290;          /* 0x10B71290 */
BrBind39870 g_brBindAAAD4[21];      /* 0x100AAAD4 .. 0x100AAB7C */
int         g_brFlag4648[21];       /* 0x10AC4648 */
}

int BrCfgFindConflicts(int kind)
{
    int found = 0;
    int i;

    for (i = 0; i < 21; i++) {
        int  aI;
        char bI;
        int  j;

        g_brFlag4648[i] = 0;

        aI = g_brCfgB71290.GetA(kind, g_brBindAAAD4[i].key);
        bI = g_brCfgB71290.GetB(kind, g_brBindAAAD4[i].key);

        if (i + 1 >= 21)
            continue;

        for (j = i + 1; j < 21; j++) {
            int  aJ;
            char bJ;

            if (i < 12 && (g_brBindAAAD4[j].key == 0x0C
                           || g_brBindAAAD4[j].key == 0x0D
                           || g_brBindAAAD4[j].key == 0x0E))
                continue;

            aJ = g_brCfgB71290.GetA(kind, g_brBindAAAD4[j].key);
            bJ = g_brCfgB71290.GetB(kind, g_brBindAAAD4[j].key);

            if (aI == 0 && bI == 0)
                continue;
            if (aI != aJ)
                continue;
            if (bI != bJ)
                continue;

            found = 1;
            g_brFlag4648[j] = 1;
            g_brFlag4648[i] = 1;
        }
    }

    return found;
}
